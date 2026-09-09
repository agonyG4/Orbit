#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QUrl>
#include <QtTest>

#include "backend/fake_backend_client.h"
#include "controllers/preview_controller.h"
#include "models/directory_model.h"

using namespace Astrea::Explorer::Native::Backend;

class PreviewControllerTest final : public QObject
{
    Q_OBJECT

private slots:
    void derivesLocalPreviewForSupportedImage();
    void preservesBackendPreviewUrl();
    void suppressesRemoteAndDirectoryPreviews();
    void rejectsUnsupportedPreviewTypes();
    void appliesOnlyCurrentPreviewGeneration();
    void schedulesVisiblePathsInModelOrder();
    void replacesQueuedViewportWithLatestRange();
    void keepsOneBatchInFlight();
    void ignoresRemoteAndMetadataLimitedEntries();
    void disabledPreviewsBlockScheduling();
    void hydratesGeneratedVideoImmediately();
    void rejectsSourceVersionChanges();
    void prioritizesSelectedPreviewWithoutDroppingViewport();
    void upgradesSelectedPreviewAfterLowerResolutionResult();
    void suppressesRepeatedUnsupportedBySourceVersion();
    void changedSourceVersionEscapesDeferredState();
    void retriesDeferredVisibleItemOnceAfterDeadline();
};

DirectoryEntry previewEntry(const QString &path)
{
    DirectoryEntry entry;
    entry.fileName = QFileInfo(path).fileName();
    entry.filePath = path;
    entry.fileUrl = QUrl::fromLocalFile(path);
    return entry;
}

void PreviewControllerTest::derivesLocalPreviewForSupportedImage()
{
    DirectoryModel model;
    FakeRustBackendClient client;
    PreviewController controller(&client, &model);

    const DirectoryEntry entry = previewEntry(QStringLiteral("/fixture/photo.png"));
    QCOMPARE(
        controller.previewUrl(entry, false),
        QUrl(QStringLiteral("file:///fixture/photo.png")));
}

void PreviewControllerTest::preservesBackendPreviewUrl()
{
    DirectoryModel model;
    FakeRustBackendClient client;
    PreviewController controller(&client, &model);
    DirectoryEntry entry = previewEntry(QStringLiteral("/fixture/photo.png"));
    entry.filePreviewUrl = QUrl(QStringLiteral("file:///cache/preview.png"));

    QCOMPARE(controller.previewUrl(entry, false), entry.filePreviewUrl);
}

void PreviewControllerTest::suppressesRemoteAndDirectoryPreviews()
{
    DirectoryModel model;
    FakeRustBackendClient client;
    PreviewController controller(&client, &model);
    DirectoryEntry image = previewEntry(QStringLiteral("/fixture/photo.png"));
    image.fileRemote = true;
    DirectoryEntry directory = previewEntry(QStringLiteral("/fixture/photos"));
    directory.fileIsDir = true;

    QVERIFY(controller.previewUrl(image, true).isEmpty());
    QVERIFY(controller.previewUrl(image, false).isEmpty());
    QVERIFY(controller.previewUrl(directory, false).isEmpty());
}

void PreviewControllerTest::rejectsUnsupportedPreviewTypes()
{
    DirectoryModel model;
    FakeRustBackendClient client;
    PreviewController controller(&client, &model);
    const DirectoryEntry entry = previewEntry(QStringLiteral("/fixture/document.txt"));

    QVERIFY(controller.previewUrl(entry, false).isEmpty());
}

