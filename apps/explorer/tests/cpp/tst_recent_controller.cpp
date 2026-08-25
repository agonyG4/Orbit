#include <algorithm>
#include <functional>

#include <QDir>
#include <QDateTime>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

#include "controllers/recent_controller.h"
#include "models/directory_model.h"
#include "services/launch_service.h"

using namespace Astrea::Explorer::Native::Backend;

namespace {

class ScopedEnvironment final
{
public:
    ScopedEnvironment(const char *name, const QByteArray &value)
        : m_name(name)
        , m_hadValue(qEnvironmentVariableIsSet(name))
        , m_previous(qgetenv(name))
    {
        qputenv(m_name.constData(), value);
    }

    ~ScopedEnvironment()
    {
        if (m_hadValue) {
            qputenv(m_name.constData(), m_previous);
        } else {
            qunsetenv(m_name.constData());
        }
    }

private:
    QByteArray m_name;
    bool m_hadValue = false;
    QByteArray m_previous;
};

} // namespace

class RecentControllerTest final : public QObject
{
    Q_OBJECT

private slots:
    void mergesNewestRecordAndSortsByAccess();
    void loadsFinderLaunchAndXbelSources();
    void loadsDesktopLaunchRecordsAndPreservesAccessMetadata();
    void loadsDesktopRecordsWithMalformedTimestampUsingMtimeFallback();
    void qualifiesMixedHistoryThroughDirectoryModelAndLauncher();
    void preservesAccessTimestampOverFilesystemModificationTime();
    void preservesRecoverableLaunchRecordsAndRejectsInvalidRecords();
    void preservesFinderRecordsWithMissingTargets();
    void boundsLaunchHistoryCandidatesBeforeFinalMerge();
    void remainsDeterministicUnderRepeatedRecentRefreshes();
    void limitsMergedResults();
    void publishesLatestAsyncLoadAndRecordsImmediately();
};

class ControllerManualDispatch final
{
public:
    void operator()(std::function<void()> job)
    {
        jobs.append(std::move(job));
    }

    QVector<std::function<void()>> jobs;
};

QVector<DirectoryEntry> loadThroughNativeController(const RecentSourcePaths &paths)
{
    RecentStore store(paths, nullptr, [](std::function<void()> job) {
        job();
    });
    RecentController controller(&store);
    controller.loadAsync();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    return controller.currentEntries();
}

RecentRecord recentRecord(const QString &path, qint64 lastAccessed, const QString &source)
{
    RecentRecord record;
    record.entry.fileName = QFileInfo(path).fileName();
    record.entry.filePath = path;
    record.entry.fileUrl = QUrl::fromLocalFile(path);
    record.entry.fileKind = QStringLiteral("TXT");
    record.lastAccessed = lastAccessed;
    record.source = source;
    return record;
}

void RecentControllerTest::mergesNewestRecordAndSortsByAccess()
{
    RecentController controller;
    const QVector<DirectoryEntry> entries = controller.merge({
        recentRecord(QStringLiteral("/fixture/old.txt"), 100, QStringLiteral("finder")),
        recentRecord(QStringLiteral("/fixture/new.txt"), 300, QStringLiteral("launch")),
        recentRecord(QStringLiteral("/fixture/old.txt"), 200, QStringLiteral("xbel")),
    });

    QCOMPARE(entries.size(), 2);
    QCOMPARE(entries.at(0).filePath, QStringLiteral("/fixture/new.txt"));
    QCOMPARE(entries.at(0).lastAccessed, 300);
    QCOMPARE(entries.at(0).recentSource, QStringLiteral("launch"));
    QCOMPARE(entries.at(1).filePath, QStringLiteral("/fixture/old.txt"));
    QCOMPARE(entries.at(1).lastAccessed, 200);
    QCOMPARE(entries.at(1).recentSource, QStringLiteral("xbel"));
}

