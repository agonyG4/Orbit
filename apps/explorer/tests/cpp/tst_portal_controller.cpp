#include <QSignalSpy>
#include <QtTest>

#include "controllers/portal_controller.h"

using namespace Astrea::Explorer::Native::Backend;

class PortalControllerTest final : public QObject
{
    Q_OBJECT

private slots:
    void acceptsExactlyOnce();
    void rejectAndCloseDoNotDuplicateTerminalResult();
};

void PortalControllerTest::acceptsExactlyOnce()
{
    PortalController controller;
    QSignalSpy completedSpy(&controller, &PortalController::completed);
    controller.begin({QStringLiteral("open"), false, false, QStringLiteral("/tmp")});
    controller.setSelectedPaths({QStringLiteral("/tmp/file.txt")});
    controller.accept();
    controller.accept();

    QTRY_COMPARE(completedSpy.count(), 1);
    const PortalResult result = completedSpy.takeFirst().at(0).value<PortalResult>();
    QCOMPARE(result.accepted, true);
    QCOMPARE(result.paths, QStringList({QStringLiteral("/tmp/file.txt")}));
}

void PortalControllerTest::rejectAndCloseDoNotDuplicateTerminalResult()
{
    PortalController controller;
    QSignalSpy completedSpy(&controller, &PortalController::completed);
    controller.begin({QStringLiteral("save"), false, false, QStringLiteral("/tmp")});
    controller.reject();
    controller.close();
    controller.consumerDied();

    QTRY_COMPARE(completedSpy.count(), 1);
    const PortalResult result = completedSpy.takeFirst().at(0).value<PortalResult>();
    QCOMPARE(result.accepted, false);
    QVERIFY(result.reason == QStringLiteral("rejected"));
}

QTEST_GUILESS_MAIN(PortalControllerTest)

#include "tst_portal_controller.moc"
