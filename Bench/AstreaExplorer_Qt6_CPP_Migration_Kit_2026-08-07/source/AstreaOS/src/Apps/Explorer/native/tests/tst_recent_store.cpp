#include <functional>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QThread>
#include <QtTest>

#include "services/recent_store.h"

using namespace Astrea::Explorer::Native::Backend;

namespace {

class ManualDispatch final
{
public:
    void operator()(std::function<void()> job)
    {
        m_jobs.append(std::move(job));
    }

    int size() const
    {
        return m_jobs.size();
    }

    void run(int index)
    {
        QVERIFY(index >= 0 && index < m_jobs.size());
        std::function<void()> job = std::move(m_jobs[index]);
        m_jobs[index] = {};
        job();
        QCoreApplication::processEvents();
    }

private:
    QVector<std::function<void()>> m_jobs;
};

QString writeFinderSource(const QString &directory, const QJsonArray &items)
{
    const QString path = QDir(directory).filePath(QStringLiteral("finder-recents.json"));
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return {};
    }
    file.write(QJsonDocument(items).toJson(QJsonDocument::Compact));
    file.close();
    return path;
}

QString writeLaunchSource(const QString &directory, const QList<QJsonObject> &records)
{
    const QString path = QDir(directory).filePath(QStringLiteral("history.jsonl"));
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return {};
    }
    for (const QJsonObject &record : records) {
        file.write(QJsonDocument(record).toJson(QJsonDocument::Compact));
        file.write("\n");
    }
    file.close();
    return path;
}

RecentSourcePaths sourcePaths(const QString &finderPath, const QString &launchPath, int limit = 60)
{
    RecentSourcePaths paths;
    paths.finderPath = finderPath;
    paths.launchHistoryPath = launchPath;
    paths.limit = limit;
    return paths;
}

RecentRecord finderRecord(const QString &path, qint64 timestamp)
{
    RecentRecord record;
    record.entry.fileName = QFileInfo(path).fileName();
    record.entry.filePath = QFileInfo(path).absoluteFilePath();
    record.entry.fileUrl = QUrl::fromLocalFile(record.entry.filePath);
    record.entry.fileKind = QStringLiteral("TXT");
    record.entry.fileModified = QDateTime::fromMSecsSinceEpoch(timestamp);
    record.lastAccessed = timestamp;
    record.source = QStringLiteral("finder");
    return record;
}

} // namespace

class RecentStoreTest final : public QObject
{
    Q_OBJECT

private slots:
    void startsEmpty();
    void loadsExistingRecordsAndRejectsMalformedInput();
    void preservesTimestampFallbackAndFinderStaleTargets();
    void recordsImmediatelyAndDeduplicatesWithRetention();
    void publishesOnlyNewestLoadGeneration();
    void writesAtomicallyAndReloadsAfterRestart();
    void coalescesPendingSavesToNewestSnapshot();
    void preservesMemoryWhenPersistenceFails();
    void productionLoadRunsOffOwnerThread();
};

void RecentStoreTest::startsEmpty()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ManualDispatch dispatch;
    RecentStore store(sourcePaths(
        QDir(directory.path()).filePath(QStringLiteral("missing-finder.json")),
        QDir(directory.path()).filePath(QStringLiteral("missing-history.jsonl"))),
        nullptr,
        std::ref(dispatch));

    QVERIFY(store.records().isEmpty());
}

void RecentStoreTest::loadsExistingRecordsAndRejectsMalformedInput()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString validPath = QDir(directory.path()).filePath(QStringLiteral("valid.txt"));
    QFile valid(validPath);
    QVERIFY(valid.open(QIODevice::WriteOnly));
    valid.write("valid");
    valid.close();

    const QString finderPath = writeFinderSource(directory.path(), {
        QJsonObject {
            {QStringLiteral("filePath"), validPath},
            {QStringLiteral("fileName"), QStringLiteral("Valid")},
            {QStringLiteral("lastAccessed"), 400},
        },
        QJsonValue(QStringLiteral("malformed")),
    });
    QVERIFY(!finderPath.isEmpty());
    const QString launchPath = writeLaunchSource(directory.path(), {
        QJsonObject {
            {QStringLiteral("status"), QStringLiteral("ok")},
            {QStringLiteral("kind"), QStringLiteral("file")},
            {QStringLiteral("target"), validPath},
            {QStringLiteral("timestamp_ms"), 600},
        },
        QJsonObject {
            {QStringLiteral("status"), QStringLiteral("failed")},
            {QStringLiteral("kind"), QStringLiteral("file")},
            {QStringLiteral("target"), validPath},
            {QStringLiteral("timestamp_ms"), 900},
        },
    });
    QVERIFY(!launchPath.isEmpty());

    ManualDispatch dispatch;
    RecentStore store(sourcePaths(finderPath, launchPath), nullptr, std::ref(dispatch));
    QSignalSpy ready(&store, &RecentStore::loadReady);
    const quint64 request = store.load();
    QCOMPARE(dispatch.size(), 1);
    dispatch.run(0);

    QCOMPARE(ready.count(), 1);
    QCOMPARE(ready.at(0).at(0).toULongLong(), request);
    const QVector<RecentRecord> records = store.records();
    QCOMPARE(records.size(), 1);
    QCOMPARE(records.constFirst().entry.filePath, QFileInfo(validPath).absoluteFilePath());
    QCOMPARE(records.constFirst().lastAccessed, 600);
}

