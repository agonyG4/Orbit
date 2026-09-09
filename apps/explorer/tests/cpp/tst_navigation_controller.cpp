#include <functional>

#include <QFile>
#include <QJsonArray>
#include <QJsonObject>
#include <QSignalSpy>
#include <QSet>
#include <QTemporaryDir>
#include <QtTest>

#include "backend/fake_backend_client.h"
#include "controllers/navigation_controller.h"
#include "controllers/recent_controller.h"
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
    void loadsRecentPathWithoutBackendListingAndPreservesHistory();
    void ignoresRecentCompletionAfterNavigationAway();
    void honorsConfiguredRemotePrefixesAtPathBoundaries();
    void forwardsListingOptionsToBackend();
    void requestsVisibleVisualMetadataInBoundedBatches();
    void queuesVisualMetadataInModelOrder();
    void latestVisibleRangeReplacesQueuedWork();
    void latestVisibleRangeWinsAfterInFlightBatch();
    void duplicateVisibleRangeSchedulingDoesNotDuplicateRequests();
    void ignoresStaleVisualMetadataAfterNavigation();
    void skipsRemoteVisualMetadataRequests();
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

class NavigationManualDispatch final
{
public:
    void operator()(std::function<void()> job)
    {
        jobs.append(std::move(job));
    }

    QVector<std::function<void()>> jobs;
};

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

void NavigationControllerTest::forwardsListingOptionsToBackend()
{
    FakeRustBackendClient client;
    DirectoryModel model;
    DirectoryWatchService watcher;
    NavigationController navigation(&client, &model, &watcher);

    navigation.setShowHidden(true);
    navigation.setSortField(QStringLiteral("date"));
    navigation.setSortAscending(false);
    navigation.setFoldersFirst(false);
    navigation.setPreviews(false);
    const BackendRequestId requestId = navigation.navigateTo(QStringLiteral("/fixture"));
    QCOMPARE(client.listRequests().constLast().path, QStringLiteral("/fixture"));
    QCOMPARE(client.listRequests().constLast().showHidden, true);
    QCOMPARE(client.listRequests().constLast().sortField, QStringLiteral("date"));
    QCOMPARE(client.listRequests().constLast().sortAscending, false);
    QCOMPARE(client.listRequests().constLast().foldersFirst, false);
    QCOMPARE(client.listRequests().constLast().previews, false);
    client.completeList(requestId, {});
}

void NavigationControllerTest::requestsVisibleVisualMetadataInBoundedBatches()
{
    FakeRustBackendClient client;
    DirectoryModel model;
    DirectoryWatchService watcher;
    NavigationController navigation(&client, &model, &watcher);

    const BackendRequestId listRequest = navigation.navigateTo(QStringLiteral("/fixture"));
    QVector<DirectoryEntry> entries;
    for (int index = 0; index < 70; ++index) {
        entries.append(navigationEntry(
            QStringLiteral("file-%1.txt").arg(index),
            QStringLiteral("/fixture/file-%1.txt").arg(index)));
    }
    client.completeList(listRequest, entries);
    QTRY_COMPARE(model.count(), 70);

    navigation.requestFileVisualMetadata(0, 69);
    QTRY_COMPARE_WITH_TIMEOUT(client.utilityRequests().size(), 1, 1000);
    const UtilityRequest firstRequest = client.utilityRequests().constFirst();
    QCOMPARE(firstRequest.operation, QStringLiteral("file-visual-metadata"));
    QCOMPARE(firstRequest.arguments.size(), 64);
    QCOMPARE(QSet<QString>(firstRequest.arguments.cbegin(), firstRequest.arguments.cend()).size(), 64);

    UtilityResult result;
    result.operation = QStringLiteral("file-visual-metadata");
    result.ok = true;
    QJsonArray items;
    QJsonObject item;
    item.insert(QStringLiteral("filePath"), firstRequest.arguments.constFirst());
    item.insert(QStringLiteral("fileIconNames"), QJsonArray {QStringLiteral("text-x-generic")});
    item.insert(QStringLiteral("fileEmblemNames"), QJsonArray {QStringLiteral("readonly")});
    item.insert(QStringLiteral("fileIconMetadataReady"), true);
    items.append(item);
    result.data.insert(QStringLiteral("items"), items);
    client.completeUtility(BackendRequestId(2), result);

    int firstRow = -1;
    for (int row = 0; row < model.count(); ++row) {
        if (model.data(model.index(row, 0), DirectoryModel::FilePathRole).toString()
            == firstRequest.arguments.constFirst()) {
            firstRow = row;
            break;
        }
    }
    QVERIFY(firstRow >= 0);
    QTRY_COMPARE(model.data(model.index(firstRow, 0), DirectoryModel::FileIconMetadataReadyRole).toBool(), true);
    QTRY_COMPARE_WITH_TIMEOUT(client.utilityRequests().size(), 2, 1000);
    QCOMPARE(client.utilityRequests().at(1).arguments.size(), 6);
}