void PreviewControllerTest::appliesOnlyCurrentPreviewGeneration()
{
    DirectoryModel model;
    FakeRustBackendClient client;
    DirectoryEntry entry = previewEntry(QStringLiteral("/fixture/photo.png"));
    model.applyEntries({entry}, 7);
    PreviewController controller(&client, &model);

    controller.beginGeneration(7, false);
    const quint64 generation = controller.generation();
    QVERIFY(controller.applyPreview(
        entry.filePath,
        QUrl(QStringLiteral("file:///cache/current.png")),
        generation));
    QCOMPARE(
        model.data(model.index(0, 0), DirectoryModel::FilePreviewUrlRole).toUrl(),
        QUrl(QStringLiteral("file:///cache/current.png")));
    QVERIFY(!controller.applyPreview(
        entry.filePath,
        QUrl(QStringLiteral("file:///cache/stale.png")),
        generation - 1));
    QCOMPARE(
        model.data(model.index(0, 0), DirectoryModel::FilePreviewUrlRole).toUrl(),
        QUrl(QStringLiteral("file:///cache/current.png")));
    controller.beginGeneration(8, true);
    const quint64 remoteGeneration = controller.generation();
    QVERIFY(!controller.applyPreview(
        entry.filePath,
        QUrl(QStringLiteral("file:///cache/remote.png")),
        remoteGeneration));
}

void PreviewControllerTest::schedulesVisiblePathsInModelOrder()
{
    DirectoryModel model;
    FakeRustBackendClient client;
    QVector<DirectoryEntry> entries {
        previewEntry(QStringLiteral("/fixture/zero.png")),
        previewEntry(QStringLiteral("/fixture/one.mp4")),
        previewEntry(QStringLiteral("/fixture/two.jpg")),
        previewEntry(QStringLiteral("/fixture/three.webm")),
    };
    model.applyEntries(entries, 12);
    PreviewController controller(&client, &model);
    controller.beginGeneration(12, false);
    controller.setEnabled(true);

    controller.requestVisibleRange(0, 3, 192);
    QTRY_COMPARE(client.utilityRequests().size(), 1);
    const UtilityRequest request = client.utilityRequests().at(0);
    const QStringList expectedPaths {
        QStringLiteral("/fixture/zero.png"),
        QStringLiteral("/fixture/one.mp4"),
        QStringLiteral("/fixture/two.jpg"),
        QStringLiteral("/fixture/three.webm"),
    };

    QCOMPARE(request.operation, QStringLiteral("thumbnail-batch"));
    QCOMPARE(request.arguments.at(0), QStringLiteral("192"));
    QCOMPARE(request.arguments.mid(1), expectedPaths);
}

void PreviewControllerTest::replacesQueuedViewportWithLatestRange()
{
    DirectoryModel model;
    FakeRustBackendClient client;
    QVector<DirectoryEntry> entries;
    for (int index = 0; index < 8; ++index) {
        entries.append(previewEntry(QStringLiteral("/fixture/%1.png").arg(index)));
    }
    model.applyEntries(entries, 13);
    PreviewController controller(&client, &model);
    controller.beginGeneration(13, false);
    controller.setEnabled(true);

    controller.requestVisibleRange(0, 1, 128);
    controller.requestVisibleRange(6, 7, 128);
    QTRY_COMPARE(client.utilityRequests().size(), 1);
    const QStringList expectedPaths {
        QStringLiteral("/fixture/6.png"),
        QStringLiteral("/fixture/7.png"),
    };
    QCOMPARE(client.utilityRequests().at(0).arguments.mid(1), expectedPaths);
}

void PreviewControllerTest::keepsOneBatchInFlight()
{
    DirectoryModel model;
    FakeRustBackendClient client;
    model.applyEntries(
        {
            previewEntry(QStringLiteral("/fixture/one.png")),
            previewEntry(QStringLiteral("/fixture/two.png")),
        },
        14);
    PreviewController controller(&client, &model);
    controller.beginGeneration(14, false);
    controller.setEnabled(true);

    controller.requestVisibleRange(0, 0, 128);
    QTRY_COMPARE(client.utilityRequests().size(), 1);
    controller.requestVisibleRange(1, 1, 128);
    QTest::qWait(80);
    QCOMPARE(client.utilityRequests().size(), 1);

    UtilityResult result;
    result.requestId = client.utilityRequests().at(0).operation == QStringLiteral("thumbnail-batch")
        ? 1
        : 0;
    result.operation = QStringLiteral("thumbnail-batch");
    result.ok = true;
    result.data.insert(QStringLiteral("items"), QJsonArray {});
    client.completeUtility(result.requestId, result);
    QTRY_COMPARE(client.utilityRequests().size(), 2);
    QCOMPARE(
        client.utilityRequests().at(1).arguments.mid(1),
        QStringList {QStringLiteral("/fixture/two.png")});
}

