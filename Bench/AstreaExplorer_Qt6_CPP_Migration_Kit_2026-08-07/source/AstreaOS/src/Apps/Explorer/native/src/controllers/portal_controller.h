#pragma once

#include <QObject>
#include <QStringList>
#include <QMetaType>

namespace Astrea::Explorer::Native::Backend {

struct PortalOptions
{
    QString mode {QStringLiteral("open")};
    bool multiple = false;
    bool directoryOnly = false;
    QString currentLocation;
};

struct PortalResult
{
    bool accepted = false;
    QStringList paths;
    QString reason;
};

class PortalController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool active READ active NOTIFY stateChanged)
    Q_PROPERTY(QString selectionMode READ selectionMode NOTIFY stateChanged)
    Q_PROPERTY(bool multiple READ multiple NOTIFY stateChanged)
    Q_PROPERTY(bool directoryOnly READ directoryOnly NOTIFY stateChanged)
    Q_PROPERTY(QString currentLocation READ currentLocation NOTIFY stateChanged)
    Q_PROPERTY(QStringList selectedPaths READ selectedPaths NOTIFY selectedPathsChanged)

public:
    explicit PortalController(QObject *parent = nullptr);

    bool active() const;
    QString selectionMode() const;
    bool multiple() const;
    bool directoryOnly() const;
    QString currentLocation() const;
    QStringList selectedPaths() const;

    Q_INVOKABLE void begin(const PortalOptions &options);
    Q_INVOKABLE void setSelectedPaths(const QStringList &paths);
    Q_INVOKABLE void accept();
    Q_INVOKABLE void reject();
    Q_INVOKABLE void close();
    Q_INVOKABLE void consumerDied();
    Q_INVOKABLE void timeout();

signals:
    void stateChanged();
    void selectedPathsChanged();
    void completed(const Astrea::Explorer::Native::Backend::PortalResult &result);

private:
    void finish(bool accepted, const QString &reason);

    bool m_active = false;
    bool m_terminal = false;
    PortalOptions m_options;
    QStringList m_selectedPaths;
};

} // namespace Astrea::Explorer::Native::Backend

Q_DECLARE_METATYPE(Astrea::Explorer::Native::Backend::PortalOptions)
Q_DECLARE_METATYPE(Astrea::Explorer::Native::Backend::PortalResult)
