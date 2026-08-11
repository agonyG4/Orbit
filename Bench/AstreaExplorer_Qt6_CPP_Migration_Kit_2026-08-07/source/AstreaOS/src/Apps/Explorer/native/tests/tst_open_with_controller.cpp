#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QtTest>

#include "controllers/open_with_controller.h"

using namespace Astrea::Explorer::Native::Backend;

class OpenWithControllerTest final : public QObject
{
    Q_OBJECT

private slots:
    void exposesTypedApplicationCatalogAndSelection();
    void resolvesDesktopEntryForRecentHistory();
    void resolvesManyDesktopEntriesThroughOneIndexedCatalog();
};

void OpenWithControllerTest::exposesTypedApplicationCatalogAndSelection()
{
    OpenWithController controller;
    const QVector<OpenWithApplication> applications {
        {QStringLiteral("org.example.Editor"), QStringLiteral("Editor"),
         QStringLiteral("accessories-text-editor"), QStringLiteral("/tmp/editor.desktop"), true},
        {QStringLiteral("org.example.Viewer"), QStringLiteral("Viewer"),
         QStringLiteral("image-viewer"), QStringLiteral("/tmp/viewer.desktop"), false},
    };
    controller.setApplications(applications);

    QCOMPARE(controller.applications().size(), 2);
    QCOMPARE(controller.selectedApplicationId(), QStringLiteral("org.example.Editor"));
    controller.selectApplication(QStringLiteral("org.example.Viewer"));
    QCOMPARE(controller.selectedApplicationId(), QStringLiteral("org.example.Viewer"));
    QCOMPARE(controller.selectedApplication().name, QStringLiteral("Viewer"));
}

void OpenWithControllerTest::resolvesDesktopEntryForRecentHistory()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString desktopFile = QDir(directory.path()).filePath(QStringLiteral("example.desktop"));
    QFile file(desktopFile);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QVERIFY(file.write(QByteArrayLiteral(
        "[Desktop Entry]\n"
        "Type=Application\n"
        "Name=Example Application\n"
        "Icon=example-icon\n"
        "Exec=example\n")) > 0);
    file.close();

    const OpenWithApplication application =
        OpenWithController::resolveDesktopEntry(desktopFile);
    QCOMPARE(application.id, QStringLiteral("example"));
    QCOMPARE(application.name, QStringLiteral("Example Application"));
    QCOMPARE(application.icon, QStringLiteral("example-icon"));
    QCOMPARE(application.desktopFile, QFileInfo(desktopFile).absoluteFilePath());
}

void OpenWithControllerTest::resolvesManyDesktopEntriesThroughOneIndexedCatalog()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    for (int index = 0; index < 8; ++index) {
        const QString desktopFile = QDir(directory.path()).filePath(
            QStringLiteral("example-%1.desktop").arg(index));
        QFile file(desktopFile);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QVERIFY(file.write(QStringLiteral(
            "[Desktop Entry]\n"
            "Type=Application\n"
            "Name=Example %1\n"
            "Icon=example-%1\n"
            "Exec=example-%1\n").arg(index).toUtf8()) > 0);
        file.close();
    }

    const OpenWithController::DesktopCatalog catalog =
        OpenWithController::buildDesktopCatalog({directory.path()});
    QCOMPARE(catalog.size(), 8);
    for (int index = 0; index < 8; ++index) {
        const OpenWithApplication application = OpenWithController::resolveDesktopEntry(
            QStringLiteral("example-%1.desktop").arg(index),
            &catalog);
        QCOMPARE(application.name, QStringLiteral("Example %1").arg(index));
        QCOMPARE(application.icon, QStringLiteral("example-%1").arg(index));
    }
}

QTEST_GUILESS_MAIN(OpenWithControllerTest)

#include "tst_open_with_controller.moc"