void NavigationControllerTest::queuesVisualMetadataInModelOrder()
{
    FakeRustBackendClient client;
    DirectoryModel model;
    DirectoryWatchService watcher;
    NavigationController navigation(&client, &model, &watcher);

    const BackendRequestId listRequest = navigation.navigateTo(QStringLiteral("/fixture"));
    QVector<DirectoryEntry> entries;
    for (int index = 0; index < 6; ++index) {
        entries.append(navigationEntry(
            QStringLiteral("row-%1.txt").arg(index),
            QStringLiteral("/fixture/row-%1.txt").arg(index)));
    }
    client.completeList(listRequest, entries);
    QTRY_COMPARE(model.count(), 6);

    navigation.requestFileVisualMetadata(0, 5);
    QTRY_COMPARE_WITH_TIMEOUT(client.utilityRequests().size(), 1, 1000);
    const QStringList expected {
        QStringLiteral("/fixture/row-0.txt"),
        QStringLiteral("/fixture/row-1.txt"),
        QStringLiteral("/fixture/row-2.txt"),
        QStringLiteral("/fixture/row-3.txt"),
        QStringLiteral("/fixture/row-4.txt"),
        QStringLiteral("/fixture/row-5.txt"),
    };
    QCOMPARE(
        client.utilityRequests().constFirst().arguments,
        expected);
}

void NavigationControllerTest::latestVisibleRangeReplacesQueuedWork()
{
    FakeRustBackendClient client;
    DirectoryModel model;
    DirectoryWatchService watcher;
    NavigationController navigation(&client, &model, &watcher);

    const BackendRequestId listRequest = navigation.navigateTo(QStringLiteral("/fixture"));
    QVector<DirectoryEntry> entries;
    for (int index = 0; index < 12; ++index) {
        entries.append(navigationEntry(
            QStringLiteral("row-%1.txt").arg(index),
            QStringLiteral("/fixture/row-%1.txt").arg(index)));
    }
    client.completeList(listRequest, entries);
    QTRY_COMPARE(model.count(), 12);

    navigation.requestFileVisualMetadata(0, 2);
    navigation.requestFileVisualMetadata(7, 9);
    QTRY_COMPARE_WITH_TIMEOUT(client.utilityRequests().size(), 1, 1000);
    const QStringList expected {
        QStringLiteral("/fixture/row-7.txt"),
        QStringLiteral("/fixture/row-8.txt"),
        QStringLiteral("/fixture/row-9.txt"),
    };
    QCOMPARE(
        client.utilityRequests().constFirst().arguments,
        expected);
}