void RecentControllerTest::loadsFinderLaunchAndXbelSources()
{
    RecentController controller;
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString root = directory.path();
    const QString finderPath = QDir(root).filePath(QStringLiteral("finder.json"));
    const QString launchPath = QDir(root).filePath(QStringLiteral("history.jsonl"));
    const QString xbelPath = QDir(root).filePath(QStringLiteral("recently-used.xbel"));

    const QString finderFile = QDir(root).filePath(QStringLiteral("finder.txt"));
    const QString launchFile = QDir(root).filePath(QStringLiteral("launch.txt"));
    const QString xbelFile = QDir(root).filePath(QStringLiteral("xbel.png"));
    QFile finderFixture(finderFile);
    QFile launchFixture(launchFile);
    QFile xbelFixture(xbelFile);
    QVERIFY(finderFixture.open(QIODevice::WriteOnly));
    QVERIFY(launchFixture.open(QIODevice::WriteOnly));
    QVERIFY(xbelFixture.open(QIODevice::WriteOnly));
    finderFixture.close();
    launchFixture.close();
    xbelFixture.close();
    QFile finder(finderPath);
    QVERIFY(finder.open(QIODevice::WriteOnly));
    const QJsonArray finderItems {
        QJsonObject {
            {QStringLiteral("filePath"), finderFile},
            {QStringLiteral("lastAccessed"), 100},
            {QStringLiteral("recentSource"), QStringLiteral("finder")},
        },
    };
    QVERIFY(finder.write(QJsonDocument(finderItems).toJson(QJsonDocument::Compact)) > 0);
    finder.close();

    QFile launch(launchPath);
    QVERIFY(launch.open(QIODevice::WriteOnly));
    const QJsonObject launchObject {
        {QStringLiteral("status"), QStringLiteral("ok")},
        {QStringLiteral("kind"), QStringLiteral("file")},
        {QStringLiteral("target"), launchFile},
        {QStringLiteral("timestamp_ms"), 300},
    };
    const QByteArray launchPayload = QJsonDocument(launchObject).toJson(QJsonDocument::Compact) + "\n";
    QVERIFY(launch.write(launchPayload) > 0);
    launch.close();

    QFile xbel(xbelPath);
    QVERIFY(xbel.open(QIODevice::WriteOnly));
    const QByteArray xbelPayload = QByteArrayLiteral(
                                       "<?xml version=\"1.0\"?><xbel><bookmark href=\"")
        + QUrl::fromLocalFile(xbelFile).toEncoded()
        + QByteArrayLiteral("\" visited=\"2026-01-01T00:00:04Z\"/></xbel>");
    QVERIFY(xbel.write(xbelPayload) > 0);
    xbel.close();

    RecentSourcePaths paths;
    paths.finderPath = finderPath;
    paths.launchHistoryPath = launchPath;
    paths.xbelPath = xbelPath;
    const QVector<DirectoryEntry> entries = loadThroughNativeController(paths);

    QCOMPARE(entries.size(), 3);
    QCOMPARE(entries.at(0).filePath, xbelFile);
    QCOMPARE(entries.at(0).recentSource, QStringLiteral("xbel"));
    QCOMPARE(entries.at(1).filePath, launchFile);
    QCOMPARE(entries.at(1).recentSource, QStringLiteral("launch"));
    QCOMPARE(entries.at(2).filePath, finderFile);
    QCOMPARE(entries.at(2).recentSource, QStringLiteral("finder"));
    QVERIFY(!entries.at(0).filePreviewUrl.isEmpty());
}

void RecentControllerTest::loadsDesktopLaunchRecordsAndPreservesAccessMetadata()
{
    RecentController controller;
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString desktopFile = QDir(directory.path()).filePath(QStringLiteral("example.desktop"));
    QFile desktop(desktopFile);
    QVERIFY(desktop.open(QIODevice::WriteOnly));
    QVERIFY(desktop.write(QByteArrayLiteral(
        "[Desktop Entry]\n"
        "Type=Application\n"
        "Name=Example Application\n"
        "Icon=example-icon\n"
        "Exec=example\n")) > 0);
    desktop.close();

    const QString historyPath = QDir(directory.path()).filePath(QStringLiteral("history.jsonl"));
    QFile history(historyPath);
    QVERIFY(history.open(QIODevice::WriteOnly));
    const QJsonObject record {
        {QStringLiteral("status"), QStringLiteral("ok")},
        {QStringLiteral("kind"), QStringLiteral("desktop")},
        {QStringLiteral("target"), QStringLiteral("example")},
        {QStringLiteral("argv"), QJsonArray {QStringLiteral("gio"), QStringLiteral("launch"), desktopFile}},
        {QStringLiteral("timestamp_ms"), 456},
    };
    QVERIFY(history.write(QJsonDocument(record).toJson(QJsonDocument::Compact) + '\n') > 0);
    history.close();

    RecentSourcePaths paths;
    paths.launchHistoryPath = historyPath;
    const QVector<DirectoryEntry> entries = loadThroughNativeController(paths);

    QCOMPARE(entries.size(), 1);
    const DirectoryEntry &entry = entries.constFirst();
    QCOMPARE(entry.filePath, QFileInfo(desktopFile).absoluteFilePath());
    QCOMPARE(entry.fileName, QStringLiteral("Example Application"));
    QCOMPARE(entry.fileKind, QStringLiteral("Aplicativo"));
    QCOMPARE(entry.fileIconName, QStringLiteral("example-icon"));
    QCOMPARE(entry.lastAccessed, 456);
    QCOMPARE(entry.fileModified.toMSecsSinceEpoch(), 456);
    QCOMPARE(entry.fileUrl, QUrl::fromLocalFile(QFileInfo(desktopFile).absoluteFilePath()));
    QVERIFY(entry.fileExecutable);
}

