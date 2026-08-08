#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include "backend/fake_backend_client.h"
#include "controllers/navigation_controller.h"
#include "models/directory_model.h"
#include "services/directory_watch_service.h"

using namespace Astrea::Explorer::Native::Backend;

class NavigationControllerTest final : public QObject
{
    Q_OBJECT

private slots:
    void rejectsLateNavigationResult();
    void cancelsSupersededSearch();
    void preservesTabsAndHistory();
    void reportsInaccessiblePath();
    void debouncesLocalWatcherChanges();
    void suppressesWatcherForRemotePath();
};

DirectoryEntry navigationEntry(const QString &name, const QString &path)
{
    DirectoryEntry entry;
    entry.fileName = name;
    entry.filePath = path;
    entry.fileUrl = QUrl::fromLocalFile(path);
    entry.fileKind = QStringLiteral("TXT");
    return entry;
}

void NavigationControllerTest::rejectsLateNavigationResult()
{
    FakeRustBackendClient client;
    DirectoryModel model;
    DirectoryWatchService watcher;
    NavigationController navigation(&client, &model, &watcher);

    const BackendRequestId requestA = navigation.navigateTo(QStringLiteral("/A"));
    const BackendRequestId requestB = navigation.navigateTo(QStringLiteral("/B"));
    QCOMPARE(client.cancelledRequests(), QVector<BackendRequestId>({requestA}));

    client.completeList(
        requestB,
        {navigationEntry(QStringLiteral("b.txt"), QStringLiteral("/B/b.txt"))});
    QTRY_COMPARE(navigation.loading(), false);
    client.completeList(
        requestA,
        {navigationEntry(QStringLiteral("a.txt"), QStringLiteral("/A/a.txt"))});

    QCOMPARE(navigation.currentPath(), QStringLiteral("/B"));
    QCOMPARE(model.paths(), QVector<QString>({QStringLiteral("/B/b.txt")}));
    QCOMPARE(navigation.loadError(), QString());
}

void NavigationControllerTest::cancelsSupersededSearch()
{
    FakeRustBackendClient client;
    DirectoryModel model;
    DirectoryWatchService watcher;
    NavigationController navigation(&client, &model, &watcher);

    const BackendRequestId directoryRequest = navigation.navigateTo(QStringLiteral("/root"));
    client.completeList(directoryRequest, {});

    const BackendRequestId searchA = navigation.submitSearch(
        QStringLiteral("/root"),
        QStringLiteral("first"));
    const BackendRequestId searchB = navigation.submitSearch(
        QStringLiteral("/root"),
        QStringLiteral("second"));

    QCOMPARE(client.cancelledRequests().contains(searchA), true);
    client.completeSearch(
        searchA,
        {navigationEntry(QStringLiteral("first.txt"), QStringLiteral("/root/first.txt"))});
    client.completeSearch(
        searchB,
        {navigationEntry(QStringLiteral("second.txt"), QStringLiteral("/root/second.txt"))});
    QTRY_COMPARE(navigation.loading(), false);

    QVERIFY(navigation.searchActive());
    QCOMPARE(navigation.searchQuery(), QStringLiteral("second"));
    QCOMPARE(model.paths(), QVector<QString>({QStringLiteral("/root/second.txt")}));
}

void NavigationControllerTest::preservesTabsAndHistory()
{
    FakeRustBackendClient client;
    DirectoryModel model;
    DirectoryWatchService watcher;
    NavigationController navigation(&client, &model, &watcher);

    const BackendRequestId homeRequest = navigation.navigateTo(QStringLiteral("/home"));
    client.completeList(homeRequest, {});
    const BackendRequestId oneRequest = navigation.navigateTo(QStringLiteral("/one"));
    client.completeList(oneRequest, {});
    const BackendRequestId twoRequest = navigation.navigateTo(QStringLiteral("/two"));
    client.completeList(twoRequest, {});

    navigation.goBack();
    QCOMPARE(navigation.currentPath(), QStringLiteral("/one"));
    QCOMPARE(navigation.historyIndex(), 1);
    navigation.goForward();
    QCOMPARE(navigation.currentPath(), QStringLiteral("/two"));
    QCOMPARE(navigation.historyIndex(), 2);

    navigation.createTab(QStringLiteral("/other"));
    QCOMPARE(navigation.tabCount(), 2);
    QCOMPARE(navigation.activeTabIndex(), 1);
    QCOMPARE(navigation.currentPath(), QStringLiteral("/other"));

    navigation.switchTab(0);
    QCOMPARE(navigation.currentPath(), QStringLiteral("/two"));
    QCOMPARE(navigation.activeTabIndex(), 0);
    navigation.closeTab(1);
    QCOMPARE(navigation.tabCount(), 1);
}

void NavigationControllerTest::reportsInaccessiblePath()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    FakeRustBackendClient client;
    DirectoryModel model;
    DirectoryWatchService watcher;
    NavigationController navigation(&client, &model, &watcher);
    QSignalSpy failedSpy(&navigation, &NavigationController::navigationFailed);

    const BackendRequestId requestId = navigation.navigateTo(
        directory.filePath(QStringLiteral("missing")));
    client.failRequest(requestId, QStringLiteral("backend_exit"), QStringLiteral("not found"));

    QTRY_COMPARE(failedSpy.count(), 1);
    QCOMPARE(navigation.loading(), false);
    QCOMPARE(navigation.loadError(), QStringLiteral("not found"));
    QCOMPARE(model.rowCount(), 0);
}

void NavigationControllerTest::debouncesLocalWatcherChanges()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    DirectoryWatchService watcher;
    QSignalSpy refreshSpy(&watcher, &DirectoryWatchService::directoryChanged);
    watcher.watchLocalDirectory(directory.path());
    QCOMPARE(watcher.watchedPath(), directory.path());

    for (int i = 0; i < 3; ++i) {
        QVERIFY(QMetaObject::invokeMethod(
            &watcher,
            "handleDirectoryChanged",
            Qt::DirectConnection,
            Q_ARG(QString, directory.path())));
    }

    QTRY_COMPARE_WITH_TIMEOUT(refreshSpy.count(), 1, 1000);
    QCOMPARE(refreshSpy.takeFirst().at(0).toString(), directory.path());
}

void NavigationControllerTest::suppressesWatcherForRemotePath()
{
    FakeRustBackendClient client;
    DirectoryModel model;
    DirectoryWatchService watcher;
    NavigationController navigation(&client, &model, &watcher);

    const BackendRequestId requestId = navigation.navigateTo(QStringLiteral("smb://server/share"));
    QCOMPARE(navigation.remoteDirectoryActive(), true);
    QCOMPARE(watcher.watchedPath(), QString());
    client.completeList(requestId, {});
    QCOMPARE(watcher.watchedPath(), QString());
}

QTEST_MAIN(NavigationControllerTest)

#include "tst_navigation_controller.moc"
