#include "icon_theme_service.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMimeDatabase>
#include <QMimeType>
#include <QPainter>
#include <QPainterPath>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>

#include <algorithm>

namespace Astrea::Explorer::Native::Services {

namespace {

constexpr int kMaxRenderedCacheEntries = 256;
constexpr int kMaxIconSize = 512;

QStringList specialDirectoryCandidates(const QString &path)
{
    const QString normalized = QDir::cleanPath(QFileInfo(path).absoluteFilePath());
    const QList<QPair<QStandardPaths::StandardLocation, QString>> locations {
        {QStandardPaths::DesktopLocation, QStringLiteral("user-desktop")},
        {QStandardPaths::DownloadLocation, QStringLiteral("folder-download")},
        {QStandardPaths::DocumentsLocation, QStringLiteral("folder-documents")},
        {QStandardPaths::PicturesLocation, QStringLiteral("folder-pictures")},
        {QStandardPaths::MusicLocation, QStringLiteral("folder-music")},
        {QStandardPaths::MoviesLocation, QStringLiteral("folder-videos")},
        {QStandardPaths::TemplatesLocation, QStringLiteral("folder-templates")},
        {QStandardPaths::PublicShareLocation, QStringLiteral("folder-publicshare")},
    };

    for (const auto &[location, iconName] : locations) {
        for (const QString &candidate : QStandardPaths::standardLocations(location)) {
            if (!candidate.isEmpty() && QDir::cleanPath(QFileInfo(candidate).absoluteFilePath()) == normalized) {
                return {iconName};
            }
        }
    }

    if (normalized == QDir::cleanPath(QDir::homePath())) {
        return {QStringLiteral("user-home")};
    }

    const QString trashPath = QDir(QStandardPaths::writableLocation(QStandardPaths::HomeLocation))
        .filePath(QStringLiteral(".local/share/Trash/files"));
    if (normalized == QDir::cleanPath(QFileInfo(trashPath).absoluteFilePath())) {
        return {QStringLiteral("user-trash")};
    }

    return {};
}

QString fileNameForMimeLookup(const QString &path)
{
    const QUrl url(path);
    if (url.isValid() && !url.scheme().isEmpty()) {
        return QFileInfo(url.path()).fileName();
    }
    return QFileInfo(path).fileName();
}

QSize boundedPixelSize(const QSize &logicalSize, qreal devicePixelRatio)
{
    const qreal dpr = std::clamp(devicePixelRatio, 0.5, 4.0);
    const int width = std::clamp(qRound(std::max(1, logicalSize.width()) * dpr), 1, kMaxIconSize * 4);
    const int height = std::clamp(qRound(std::max(1, logicalSize.height()) * dpr), 1, kMaxIconSize * 4);
    return {width, height};
}

bool isSymbolicName(const QString &name)
{
    return name.endsWith(QStringLiteral("-symbolic"));
}

QString symbolicBaseName(const QString &name)
{
    return isSymbolicName(name)
        ? name.left(name.size() - QStringLiteral("-symbolic").size())
        : name;
}

QJsonObject readThemeConfigObject(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    return document.isObject() ? document.object() : QJsonObject();
}

} // namespace

IconThemeService::IconThemeService(QObject *parent)
    : IconThemeService(QString(), parent)
{
}

IconThemeService::IconThemeService(const QString &configPath, QObject *parent)
    : QObject(parent)
    , m_configPath(configPath.trimmed().isEmpty() ? canonicalConfigPath() : QFileInfo(configPath).absoluteFilePath())
    , m_watcher(new QFileSystemWatcher(this))
    , m_reloadTimer(new QTimer(this))
    , m_platformTheme(QIcon::themeName())
{
    m_reloadTimer->setSingleShot(true);
    m_reloadTimer->setInterval(60);
    connect(m_reloadTimer, &QTimer::timeout, this, &IconThemeService::reloadConfig);
    connect(m_watcher, &QFileSystemWatcher::fileChanged, this, &IconThemeService::scheduleReload);
    connect(m_watcher, &QFileSystemWatcher::directoryChanged, this, &IconThemeService::scheduleReload);
    reloadConfig();
}

QString IconThemeService::effectiveTheme() const
{
    return m_effectiveTheme;
}

QString IconThemeService::configuredBaseTheme() const
{
    const QString configured = readThemeConfigObject(m_configPath)
        .value(QStringLiteral("desktop_icon_theme"))
        .toString()
        .trimmed();
    return isValidThemeIdentifier(configured) ? configured : QString();
}

IconThemeService::AppearanceMode IconThemeService::appearance() const
{
    const QJsonObject object = readThemeConfigObject(m_configPath);
    if (object.value(QStringLiteral("theme")).toString().trimmed().compare(
            QStringLiteral("light"), Qt::CaseInsensitive) == 0
        || object.value(QStringLiteral("theme_mode")).toInt() == 1) {
        return AppearanceMode::Light;
    }
    return AppearanceMode::Dark;
}

quint64 IconThemeService::revision() const
{
    return m_revision;
}

bool IconThemeService::isValidThemeIdentifier(const QString &themeName)
{
    static const QRegularExpression expression(QStringLiteral("^[A-Za-z0-9][A-Za-z0-9._+-]{0,127}$"));
    return expression.match(themeName).hasMatch()
        && !themeName.contains(QStringLiteral(".."));
}

QStringList IconThemeService::iconCandidatesForFile(
    const QString &path,
    bool isDirectory,
    bool isExecutable) const
{
    QStringList candidates;
    if (isDirectory) {
        for (const QString &name : specialDirectoryCandidates(path)) {
            appendUnique(candidates, name);
        }
        appendUnique(candidates, QStringLiteral("folder"));
        appendUnique(candidates, QStringLiteral("inode-directory"));
        return candidates;
    }

    if (isExecutable) {
        appendUnique(candidates, QStringLiteral("application-x-executable"));
    }

    const QString fileName = fileNameForMimeLookup(path);
    const QMimeType mimeType = QMimeDatabase().mimeTypeForFile(
        fileName.isEmpty() ? path : fileName,
        QMimeDatabase::MatchExtension);
    if (mimeType.isValid()) {
        appendUnique(candidates, mimeType.iconName());
        appendUnique(candidates, mimeType.genericIconName());
    }
    for (const QString &name : fallbackCandidates()) {
        appendUnique(candidates, name);
    }
    return candidates;
}

QStringList IconThemeService::iconCandidatesForNames(const QStringList &names) const
{
    QStringList candidates;
    for (const QString &name : names) {
        appendUnique(candidates, name.trimmed());
    }
    for (const QString &name : fallbackCandidates()) {
        appendUnique(candidates, name);
    }
    return candidates;
}

QStringList IconThemeService::symbolicCandidatesForNames(const QStringList &names) const
{
    QStringList candidates;
    bool folderSemantic = false;
    bool removableDriveSemantic = false;
    bool hardDiskSemantic = false;
    bool networkSemantic = false;

    for (const QString &rawName : names) {
        const QString name = rawName.trimmed();
        if (name.isEmpty()) {
            continue;
        }

        const QString baseName = symbolicBaseName(name);
        folderSemantic = folderSemantic
            || baseName == QStringLiteral("inode-directory")
            || baseName.startsWith(QStringLiteral("folder"));
        removableDriveSemantic = removableDriveSemantic
            || baseName == QStringLiteral("drive-removable-media");
        hardDiskSemantic = hardDiskSemantic
            || baseName == QStringLiteral("drive-harddisk");
        networkSemantic = networkSemantic
            || baseName == QStringLiteral("network-workgroup");

        if (isSymbolicName(name)) {
            appendUnique(candidates, name);
            continue;
        }

        if (name == QStringLiteral("inode-directory")) {
            appendUnique(candidates, QStringLiteral("folder-symbolic"));
        } else if (name == QStringLiteral("folder-home")) {
            appendUnique(candidates, QStringLiteral("user-home-symbolic"));
        } else if (name == QStringLiteral("folder-desktop")) {
            appendUnique(candidates, QStringLiteral("user-desktop-symbolic"));
        } else if (name == QStringLiteral("folder-downloads")) {
            appendUnique(candidates, QStringLiteral("folder-download-symbolic"));
            appendUnique(candidates, QStringLiteral("folder-downloads-symbolic"));
        } else {
            appendUnique(candidates, name + QStringLiteral("-symbolic"));
        }
    }

    if (removableDriveSemantic) {
        appendUnique(candidates, QStringLiteral("drive-removable-media-symbolic"));
        appendUnique(candidates, QStringLiteral("drive-harddisk-symbolic"));
    } else if (hardDiskSemantic) {
        appendUnique(candidates, QStringLiteral("drive-harddisk-symbolic"));
    }
    if (networkSemantic) {
        appendUnique(candidates, QStringLiteral("network-workgroup-symbolic"));
    }
    if (folderSemantic) {
        appendUnique(candidates, QStringLiteral("folder-symbolic"));
    }
    appendUnique(candidates, QStringLiteral("image-missing-symbolic"));
    return candidates;
}

QIcon IconThemeService::resolveIcon(const QStringList &candidates) const
{
    for (const QString &candidate : candidates) {
        if (candidate.isEmpty()) {
            continue;
        }
        const QIcon icon = QIcon::fromTheme(candidate);
        if (!icon.isNull()) {
            return icon;
        }
    }
    return {};
}

QImage IconThemeService::renderIcon(
    const QStringList &candidates,
    const QSize &logicalSize,
    qreal devicePixelRatio) const
{
    const qreal boundedDpr = std::clamp(devicePixelRatio, 0.5, 4.0);
    const QSize pixelSize = boundedPixelSize(logicalSize, boundedDpr);
    const QStringList normalizedCandidates = candidates.isEmpty()
        ? fallbackCandidates()
        : candidates;
    const bool symbolic = !normalizedCandidates.isEmpty()
        && std::all_of(normalizedCandidates.cbegin(), normalizedCandidates.cend(), isSymbolicName);
    const QString key = cacheKey(normalizedCandidates, pixelSize, boundedDpr);
    if (m_renderedCache.contains(key)) {
        m_cacheOrder.removeAll(key);
        m_cacheOrder.append(key);
        return m_renderedCache.value(key);
    }

    QImage result(pixelSize, QImage::Format_ARGB32_Premultiplied);
    result.fill(Qt::transparent);
    const QIcon icon = resolveIcon(normalizedCandidates);
    bool renderedThemeIcon = false;
    if (!icon.isNull()) {
        const QPixmap pixmap = icon.pixmap(pixelSize);
        if (!pixmap.isNull()) {
            renderedThemeIcon = true;
            QImage rendered = pixmap.toImage().scaled(
                pixelSize,
                Qt::KeepAspectRatio,
                Qt::SmoothTransformation);
            QPainter painter(&result);
            painter.drawImage(
                (pixelSize.width() - rendered.width()) / 2,
                (pixelSize.height() - rendered.height()) / 2,
                rendered);
        }
    }

    if (!renderedThemeIcon) {
        result = builtInFallback(pixelSize, boundedDpr, symbolic);
    }
    result.setDevicePixelRatio(boundedDpr);

    m_renderedCache.insert(key, result);
    m_cacheOrder.removeAll(key);
    m_cacheOrder.append(key);
    while (m_cacheOrder.size() > kMaxRenderedCacheEntries) {
        m_renderedCache.remove(m_cacheOrder.takeFirst());
    }
    return result;
}

QString IconThemeService::iconSourceForNames(const QStringList &names, int size) const
{
    const QStringList candidates = iconCandidatesForNames(names);
    const QByteArray encoded = QUrl::toPercentEncoding(candidates.join(QLatin1Char('|')));
    return QStringLiteral("image://astrea-icons/theme/")
        + QString::fromLatin1(encoded)
        + QStringLiteral("?revision=")
        + QString::number(m_revision)
        + QStringLiteral("&size=")
        + QString::number(std::clamp(size, 1, kMaxIconSize));
}

QString IconThemeService::symbolicIconSourceForNames(const QStringList &names, int size) const
{
    const QByteArray encoded = QUrl::toPercentEncoding(
        symbolicCandidatesForNames(names).join(QLatin1Char('|')));
    return QStringLiteral("image://astrea-icons/theme/")
        + QString::fromLatin1(encoded)
        + QStringLiteral("?revision=")
        + QString::number(m_revision)
        + QStringLiteral("&size=")
        + QString::number(std::clamp(size, 1, kMaxIconSize));
}

QString IconThemeService::fileIconSource(
    const QString &path,
    bool isDirectory,
    bool isExecutable,
    int size,
    const QString &semanticIconName) const
{
    QStringList names;
    if (!semanticIconName.trimmed().isEmpty()) {
        names.append(semanticIconName.trimmed());
    }
    names.append(iconCandidatesForFile(path, isDirectory, isExecutable));
    return iconSourceForNames(names, size);
}

void IconThemeService::reloadConfig()
{
    applyTheme(selectTheme());
    updateWatcher();
}

void IconThemeService::scheduleReload(const QString &path)
{
    Q_UNUSED(path);
    if (!m_reloadTimer->isActive()) {
        m_reloadTimer->start();
    }
}

QString IconThemeService::canonicalConfigPath()
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::ConfigLocation))
        .filePath(QStringLiteral("AstreaOS/ui/theme.json"));
}