void RecentControllerTest::loadsDesktopRecordsWithMalformedTimestampUsingMtimeFallback()
{
    RecentController controller;
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString desktopFile = QDir(directory.path()).filePath(QStringLiteral("fallback.desktop"));
    QFile desktop(desktopFile);
    QVERIFY(desktop.open(QIODevice::WriteOnly));
    QVERIFY(desktop.write(QByteArrayLiteral(
        "[Desktop Entry]\n"
        "Type=Application\n"
        "Name=Fallback Application\n"
        "Icon=fallback-icon\n"
        "Exec=fallback\n")) > 0);
    desktop.close();
    const qint64 mtime = QFileInfo(desktopFile).lastModified().toMSecsSinceEpoch();
    QVERIFY(mtime > 0);

    const QString historyPath = QDir(directory.path()).filePath(QStringLiteral("history.jsonl"));
    QFile history(historyPath);
    QVERIFY(history.open(QIODevice::WriteOnly));
    const QJsonObject record {
        {QStringLiteral("status"), QStringLiteral("ok")},
        {QStringLiteral("kind"), QStringLiteral("desktop")},
        {QStringLiteral("target"), QStringLiteral("fallback")},
        {QStringLiteral("argv"), QJsonArray {QStringLiteral("gio"), desktopFile}},
        {QStringLiteral("timestamp_ms"), QStringLiteral("not-a-timestamp")},
    };
    QVERIFY(history.write(QJsonDocument(record).toJson(QJsonDocument::Compact) + '\n') > 0);
    history.close();

    RecentSourcePaths paths;
    paths.launchHistoryPath = historyPath;
    const QVector<DirectoryEntry> entries = loadThroughNativeController(paths);

    QCOMPARE(entries.size(), 1);
    QCOMPARE(entries.constFirst().filePath, QFileInfo(desktopFile).absoluteFilePath());
    QCOMPARE(entries.constFirst().lastAccessed, mtime);
    QCOMPARE(entries.constFirst().fileModified.toMSecsSinceEpoch(), mtime);
    QCOMPARE(entries.constFirst().fileName, QStringLiteral("Fallback Application"));
}

