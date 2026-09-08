#pragma once

#include <QHash>
#include <QSet>
#include <QStringList>
#include <QVariant>

namespace Astrea::Explorer::Native::Services {

class FreedesktopIconThemeCatalog final
{
public:
    bool themeExists(const QString &themeName) const;
    QStringList themeFamily(const QString &themeName) const;
    QString resolveIconName(
        const QString &themeName,
        const QStringList &candidates) const;
    QStringList watchPaths(const QStringList &themeNames) const;
    void invalidate() const;

private:
    struct ThemeMetadata final {
        QStringList inherits;
        QStringList directories;
        QStringList roots;
    };

    static bool isValidThemeIdentifier(const QString &themeName);
    static bool isSafeRelativePath(const QString &path);
    static bool isFilesystemPath(const QString &path);
    static QStringList splitList(const QVariant &value);
    static void appendUnique(QStringList &values, const QString &value);

    void refreshSearchSnapshot() const;
    const ThemeMetadata *metadataFor(const QString &themeName) const;
    bool hasIcon(const QString &themeName, const QString &candidate) const;
    void appendTheme(
        const QString &themeName,
        QSet<QString> &visited,
        QStringList &family,
        bool deferHicolor) const;
    void appendWatchPathsForTheme(
        const QString &themeName,
        QSet<QString> &visited,
        QStringList &paths) const;

    mutable QStringList m_searchPaths;
    mutable QString m_fallbackThemeName;
    mutable QHash<QString, ThemeMetadata> m_metadata;
    mutable QSet<QString> m_missingThemes;
    mutable QHash<QString, bool> m_presence;
};

} // namespace Astrea::Explorer::Native::Services