void IconThemeService::appendUnique(QStringList &names, const QString &name)
{
    const QString normalized = name.trimmed();
    if (!normalized.isEmpty() && !names.contains(normalized)) {
        names.append(normalized);
    }
}

QStringList IconThemeService::fallbackCandidates()
{
    return {
        QStringLiteral("application-x-generic"),
        QStringLiteral("text-x-generic"),
        QStringLiteral("unknown"),
    };
}

bool IconThemeService::themeIsUsable(const QString &themeName) const
{
    if (!isValidThemeIdentifier(themeName)) {
        return false;
    }

    // QIcon is the authority for whether a theme can actually resolve icons;
    // do not reconstruct an icon-theme filesystem path just to probe it.
    const QString previousTheme = QIcon::themeName();
    QIcon::setThemeName(themeName);
    const QStringList probes {
        QStringLiteral("folder"),
        QStringLiteral("inode-directory"),
        QStringLiteral("application-x-generic"),
        QStringLiteral("text-x-generic"),
        QStringLiteral("user-home"),
        QStringLiteral("application-pdf"),
    };
    bool usable = false;
    for (const QString &probe : probes) {
        if (QIcon::hasThemeIcon(probe) || !QIcon::fromTheme(probe).isNull()) {
            usable = true;
            break;
        }
    }
    QIcon::setThemeName(previousTheme);
    return usable;
}

