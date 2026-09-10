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
    void retainsViewportUpgradeWhenLowerTargetIsInFlight();
    void suppressesViewportDowngradeAgainstHigherTargetInFlight();
    void collapsesRapidViewportTargetsToLatestRangeRequirement();
    void discardsViewportUpgradeWhenPathLeavesLatestRange();
    void ignoresRemoteAndMetadataLimitedEntries();
    void rejectsBrokenSymlinkFromThumbnailScheduling();
    void followsValidSymlinkForThumbnailScheduling();
    void disabledPreviewsBlockScheduling();
    void staleGenerationSamePathQueuesCurrentViewportAfterCompletion();
    void staleGenerationSamePathQueuesCurrentSelectedPreviewAfterCompletion();
    void disabledInFlightResultsDoNotHydrateOrPoisonEnabledLifecycle();
    void hydratesGeneratedVideoImmediately();
    void rejectsSourceVersionChanges();
    void prioritizesSelectedPreviewWithoutDroppingViewport();
    void retainsSelectedUpgradeAcrossViewportReplacement();
    void upgradesSelectedPreviewAfterLowerResolutionResult();
    void suppressesRepeatedUnsupportedBySourceVersion();
    void changedSourceVersionEscapesDeferredState();
    void retriesDeferredVisibleItemOnceAfterDeadline();
    void doesNotPollUnavailableItem();
    void retriesDeferredSelectedItemOutsideVisibleRange();
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

void PreviewControllerTest::retainsViewportUpgradeWhenLowerTargetIsInFlight()
{
    DirectoryModel model;
    FakeRustBackendClient client;
    const DirectoryEntry entry = previewEntry(QStringLiteral("/fixture/upgrade.png"));
    model.applyEntries({entry}, 141);
    PreviewController controller(&client, &model);
    controller.beginGeneration(141, false);
    controller.setEnabled(true);

    controller.requestVisibleRange(0, 0, 128);
    QTRY_COMPARE(client.utilityRequests().size(), 1);
    controller.requestVisibleRange(0, 0, 256);
    QTest::qWait(80);
    QCOMPARE(client.utilityRequests().size(), 1);

    UtilityResult result;
    result.requestId = 1;
    result.operation = QStringLiteral("thumbnail-batch");
    result.ok = true;
    result.data.insert(QStringLiteral("items"), QJsonArray {});
    client.completeUtility(1, result);

    QTRY_COMPARE(client.utilityRequests().size(), 2);
    QCOMPARE(client.utilityRequests().at(1).arguments.at(0), QStringLiteral("256"));
    QCOMPARE(
        client.utilityRequests().at(1).arguments.mid(1),
        QStringList {entry.filePath});
}

void PreviewControllerTest::suppressesViewportDowngradeAgainstHigherTargetInFlight()
{
    DirectoryModel model;
    FakeRustBackendClient client;
    const DirectoryEntry entry = previewEntry(QStringLiteral("/fixture/downgrade.png"));
    model.applyEntries({entry}, 142);
    PreviewController controller(&client, &model);
    controller.beginGeneration(142, false);
    controller.setEnabled(true);

    controller.requestVisibleRange(0, 0, 256);
    QTRY_COMPARE(client.utilityRequests().size(), 1);
    controller.requestVisibleRange(0, 0, 128);
    QTest::qWait(80);
    QCOMPARE(client.utilityRequests().size(), 1);

    UtilityResult result;
    result.requestId = 1;
    result.operation = QStringLiteral("thumbnail-batch");
    result.ok = true;
    result.data.insert(QStringLiteral("items"), QJsonArray {});
    client.completeUtility(1, result);
    QTest::qWait(80);
    QCOMPARE(client.utilityRequests().size(), 1);
}