void NavigationControllerTest::latestVisibleRangeWinsAfterInFlightBatch()
{
    FakeRustBackendClient client;
    DirectoryModel model;
    DirectoryWatchService watcher;
    NavigationController navigation(&client, &model, &watcher);

    const BackendRequestId listRequest = navigation.navigateTo(QStringLiteral("/fixture"));
    QVector<DirectoryEntry> entries;
    for (int index = 0; index < 12; ++index) {
        entries.append(navigationEntry(
            QStringLiteral("row-%1.txt").arg(index),
            QStringLiteral("/fixture/row-%1.txt").arg(index)));
    }
    client.completeList(listRequest, entries);
    QTRY_COMPARE(model.count(), 12);

    navigation.requestFileVisualMetadata(0, 2);
    QTRY_COMPARE_WITH_TIMEOUT(client.utilityRequests().size(), 1, 1000);
    navigation.requestFileVisualMetadata(5, 7);
    navigation.requestFileVisualMetadata(9, 11);

    UtilityResult result;
    result.operation = QStringLiteral("file-visual-metadata");
    result.ok = true;
    result.data.insert(QStringLiteral("items"), QJsonArray {});
    client.completeUtility(BackendRequestId(2), result);

    QTRY_COMPARE_WITH_TIMEOUT(client.utilityRequests().size(), 2, 1000);
    const QStringList expected {
        QStringLiteral("/fixture/row-9.txt"),
        QStringLiteral("/fixture/row-10.txt"),
        QStringLiteral("/fixture/row-11.txt"),
    };
    QCOMPARE(
        client.utilityRequests().at(1).arguments,
        expected);
}

void NavigationControllerTest::duplicateVisibleRangeSchedulingDoesNotDuplicateRequests()
{
    FakeRustBackendClient client;
    DirectoryModel model;
    DirectoryWatchService watcher;
    NavigationController navigation(&client, &model, &watcher);

    const BackendRequestId listRequest = navigation.navigateTo(QStringLiteral("/fixture"));
    QVector<DirectoryEntry> entries;
    for (int index = 0; index < 3; ++index) {
        entries.append(navigationEntry(
            QStringLiteral("row-%1.txt").arg(index),
            QStringLiteral("/fixture/row-%1.txt").arg(index)));
    }
    client.completeList(listRequest, entries);
    QTRY_COMPARE(model.count(), 3);

    navigation.requestFileVisualMetadata(0, 2);
    navigation.requestFileVisualMetadata(0, 2);
    QTRY_COMPARE_WITH_TIMEOUT(client.utilityRequests().size(), 1, 1000);
    QTest::qWait(100);
    QCOMPARE(client.utilityRequests().size(), 1);
}

void NavigationControllerTest::ignoresStaleVisualMetadataAfterNavigation()
{
    FakeRustBackendClient client;
    DirectoryModel model;
    DirectoryWatchService watcher;
    NavigationController navigation(&client, &model, &watcher);

    const BackendRequestId firstList = navigation.navigateTo(QStringLiteral("/first"));
    client.completeList(firstList, {
        navigationEntry(QStringLiteral("old.txt"), QStringLiteral("/first/old.txt"))});
    QTRY_COMPARE(model.count(), 1);
    navigation.requestFileVisualMetadata(0, 0);
    QTRY_COMPARE_WITH_TIMEOUT(client.utilityRequests().size(), 1, 1000);

    const BackendRequestId secondList = navigation.navigateTo(QStringLiteral("/second"));
    QVERIFY(client.cancelledRequests().contains(BackendRequestId(2)));
    client.completeUtility(BackendRequestId(2), UtilityResult {
        .requestId = 2,
        .operation = QStringLiteral("file-visual-metadata"),
        .ok = true,
        .data = QJsonObject {{QStringLiteral("items"), QJsonArray {
            QJsonObject {{QStringLiteral("filePath"), QStringLiteral("/first/old.txt")},
                {QStringLiteral("fileIconMetadataReady"), true}}}}}});
    client.completeList(secondList, {});

    QTRY_COMPARE(model.count(), 0);
    QCOMPARE(navigation.currentPath(), QStringLiteral("/second"));
}

