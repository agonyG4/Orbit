#pragma once

#include <QString>

namespace Astrea::Explorer::Native::Services {

struct ExplorerSettings
{
    QString currentPath;
    bool showPreview = false;
    QString viewMode {QStringLiteral("list")};
    QString sortField {QStringLiteral("name")};
    bool sortAscending = true;
    bool showHidden = false;
    bool foldersFirst = true;
    bool groupingEnabled = true;
    double zoomLevel = 1.0;
    QString autoMountDeviceIdsJson {QStringLiteral("[]")};
    QString sidebarFavoritesJson {QStringLiteral("[]")};
    QString sidebarHiddenDefaultFavoritesJson {QStringLiteral("[]")};
};

class SettingsService final
{
public:
    explicit SettingsService(QString filePath);

    ExplorerSettings load() const;
    bool save(const ExplorerSettings &settings) const;

    QString filePath() const;

private:
    QString m_filePath;
};

} // namespace Astrea::Explorer::Native::Services