void RecentControllerTest::qualifiesMixedHistoryThroughDirectoryModelAndLauncher()
{
    RecentController controller;
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString applicationsRoot = QDir(directory.path()).filePath(
        QStringLiteral("applications"));
    QVERIFY(QDir().mkpath(applicationsRoot));
    const QString desktopFile = QDir(applicationsRoot).filePath(
        QStringLiteral("org.example.TestApp.desktop"));
    QFile desktop(desktopFile);
    QVERIFY(desktop.open(QIODevice::WriteOnly));
    QVERIFY(desktop.write(QByteArrayLiteral(
        "[Desktop Entry]\n"
        "Type=Application\n"
        "Name=Test Application\n"
        "Icon=test-application\n"
        "Exec=test-application\n")) > 0);
    desktop.close();

    const QString existingFile = QDir(directory.path()).filePath(
        QStringLiteral("document.txt"));
    QFile file(existingFile);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QVERIFY(file.write(QByteArrayLiteral("recent document")) > 0);
    file.close();

    const QString historyPath = QDir(directory.path()).filePath(
        QStringLiteral("launch-history.jsonl"));
    QFile history(historyPath);
    QVERIFY(history.open(QIODevice::WriteOnly));
    QVERIFY(history.write(QByteArrayLiteral("malformed\n")) > 0);
    const QJsonObject missingFile {
        {QStringLiteral("status"), QStringLiteral("ok")},
        {QStringLiteral("kind"), QStringLiteral("file")},
        {QStringLiteral("target"), QDir(directory.path()).filePath(QStringLiteral("missing.txt"))},
        {QStringLiteral("timestamp_ms"), 1400},
    };
    const QJsonObject missingDesktop {
        {QStringLiteral("status"), QStringLiteral("ok")},
        {QStringLiteral("kind"), QStringLiteral("desktop")},
        {QStringLiteral("target"), QStringLiteral("org.example.Missing.desktop")},
        {QStringLiteral("timestamp_ms"), 1300},
    };
    const QJsonObject oldFile {
        {QStringLiteral("status"), QStringLiteral("ok")},
        {QStringLiteral("kind"), QStringLiteral("file")},
        {QStringLiteral("target"), existingFile},
        {QStringLiteral("timestamp_ms"), 500},
    };
    const QJsonObject newFile {
        {QStringLiteral("status"), QStringLiteral("ok")},
        {QStringLiteral("kind"), QStringLiteral("file")},
        {QStringLiteral("target"), existingFile},
        {QStringLiteral("timestamp_ms"), 700},
    };
    const QJsonObject desktopRecord {
        {QStringLiteral("status"), QStringLiteral("ok")},
        {QStringLiteral("kind"), QStringLiteral("desktop")},
        {QStringLiteral("target"), QStringLiteral("org.example.TestApp.desktop")},
        {QStringLiteral("argv"), QJsonArray {QStringLiteral("astrea-launch")}},
        {QStringLiteral("timestamp_ms"), 900},
    };
    for (const QJsonObject &object : {missingFile, missingDesktop, oldFile, newFile, desktopRecord}) {
        QVERIFY(history.write(QJsonDocument(object).toJson(QJsonDocument::Compact) + '\n') > 0);
    }
    history.close();

    ScopedEnvironment dataHome("XDG_DATA_HOME", directory.path().toUtf8());
    RecentSourcePaths paths;
    paths.launchHistoryPath = historyPath;
    const QVector<DirectoryEntry> entries = loadThroughNativeController(paths);

    QCOMPARE(entries.size(), 2);
    QCOMPARE(entries.at(0).fileName, QStringLiteral("Test Application"));
    QCOMPARE(entries.at(0).fileKind, QStringLiteral("Aplicativo"));
    QCOMPARE(entries.at(0).fileIconName, QStringLiteral("test-application"));
    QCOMPARE(entries.at(0).lastAccessed, 900);
    QCOMPARE(entries.at(0).fileModified.toMSecsSinceEpoch(), 900);
    QCOMPARE(entries.at(1).filePath, QFileInfo(existingFile).absoluteFilePath());
    QCOMPARE(entries.at(1).lastAccessed, 700);
    QCOMPARE(entries.at(1).fileModified.toMSecsSinceEpoch(), 700);

    DirectoryModel model;
    QVERIFY(model.applyEntries(entries, 1));
    QCOMPARE(model.count(), 2);
    QCOMPARE(
        model.roleNames().value(DirectoryModel::FileIconNameRole),
        QByteArrayLiteral("fileIconName"));
    const QVariantMap desktopItem = model.get(0);
    QCOMPARE(
        desktopItem.value(QStringLiteral("fileName")).toString(),
        QStringLiteral("Test Application"));
    QCOMPARE(
        desktopItem.value(QStringLiteral("fileIconName")).toString(),
        QStringLiteral("test-application"));
    QCOMPARE(desktopItem.value(QStringLiteral("lastAccessed")).toLongLong(), 900);

    Astrea::Explorer::Native::Services::LaunchService launcher(
        QStringLiteral("/opt/Astrea/bin/astrea-launch"),
        QStringLiteral("/opt/Astrea/bin/astrea-windows-run"));
    const Astrea::Explorer::Native::Services::LaunchSpec launch =
        launcher.desktopLaunch(entries.at(0).filePath);
    QCOMPARE(launch.program, QStringLiteral("/opt/Astrea/bin/astrea-launch"));
    QCOMPARE(
        launch.arguments,
        QStringList({QStringLiteral("--desktop"), entries.at(0).filePath}));
}

