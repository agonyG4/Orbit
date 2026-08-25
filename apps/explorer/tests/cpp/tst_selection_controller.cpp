#include <QQmlComponent>
#include <QQmlEngine>
#include <QSignalSpy>
#include <QtTest>

#include "controllers/app_state_facade.h"
#include "controllers/selection_controller.h"
#include "models/directory_model.h"
#include "services/directory_watch_service.h"
#include "controllers/navigation_controller.h"
#include "backend/fake_backend_client.h"

using namespace Astrea::Explorer::Native::Backend;

class SelectionControllerTest final : public QObject
{
    Q_OBJECT

private slots:
    void selectsSingleItem();
    void replacesSelectionWithoutModifiers();
    void togglesCtrlMultiSelection();
    void selectsShiftRange();
    void preservesSelectionForDragOnlyWhenSelected();
    void selectsAllItems();
    void reconcilesSelectionByStablePath();
    void removesSelectedPathsAfterReplacement();
    void selectsDuplicateNamesByPath();
    void exposesFacadeToQmlEngine();
};

DirectoryEntry selectionEntry(const QString &name, const QString &path)
{
    DirectoryEntry entry;
    entry.fileName = name;
    entry.filePath = path;
    entry.fileUrl = QUrl::fromLocalFile(path);
    return entry;
}

QVector<DirectoryEntry> selectionEntries()
{
    return {
        selectionEntry(QStringLiteral("one.txt"), QStringLiteral("/fixture/one.txt")),
        selectionEntry(QStringLiteral("two.txt"), QStringLiteral("/fixture/two.txt")),
        selectionEntry(QStringLiteral("three.txt"), QStringLiteral("/fixture/three.txt")),
        selectionEntry(QStringLiteral("four.txt"), QStringLiteral("/fixture/four.txt")),
    };
}

void SelectionControllerTest::selectsSingleItem()
{
    DirectoryModel model;
    QVERIFY(model.applyEntries(selectionEntries(), 1));
    SelectionController selection(&model);

    selection.handleSelection(QStringLiteral("two.txt"), 1, false, false, false);

    QCOMPARE(selection.selectedFile(), QStringLiteral("two.txt"));
    QCOMPARE(selection.selectedFiles(), QStringList({QStringLiteral("two.txt")}));
    QCOMPARE(selection.lastSelectedIndex(), 1);
    QVERIFY(selection.isSelected(QStringLiteral("two.txt")));
    QVERIFY(!selection.isSelected(QStringLiteral("one.txt")));
}

void SelectionControllerTest::replacesSelectionWithoutModifiers()
{
    DirectoryModel model;
    QVERIFY(model.applyEntries(selectionEntries(), 1));
    SelectionController selection(&model);

    selection.handleSelection(QStringLiteral("one.txt"), 0, false, false, false);
    selection.handleSelection(QStringLiteral("two.txt"), 1, false, false, false);

    QCOMPARE(selection.selectedFiles(), QStringList({QStringLiteral("two.txt")}));
    QVERIFY(!selection.isSelected(QStringLiteral("one.txt")));
}

void SelectionControllerTest::togglesCtrlMultiSelection()
{
    DirectoryModel model;
    QVERIFY(model.applyEntries(selectionEntries(), 1));
    SelectionController selection(&model);

    selection.handleSelection(QStringLiteral("one.txt"), 0, false, false, false);
    selection.handleSelection(QStringLiteral("three.txt"), 2, true, false, false);
    QCOMPARE(
        selection.selectedFiles(),
        QStringList({QStringLiteral("one.txt"), QStringLiteral("three.txt")}));
    QCOMPARE(selection.selectedFile(), QStringLiteral("three.txt"));

    selection.handleSelection(QStringLiteral("one.txt"), 0, true, false, false);
    QCOMPARE(selection.selectedFiles(), QStringList({QStringLiteral("three.txt")}));
    QCOMPARE(selection.selectedFile(), QStringLiteral("three.txt"));
}

void SelectionControllerTest::selectsShiftRange()
{
    DirectoryModel model;
    QVERIFY(model.applyEntries(selectionEntries(), 1));
    SelectionController selection(&model);

    selection.handleSelection(QStringLiteral("two.txt"), 1, false, false, false);
    selection.handleSelection(QStringLiteral("four.txt"), 3, false, true, false);

    QCOMPARE(
        selection.selectedFiles(),
        QStringList(
            {QStringLiteral("two.txt"),
             QStringLiteral("three.txt"),
             QStringLiteral("four.txt")}));
    QCOMPARE(selection.selectedFile(), QStringLiteral("four.txt"));
}