void PreviewControllerTest::ignoresRemoteAndMetadataLimitedEntries()
{
    DirectoryModel model;
    FakeRustBackendClient client;
    DirectoryEntry remote = previewEntry(QStringLiteral("/fixture/remote.png"));
    remote.fileRemote = true;
    DirectoryEntry limited = previewEntry(QStringLiteral("/fixture/limited.png"));
    limited.fileMetadataLimited = true;
    DirectoryEntry directory = previewEntry(QStringLiteral("/fixture/folder"));
    directory.fileIsDir = true;
    DirectoryEntry local = previewEntry(QStringLiteral("/fixture/local.png"));
    model.applyEntries({remote, limited, directory, local}, 15);
    PreviewController controller(&client, &model);
    controller.beginGeneration(15, false);
    controller.setEnabled(true);

    controller.requestVisibleRange(0, 3, 128);
    QTRY_COMPARE(client.utilityRequests().size(), 1);
    QCOMPARE(
        client.utilityRequests().at(0).arguments.mid(1),
        QStringList {QStringLiteral("/fixture/local.png")});
}

void PreviewControllerTest::disabledPreviewsBlockScheduling()
{
    DirectoryModel model;
    FakeRustBackendClient client;
    model.applyEntries({previewEntry(QStringLiteral("/fixture/photo.png"))}, 16);
    PreviewController controller(&client, &model);
    controller.beginGeneration(16, false);
    controller.setEnabled(false);
    controller.requestVisibleRange(0, 0, 128);
    QTest::qWait(80);
    QCOMPARE(client.utilityRequests().size(), 0);
}

void PreviewControllerTest::hydratesGeneratedVideoImmediately()
{
    DirectoryModel model;
    FakeRustBackendClient client;
    DirectoryEntry entry = previewEntry(QStringLiteral("/fixture/movie.mp4"));
    entry.fileSize = 10;
    model.applyEntries({entry}, 17);
    PreviewController controller(&client, &model);
    controller.beginGeneration(17, false);
    controller.setEnabled(true);
    controller.requestVisibleRange(0, 0, 256);
    QTRY_COMPARE(client.utilityRequests().size(), 1);

    UtilityResult result;
    result.requestId = 1;
    result.operation = QStringLiteral("thumbnail-batch");
    result.ok = true;
    result.data.insert(
        QStringLiteral("items"),
        QJsonArray {
            QJsonObject {
                {QStringLiteral("filePath"), entry.filePath},
                {QStringLiteral("status"), QStringLiteral("generated")},
                {QStringLiteral("previewUrl"), QStringLiteral("file:///cache/movie.png")},
                {QStringLiteral("cacheTier"), QStringLiteral("large")},
                {QStringLiteral("sourceVersion"), QStringLiteral("0:10")},
            },
        });
    client.completeUtility(1, result);

    QTRY_COMPARE(
        model.data(model.index(0, 0), DirectoryModel::FilePreviewUrlRole).toUrl(),
        QUrl(QStringLiteral("file:///cache/movie.png")));
}

void PreviewControllerTest::rejectsSourceVersionChanges()
{
    DirectoryModel model;
    FakeRustBackendClient client;
    DirectoryEntry entry = previewEntry(QStringLiteral("/fixture/movie.mp4"));
    entry.fileSize = 10;
    model.applyEntries({entry}, 18);
    PreviewController controller(&client, &model);
    controller.beginGeneration(18, false);
    controller.setEnabled(true);
    controller.requestVisibleRange(0, 0, 256);
    QTRY_COMPARE(client.utilityRequests().size(), 1);

    entry.fileSize = 11;
    model.applyEntries({entry}, 18);
    UtilityResult result;
    result.requestId = 1;
    result.operation = QStringLiteral("thumbnail-batch");
    result.ok = true;
    result.data.insert(
        QStringLiteral("items"),
        QJsonArray {
            QJsonObject {
                {QStringLiteral("filePath"), entry.filePath},
                {QStringLiteral("status"), QStringLiteral("generated")},
                {QStringLiteral("previewUrl"), QStringLiteral("file:///cache/stale.png")},
                {QStringLiteral("sourceVersion"), QStringLiteral("0:10")},
            },
        });
    client.completeUtility(1, result);

    QTest::qWait(40);
    QVERIFY(
        model.data(model.index(0, 0), DirectoryModel::FilePreviewUrlRole)
            .toUrl()
            .isEmpty());
}