void RecentControllerTest::preservesAccessTimestampOverFilesystemModificationTime()
{
    RecentController controller;
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString filePath = QDir(directory.path()).filePath(QStringLiteral("mtime.txt"));
    QFile file(filePath);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QVERIFY(file.write(QByteArrayLiteral("timestamp contract")) > 0);
    file.close();
    const qint64 filesystemModificationTime =
        QFileInfo(filePath).lastModified().toMSecsSinceEpoch();
    QVERIFY(filesystemModificationTime > 1000);

    const QString historyPath = QDir(directory.path()).filePath(QStringLiteral("history.jsonl"));
    QFile history(historyPath);
    QVERIFY(history.open(QIODevice::WriteOnly));
    const QJsonObject record {
        {QStringLiteral("status"), QStringLiteral("ok")},
        {QStringLiteral("kind"), QStringLiteral("file")},
        {QStringLiteral("target"), filePath},
        {QStringLiteral("timestamp_ms"), 1000},
    };
    QVERIFY(history.write(QJsonDocument(record).toJson(QJsonDocument::Compact) + '\n') > 0);
    history.close();

    RecentSourcePaths paths;
    paths.launchHistoryPath = historyPath;
    const QVector<DirectoryEntry> entries = loadThroughNativeController(paths);
    QCOMPARE(entries.size(), 1);
    QCOMPARE(entries.constFirst().lastAccessed, 1000);
    QCOMPARE(entries.constFirst().fileModified.toMSecsSinceEpoch(), 1000);
    QVERIFY(entries.constFirst().fileModified.toMSecsSinceEpoch() != filesystemModificationTime);
}

void RecentControllerTest::preservesRecoverableLaunchRecordsAndRejectsInvalidRecords()
{
    RecentController controller;
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString validPath = QDir(directory.path()).filePath(QStringLiteral("valid.txt"));
    const QString malformedPath = QDir(directory.path()).filePath(QStringLiteral("malformed.txt"));
    const QString missingTimestampPath = QDir(directory.path()).filePath(QStringLiteral("missing-timestamp.txt"));
    for (const QString &path : {validPath, malformedPath, missingTimestampPath}) {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QVERIFY(file.write(QByteArrayLiteral("recent")) > 0);
        file.close();
    }
    const qint64 malformedMtime = QFileInfo(malformedPath).lastModified().toMSecsSinceEpoch();
    const qint64 missingTimestampMtime = QFileInfo(missingTimestampPath).lastModified().toMSecsSinceEpoch();
    QVERIFY(malformedMtime > 0);
    QVERIFY(missingTimestampMtime > 0);
    const qint64 validTimestamp = qMax<qint64>(2000, QFileInfo(validPath).lastModified().toMSecsSinceEpoch() + 1000);
    const QString historyPath = QDir(directory.path()).filePath(QStringLiteral("history.jsonl"));
    QFile history(historyPath);
    QVERIFY(history.open(QIODevice::WriteOnly));
    QVERIFY(history.write(QByteArrayLiteral("not-json\n")) > 0);
    const QJsonObject missingTarget {
        {QStringLiteral("status"), QStringLiteral("ok")},
        {QStringLiteral("kind"), QStringLiteral("file")},
        {QStringLiteral("target"), QDir(directory.path()).filePath(QStringLiteral("missing.txt"))},
        {QStringLiteral("timestamp_ms"), QStringLiteral("not-a-timestamp")},
    };
    const QJsonObject valid {
        {QStringLiteral("status"), QStringLiteral("ok")},
        {QStringLiteral("kind"), QStringLiteral("file")},
        {QStringLiteral("target"), validPath},
        {QStringLiteral("timestamp_ms"), validTimestamp},
    };
    const QJsonObject malformedTimestamp {
        {QStringLiteral("status"), QStringLiteral("ok")},
        {QStringLiteral("kind"), QStringLiteral("file")},
        {QStringLiteral("target"), malformedPath},
        {QStringLiteral("timestamp_ms"), QStringLiteral("not-a-timestamp")},
    };
    const QJsonObject missingTimestamp {
        {QStringLiteral("status"), QStringLiteral("ok")},
        {QStringLiteral("kind"), QStringLiteral("file")},
        {QStringLiteral("target"), missingTimestampPath},
    };
    QVERIFY(history.write(QJsonDocument(missingTarget).toJson(QJsonDocument::Compact) + '\n') > 0);
    QVERIFY(history.write(QJsonDocument(valid).toJson(QJsonDocument::Compact) + '\n') > 0);
    QVERIFY(history.write(QJsonDocument(malformedTimestamp).toJson(QJsonDocument::Compact) + '\n') > 0);
    QVERIFY(history.write(QJsonDocument(missingTimestamp).toJson(QJsonDocument::Compact) + '\n') > 0);
    history.close();

    RecentSourcePaths paths;
    paths.launchHistoryPath = historyPath;
    const QVector<DirectoryEntry> entries = loadThroughNativeController(paths);

    QCOMPARE(entries.size(), 3);
    auto findEntry = [&entries](const QString &path) -> DirectoryEntry {
        const QString absolute = QFileInfo(path).absoluteFilePath();
        for (const DirectoryEntry &entry : entries) {
            if (entry.filePath == absolute) {
                return entry;
            }
        }
        return {};
    };
    const DirectoryEntry validEntry = findEntry(validPath);
    const DirectoryEntry malformedEntry = findEntry(malformedPath);
    const DirectoryEntry missingTimestampEntry = findEntry(missingTimestampPath);
    QVERIFY(!validEntry.filePath.isEmpty());
    QVERIFY(!malformedEntry.filePath.isEmpty());
    QVERIFY(!missingTimestampEntry.filePath.isEmpty());
    QCOMPARE(validEntry.lastAccessed, validTimestamp);
    QCOMPARE(malformedEntry.lastAccessed, malformedMtime);
    QCOMPARE(missingTimestampEntry.lastAccessed, missingTimestampMtime);
    QVERIFY(findEntry(QDir(directory.path()).filePath(QStringLiteral("missing.txt"))).filePath.isEmpty());
}