QString IconThemeService::resolveAppearanceVariant(
    const QString &baseTheme,
    AppearanceMode appearanceMode) const
{
    const QString normalized = baseTheme.trimmed();
    if (!isValidThemeIdentifier(normalized)) {
        return {};
    }

    const bool explicitVariant = normalized.endsWith(QStringLiteral("-dark"))
        || normalized.endsWith(QStringLiteral("-light"));
    if (explicitVariant) {
        return themeIsUsable(normalized) ? normalized : QString();
    }

    const QString preferredVariant = normalized
        + (appearanceMode == AppearanceMode::Light
                ? QStringLiteral("-light")
                : QStringLiteral("-dark"));
    if (themeIsUsable(preferredVariant)) {
        return preferredVariant;
    }
    return themeIsUsable(normalized) ? normalized : QString();
}

IconThemeService::ThemeSelection IconThemeService::selectTheme() const
{
    const QString environmentTheme = qEnvironmentVariable("ASTREA_ICON_THEME").trimmed();
    if (isValidThemeIdentifier(environmentTheme) && themeIsUsable(environmentTheme)) {
        return {environmentTheme, QStringLiteral("environment")};
    }

    const QString configuredBase = configuredBaseTheme();
    if (const QString configuredVariant = resolveAppearanceVariant(configuredBase, appearance());
        !configuredVariant.isEmpty()) {
        return {configuredVariant, QStringLiteral("config")};
    }

    if (themeIsUsable(m_platformTheme)) {
        return {m_platformTheme, QStringLiteral("platform")};
    }

    if (const QString compatibilityVariant = resolveAppearanceVariant(
            QStringLiteral("MacTahoe"), appearance());
        !compatibilityVariant.isEmpty()) {
        return {compatibilityVariant, QStringLiteral("compatibility-default")};
    }

    return {{}, QStringLiteral("qt-freedesktop-fallback")};
}