void RecentStoreTest::preservesTimestampFallbackAndFinderStaleTargets()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString existingPath = QDir(directory.path()).filePath(QStringLiteral("existing.txt"));
    QFile existing(existingPath);
    QVERIFY(existing.open(QIODevice::WriteOnly));
    existing.write("existing");
    existing.close();
    const qint64 mtime = QFileInfo(existingPath).lastModified().toMSecsSinceEpoch();
    QVERIFY(mtime > 0);

    const QString stalePath = QDir(directory.path()).filePath(QStringLiteral("stale.txt"));
    const QString finderPath = writeFinderSource(directory.path(), {
        QJsonObject {
            {QStringLiteral("filePath"), stalePath},
            {QStringLiteral("fileName"), QStringLiteral("Stale")},
            {QStringLiteral("lastAccessed"), 100},
        },
    });
    const QString launchPath = writeLaunchSource(directory.path(), {
        QJsonObject {
            {QStringLiteral("status"), QStringLiteral("ok")},
            {QStringLiteral("kind"), QStringLiteral("file")},
            {QStringLiteral("target"), existingPath},
            {QStringLiteral("timestamp_ms"), QStringLiteral("not-a-number")},
        },
    });
    QVERIFY(!finderPath.isEmpty());
    QVERIFY(!launchPath.isEmpty());

    ManualDispatch dispatch;
    RecentStore store(sourcePaths(finderPath, launchPath), nullptr, std::ref(dispatch));
    QSignalSpy ready(&store, &RecentStore::loadReady);
    store.load();
    dispatch.run(0);

    QCOMPARE(ready.count(), 1);
    const QVector<RecentRecord> records = store.records();
    QCOMPARE(records.size(), 2);
    auto findByPath = [&records](const QString &path) -> RecentRecord {
        const QString absolute = QFileInfo(path).absoluteFilePath();
        for (const RecentRecord &record : records) {
            if (record.entry.filePath == absolute) {
                return record;
            }
        }
        return {};
    };
    QCOMPARE(findByPath(existingPath).lastAccessed, mtime);
    QCOMPARE(findByPath(stalePath).entry.fileName, QStringLiteral("Stale"));
}

void RecentStoreTest::recordsImmediatelyAndDeduplicatesWithRetention()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString firstPath = QDir(directory.path()).filePath(QStringLiteral("first.txt"));
    const QString secondPath = QDir(directory.path()).filePath(QStringLiteral("second.txt"));
    for (const QString &path : {firstPath, secondPath}) {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(path.toUtf8());
        file.close();
    }

    ManualDispatch dispatch;
    RecentStore store(sourcePaths(
        QDir(directory.path()).filePath(QStringLiteral("finder.json")),
        QDir(directory.path()).filePath(QStringLiteral("history.jsonl")),
        2),
        nullptr,
        std::ref(dispatch));
    store.recordAccess(finderRecord(firstPath, 100));
    store.recordAccess(finderRecord(secondPath, 200));
    store.recordAccess(finderRecord(firstPath, 300));

    const QVector<RecentRecord> records = store.records();
    QCOMPARE(records.size(), 2);
    QCOMPARE(records.at(0).entry.filePath, QFileInfo(firstPath).absoluteFilePath());
    QCOMPARE(records.at(0).lastAccessed, 300);
    QCOMPARE(records.at(1).entry.filePath, QFileInfo(secondPath).absoluteFilePath());
    QVERIFY(dispatch.size() >= 1);
}