void PreviewControllerTest::prioritizesSelectedPreviewWithoutDroppingViewport()
{
    DirectoryModel model;
    FakeRustBackendClient client;
    model.applyEntries(
        {
            previewEntry(QStringLiteral("/fixture/one.png")),
            previewEntry(QStringLiteral("/fixture/two.mp4")),
            previewEntry(QStringLiteral("/fixture/three.png")),
        },
        19);
    PreviewController controller(&client, &model);
    controller.beginGeneration(19, false);
    controller.setEnabled(true);
    controller.requestVisibleRange(0, 2, 128);
    controller.requestSelectedPreview(QStringLiteral("/fixture/two.mp4"), 640);
    QTRY_COMPARE(client.utilityRequests().size(), 1);
    QCOMPARE(
        client.utilityRequests().at(0).arguments.mid(1),
        QStringList {QStringLiteral("/fixture/two.mp4")});
    QCOMPARE(client.utilityRequests().at(0).arguments.at(0), QStringLiteral("640"));

    UtilityResult result;
    result.requestId = 1;
    result.operation = QStringLiteral("thumbnail-batch");
    result.ok = true;
    result.data.insert(QStringLiteral("items"), QJsonArray {});
    client.completeUtility(1, result);
    QTRY_COMPARE(client.utilityRequests().size(), 2);
    const QStringList expectedPaths {
        QStringLiteral("/fixture/one.png"),
        QStringLiteral("/fixture/two.mp4"),
        QStringLiteral("/fixture/three.png"),
    };
    QCOMPARE(client.utilityRequests().at(1).arguments.mid(1), expectedPaths);
}

void PreviewControllerTest::upgradesSelectedPreviewAfterLowerResolutionResult()
{
    DirectoryModel model;
    FakeRustBackendClient client;
    DirectoryEntry entry = previewEntry(QStringLiteral("/fixture/selected.mp4"));
    entry.fileSize = 10;
    model.applyEntries({entry}, 20);
    PreviewController controller(&client, &model);
    controller.beginGeneration(20, false);
    controller.setEnabled(true);

    controller.requestVisibleRange(0, 0, 128);
    QTRY_COMPARE(client.utilityRequests().size(), 1);

    UtilityResult result;
    result.requestId = 1;
    result.operation = QStringLiteral("thumbnail-batch");
    result.ok = true;
    result.data.insert(
        QStringLiteral("items"),
        QJsonArray {
            QJsonObject {
                {QStringLiteral("filePath"), entry.filePath},
                {QStringLiteral("status"), QStringLiteral("ready")},
                {QStringLiteral("previewUrl"), QStringLiteral("file:///cache/normal.png")},
                {QStringLiteral("cacheTier"), QStringLiteral("normal")},
                {QStringLiteral("sourceVersion"), QStringLiteral("0:10")},
            },
        });
    client.completeUtility(1, result);
    QTRY_COMPARE(
        model.data(model.index(0, 0), DirectoryModel::FilePreviewUrlRole).toUrl(),
        QUrl(QStringLiteral("file:///cache/normal.png")));

    controller.requestSelectedPreview(entry.filePath, 640);
    QTRY_COMPARE(client.utilityRequests().size(), 2);
    QCOMPARE(client.utilityRequests().at(1).arguments.at(0), QStringLiteral("640"));
    QCOMPARE(
        client.utilityRequests().at(1).arguments.mid(1),
        QStringList {entry.filePath});
}