void IconThemeService::applyTheme(const ThemeSelection &selection)
{
    if (selection.name == m_effectiveTheme) {
        m_themeSource = selection.source;
        return;
    }

    m_effectiveTheme = selection.name;
    m_themeSource = selection.source;
    if (!selection.name.isEmpty()) {
        QIcon::setThemeName(selection.name);
    } else if (!m_platformTheme.isEmpty()) {
        QIcon::setThemeName(m_platformTheme);
    }
    ++m_revision;
    clearRenderedCache();
    const QString appearanceName = appearance() == AppearanceMode::Light
        ? QStringLiteral("light")
        : QStringLiteral("dark");
    qInfo().noquote() << "Astrea icon theme:"
                      << "configured=" << (configuredBaseTheme().isEmpty()
                              ? QStringLiteral("<none>")
                              : configuredBaseTheme())
                      << "appearance=" << appearanceName
                      << "effective=" << (m_effectiveTheme.isEmpty()
                              ? QStringLiteral("Qt/Freedesktop fallback")
                              : m_effectiveTheme)
                      << "source=" << m_themeSource;
    emit themeChanged();
}

void IconThemeService::updateWatcher()
{
    const QString parentPath = QFileInfo(m_configPath).absolutePath();
    for (const QString &path : m_watcher->files()) {
        m_watcher->removePath(path);
    }
    for (const QString &path : m_watcher->directories()) {
        m_watcher->removePath(path);
    }
    if (QFileInfo(m_configPath).isFile()) {
        m_watcher->addPath(m_configPath);
    }
    if (QFileInfo(parentPath).isDir()) {
        m_watcher->addPath(parentPath);
    }
}

