#include "freedesktop_icon_theme_catalog.h"

#include <QDir>
#include <QFileInfo>
#include <QIcon>
#include <QImageReader>
#include <QRegularExpression>
#include <QSettings>

#include <algorithm>
#include <cmath>
#include <tuple>

namespace Astrea::Explorer::Native::Services {

namespace {

constexpr auto kHicolorTheme = "hicolor";
constexpr auto kIconThemeGroup = "Icon Theme";
constexpr qsizetype kMaxPresenceEntries = 4096;
constexpr int kScalePenalty = 1000000;

struct IconMatch final {
    QString path;
    int scalePenalty = 0;
    int distance = 0;
    int rootIndex = 0;
    int directoryIndex = 0;
    int extensionIndex = 0;
};

const QStringList &iconExtensions()
{
    static const QStringList extensions {
        QStringLiteral(".png"),
        QStringLiteral(".svg"),
        QStringLiteral(".xpm"),
    };
    return extensions;
}

bool supportsExtension(const QString &extension)
{
    const QByteArray format = extension.mid(1).toLatin1();
    const QList<QByteArray> supportedFormats = QImageReader::supportedImageFormats();
    return std::any_of(
        supportedFormats.cbegin(),
        supportedFormats.cend(),
        [&format](const QByteArray &supported) {
            return supported.compare(format, Qt::CaseInsensitive) == 0;
        });
}

int requestedPixelSize(const QSize &logicalSize, qreal devicePixelRatio)
{
    const int logical = qMax(1, qMin(logicalSize.width(), logicalSize.height()));
    return qMax(1, qRound(logical * qMax<qreal>(1.0, devicePixelRatio)));
}

int requestedScale(qreal devicePixelRatio)
{
    return qMax(1, qCeil(qMax<qreal>(1.0, devicePixelRatio)));
}

int absoluteDistance(int requested, int lower, int upper)
{
    if (requested < lower) {
        return lower - requested;
    }
    if (requested > upper) {
        return requested - upper;
    }
    return 0;
}

bool isBetterMatch(const IconMatch &candidate, const IconMatch &current)
{
    return std::tie(
               candidate.scalePenalty,
               candidate.distance,
               candidate.rootIndex,
               candidate.directoryIndex,
               candidate.extensionIndex)
        < std::tie(
            current.scalePenalty,
            current.distance,
            current.rootIndex,
            current.directoryIndex,
            current.extensionIndex);
}

} // namespace

bool FreedesktopIconThemeCatalog::themeExists(const QString &themeName) const
{
    return metadataFor(themeName) != nullptr;
}

QStringList FreedesktopIconThemeCatalog::themeFamily(const QString &themeName) const
{
    refreshSearchSnapshot();

    QStringList family;
    QSet<QString> visited;
    appendTheme(themeName, visited, family, false);

    const QString fallbackTheme = m_fallbackThemeName.trimmed();
    if (!fallbackTheme.isEmpty()) {
        appendTheme(fallbackTheme, visited, family, true);
    }
    appendTheme(QString::fromLatin1(kHicolorTheme), visited, family, false);
    return family;
}

FreedesktopIconThemeCatalog::ResolvedIconAsset FreedesktopIconThemeCatalog::resolveIconAsset(
    const QString &themeName,
    const QStringList &candidates,
    const QSize &logicalSize,
    qreal devicePixelRatio) const
{
    const QStringList family = themeFamily(themeName);
    for (const QString &theme : family) {
        for (const QString &candidate : candidates) {
            const QString normalizedCandidate = candidate.trimmed();
            if (normalizedCandidate.isEmpty()
                || normalizedCandidate.contains(QLatin1Char('/'))
                || normalizedCandidate.contains(QLatin1Char('\\'))) {
                continue;
            }
            const QString path = findIconPath(
                theme,
                normalizedCandidate,
                logicalSize,
                devicePixelRatio);
            if (!path.isEmpty()) {
                return {theme, normalizedCandidate, path};
            }
        }
    }
    return {};
}

QString FreedesktopIconThemeCatalog::resolveIconName(
    const QString &themeName,
    const QStringList &candidates) const
{
    const QStringList family = themeFamily(themeName);
    for (const QString &theme : family) {
        for (const QString &candidate : candidates) {
            const QString normalizedCandidate = candidate.trimmed();
            if (!normalizedCandidate.isEmpty() && hasIcon(theme, normalizedCandidate)) {
                return normalizedCandidate;
            }
        }
    }
    return {};
}

QStringList FreedesktopIconThemeCatalog::searchRootPaths() const
{
    refreshSearchSnapshot();

    QStringList paths;
    for (const QString &basePath : m_searchPaths) {
        if (isFilesystemPath(basePath) && QFileInfo(basePath).isDir()) {
            appendUnique(paths, QFileInfo(basePath).absoluteFilePath());
        }
    }
    return paths;
}

QStringList FreedesktopIconThemeCatalog::watchPaths(const QStringList &themeNames) const
{
    refreshSearchSnapshot();

    QStringList paths;
    QSet<QString> visited;
    for (const QString &themeName : themeNames) {
        const QString normalizedTheme = themeName.trimmed();
        if (!isValidThemeIdentifier(normalizedTheme)) {
            continue;
        }
        for (const QString &familyTheme : themeFamily(normalizedTheme)) {
            appendWatchPathsForTheme(familyTheme, visited, paths);
        }
        appendWatchPathsForTheme(normalizedTheme, visited, paths);
    }
    return paths;
}

void FreedesktopIconThemeCatalog::invalidate() const
{
    m_searchPaths.clear();
    m_fallbackThemeName.clear();
    m_metadata.clear();
    m_missingThemes.clear();
    m_presence.clear();
}

bool FreedesktopIconThemeCatalog::isValidThemeIdentifier(const QString &themeName)
{
    static const QRegularExpression expression(
        QStringLiteral("^[A-Za-z0-9][A-Za-z0-9._+-]{0,127}$"));
    return expression.match(themeName).hasMatch()
        && !themeName.contains(QStringLiteral(".."));
}

bool FreedesktopIconThemeCatalog::isSafeRelativePath(const QString &path)
{
    const QString normalized = path.trimmed();
    if (normalized.isEmpty() || QDir::isAbsolutePath(normalized)) {
        return false;
    }
    const QStringList components = normalized.split(QRegularExpression(QStringLiteral("[/\\\\]")));
    return std::none_of(components.cbegin(), components.cend(), [](const QString &component) {
        return component == QStringLiteral("..");
    });
}

bool FreedesktopIconThemeCatalog::isFilesystemPath(const QString &path)
{
    return !path.startsWith(QStringLiteral(":/"))
        && !path.startsWith(QStringLiteral("qrc:/"));
}

QStringList FreedesktopIconThemeCatalog::splitList(const QVariant &value)
{
    QStringList result;
    QStringList values = value.toStringList();
    if (values.isEmpty()) {
        values.append(value.toString());
    }
    for (const QString &rawValue : values) {
        for (const QString &entry : rawValue.split(QLatin1Char(','), Qt::SkipEmptyParts)) {
            appendUnique(result, entry);
        }
    }
    return result;
}

void FreedesktopIconThemeCatalog::appendUnique(QStringList &values, const QString &value)
{
    const QString normalized = value.trimmed();
    if (!normalized.isEmpty() && !values.contains(normalized)) {
        values.append(normalized);
    }
}

void FreedesktopIconThemeCatalog::refreshSearchSnapshot() const
{
    const QStringList searchPaths = QIcon::themeSearchPaths();
    const QString fallbackTheme = QIcon::fallbackThemeName();
    if (searchPaths == m_searchPaths && fallbackTheme == m_fallbackThemeName) {
        return;
    }

    m_searchPaths = searchPaths;
    m_fallbackThemeName = fallbackTheme;
    m_metadata.clear();
    m_missingThemes.clear();
    m_presence.clear();
}

const FreedesktopIconThemeCatalog::ThemeMetadata *
FreedesktopIconThemeCatalog::metadataFor(const QString &themeName) const
{
    refreshSearchSnapshot();

    const QString normalizedTheme = themeName.trimmed();
    if (!isValidThemeIdentifier(normalizedTheme)) {
        return nullptr;
    }
    if (const auto existing = m_metadata.constFind(normalizedTheme); existing != m_metadata.cend()) {
        return &existing.value();
    }
    if (m_missingThemes.contains(normalizedTheme)) {
        return nullptr;
    }

    ThemeMetadata metadata;
    bool selectedIndex = false;
    bool validIndex = false;
    for (const QString &basePath : m_searchPaths) {
        const QString themeRoot = QDir(basePath).filePath(normalizedTheme);
        if (QFileInfo(themeRoot).isDir()) {
            appendUnique(metadata.roots, QFileInfo(themeRoot).absoluteFilePath());
        }

        if (selectedIndex) {
            continue;
        }

        const QString indexPath = QDir(themeRoot).filePath(QStringLiteral("index.theme"));
        if (!QFileInfo(indexPath).isFile()) {
            continue;
        }
        selectedIndex = true;

        QSettings settings(indexPath, QSettings::IniFormat);
        if (settings.status() == QSettings::AccessError) {
            break;
        }

        settings.beginGroup(QString::fromLatin1(kIconThemeGroup));
        const QString displayName = settings.value(QStringLiteral("Name")).toString().trimmed();
        const QString comment = settings.value(QStringLiteral("Comment")).toString().trimmed();
        const QStringList directories = splitList(settings.value(QStringLiteral("Directories")));
        const QStringList scaledDirectories = splitList(
            settings.value(QStringLiteral("ScaledDirectories")));
        const QStringList inherits = splitList(settings.value(QStringLiteral("Inherits")));
        if (displayName.isEmpty() || comment.isEmpty()
            || (directories.isEmpty() && scaledDirectories.isEmpty())) {
            settings.endGroup();
            break;
        }

        QStringList allDirectories = directories;
        for (const QString &directory : scaledDirectories) {
            appendUnique(allDirectories, directory);
        }
        settings.endGroup();

        bool validDirectories = true;
        QList<DirectoryMetadata> parsedDirectories;
        for (const QString &directory : allDirectories) {
            if (!isSafeRelativePath(directory)) {
                validDirectories = false;
                break;
            }

            settings.beginGroup(directory);
            bool sizeIsValid = false;
            const int size = settings.value(QStringLiteral("Size")).toInt(&sizeIsValid);
            const int scale = settings.value(QStringLiteral("Scale"), 1).toInt();
            const QString typeValue = settings.value(
                                                       QStringLiteral("Type"),
                                                       QStringLiteral("Threshold"))
                                          .toString()
                                          .trimmed();
            const int minSize = settings.value(QStringLiteral("MinSize"), size).toInt();
            const int maxSize = settings.value(QStringLiteral("MaxSize"), size).toInt();
            const int threshold = settings.value(QStringLiteral("Threshold"), 2).toInt();
            settings.endGroup();

            DirectoryType type = DirectoryType::Threshold;
            if (typeValue.compare(QStringLiteral("Fixed"), Qt::CaseInsensitive) == 0) {
                type = DirectoryType::Fixed;
            } else if (typeValue.compare(QStringLiteral("Scalable"), Qt::CaseInsensitive) == 0) {
                type = DirectoryType::Scalable;
            } else if (!typeValue.isEmpty()
                       && typeValue.compare(QStringLiteral("Threshold"), Qt::CaseInsensitive) != 0) {
                validDirectories = false;
                break;
            }

            if (!sizeIsValid || size <= 0 || scale <= 0 || minSize <= 0 || maxSize < minSize
                || threshold < 0) {
                validDirectories = false;
                break;
            }

            parsedDirectories.append({directory, size, scale, minSize, maxSize, threshold, type});
        }
        if (validDirectories) {
            metadata.inherits = inherits;
            metadata.directories = parsedDirectories;
            validIndex = true;
        } else {
            break;
        }
    }

    if (!selectedIndex || !validIndex) {
        m_missingThemes.insert(normalizedTheme);
        return nullptr;
    }
    m_metadata.insert(normalizedTheme, metadata);
    return &m_metadata[normalizedTheme];
}

QString FreedesktopIconThemeCatalog::findIconPath(
    const QString &themeName,
    const QString &candidate,
    const QSize &logicalSize,
    qreal devicePixelRatio) const
{
    const ThemeMetadata *metadata = metadataFor(themeName);
    if (!metadata) {
        return {};
    }

    const int requestedPixels = requestedPixelSize(logicalSize, devicePixelRatio);
    const int iconScale = requestedScale(devicePixelRatio);
    bool found = false;
    IconMatch best;
    for (int rootIndex = 0; rootIndex < metadata->roots.size(); ++rootIndex) {
        const QString &root = metadata->roots.at(rootIndex);
        for (int directoryIndex = 0; directoryIndex < metadata->directories.size(); ++directoryIndex) {
            const DirectoryMetadata &directory = metadata->directories.at(directoryIndex);
            const QString prefix = QDir(root).filePath(directory.path + QLatin1Char('/') + candidate);
            for (int extensionIndex = 0; extensionIndex < iconExtensions().size(); ++extensionIndex) {
                const QString &extension = iconExtensions().at(extensionIndex);
                if (!supportsExtension(extension)) {
                    continue;
                }
                const QString path = prefix + extension;
                if (!QFileInfo(path).isFile()) {
                    continue;
                }
                const int scaledSize = directory.size * directory.scale;
                int lower = scaledSize;
                int upper = scaledSize;
                switch (directory.type) {
                case DirectoryType::Fixed:
                    break;
                case DirectoryType::Scalable:
                    lower = directory.minSize * directory.scale;
                    upper = directory.maxSize * directory.scale;
                    break;
                case DirectoryType::Threshold:
                    lower = qMax(1, directory.size - directory.threshold) * directory.scale;
                    upper = (directory.size + directory.threshold) * directory.scale;
                    break;
                }
                IconMatch match;
                match.path = path;
                match.scalePenalty = directory.scale == iconScale ? 0 : kScalePenalty;
                match.distance = absoluteDistance(requestedPixels, lower, upper);
                match.rootIndex = rootIndex;
                match.directoryIndex = directoryIndex;
                match.extensionIndex = extensionIndex;
                if (!found || isBetterMatch(match, best)) {
                    best = match;
                    found = true;
                }
            }
        }
    }
    return found ? best.path : QString();
}

bool FreedesktopIconThemeCatalog::hasIcon(
    const QString &themeName,
    const QString &candidate) const
{
    const QString normalizedTheme = themeName.trimmed();
    const QString normalizedCandidate = candidate.trimmed();
    if (normalizedCandidate.isEmpty()
        || normalizedCandidate.contains(QLatin1Char('/'))
        || normalizedCandidate.contains(QLatin1Char('\\'))) {
        return false;
    }
    const QString cacheKey = normalizedTheme + QLatin1Char('\x1f') + normalizedCandidate;
    if (const auto existing = m_presence.constFind(cacheKey); existing != m_presence.cend()) {
        return existing.value();
    }

    const bool present = !findIconPath(
                              normalizedTheme,
                              normalizedCandidate,
                              QSize(16, 16),
                              1.0)
                              .isEmpty();
    if (m_presence.size() >= kMaxPresenceEntries) {
        m_presence.clear();
    }
    m_presence.insert(cacheKey, present);
    return present;
}

void FreedesktopIconThemeCatalog::appendTheme(
    const QString &themeName,
    QSet<QString> &visited,
    QStringList &family,
    bool deferHicolor) const
{
    const QString normalizedTheme = themeName.trimmed();
    if (!isValidThemeIdentifier(normalizedTheme)
        || (deferHicolor && normalizedTheme == QString::fromLatin1(kHicolorTheme))
        || visited.contains(normalizedTheme)) {
        return;
    }
    visited.insert(normalizedTheme);

    const ThemeMetadata *metadata = metadataFor(normalizedTheme);
    if (!metadata) {
        return;
    }
    family.append(normalizedTheme);
    for (const QString &inheritedTheme : metadata->inherits) {
        appendTheme(inheritedTheme, visited, family, true);
    }
}

void FreedesktopIconThemeCatalog::appendWatchPathsForTheme(
    const QString &themeName,
    QSet<QString> &visited,
    QStringList &paths) const
{
    const QString normalizedTheme = themeName.trimmed();
    if (!isValidThemeIdentifier(normalizedTheme) || visited.contains(normalizedTheme)) {
        return;
    }
    visited.insert(normalizedTheme);

    for (const QString &basePath : m_searchPaths) {
        if (!isFilesystemPath(basePath)) {
            continue;
        }
        const QString themeRoot = QFileInfo(QDir(basePath).filePath(normalizedTheme)).absoluteFilePath();
        if (QFileInfo(themeRoot).isDir()) {
            appendUnique(paths, themeRoot);
            const QString indexPath = QDir(themeRoot).filePath(QStringLiteral("index.theme"));
            if (QFileInfo(indexPath).isFile()) {
                appendUnique(paths, QFileInfo(indexPath).absoluteFilePath());
            }
        }
    }

    const ThemeMetadata *metadata = metadataFor(normalizedTheme);
    if (!metadata) {
        return;
    }
    for (const QString &root : metadata->roots) {
        if (!isFilesystemPath(root)) {
            continue;
        }
        for (const DirectoryMetadata &directory : metadata->directories) {
            const QString iconDirectory = QFileInfo(QDir(root).filePath(directory.path)).absoluteFilePath();
            if (QFileInfo(iconDirectory).isDir()) {
                appendUnique(paths, iconDirectory);
            }
        }
    }
    for (const QString &inheritedTheme : metadata->inherits) {
        appendWatchPathsForTheme(inheritedTheme, visited, paths);
    }
}

} // namespace Astrea::Explorer::Native::Services
