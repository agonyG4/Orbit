#include <QSignalSpy>
#include <QtTest>

#include "models/directory_model.h"

using namespace Astrea::Explorer::Native::Backend;

class DirectoryModelTest final : public QObject
{
    Q_OBJECT

private slots:
    void exposesLegacyRolesAndValues();
    void exposesQmlListModelCompatibility();
    void startsEmpty();
    void handlesTenThousandEntriesWithoutIncrementalSignals();
    void rejectsStaleGenerations();
    void acceptsSortOrderReplacement();
    void updatesOnlyMatchingPreview();
    void exposesRecentOnlyRoles();
};

DirectoryEntry makeEntry(
    const QString &name,
    const QString &path,
    bool isDirectory = false)
{
    DirectoryEntry entry;
    entry.fileName = name;
    entry.filePath = path;
    entry.fileUrl = QUrl::fromLocalFile(path);
    entry.fileIsDir = isDirectory;
    entry.fileExecutable = !isDirectory;
    entry.fileHidden = name.startsWith(QLatin1Char('.'));
    entry.fileSize = 42;
    entry.fileModified = QDateTime::fromMSecsSinceEpoch(1723265945000, QTimeZone::UTC);
    entry.fileKind = isDirectory ? QStringLiteral("Folder") : QStringLiteral("TXT");
    entry.filePreviewUrl = QUrl(QStringLiteral("file:///tmp/preview.png"));
    entry.fileRemote = true;
    entry.fileMetadataLimited = true;
    entry.fileFilesystem = QStringLiteral("fuse.test");
    return entry;
}

void DirectoryModelTest::exposesLegacyRolesAndValues()
{
    DirectoryModel model;
    const DirectoryEntry entry = makeEntry(
        QStringLiteral("photo #1.txt"),
        QStringLiteral("/tmp/example/photo #1.txt"));

    const QHash<int, QByteArray> roles = model.roleNames();
    const QList<QByteArray> expectedRoles = {
        QByteArrayLiteral("fileName"),
        QByteArrayLiteral("filePath"),
        QByteArrayLiteral("fileUrl"),
        QByteArrayLiteral("fileIsDir"),
        QByteArrayLiteral("fileExecutable"),
        QByteArrayLiteral("fileHidden"),
        QByteArrayLiteral("fileSize"),
        QByteArrayLiteral("fileModified"),
        QByteArrayLiteral("fileKind"),
        QByteArrayLiteral("filePreviewUrl"),
        QByteArrayLiteral("fileRemote"),
        QByteArrayLiteral("fileMetadataLimited"),
        QByteArrayLiteral("fileFilesystem"),
        QByteArrayLiteral("lastAccessed"),
        QByteArrayLiteral("recentSource"),
    };
    QCOMPARE(roles.size(), expectedRoles.size());
    for (const QByteArray &roleName : expectedRoles) {
        QVERIFY(roles.values().contains(roleName));
    }

    QVERIFY(model.applyEntries({entry}, 1));
    const QModelIndex index = model.index(0, 0);
    QCOMPARE(model.data(index, DirectoryModel::FileNameRole).toString(), entry.fileName);
    QCOMPARE(model.data(index, DirectoryModel::FilePathRole).toString(), entry.filePath);
    QCOMPARE(model.data(index, DirectoryModel::FileUrlRole).toUrl(), entry.fileUrl);
    QCOMPARE(model.data(index, DirectoryModel::FileIsDirRole).toBool(), entry.fileIsDir);
    QCOMPARE(model.data(index, DirectoryModel::FileExecutableRole).toBool(), entry.fileExecutable);
    QCOMPARE(model.data(index, DirectoryModel::FileHiddenRole).toBool(), entry.fileHidden);
    QCOMPARE(model.data(index, DirectoryModel::FileSizeRole).toLongLong(), entry.fileSize);
    QCOMPARE(model.data(index, DirectoryModel::FileModifiedRole).toDateTime(), entry.fileModified);
    QCOMPARE(model.data(index, DirectoryModel::FileKindRole).toString(), entry.fileKind);
    QCOMPARE(model.data(index, DirectoryModel::FilePreviewUrlRole).toUrl(), entry.filePreviewUrl);
    QCOMPARE(model.data(index, DirectoryModel::FileRemoteRole).toBool(), entry.fileRemote);
    QCOMPARE(
        model.data(index, DirectoryModel::FileMetadataLimitedRole).toBool(),
        entry.fileMetadataLimited);
    QCOMPARE(
        model.data(index, DirectoryModel::FileFilesystemRole).toString(),
        entry.fileFilesystem);
}

void DirectoryModelTest::startsEmpty()
{
    DirectoryModel model;

    QCOMPARE(model.rowCount(), 0);
    QVERIFY(model.paths().isEmpty());
    QVERIFY(!model.data(QModelIndex(), DirectoryModel::FileNameRole).isValid());
}

void DirectoryModelTest::exposesQmlListModelCompatibility()
{
    DirectoryModel model;
    const DirectoryEntry first = makeEntry(
        QStringLiteral("first.txt"),
        QStringLiteral("/tmp/first.txt"));
    const DirectoryEntry second = makeEntry(
        QStringLiteral("second"),
        QStringLiteral("/tmp/second"),
        true);

    QSignalSpy countSpy(&model, &DirectoryModel::countChanged);
    QVERIFY(model.applyEntries({first, second}, 1));

    QCOMPARE(model.count(), 2);
    QCOMPARE(countSpy.count(), 1);
    const QVariantMap firstMap = model.get(0);
    QCOMPARE(firstMap.value(QStringLiteral("fileName")).toString(), first.fileName);
    QCOMPARE(firstMap.value(QStringLiteral("filePath")).toString(), first.filePath);
    QCOMPARE(firstMap.value(QStringLiteral("fileIsDir")).toBool(), first.fileIsDir);
    QVERIFY(model.get(-1).isEmpty());
    QVERIFY(model.get(model.count()).isEmpty());
}