void IconThemeService::clearRenderedCache() const
{
    m_renderedCache.clear();
    m_cacheOrder.clear();
}

QImage IconThemeService::builtInFallback(
    const QSize &pixelSize,
    qreal devicePixelRatio,
    bool symbolic) const
{
    QImage image(pixelSize, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    const QRectF bounds = QRectF(image.rect()).adjusted(
        pixelSize.width() * 0.18,
        pixelSize.height() * 0.18,
        -pixelSize.width() * 0.18,
        -pixelSize.height() * 0.18);
    if (symbolic) {
        painter.setPen(QPen(Qt::white, std::max(1.0, pixelSize.width() * 0.10)));
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(bounds, pixelSize.width() * 0.18, pixelSize.height() * 0.18);
        painter.drawLine(
            QPointF(bounds.left() + bounds.width() * 0.30, bounds.center().y()),
            QPointF(bounds.right() - bounds.width() * 0.30, bounds.center().y()));
        image.setDevicePixelRatio(devicePixelRatio);
        return image;
    }

    const QColor accent(QStringLiteral("#6f8cff"));
    const QColor shadow(QStringLiteral("#26324f"));
    const QRectF fullColorBounds = QRectF(image.rect()).adjusted(
        pixelSize.width() * 0.12,
        pixelSize.height() * 0.12,
        -pixelSize.width() * 0.12,
        -pixelSize.height() * 0.12);
    painter.setPen(QPen(shadow, std::max(1.0, pixelSize.width() * 0.06)));
    painter.setBrush(accent);
    if (pixelSize.width() >= pixelSize.height()) {
        QPainterPath folder;
        folder.moveTo(fullColorBounds.left(), fullColorBounds.top() + fullColorBounds.height() * 0.2);
        folder.lineTo(fullColorBounds.left() + fullColorBounds.width() * 0.35, fullColorBounds.top() + fullColorBounds.height() * 0.2);
        folder.lineTo(fullColorBounds.left() + fullColorBounds.width() * 0.48, fullColorBounds.top() + fullColorBounds.height() * 0.34);
        folder.lineTo(fullColorBounds.right(), fullColorBounds.top() + fullColorBounds.height() * 0.34);
        folder.lineTo(fullColorBounds.right(), fullColorBounds.bottom());
        folder.lineTo(fullColorBounds.left(), fullColorBounds.bottom());
        folder.closeSubpath();
        painter.drawPath(folder);
    } else {
        painter.drawRoundedRect(fullColorBounds, pixelSize.width() * 0.1, pixelSize.height() * 0.1);
        painter.setPen(QPen(Qt::white, std::max(1.0, pixelSize.width() * 0.06)));
        painter.drawLine(
            QPointF(fullColorBounds.left() + fullColorBounds.width() * 0.25, fullColorBounds.top() + fullColorBounds.height() * 0.42),
            QPointF(fullColorBounds.right() - fullColorBounds.width() * 0.25, fullColorBounds.top() + fullColorBounds.height() * 0.42));
        painter.drawLine(
            QPointF(fullColorBounds.left() + fullColorBounds.width() * 0.25, fullColorBounds.top() + fullColorBounds.height() * 0.62),
            QPointF(fullColorBounds.right() - fullColorBounds.width() * 0.25, fullColorBounds.top() + fullColorBounds.height() * 0.62));
    }
    image.setDevicePixelRatio(devicePixelRatio);
    return image;
}

QString IconThemeService::cacheKey(
    const QStringList &candidates,
    const QSize &pixelSize,
    qreal devicePixelRatio) const
{
    return QString::number(m_revision)
        + QLatin1Char('|')
        + candidates.join(QLatin1Char('\x1f'))
        + QLatin1Char('|')
        + QString::number(pixelSize.width())
        + QLatin1Char('x')
        + QString::number(pixelSize.height())
        + QLatin1Char('|')
        + QString::number(qRound(devicePixelRatio * 100.0));
}

} // namespace Astrea::Explorer::Native::Services