void PreviewControllerTest::collapsesRapidViewportTargetsToLatestRangeRequirement()
{
    DirectoryModel model;
    FakeRustBackendClient client;
    const DirectoryEntry entry = previewEntry(QStringLiteral("/fixture/rapid.png"));
    model.applyEntries({entry}, 143);
    PreviewController controller(&client, &model);
    controller.beginGeneration(143, false);
    controller.setEnabled(true);

    controller.requestVisibleRange(0, 0, 128);
    QTRY_COMPARE(client.utilityRequests().size(), 1);
    controller.requestVisibleRange(0, 0, 256);
    controller.requestVisibleRange(0, 0, 640);
    controller.requestVisibleRange(0, 0, 320);

    UtilityResult result;
    result.requestId = 1;
    result.operation = QStringLiteral("thumbnail-batch");
    result.ok = true;
    result.data.insert(QStringLiteral("items"), QJsonArray {});
    client.completeUtility(1, result);

    QTRY_COMPARE(client.utilityRequests().size(), 2);
    QCOMPARE(client.utilityRequests().at(1).arguments.at(0), QStringLiteral("320"));
}

void PreviewControllerTest::discardsViewportUpgradeWhenPathLeavesLatestRange()
{
    DirectoryModel model;
    FakeRustBackendClient client;
    const DirectoryEntry first = previewEntry(QStringLiteral("/fixture/first.png"));
    const DirectoryEntry second = previewEntry(QStringLiteral("/fixture/second.png"));
    model.applyEntries({first, second}, 144);
    PreviewController controller(&client, &model);
    controller.beginGeneration(144, false);
    controller.setEnabled(true);

    controller.requestVisibleRange(0, 0, 128);
    QTRY_COMPARE(client.utilityRequests().size(), 1);
    controller.requestVisibleRange(0, 0, 256);
    controller.requestVisibleRange(1, 1, 256);

    UtilityResult result;
    result.requestId = 1;
    result.operation = QStringLiteral("thumbnail-batch");
    result.ok = true;
    result.data.insert(QStringLiteral("items"), QJsonArray {});
    client.completeUtility(1, result);

    QTRY_COMPARE(client.utilityRequests().size(), 2);
    QCOMPARE(
        client.utilityRequests().at(1).arguments.mid(1),
        QStringList {second.filePath});
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

void PreviewControllerTest::rejectsBrokenSymlinkFromThumbnailScheduling()
{
    DirectoryModel model;
    FakeRustBackendClient client;
    DirectoryEntry entry = previewEntry(QStringLiteral("/fixture/broken.png"));
    entry.fileIsSymlink = true;
    entry.fileSymlinkBroken = true;
    model.applyEntries({entry}, 145);
    PreviewController controller(&client, &model);
    controller.beginGeneration(145, false);
    controller.setEnabled(true);

    controller.requestVisibleRange(0, 0, 128);
    QTest::qWait(80);
    QCOMPARE(client.utilityRequests().size(), 0);
    QVERIFY(controller.previewUrl(entry, false).isEmpty());
}

void PreviewControllerTest::followsValidSymlinkForThumbnailScheduling()
{
    DirectoryModel model;
    FakeRustBackendClient client;
    DirectoryEntry entry = previewEntry(QStringLiteral("/fixture/link.png"));
    entry.fileIsSymlink = true;
    entry.fileSymlinkBroken = false;
    model.applyEntries({entry}, 146);
    PreviewController controller(&client, &model);
    controller.beginGeneration(146, false);
    controller.setEnabled(true);

    controller.requestVisibleRange(0, 0, 128);
    QTRY_COMPARE(client.utilityRequests().size(), 1);
    QCOMPARE(
        client.utilityRequests().at(0).arguments.mid(1),
        QStringList {entry.filePath});
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

void PreviewControllerTest::staleGenerationSamePathQueuesCurrentViewportAfterCompletion()
{
    DirectoryModel model;
    FakeRustBackendClient client;
    const DirectoryEntry entry = previewEntry(QStringLiteral("/fixture/photo.png"));
    model.applyEntries({entry}, 1);
    PreviewController controller(&client, &model);
    controller.beginGeneration(1, false);
    controller.requestVisibleRange(0, 0, 256);
    QTRY_COMPARE(client.utilityRequests().size(), 1);

    model.applyEntries({entry}, 2);
    controller.beginGeneration(2, false);
    controller.requestVisibleRange(0, 0, 256);
    QTest::qWait(80);
    QCOMPARE(client.utilityRequests().size(), 1);

    UtilityResult stale;
    stale.requestId = 1;
    stale.operation = QStringLiteral("thumbnail-batch");
    stale.ok = true;
    stale.data.insert(
        QStringLiteral("items"),
        QJsonArray {
            QJsonObject {
                {QStringLiteral("filePath"), entry.filePath},
                {QStringLiteral("status"), QStringLiteral("generated")},
                {QStringLiteral("previewUrl"), QStringLiteral("file:///cache/stale.png")},
                {QStringLiteral("cacheTier"), QStringLiteral("large")},
                {QStringLiteral("sourceVersion"), QStringLiteral("0:0")},
            },
        });
    client.completeUtility(1, stale);

    QTRY_COMPARE(client.utilityRequests().size(), 2);
    const QStringList expectedRequest {
        QStringLiteral("256"),
        entry.filePath,
    };
    QCOMPARE(
        client.utilityRequests().at(1).arguments,
        expectedRequest);
    QVERIFY(
        model.data(model.index(0, 0), DirectoryModel::FilePreviewUrlRole)
            .toUrl()
            .isEmpty());

    UtilityResult current = stale;
    current.requestId = 2;
    current.data.insert(
        QStringLiteral("items"),
        QJsonArray {
            QJsonObject {
                {QStringLiteral("filePath"), entry.filePath},
                {QStringLiteral("status"), QStringLiteral("generated")},
                {QStringLiteral("previewUrl"), QStringLiteral("file:///cache/current.png")},
                {QStringLiteral("cacheTier"), QStringLiteral("large")},
                {QStringLiteral("sourceVersion"), QStringLiteral("0:0")},
            },
        });
    client.completeUtility(2, current);

    QTRY_COMPARE(
        model.data(model.index(0, 0), DirectoryModel::FilePreviewUrlRole).toUrl(),
        QUrl(QStringLiteral("file:///cache/current.png")));
}

void PreviewControllerTest::staleGenerationSamePathQueuesCurrentSelectedPreviewAfterCompletion()
{
    DirectoryModel model;
    FakeRustBackendClient client;
    const DirectoryEntry entry = previewEntry(QStringLiteral("/fixture/selected.mp4"));
    model.applyEntries({entry}, 3);
    PreviewController controller(&client, &model);
    controller.beginGeneration(3, false);
    controller.requestSelectedPreview(entry.filePath, 256);
    QTRY_COMPARE(client.utilityRequests().size(), 1);

    model.applyEntries({entry}, 4);
    controller.beginGeneration(4, false);
    controller.requestSelectedPreview(entry.filePath, 256);
    QTest::qWait(20);
    QCOMPARE(client.utilityRequests().size(), 1);

    UtilityResult stale;
    stale.requestId = 1;
    stale.operation = QStringLiteral("thumbnail-batch");
    stale.ok = true;
    stale.data.insert(
        QStringLiteral("items"),
        QJsonArray {
            QJsonObject {
                {QStringLiteral("filePath"), entry.filePath},
                {QStringLiteral("status"), QStringLiteral("generated")},
                {QStringLiteral("previewUrl"), QStringLiteral("file:///cache/stale-selected.png")},
                {QStringLiteral("cacheTier"), QStringLiteral("large")},
                {QStringLiteral("sourceVersion"), QStringLiteral("0:0")},
            },
        });
    client.completeUtility(1, stale);

    QTRY_COMPARE(client.utilityRequests().size(), 2);
    const QStringList expectedRequest {
        QStringLiteral("256"),
        entry.filePath,
    };
    QCOMPARE(
        client.utilityRequests().at(1).arguments,
        expectedRequest);
    QVERIFY(
        model.data(model.index(0, 0), DirectoryModel::FilePreviewUrlRole)
            .toUrl()
            .isEmpty());

    UtilityResult current = stale;
    current.requestId = 2;
    current.data.insert(
        QStringLiteral("items"),
        QJsonArray {
            QJsonObject {
                {QStringLiteral("filePath"), entry.filePath},
                {QStringLiteral("status"), QStringLiteral("generated")},
                {QStringLiteral("previewUrl"), QStringLiteral("file:///cache/current-selected.png")},
                {QStringLiteral("cacheTier"), QStringLiteral("large")},
                {QStringLiteral("sourceVersion"), QStringLiteral("0:0")},
            },
        });
    client.completeUtility(2, current);

    QTRY_COMPARE(
        model.data(model.index(0, 0), DirectoryModel::FilePreviewUrlRole).toUrl(),
        QUrl(QStringLiteral("file:///cache/current-selected.png")));
}

void PreviewControllerTest::disabledInFlightResultsDoNotHydrateOrPoisonEnabledLifecycle()
{
    const QStringList statuses {
        QStringLiteral("deferred"),
        QStringLiteral("failed"),
        QStringLiteral("unavailable"),
    };
    for (const QString &status : statuses) {
        DirectoryModel model;
        FakeRustBackendClient client;
        const DirectoryEntry entry = previewEntry(
            QStringLiteral("/fixture/disabled-%1.png").arg(status));
        model.applyEntries({entry}, 5);
        PreviewController controller(&client, &model);
        controller.beginGeneration(5, false);
        controller.requestVisibleRange(0, 0, 128);
        QTRY_COMPARE(client.utilityRequests().size(), 1);
        controller.setEnabled(false);

        UtilityResult result;
        result.requestId = 1;
        result.operation = QStringLiteral("thumbnail-batch");
        result.ok = true;
        QJsonObject item {
            {QStringLiteral("filePath"), entry.filePath},
            {QStringLiteral("status"), status},
            {QStringLiteral("sourceVersion"), QStringLiteral("0:0")},
        };
        if (status == QStringLiteral("deferred")) {
            item.insert(QStringLiteral("retryable"), true);
            item.insert(QStringLiteral("retryAfterMs"), 1000);
        } else if (status == QStringLiteral("unavailable")) {
            item.insert(QStringLiteral("retryable"), false);
        }
        result.data.insert(QStringLiteral("items"), QJsonArray {item});
        client.completeUtility(1, result);
        QTest::qWait(50);
        QVERIFY(
            model.data(model.index(0, 0), DirectoryModel::FilePreviewUrlRole)
                .toUrl()
                .isEmpty());

        controller.setEnabled(true);
        controller.requestVisibleRange(0, 0, 128);
        QTRY_COMPARE(client.utilityRequests().size(), 2);
        const QStringList expectedRequest {
            QStringLiteral("128"),
            entry.filePath,
        };
        QCOMPARE(
            client.utilityRequests().at(1).arguments,
            expectedRequest);
    }

    DirectoryModel model;
    FakeRustBackendClient client;
    const DirectoryEntry entry = previewEntry(QStringLiteral("/fixture/disabled-generated.png"));
    model.applyEntries({entry}, 6);
    PreviewController controller(&client, &model);
    controller.beginGeneration(6, false);
    controller.requestVisibleRange(0, 0, 128);
    QTRY_COMPARE(client.utilityRequests().size(), 1);
    controller.setEnabled(false);

    UtilityResult generated;
    generated.requestId = 1;
    generated.operation = QStringLiteral("thumbnail-batch");
    generated.ok = true;
    generated.data.insert(
        QStringLiteral("items"),
        QJsonArray {
            QJsonObject {
                {QStringLiteral("filePath"), entry.filePath},
                {QStringLiteral("status"), QStringLiteral("generated")},
                {QStringLiteral("previewUrl"), QStringLiteral("file:///cache/disabled.png")},
                {QStringLiteral("cacheTier"), QStringLiteral("normal")},
                {QStringLiteral("sourceVersion"), QStringLiteral("0:0")},
            },
        });
    client.completeUtility(1, generated);
    QTest::qWait(50);
    QVERIFY(
        model.data(model.index(0, 0), DirectoryModel::FilePreviewUrlRole)
            .toUrl()
            .isEmpty());

    controller.setEnabled(true);
    controller.requestVisibleRange(0, 0, 128);
    QTRY_COMPARE(client.utilityRequests().size(), 2);
    UtilityResult enabled = generated;
    enabled.requestId = 2;
    enabled.data.insert(
        QStringLiteral("items"),
        QJsonArray {
            QJsonObject {
                {QStringLiteral("filePath"), entry.filePath},
                {QStringLiteral("status"), QStringLiteral("ready")},
                {QStringLiteral("previewUrl"), QStringLiteral("file:///cache/enabled.png")},
                {QStringLiteral("cacheTier"), QStringLiteral("normal")},
                {QStringLiteral("sourceVersion"), QStringLiteral("0:0")},
            },
        });
    client.completeUtility(2, enabled);
    QTRY_COMPARE(
        model.data(model.index(0, 0), DirectoryModel::FilePreviewUrlRole).toUrl(),
        QUrl(QStringLiteral("file:///cache/enabled.png")));
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

void PreviewControllerTest::retainsSelectedUpgradeAcrossViewportReplacement()
{
    DirectoryModel model;
    FakeRustBackendClient client;
    const DirectoryEntry selected = previewEntry(QStringLiteral("/fixture/selected-upgrade.mp4"));
    const DirectoryEntry unrelated = previewEntry(QStringLiteral("/fixture/unrelated.png"));
    model.applyEntries({selected, unrelated}, 147);
    PreviewController controller(&client, &model);
    controller.beginGeneration(147, false);
    controller.setEnabled(true);

    controller.requestVisibleRange(0, 0, 128);
    QTRY_COMPARE(client.utilityRequests().size(), 1);
    controller.requestSelectedPreview(selected.filePath, 256);
    controller.requestVisibleRange(1, 1, 128);
    QTest::qWait(80);
    QCOMPARE(client.utilityRequests().size(), 1);

    UtilityResult result;
    result.requestId = 1;
    result.operation = QStringLiteral("thumbnail-batch");
    result.ok = true;
    result.data.insert(QStringLiteral("items"), QJsonArray {});
    client.completeUtility(1, result);

    QTRY_COMPARE(client.utilityRequests().size(), 2);
    QCOMPARE(client.utilityRequests().at(1).arguments.at(0), QStringLiteral("256"));
    QCOMPARE(
        client.utilityRequests().at(1).arguments.mid(1),
        QStringList {selected.filePath});
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
                {QStringLiteral("retryable"), true},
                {QStringLiteral("retryAfterMs"), 30},
                {QStringLiteral("reason"), QStringLiteral("recently-modified")},
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
            {QStringLiteral("retryable"), true},
            {QStringLiteral("retryAfterMs"), 250},
            {QStringLiteral("reason"), QStringLiteral("recently-modified")},
            {QStringLiteral("sourceVersion"), QStringLiteral("0:12")},
        },
        });
    client.completeUtility(1, deferred);
    QTest::qWait(10);
    QCOMPARE(client.utilityRequests().size(), 1);

    QTRY_COMPARE_WITH_TIMEOUT(client.utilityRequests().size(), 2, 1000);
    QTest::qWait(100);
    QCOMPARE(client.utilityRequests().size(), 2);
}

