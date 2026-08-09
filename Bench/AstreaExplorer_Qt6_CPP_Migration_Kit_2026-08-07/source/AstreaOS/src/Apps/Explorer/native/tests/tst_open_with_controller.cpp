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

QTEST_GUILESS_MAIN(OpenWithControllerTest)

#include "tst_open_with_controller.moc"