void PreviewControllerTest::suppressesRepeatedUnsupportedBySourceVersion()
{
    DirectoryModel model;
    FakeRustBackendClient client;
    DirectoryEntry entry = previewEntry(QStringLiteral("/fixture/notes.txt"));
    entry.fileSize = 5;
    model.applyEntries({entry}, 20);
    PreviewController controller(&client, &model);
    controller.beginGeneration(20, false);
    controller.setEnabled(true);

    controller.requestVisibleRange(0, 0, 128);
    QTRY_COMPARE(client.utilityRequests().size(), 1);

    UtilityResult result;
    result.requestId = 1;
    result.operation = QStringLiteral("thumbnail-batch");
    result.ok = true;
    result.data.insert(
        QStringLiteral("items"),
        QJsonArray {
            QJsonObject {
                {QStringLiteral("filePath"), entry.filePath},
                {QStringLiteral("status"), QStringLiteral("unsupported")},
                {QStringLiteral("sourceVersion"), QStringLiteral("0:5")},
            },
        });
    client.completeUtility(1, result);
    QTest::qWait(40);

    controller.requestVisibleRange(0, 0, 128);
    QTest::qWait(100);
    QCOMPARE(client.utilityRequests().size(), 1);

    entry.fileSize = 6;
    model.applyEntries({entry}, 20);
    controller.requestVisibleRange(0, 0, 128);
    QTRY_COMPARE(client.utilityRequests().size(), 2);
}

void PreviewControllerTest::changedSourceVersionEscapesDeferredState()
{
    DirectoryModel model;
    FakeRustBackendClient client;
    DirectoryEntry entry = previewEntry(QStringLiteral("/fixture/changing.mp4"));
    entry.fileSize = 10;
    model.applyEntries({entry}, 21);
    PreviewController controller(&client, &model);
    controller.beginGeneration(21, false);
    controller.setEnabled(true);

    controller.requestVisibleRange(0, 0, 128);
    QTRY_COMPARE(client.utilityRequests().size(), 1);

    UtilityResult result;
    result.requestId = 1;
    result.operation = QStringLiteral("thumbnail-batch");
    result.ok = true;
    result.data.insert(
        QStringLiteral("items"),
        QJsonArray {
            QJsonObject {
                {QStringLiteral("filePath"), entry.filePath},
                {QStringLiteral("status"), QStringLiteral("deferred")},
                {QStringLiteral("sourceVersion"), QStringLiteral("0:10")},
            },
        });
    client.completeUtility(1, result);
    QTest::qWait(40);

    entry.fileSize = 11;
    model.applyEntries({entry}, 21);
    controller.requestVisibleRange(0, 0, 128);
    QTRY_COMPARE(client.utilityRequests().size(), 2);
}

void PreviewControllerTest::retriesDeferredVisibleItemOnceAfterDeadline()
{
    DirectoryModel model;
    FakeRustBackendClient client;
    DirectoryEntry entry = previewEntry(QStringLiteral("/fixture/recent.mp4"));
    entry.fileSize = 12;
    model.applyEntries({entry}, 22);
    PreviewController controller(&client, &model);
    controller.beginGeneration(22, false);
    controller.setEnabled(true);

    controller.requestVisibleRange(0, 0, 128);
    QTRY_COMPARE(client.utilityRequests().size(), 1);

    UtilityResult deferred;
    deferred.requestId = 1;
    deferred.operation = QStringLiteral("thumbnail-batch");
    deferred.ok = true;
    deferred.data.insert(
        QStringLiteral("items"),
        QJsonArray {
            QJsonObject {
                {QStringLiteral("filePath"), entry.filePath},
                {QStringLiteral("status"), QStringLiteral("deferred")},
                {QStringLiteral("sourceVersion"), QStringLiteral("0:12")},
            },
        });
    client.completeUtility(1, deferred);
    QTest::qWait(40);
    QCOMPARE(client.utilityRequests().size(), 1);

    QTRY_COMPARE_WITH_TIMEOUT(client.utilityRequests().size(), 2, 3800);
    QTest::qWait(100);
    QCOMPARE(client.utilityRequests().size(), 2);
}

QTEST_GUILESS_MAIN(PreviewControllerTest)

#include "tst_preview_controller.moc"
