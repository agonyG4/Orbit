#include <QtTest>

#include "controllers/open_with_controller.h"

using namespace Astrea::Explorer::Native::Backend;

class OpenWithControllerTest final : public QObject
{
    Q_OBJECT

private slots:
    void exposesTypedApplicationCatalogAndSelection();
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

QTEST_GUILESS_MAIN(OpenWithControllerTest)

#include "tst_open_with_controller.moc"