void SelectionControllerTest::preservesSelectionForDragOnlyWhenSelected()
{
    DirectoryModel model;
    QVERIFY(model.applyEntries(selectionEntries(), 1));
    SelectionController selection(&model);

    selection.handleSelection(QStringLiteral("one.txt"), 0, false, false, false);
    selection.handleSelection(QStringLiteral("three.txt"), 2, true, false, false);
    selection.prepareSelectionForDrag(QStringLiteral("one.txt"), 0);

    QCOMPARE(
        selection.selectedFiles(),
        QStringList({QStringLiteral("one.txt"), QStringLiteral("three.txt")}));

    selection.prepareSelectionForDrag(QStringLiteral("four.txt"), 3);
    QCOMPARE(selection.selectedFiles(), QStringList({QStringLiteral("four.txt")}));
}

void SelectionControllerTest::selectsAllItems()
{
    DirectoryModel model;
    QVERIFY(model.applyEntries(selectionEntries(), 1));
    SelectionController selection(&model);

    selection.selectAll();

    QCOMPARE(selection.selectedFiles().size(), 4);
    QCOMPARE(selection.selectedFile(), QStringLiteral("four.txt"));
    QCOMPARE(selection.lastSelectedIndex(), -1);
}

void SelectionControllerTest::reconcilesSelectionByStablePath()
{
    DirectoryModel model;
    QVERIFY(model.applyEntries(selectionEntries(), 1));
    SelectionController selection(&model);
    selection.handleSelection(QStringLiteral("two.txt"), 1, false, false, false);
    selection.handleSelection(QStringLiteral("four.txt"), 3, true, false, false);

    QVERIFY(model.applyEntries(
        {selectionEntry(QStringLiteral("four-renamed.txt"), QStringLiteral("/fixture/four.txt")),
         selectionEntry(QStringLiteral("two-renamed.txt"), QStringLiteral("/fixture/two.txt"))},
        2));

    QCOMPARE(
        selection.selectedFiles(),
        QStringList({QStringLiteral("two-renamed.txt"), QStringLiteral("four-renamed.txt")}));
    QCOMPARE(selection.selectedFile(), QStringLiteral("four-renamed.txt"));
    QCOMPARE(selection.lastSelectedIndex(), 0);
}

void SelectionControllerTest::removesSelectedPathsAfterReplacement()
{
    DirectoryModel model;
    QVERIFY(model.applyEntries(selectionEntries(), 1));
    SelectionController selection(&model);
    QSignalSpy changedSpy(&selection, &SelectionController::selectedFilesChanged);
    selection.selectByName(QStringLiteral("three.txt"));

    QVERIFY(model.applyEntries(
        {selectionEntry(QStringLiteral("one.txt"), QStringLiteral("/fixture/one.txt"))},
        2));

    QVERIFY(changedSpy.count() >= 1);
    QCOMPARE(selection.selectedFile(), QString());
    QVERIFY(selection.selectedFiles().isEmpty());
    QCOMPARE(selection.lastSelectedIndex(), -1);
}

void SelectionControllerTest::selectsDuplicateNamesByPath()
{
    DirectoryModel model;
    QVERIFY(model.applyEntries(
        {selectionEntry(QStringLiteral("report.txt"), QStringLiteral("/one/report.txt")),
         selectionEntry(QStringLiteral("report.txt"), QStringLiteral("/two/report.txt"))},
        1));
    SelectionController selection(&model);

    selection.selectByPath(QStringLiteral("/two/report.txt"));

    QVERIFY(selection.isPathSelected(QStringLiteral("/two/report.txt")));
    QVERIFY(!selection.isPathSelected(QStringLiteral("/one/report.txt")));
    QCOMPARE(selection.selectedPaths(), QStringList({QStringLiteral("/two/report.txt")}));
    QCOMPARE(selection.selectedFiles(), QStringList({QStringLiteral("report.txt")}));
}

void SelectionControllerTest::exposesFacadeToQmlEngine()
{
    FakeRustBackendClient client;
    DirectoryModel model;
    DirectoryWatchService watcher;
    NavigationController navigation(&client, &model, &watcher);
    SelectionController selection(&model);
    AppStateFacade facade(&navigation, &selection, &model);
    QVERIFY(model.applyEntries(selectionEntries(), 1));
    selection.selectByName(QStringLiteral("two.txt"));

    qmlRegisterSingletonInstance<AppStateFacade>(
        "Astrea.Explorer.Native.Test",
        1,
        0,
        "NativeAppState",
        &facade);
    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.setData(
        QByteArrayLiteral(
            "import QtQml\n"
            "import Astrea.Explorer.Native.Test\n"
            "QtObject {\n"
            "    property bool modelAvailable: NativeAppState.fileModel !== null\n"
            "    property string selected: NativeAppState.selectedFile\n"
            "}"),
        QUrl(QStringLiteral("qrc:/selection-facade-smoke.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));

    QObject *root = component.create();
    QVERIFY2(root != nullptr, qPrintable(component.errorString()));
    QCOMPARE(root->property("modelAvailable").toBool(), true);
    QCOMPARE(root->property("selected").toString(), QStringLiteral("two.txt"));
    delete root;
}

QTEST_MAIN(SelectionControllerTest)

#include "tst_selection_controller.moc"
