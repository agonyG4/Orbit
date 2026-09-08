#include "freedesktop_icon_theme_catalog.h"

#include <QDir>
#include <QFileInfo>
#include <QIcon>
#include <QRegularExpression>
#include <QSettings>

#include <algorithm>

namespace Astrea::Explorer::Native::Services {

namespace {

constexpr auto kHicolorTheme = "hicolor";
constexpr auto kIconThemeGroup = "Icon Theme";
constexpr qsizetype kMaxPresenceEntries = 4096;

const QStringList &iconExtensions()
{
    static const QStringList extensions {
        QStringLiteral(".png"),
        QStringLiteral(".svg"),
        QStringLiteral(".xpm"),
        QStringLiteral(".svgz"),
    };
    return extensions;
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

QStringList FreedesktopIconThemeCatalog::watchPaths(const QStringList &themeNames) const
{
    refreshSearchSnapshot();

    QStringList paths;
    for (const QString &basePath : m_searchPaths) {
        if (isFilesystemPath(basePath) && QFileInfo(basePath).isDir()) {
            appendUnique(paths, QFileInfo(basePath).absoluteFilePath());
        }
    }

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
    bool foundValidIndex = false;
    for (const QString &basePath : m_searchPaths) {
        const QString themeRoot = QDir(basePath).filePath(normalizedTheme);
        if (QFileInfo(themeRoot).isDir()) {
            appendUnique(metadata.roots, QFileInfo(themeRoot).absoluteFilePath());
        }

        if (foundValidIndex) {
            continue;
        }

        const QString indexPath = QDir(themeRoot).filePath(QStringLiteral("index.theme"));
        if (!QFileInfo(indexPath).isFile()) {
            continue;
        }

        QSettings settings(indexPath, QSettings::IniFormat);
        if (settings.status() == QSettings::AccessError) {
            continue;
        }
        settings.beginGroup(QString::fromLatin1(kIconThemeGroup));
        const QString displayName = settings.value(QStringLiteral("Name")).toString().trimmed();
        const QString comment = settings.value(QStringLiteral("Comment")).toString().trimmed();
        const QStringList directories = splitList(settings.value(QStringLiteral("Directories")));
        const QStringList scaledDirectories = splitList(
            settings.value(QStringLiteral("ScaledDirectories")));
        const QStringList inherits = splitList(settings.value(QStringLiteral("Inherits")));
        if (displayName.isEmpty() || comment.isEmpty() || directories.isEmpty()) {
            settings.endGroup();
            continue;
        }

        QStringList allDirectories = directories;
        for (const QString &directory : scaledDirectories) {
            appendUnique(allDirectories, directory);
        }

        settings.endGroup();
        bool validDirectories = true;
        for (const QString &directory : allDirectories) {
            if (!isSafeRelativePath(directory)) {
                validDirectories = false;
                break;
            }
            settings.beginGroup(directory);
            bool sizeIsValid = false;
            const int size = settings.value(QStringLiteral("Size")).toInt(&sizeIsValid);
            settings.endGroup();
            if (!sizeIsValid || size <= 0) {
                validDirectories = false;
                break;
            }
        }
        if (validDirectories) {
            metadata.inherits = inherits;
            metadata.directories = allDirectories;
            foundValidIndex = true;
        }
    }

    if (!foundValidIndex) {
        m_missingThemes.insert(normalizedTheme);
        return nullptr;
    }
    m_metadata.insert(normalizedTheme, metadata);
    return &m_metadata[normalizedTheme];
}

bool FreedesktopIconThemeCatalog::hasIcon(
    const QString &themeName,
    const QString &candidate) const
{
    if (candidate.isEmpty() || candidate.contains(QLatin1Char('/'))
        || candidate.contains(QLatin1Char('\\'))) {
        return false;
    }

    const QString normalizedTheme = themeName.trimmed();
    const QString normalizedCandidate = candidate.trimmed();
    const QString cacheKey = normalizedTheme + QLatin1Char('\x1f') + normalizedCandidate;
    if (const auto existing = m_presence.constFind(cacheKey); existing != m_presence.cend()) {
        return existing.value();
    }

    const ThemeMetadata *metadata = metadataFor(normalizedTheme);
    bool present = false;
    if (metadata) {
        for (const QString &root : metadata->roots) {
            for (const QString &directory : metadata->directories) {
                const QString prefix = QDir(root).filePath(directory + QLatin1Char('/') + normalizedCandidate);
                for (const QString &extension : iconExtensions()) {
                    if (QFileInfo(prefix + extension).isFile()) {
                        present = true;
                        break;
                    }
                }
                if (present) {
                    break;
                }
            }
            if (present) {
                break;
            }
        }
    }
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
        for (const QString &directory : metadata->directories) {
            const QString iconDirectory = QFileInfo(QDir(root).filePath(directory)).absoluteFilePath();
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