void PreviewControllerTest::doesNotPollUnavailableItem()
{
    DirectoryModel model;
    FakeRustBackendClient client;
    DirectoryEntry entry = previewEntry(QStringLiteral("/fixture/unreadable.png"));
    entry.fileSize = 12;
    model.applyEntries({entry}, 148);
    PreviewController controller(&client, &model);
    controller.beginGeneration(148, false);
    controller.setEnabled(true);

    controller.requestVisibleRange(0, 0, 128);
    QTRY_COMPARE(client.utilityRequests().size(), 1);

    UtilityResult unavailable;
    unavailable.requestId = 1;
    unavailable.operation = QStringLiteral("thumbnail-batch");
    unavailable.ok = true;
    unavailable.data.insert(
        QStringLiteral("items"),
        QJsonArray {
            QJsonObject {
                {QStringLiteral("filePath"), entry.filePath},
                {QStringLiteral("status"), QStringLiteral("unavailable")},
                {QStringLiteral("retryable"), false},
                {QStringLiteral("reason"), QStringLiteral("unreadable")},
                {QStringLiteral("sourceVersion"), QStringLiteral("0:12")},
            },
        });
    client.completeUtility(1, unavailable);
    QTest::qWait(120);
    QCOMPARE(client.utilityRequests().size(), 1);

    entry.fileSize = 13;
    model.applyEntries({entry}, 148);
    controller.requestVisibleRange(0, 0, 128);
    QTRY_COMPARE(client.utilityRequests().size(), 2);
}