void NavigationControllerTest::skipsRemoteVisualMetadataRequests()
{
    FakeRustBackendClient client;
    DirectoryModel model;
    DirectoryWatchService watcher;
    NavigationController navigation(&client, &model, &watcher);

    const BackendRequestId listRequest = navigation.navigateTo(QStringLiteral("/fixture"));
    DirectoryEntry remote = navigationEntry(QStringLiteral("remote.txt"), QStringLiteral("/fixture/remote.txt"));
    remote.fileRemote = true;
    DirectoryEntry limited = navigationEntry(QStringLiteral("limited.txt"), QStringLiteral("/fixture/limited.txt"));
    limited.fileMetadataLimited = true;
    client.completeList(listRequest, {remote, limited});
    QTRY_COMPARE(model.count(), 2);

    navigation.requestFileVisualMetadata(0, 1);
    QTest::qWait(100);
    QCOMPARE(client.utilityRequests().size(), 0);
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

void NavigationControllerTest::loadsRecentPathWithoutBackendListingAndPreservesHistory()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString recentFile = directory.filePath(QStringLiteral("recent.txt"));
    QFile fixtureFile(recentFile);
    QVERIFY(fixtureFile.open(QIODevice::WriteOnly));
    fixtureFile.write("recent fixture");
    fixtureFile.close();

    const QString finderPath = directory.filePath(QStringLiteral("finder-recents.json"));
    QFile finder(finderPath);
    QVERIFY(finder.open(QIODevice::WriteOnly));
    finder.write(QStringLiteral(
        "[{\"filePath\":\"%1\",\"lastAccessed\":1723265945000}]\n")
                     .arg(recentFile)
                     .toUtf8());
    finder.close();

    FakeRustBackendClient client;
    DirectoryModel model;
    DirectoryWatchService watcher;
    NavigationManualDispatch dispatch;
    RecentStore store(
        RecentSourcePaths {finderPath, QString(), QString(), 60},
        nullptr,
        std::ref(dispatch));
    RecentController recent(&store);
    NavigationController navigation(&client, &model, &watcher);
    RecentSourcePaths sources;
    sources.finderPath = finderPath;
    sources.limit = 60;
    navigation.setRecentController(&recent, sources);

    const BackendRequestId recentRequest = navigation.navigateTo(QStringLiteral("recent://"));
    QVERIFY(recentRequest != 0);
    QCOMPARE(client.listRequests().size(), 0);
    QCOMPARE(navigation.loading(), true);
    QCOMPARE(dispatch.jobs.size(), 1);
    dispatch.jobs.first()();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE(navigation.loading(), false);
    QCOMPARE(navigation.loadError(), QString());
    QCOMPARE(navigation.currentPath(), QStringLiteral("recent://"));
    QCOMPARE(navigation.history(), QStringList({QStringLiteral("recent://")}));
    QCOMPARE(watcher.watchedPath(), QString());
    QCOMPARE(model.count(), 1);
    QCOMPARE(model.data(model.index(0, 0), DirectoryModel::FilePathRole).toString(), recentFile);

    const BackendRequestId refreshRequest = navigation.refreshCurrentFolder();
    QVERIFY(refreshRequest != 0);
    QCOMPARE(dispatch.jobs.size(), 2);
    dispatch.jobs.last()();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE(client.listRequests().size(), 0);

    const BackendRequestId homeRequest = navigation.navigateTo(QStringLiteral("/home"));
    QCOMPARE(homeRequest, BackendRequestId(1));
    navigation.goBack();
    QCOMPARE(navigation.currentPath(), QStringLiteral("recent://"));
    QCOMPARE(navigation.historyIndex(), 0);
    QVERIFY(dispatch.jobs.size() >= 3);
    dispatch.jobs.last()();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE(navigation.loading(), false);
    QCOMPARE(model.count(), 1);
    QCOMPARE(client.listRequests().size(), 1);
    navigation.goForward();
    QCOMPARE(navigation.currentPath(), QStringLiteral("/home"));
    QCOMPARE(navigation.historyIndex(), 1);
}