void DirectoryModelTest::handlesTenThousandEntriesWithoutIncrementalSignals()
{
    DirectoryModel model;
    QSignalSpy resetSpy(&model, &QAbstractItemModel::modelReset);

    QVector<DirectoryEntry> entries;
    entries.reserve(10000);
    for (int i = 0; i < 10000; ++i) {
        entries.append(makeEntry(
            QStringLiteral("file-%1.txt").arg(i),
            QStringLiteral("/tmp/example/file-%1.txt").arg(i)));
    }

    QVERIFY(model.applyEntries(std::move(entries), 1));
    QCOMPARE(model.rowCount(), 10000);
    QCOMPARE(resetSpy.count(), 1);
    QCOMPARE(model.paths().size(), 10000);
    QCOMPARE(model.data(model.index(9999, 0), DirectoryModel::FileNameRole).toString(),
             QStringLiteral("file-9999.txt"));
}

void DirectoryModelTest::rejectsStaleGenerations()
{
    DirectoryModel model;
    QSignalSpy resetSpy(&model, &QAbstractItemModel::modelReset);

    QVERIFY(model.applyEntries(
        {makeEntry(QStringLiteral("new.txt"), QStringLiteral("/tmp/new.txt"))},
        2));
    QVERIFY(!model.applyEntries(
        {makeEntry(QStringLiteral("old.txt"), QStringLiteral("/tmp/old.txt"))},
        1));

    QCOMPARE(resetSpy.count(), 1);
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.data(model.index(0, 0), DirectoryModel::FileNameRole).toString(),
             QStringLiteral("new.txt"));
}

void DirectoryModelTest::acceptsSortOrderReplacement()
{
    DirectoryModel model;
    QSignalSpy resetSpy(&model, &QAbstractItemModel::modelReset);

    QVERIFY(model.applyEntries(
        {makeEntry(QStringLiteral("z.txt"), QStringLiteral("/tmp/z.txt")),
         makeEntry(QStringLiteral("a.txt"), QStringLiteral("/tmp/a.txt"))},
        1));
    QVERIFY(model.applyEntries(
        {makeEntry(QStringLiteral("a.txt"), QStringLiteral("/tmp/a.txt")),
         makeEntry(QStringLiteral("z.txt"), QStringLiteral("/tmp/z.txt"))},
        2));

    QCOMPARE(resetSpy.count(), 2);
    QCOMPARE(model.data(model.index(0, 0), DirectoryModel::FileNameRole).toString(),
             QStringLiteral("a.txt"));
    QCOMPARE(model.data(model.index(1, 0), DirectoryModel::FileNameRole).toString(),
             QStringLiteral("z.txt"));
}

void DirectoryModelTest::updatesOnlyMatchingPreview()
{
    DirectoryModel model;
    const QUrl oldPreview(QStringLiteral("file:///tmp/old.png"));
    const QUrl newPreview(QStringLiteral("file:///tmp/new.png"));
    DirectoryEntry first = makeEntry(QStringLiteral("first.png"), QStringLiteral("/tmp/first.png"));
    DirectoryEntry second = makeEntry(QStringLiteral("second.png"), QStringLiteral("/tmp/second.png"));
    first.filePreviewUrl = oldPreview;
    second.filePreviewUrl = oldPreview;
    QVERIFY(model.applyEntries({first, second}, 4));

    QSignalSpy changedSpy(&model, &QAbstractItemModel::dataChanged);
    QVERIFY(model.updatePreview(second.filePath, newPreview, 4));
    QVERIFY(!model.updatePreview(QStringLiteral("/tmp/missing.png"), newPreview, 4));
    QVERIFY(!model.updatePreview(first.filePath, newPreview, 3));

    QCOMPARE(changedSpy.count(), 1);
    QCOMPARE(model.data(model.index(0, 0), DirectoryModel::FilePreviewUrlRole).toUrl(), oldPreview);
    QCOMPARE(model.data(model.index(1, 0), DirectoryModel::FilePreviewUrlRole).toUrl(), newPreview);
}

void DirectoryModelTest::exposesRecentOnlyRoles()
{
    DirectoryModel model;
    DirectoryEntry entry = makeEntry(
        QStringLiteral("recent.txt"),
        QStringLiteral("/tmp/recent.txt"));
    entry.lastAccessed = 1723265945000;
    entry.recentSource = QStringLiteral("finder");

    const QHash<int, QByteArray> roles = model.roleNames();
    QCOMPARE(roles.values().count(QByteArrayLiteral("lastAccessed")), 1);
    QCOMPARE(roles.values().count(QByteArrayLiteral("recentSource")), 1);

    QVERIFY(model.applyEntries({entry}, 1));
    const QModelIndex index = model.index(0, 0);
    QCOMPARE(model.data(index, DirectoryModel::LastAccessedRole).toLongLong(), entry.lastAccessed);
    QCOMPARE(model.data(index, DirectoryModel::RecentSourceRole).toString(), entry.recentSource);
}

QTEST_MAIN(DirectoryModelTest)

#include "tst_directory_model.moc"