void RecentControllerTest::preservesFinderRecordsWithMissingTargets()
{
    RecentController controller;
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString missingPath = QDir(directory.path()).filePath(QStringLiteral("stale.txt"));
    const QString finderPath = QDir(directory.path()).filePath(QStringLiteral("finder.json"));
    QFile finder(finderPath);
    QVERIFY(finder.open(QIODevice::WriteOnly));
    const QJsonArray items {
        QJsonObject {
            {QStringLiteral("filePath"), missingPath},
            {QStringLiteral("fileName"), QStringLiteral("Stale document")},
            {QStringLiteral("fileUrl"), QUrl::fromLocalFile(missingPath).toString()},
            {QStringLiteral("fileKind"), QStringLiteral("TXT")},
            {QStringLiteral("fileSize"), 42},
            {QStringLiteral("lastAccessed"), 1234},
            {QStringLiteral("recentSource"), QStringLiteral("finder")},
        },
    };
    QVERIFY(finder.write(QJsonDocument(items).toJson(QJsonDocument::Compact)) > 0);
    finder.close();

    RecentSourcePaths paths;
    paths.finderPath = finderPath;
    const QVector<DirectoryEntry> entries = loadThroughNativeController(paths);

    QCOMPARE(entries.size(), 1);
    QCOMPARE(entries.constFirst().filePath, QFileInfo(missingPath).absoluteFilePath());
    QCOMPARE(entries.constFirst().fileName, QStringLiteral("Stale document"));
    QCOMPARE(entries.constFirst().fileSize, 42);
    QCOMPARE(entries.constFirst().lastAccessed, 1234);
    QCOMPARE(entries.constFirst().recentSource, QStringLiteral("finder"));
}