void NavigationControllerTest::ignoresRecentCompletionAfterNavigationAway()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString recentFile = directory.filePath(QStringLiteral("recent.txt"));
    QFile fixtureFile(recentFile);
    QVERIFY(fixtureFile.open(QIODevice::WriteOnly));
    fixtureFile.write("recent fixture");
    fixtureFile.close();
    const QString finderPath = directory.filePath(QStringLiteral("finder.json"));
    QFile finder(finderPath);
    QVERIFY(finder.open(QIODevice::WriteOnly));
    finder.write(QStringLiteral("[{\"filePath\":\"%1\",\"lastAccessed\":100}]\n")
                     .arg(recentFile)
                     .toUtf8());
    finder.close();

    FakeRustBackendClient client;
    DirectoryModel model;
    DirectoryWatchService watcher;
    NavigationManualDispatch dispatch;
    RecentStore store(
        RecentSourcePaths {finderPath, QString(), QString(), 60},
        nullptr,
        std::ref(dispatch));
    RecentController recent(&store);
    NavigationController navigation(&client, &model, &watcher);
    navigation.setRecentController(&recent);

    const BackendRequestId recentRequest = navigation.navigateTo(QStringLiteral("recent://"));
    QVERIFY(recentRequest != 0);
    const BackendRequestId homeRequest = navigation.navigateTo(QStringLiteral("/home"));
    QVERIFY(homeRequest != 0);
    client.completeList(homeRequest, {navigationEntry(QStringLiteral("home.txt"), QStringLiteral("/home/home.txt"))});
    QVERIFY(!dispatch.jobs.isEmpty());
    dispatch.jobs.first()();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();

    QCOMPARE(navigation.currentPath(), QStringLiteral("/home"));
    QCOMPARE(model.paths(), QVector<QString>({QStringLiteral("/home/home.txt")}));
    QVERIFY(navigation.loading() == false);
}

void NavigationControllerTest::honorsConfiguredRemotePrefixesAtPathBoundaries()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString prefix = directory.filePath(QStringLiteral("remote"));
    const QString child = QDir(prefix).filePath(QStringLiteral("child"));
    const QString sibling = directory.filePath(QStringLiteral("remote2"));
    QVERIFY(QDir().mkpath(child));
    QVERIFY(QDir().mkpath(sibling));

    const bool hadValue = qEnvironmentVariableIsSet("ASTREA_EXPLORER_REMOTE_PREFIXES");
    const QByteArray previous = qgetenv("ASTREA_EXPLORER_REMOTE_PREFIXES");
    qputenv("ASTREA_EXPLORER_REMOTE_PREFIXES", (prefix + ":" + prefix + "/").toUtf8());

    FakeRustBackendClient client;
    DirectoryModel model;
    DirectoryWatchService watcher;
    NavigationController navigation(&client, &model, &watcher);
    const BackendRequestId remoteRequest = navigation.navigateTo(child);
    QVERIFY(remoteRequest != 0);
    QVERIFY(navigation.remoteDirectoryActive());
    QCOMPARE(watcher.watchedPath(), QString());
    client.completeList(remoteRequest, {});

    const BackendRequestId siblingRequest = navigation.navigateTo(sibling);
    QVERIFY(siblingRequest != 0);
    QVERIFY(!navigation.remoteDirectoryActive());
    QCOMPARE(watcher.watchedPath(), sibling);

    if (hadValue) {
        qputenv("ASTREA_EXPLORER_REMOTE_PREFIXES", previous);
    } else {
        qunsetenv("ASTREA_EXPLORER_REMOTE_PREFIXES");
    }
}

QTEST_MAIN(NavigationControllerTest)

#include "tst_navigation_controller.moc"