void RecentStoreTest::publishesOnlyNewestLoadGeneration()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = QDir(directory.path()).filePath(QStringLiteral("item.txt"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("item");
    file.close();
    const QString finderPath = writeFinderSource(directory.path(), {
        QJsonObject {
            {QStringLiteral("filePath"), path},
            {QStringLiteral("lastAccessed"), 500},
        },
    });

    ManualDispatch dispatch;
    RecentStore store(sourcePaths(finderPath, QString()), nullptr, std::ref(dispatch));
    QSignalSpy ready(&store, &RecentStore::loadReady);
    const quint64 requestA = store.load();
    const quint64 requestB = store.load();
    QVERIFY(requestB > requestA);
    QCOMPARE(dispatch.size(), 2);

    dispatch.run(1);
    dispatch.run(0);
    QCOMPARE(ready.count(), 1);
    QCOMPARE(ready.at(0).at(0).toULongLong(), requestB);
}

void RecentStoreTest::writesAtomicallyAndReloadsAfterRestart()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString finderPath = QDir(directory.path()).filePath(QStringLiteral("finder.json"));
    const QString itemPath = QDir(directory.path()).filePath(QStringLiteral("item.txt"));
    QFile item(itemPath);
    QVERIFY(item.open(QIODevice::WriteOnly));
    item.write("item");
    item.close();

    ManualDispatch dispatch;
    RecentStore store(sourcePaths(finderPath, QString()), nullptr, std::ref(dispatch));
    store.recordAccess(finderRecord(itemPath, 900));
    QVERIFY(dispatch.size() >= 1);
    dispatch.run(0);

    QFile persisted(finderPath);
    QVERIFY(persisted.open(QIODevice::ReadOnly));
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(persisted.readAll(), &error);
    QCOMPARE(error.error, QJsonParseError::NoError);
    QVERIFY(document.isArray());
    QCOMPARE(document.array().size(), 1);
    persisted.close();

    ManualDispatch reloadDispatch;
    RecentStore restarted(sourcePaths(finderPath, QString()), nullptr, std::ref(reloadDispatch));
    QSignalSpy ready(&restarted, &RecentStore::loadReady);
    restarted.load();
    reloadDispatch.run(0);
    QCOMPARE(ready.count(), 1);
    QCOMPARE(restarted.records().size(), 1);
    QCOMPARE(restarted.records().constFirst().lastAccessed, 900);
}

void RecentStoreTest::coalescesPendingSavesToNewestSnapshot()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString firstPath = QDir(directory.path()).filePath(QStringLiteral("first.txt"));
    const QString secondPath = QDir(directory.path()).filePath(QStringLiteral("second.txt"));
    for (const QString &path : {firstPath, secondPath}) {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("item");
        file.close();
    }

    const QString finderPath = QDir(directory.path()).filePath(QStringLiteral("finder.json"));
    ManualDispatch dispatch;
    RecentStore store(sourcePaths(finderPath, QString()), nullptr, std::ref(dispatch));
    store.recordAccess(finderRecord(firstPath, 100));
    store.recordAccess(finderRecord(secondPath, 200));
    store.recordAccess(finderRecord(firstPath, 300));
    QCOMPARE(dispatch.size(), 1);

    dispatch.run(0);
    QCOMPARE(dispatch.size(), 2);
    dispatch.run(1);

    QFile persisted(finderPath);
    QVERIFY(persisted.open(QIODevice::ReadOnly));
    const QJsonDocument document = QJsonDocument::fromJson(persisted.readAll());
    QVERIFY(document.isArray());
    QCOMPARE(document.array().size(), 2);
    QCOMPARE(document.array().at(0).toObject().value(QStringLiteral("lastAccessed")).toInteger(), 300);
}

void RecentStoreTest::preservesMemoryWhenPersistenceFails()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString invalidParent = QDir(directory.path()).filePath(QStringLiteral("file-parent"));
    QFile parent(invalidParent);
    QVERIFY(parent.open(QIODevice::WriteOnly));
    parent.write("not a directory");
    parent.close();
    const QString impossiblePath = QDir(invalidParent).filePath(QStringLiteral("finder.json"));
    const QString itemPath = QDir(directory.path()).filePath(QStringLiteral("item.txt"));
    QFile item(itemPath);
    QVERIFY(item.open(QIODevice::WriteOnly));
    item.write("item");
    item.close();

    ManualDispatch dispatch;
    RecentStore store(sourcePaths(impossiblePath, QString()), nullptr, std::ref(dispatch));
    QSignalSpy failed(&store, &RecentStore::saveFinished);
    store.recordAccess(finderRecord(itemPath, 1000));
    QVERIFY(dispatch.size() >= 1);
    dispatch.run(0);
    QCOMPARE(failed.count(), 1);
    QVERIFY(!failed.at(0).at(1).toBool());
    QCOMPARE(store.records().size(), 1);
    QCOMPARE(store.records().constFirst().lastAccessed, 1000);
}

void RecentStoreTest::productionLoadRunsOffOwnerThread()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = QDir(directory.path()).filePath(QStringLiteral("item.txt"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("item");
    file.close();
    const QString finderPath = writeFinderSource(directory.path(), {
        QJsonObject {{QStringLiteral("filePath"), path}, {QStringLiteral("lastAccessed"), 1200}},
    });

    RecentStore store(sourcePaths(finderPath, QString()));
    QSignalSpy ready(&store, &RecentStore::loadReady);
    store.load();
    QTRY_COMPARE_WITH_TIMEOUT(ready.count(), 1, 3000);
    QVERIFY(ready.at(0).at(2).toULongLong()
            != reinterpret_cast<quintptr>(QThread::currentThreadId()));
}

QTEST_GUILESS_MAIN(RecentStoreTest)

#include "tst_recent_store.moc"
