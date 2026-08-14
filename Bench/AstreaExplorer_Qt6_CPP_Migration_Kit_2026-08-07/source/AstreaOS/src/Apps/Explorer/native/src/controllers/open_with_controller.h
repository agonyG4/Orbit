#pragma once

#include <QHash>
#include <QPointer>
#include <QThread>
#include <QVariantList>
#include <QVector>

#include "services/launch_service.h"
#include "services/mime_apps_service.h"

#include <memory>

namespace Astrea::Explorer::Native::Backend {

struct OpenWithApplication
{
    QString id;
    QString name;
    QString icon;
    QString desktopFile;
    bool isDefault = false;
    QStringList mimeTypes;
};

class OpenWithController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList applicationList READ applicationList NOTIFY applicationsChanged)
    Q_PROPERTY(QString selectedApplicationId READ selectedApplicationId NOTIFY selectionChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)

public:
    using DesktopCatalog = QHash<QString, OpenWithApplication>;

    explicit OpenWithController(
        Services::LaunchService *launchService = nullptr,
        QObject *parent = nullptr);
    ~OpenWithController() override;

    void setMimeAppsService(Services::MimeAppsService *mimeAppsService);

    QVector<OpenWithApplication> applications() const;
    QVariantList applicationList() const;
    QString selectedApplicationId() const;
    OpenWithApplication selectedApplication() const;
    bool busy() const;
    QString error() const;

    static DesktopCatalog buildDesktopCatalog(const QStringList &roots = {});
    static OpenWithApplication resolveDesktopEntry(
        const QString &desktopId,
        const DesktopCatalog *catalog = nullptr);

    void setApplications(const QVector<OpenWithApplication> &applications);
    Q_INVOKABLE void discover(const QString &path);
    Q_INVOKABLE void selectApplication(const QString &id);
    Q_INVOKABLE bool launchSelected(const QString &path);

signals:
    void applicationsChanged();
    void selectionChanged();
    void busyChanged();
    void errorChanged();
    void launched(const QString &applicationId);

private:
    static bool mimeMatches(const QStringList &mimeTypes, const QString &mime);

    Services::LaunchService *m_launchService = nullptr;
    std::unique_ptr<Services::MimeAppsService> m_ownedMimeAppsService;
    Services::MimeAppsService *m_mimeAppsService = nullptr;
    QVector<OpenWithApplication> m_applications;
    QString m_selectedApplicationId;
    bool m_busy = false;
    QString m_error;
    DesktopCatalog m_catalog;
    QPointer<QThread> m_discoveryThread;
    quint64 m_discoveryGeneration = 0;
    bool m_catalogReady = false;
};

} // namespace Astrea::Explorer::Native::Backend

Q_DECLARE_METATYPE(Astrea::Explorer::Native::Backend::OpenWithApplication)
Q_DECLARE_METATYPE(QVector<Astrea::Explorer::Native::Backend::OpenWithApplication>)
