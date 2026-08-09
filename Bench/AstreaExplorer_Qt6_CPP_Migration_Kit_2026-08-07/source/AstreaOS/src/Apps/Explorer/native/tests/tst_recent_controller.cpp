#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtTest>

#include "controllers/recent_controller.h"

using namespace Astrea::Explorer::Native::Backend;

class RecentControllerTest final : public QObject
{
    Q_OBJECT

private slots:
    void mergesNewestRecordAndSortsByAccess();
    void loadsFinderLaunchAndXbelSources();
    void loadsDesktopLaunchRecordsAndPreservesAccessMetadata();
    void ignoresInvalidRecentRecordsAndDoesNotNeedBackend();
    void limitsMergedResults();
};

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
    const QVector<DirectoryEntry> entries = controller.load(paths);

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
    const QVector<DirectoryEntry> entries = controller.load(paths);

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

void RecentControllerTest::ignoresInvalidRecentRecordsAndDoesNotNeedBackend()
{
    RecentController controller;
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString existingPath = QDir(directory.path()).filePath(QStringLiteral("existing.txt"));
    QFile existing(existingPath);
    QVERIFY(existing.open(QIODevice::WriteOnly));
    existing.close();
    const QString historyPath = QDir(directory.path()).filePath(QStringLiteral("history.jsonl"));
    QFile history(historyPath);
    QVERIFY(history.open(QIODevice::WriteOnly));
    QVERIFY(history.write(QByteArrayLiteral("not-json\n")) > 0);
    const QJsonObject missing {
        {QStringLiteral("status"), QStringLiteral("ok")},
        {QStringLiteral("kind"), QStringLiteral("file")},
        {QStringLiteral("target"), QDir(directory.path()).filePath(QStringLiteral("missing.txt"))},
        {QStringLiteral("timestamp_ms"), 900},
    };
    const QJsonObject validOld {
        {QStringLiteral("status"), QStringLiteral("ok")},
        {QStringLiteral("kind"), QStringLiteral("file")},
        {QStringLiteral("target"), existingPath},
        {QStringLiteral("timestamp_ms"), 100},
    };
    const QJsonObject validNew {
        {QStringLiteral("status"), QStringLiteral("ok")},
        {QStringLiteral("kind"), QStringLiteral("file")},
        {QStringLiteral("target"), existingPath},
        {QStringLiteral("timestamp_ms"), 200},
    };
    const QJsonObject malformedTimestamp {
        {QStringLiteral("status"), QStringLiteral("ok")},
        {QStringLiteral("kind"), QStringLiteral("file")},
        {QStringLiteral("target"), existingPath},
        {QStringLiteral("timestamp_ms"), QStringLiteral("not-a-timestamp")},
    };
    const QJsonObject missingTimestamp {
        {QStringLiteral("status"), QStringLiteral("ok")},
        {QStringLiteral("kind"), QStringLiteral("file")},
        {QStringLiteral("target"), existingPath},
    };
    QVERIFY(history.write(QJsonDocument(missing).toJson(QJsonDocument::Compact) + '\n') > 0);
    QVERIFY(history.write(QJsonDocument(validOld).toJson(QJsonDocument::Compact) + '\n') > 0);
    QVERIFY(history.write(QJsonDocument(validNew).toJson(QJsonDocument::Compact) + '\n') > 0);
    QVERIFY(history.write(QJsonDocument(malformedTimestamp).toJson(QJsonDocument::Compact) + '\n') > 0);
    QVERIFY(history.write(QJsonDocument(missingTimestamp).toJson(QJsonDocument::Compact) + '\n') > 0);
    history.close();

    RecentSourcePaths paths;
    paths.launchHistoryPath = historyPath;
    const QVector<DirectoryEntry> entries = controller.load(paths);

    QCOMPARE(entries.size(), 1);
    QCOMPARE(entries.constFirst().filePath, QFileInfo(existingPath).absoluteFilePath());
    QCOMPARE(entries.constFirst().lastAccessed, 200);
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

QTEST_GUILESS_MAIN(RecentControllerTest)

#include "tst_recent_controller.moc"