void RecentControllerTest::boundsLaunchHistoryCandidatesBeforeFinalMerge()
{
    RecentController controller;
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString historyPath = QDir(directory.path()).filePath(QStringLiteral("history.jsonl"));
    QFile history(historyPath);
    QVERIFY(history.open(QIODevice::WriteOnly));

    const QString highTimestampOldRecord = QDir(directory.path()).filePath(QStringLiteral("old-high.txt"));
    QFile oldFile(highTimestampOldRecord);
    QVERIFY(oldFile.open(QIODevice::WriteOnly));
    oldFile.close();
    QVERIFY(history.write(QJsonDocument(QJsonObject {
        {QStringLiteral("status"), QStringLiteral("ok")},
        {QStringLiteral("kind"), QStringLiteral("file")},
        {QStringLiteral("target"), highTimestampOldRecord},
        {QStringLiteral("timestamp_ms"), 1000000},
    }).toJson(QJsonDocument::Compact) + '\n') > 0);

    QVector<QString> desktopFiles;
    for (int i = 0; i < 12; ++i) {
        const QString desktopPath = QDir(directory.path()).filePath(
            QStringLiteral("stress-%1.desktop").arg(i));
        QFile desktop(desktopPath);
        QVERIFY(desktop.open(QIODevice::WriteOnly));
        QVERIFY(desktop.write(QStringLiteral(
            "[Desktop Entry]\n"
            "Type=Application\n"
            "Name=Stress Application %1\n"
            "Icon=stress-%1\n"
            "Exec=stress-%1\n").arg(i).toUtf8()) > 0);
        desktop.close();
        desktopFiles.append(desktopPath);
    }

    for (int i = 0; i < 600; ++i) {
        if (i % 17 == 0) {
            QVERIFY(history.write(QByteArrayLiteral("malformed\n")) > 0);
            continue;
        }
        if (i % 19 == 0) {
            QVERIFY(history.write(QJsonDocument(QJsonObject {
                {QStringLiteral("status"), QStringLiteral("ok")},
                {QStringLiteral("kind"), QStringLiteral("file")},
                {QStringLiteral("target"), QDir(directory.path()).filePath(
                    QStringLiteral("missing-%1.txt").arg(i))},
                {QStringLiteral("timestamp_ms"), 200 + i},
            }).toJson(QJsonDocument::Compact) + '\n') > 0);
            continue;
        }

        if (i % 4 == 0) {
            const QString desktopPath = desktopFiles.at(i % desktopFiles.size());
            QVERIFY(history.write(QJsonDocument(QJsonObject {
                {QStringLiteral("status"), QStringLiteral("ok")},
                {QStringLiteral("kind"), QStringLiteral("desktop")},
                {QStringLiteral("target"), QStringLiteral("stress-%1.desktop").arg(i % desktopFiles.size())},
                {QStringLiteral("argv"), QJsonArray {QStringLiteral("gio"), desktopPath}},
                {QStringLiteral("timestamp_ms"), 200 + i},
            }).toJson(QJsonDocument::Compact) + '\n') > 0);
            continue;
        }

        const QString path = QDir(directory.path()).filePath(QStringLiteral("item-%1.txt").arg(i % 80));
        QFile file(path);
        if (!file.exists()) {
            QVERIFY(file.open(QIODevice::WriteOnly));
            file.close();
        }
        QVERIFY(history.write(QJsonDocument(QJsonObject {
            {QStringLiteral("status"), QStringLiteral("ok")},
            {QStringLiteral("kind"), QStringLiteral("file")},
            {QStringLiteral("target"), path},
            {QStringLiteral("timestamp_ms"), 200 + i},
        }).toJson(QJsonDocument::Compact) + '\n') > 0);
    }
    history.close();

    RecentSourcePaths paths;
    paths.launchHistoryPath = historyPath;
    paths.limit = 8;
    const QVector<DirectoryEntry> entries = loadThroughNativeController(paths);

    QCOMPARE(entries.size(), paths.limit);
    QVERIFY(std::none_of(entries.cbegin(), entries.cend(), [&](const DirectoryEntry &entry) {
        return entry.filePath == QFileInfo(highTimestampOldRecord).absoluteFilePath();
    }));
    QVERIFY(std::any_of(entries.cbegin(), entries.cend(), [](const DirectoryEntry &entry) {
        return entry.fileKind == QStringLiteral("Aplicativo")
            && entry.fileName.startsWith(QStringLiteral("Stress Application"));
    }));

    QVector<QString> firstPaths;
    for (const DirectoryEntry &entry : entries) {
        firstPaths.append(entry.filePath);
    }
    const QVector<DirectoryEntry> repeated = loadThroughNativeController(paths);
    QVector<QString> repeatedPaths;
    for (const DirectoryEntry &entry : repeated) {
        repeatedPaths.append(entry.filePath);
    }
    QCOMPARE(repeatedPaths, firstPaths);
}

