#include "controllers/portal_controller.h"

#include <QFileInfo>

namespace Astrea::Explorer::Native::Backend {

PortalController::PortalController(QObject *parent)
    : QObject(parent)
{
}

bool PortalController::active() const
{
    return m_active;
}

QString PortalController::selectionMode() const
{
    return m_options.mode;
}

bool PortalController::multiple() const
{
    return m_options.multiple;
}

bool PortalController::directoryOnly() const
{
    return m_options.directoryOnly;
}

QString PortalController::currentLocation() const
{
    return m_options.currentLocation;
}

QStringList PortalController::selectedPaths() const
{
    return m_selectedPaths;
}

void PortalController::begin(const PortalOptions &options)
{
    m_options = options;
    m_selectedPaths.clear();
    m_active = true;
    m_terminal = false;
    emit stateChanged();
    emit selectedPathsChanged();
}

void PortalController::setSelectedPaths(const QStringList &paths)
{
    if (!m_active || m_selectedPaths == paths) {
        return;
    }
    m_selectedPaths = paths;
    emit selectedPathsChanged();
}

void PortalController::accept()
{
    if (!m_active || m_terminal) {
        return;
    }
    if (m_selectedPaths.isEmpty()) {
        finish(false, QStringLiteral("selection_required"));
        return;
    }
    QStringList paths = m_selectedPaths;
    if (!m_options.multiple && paths.size() > 1) {
        paths = {paths.constFirst()};
    }
    if (m_options.directoryOnly) {
        for (const QString &path : paths) {
            if (!QFileInfo(path).isDir()) {
                finish(false, QStringLiteral("directory_required"));
                return;
            }
        }
    }
    m_selectedPaths = paths;
    finish(true, QStringLiteral("accepted"));
}

void PortalController::reject()
{
    finish(false, QStringLiteral("rejected"));
}

void PortalController::close()
{
    finish(false, QStringLiteral("closed"));
}

void PortalController::consumerDied()
{
    finish(false, QStringLiteral("consumer_died"));
}

void PortalController::timeout()
{
    finish(false, QStringLiteral("timeout"));
}

void PortalController::finish(bool accepted, const QString &reason)
{
    if (!m_active || m_terminal) {
        return;
    }
    m_terminal = true;
    m_active = false;
    PortalResult result;
    result.accepted = accepted;
    result.paths = accepted ? m_selectedPaths : QStringList();
    result.reason = reason;
    emit stateChanged();
    emit completed(result);
}

} // namespace Astrea::Explorer::Native::Backend