void PreviewControllerTest::retriesDeferredSelectedItemOutsideVisibleRange()
{
    DirectoryModel model;
    FakeRustBackendClient client;
    const DirectoryEntry selected = previewEntry(QStringLiteral("/fixture/selected-recent.mp4"));
    const DirectoryEntry visible = previewEntry(QStringLiteral("/fixture/visible.png"));
    model.applyEntries({selected, visible}, 149);
    PreviewController controller(&client, &model);
    controller.beginGeneration(149, false);
    controller.setEnabled(true);

    controller.requestSelectedPreview(selected.filePath, 320);
    QTRY_COMPARE(client.utilityRequests().size(), 1);

    UtilityResult deferred;
    deferred.requestId = 1;
    deferred.operation = QStringLiteral("thumbnail-batch");
    deferred.ok = true;
    deferred.data.insert(
        QStringLiteral("items"),
        QJsonArray {
            QJsonObject {
                {QStringLiteral("filePath"), selected.filePath},
                {QStringLiteral("status"), QStringLiteral("deferred")},
                {QStringLiteral("retryable"), true},
                {QStringLiteral("retryAfterMs"), 250},
                {QStringLiteral("reason"), QStringLiteral("recently-modified")},
                {QStringLiteral("sourceVersion"), QStringLiteral("0:0")},
            },
        });
    client.completeUtility(1, deferred);
    controller.requestVisibleRange(1, 1, 128);

    QTest::qWait(10);
    QTRY_COMPARE_WITH_TIMEOUT(client.utilityRequests().size(), 2, 1000);
    QCOMPARE(client.utilityRequests().at(1).arguments.at(0), QStringLiteral("128"));
    QCOMPARE(
        client.utilityRequests().at(1).arguments.mid(1),
        QStringList {visible.filePath});

    UtilityResult visibleResult;
    visibleResult.requestId = 2;
    visibleResult.operation = QStringLiteral("thumbnail-batch");
    visibleResult.ok = true;
    visibleResult.data.insert(QStringLiteral("items"), QJsonArray {});
    client.completeUtility(2, visibleResult);

    QTRY_COMPARE_WITH_TIMEOUT(client.utilityRequests().size(), 3, 1000);
    QCOMPARE(client.utilityRequests().at(2).arguments.at(0), QStringLiteral("320"));
    QCOMPARE(
        client.utilityRequests().at(2).arguments.mid(1),
        QStringList {selected.filePath});
}

QTEST_GUILESS_MAIN(PreviewControllerTest)

#include "tst_preview_controller.moc"