void RecentControllerTest::remainsDeterministicUnderRepeatedRecentRefreshes()
{
    RecentController controller;
    QVector<RecentRecord> records;
    for (int round = 0; round < 80; ++round) {
        for (int item = 0; item < 12; ++item) {
            records.append(recentRecord(
                QStringLiteral("/fixture/item-%1.txt").arg(item),
                1000 + round * 12 + item,
                QStringLiteral("launch")));
        }
    }

    DirectoryModel model;
    QVector<QString> expectedPaths;
    for (int refresh = 1; refresh <= 100; ++refresh) {
        const QVector<DirectoryEntry> entries = controller.merge(records, 12);
        if (refresh == 1) {
            for (const DirectoryEntry &entry : entries) {
                expectedPaths.append(entry.filePath);
            }
        }
        QCOMPARE(entries.size(), expectedPaths.size());
        QCOMPARE(model.applyEntries(entries, static_cast<quint64>(refresh)), true);
        QCOMPARE(model.paths(), expectedPaths);
    }

    const QVector<DirectoryEntry> stale = controller.merge(
        {recentRecord(QStringLiteral("/fixture/rollback.txt"), 1, QStringLiteral("late"))},
        12);
    QVERIFY(!model.applyEntries(stale, 1));
    QCOMPARE(model.paths(), expectedPaths);
}

void RecentControllerTest::limitsMergedResults()
{
    RecentController controller;
    RecentSourcePaths paths;
    paths.limit = 2;
    const QVector<DirectoryEntry> entries = controller.merge({
        recentRecord(QStringLiteral("/fixture/one"), 100, QStringLiteral("finder")),
        recentRecord(QStringLiteral("/fixture/two"), 300, QStringLiteral("finder")),
        recentRecord(QStringLiteral("/fixture/three"), 200, QStringLiteral("finder")),
    }, paths.limit);

    QCOMPARE(entries.size(), 2);
    QCOMPARE(entries.at(0).filePath, QStringLiteral("/fixture/two"));
    QCOMPARE(entries.at(1).filePath, QStringLiteral("/fixture/three"));
}

void RecentControllerTest::publishesLatestAsyncLoadAndRecordsImmediately()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString itemPath = QDir(directory.path()).filePath(QStringLiteral("item.txt"));
    QFile item(itemPath);
    QVERIFY(item.open(QIODevice::WriteOnly));
    item.write("item");
    item.close();
    const QString finderPath = QDir(directory.path()).filePath(QStringLiteral("finder.json"));
    QFile finder(finderPath);
    QVERIFY(finder.open(QIODevice::WriteOnly));
    finder.write(QJsonDocument(QJsonArray {
        QJsonObject {{QStringLiteral("filePath"), itemPath}, {QStringLiteral("lastAccessed"), 400}},
    }).toJson(QJsonDocument::Compact));
    finder.close();

    ControllerManualDispatch dispatch;
    RecentStore store(
        RecentSourcePaths {finderPath, QString(), QString(), 60},
        nullptr,
        std::ref(dispatch));
    RecentController controller(&store);
    QSignalSpy ready(&controller, &RecentController::recentReady);
    const BackendRequestId requestA = controller.loadAsync();
    const BackendRequestId requestB = controller.loadAsync();
    QVERIFY(requestB > requestA);
    QCOMPARE(dispatch.jobs.size(), 2);

    dispatch.jobs[1]();
    QCoreApplication::processEvents();
    dispatch.jobs[0]();
    QCoreApplication::processEvents();
    QCOMPARE(ready.count(), 1);
    QCOMPARE(ready.at(0).at(0).toULongLong(), requestB);

    DirectoryEntry recordEntry;
    recordEntry.filePath = QDir(directory.path()).filePath(QStringLiteral("new.txt"));
    recordEntry.fileName = QStringLiteral("new.txt");
    recordEntry.fileUrl = QUrl::fromLocalFile(recordEntry.filePath);
    controller.recordAccess(recordEntry);
    const QVector<DirectoryEntry> current = controller.currentEntries();
    QVERIFY(!current.isEmpty());
    QCOMPARE(current.constFirst().filePath, recordEntry.filePath);
}

QTEST_GUILESS_MAIN(RecentControllerTest)

#include "tst_recent_controller.moc"
