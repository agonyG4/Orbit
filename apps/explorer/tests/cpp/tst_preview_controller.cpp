#include <QUrl>
#include <QFileInfo>
#include <QtTest>

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
    PreviewController controller(&model);

    const DirectoryEntry entry = previewEntry(QStringLiteral("/fixture/photo.png"));
    QCOMPARE(
        controller.previewUrl(entry, false),
        QUrl(QStringLiteral("file:///fixture/photo.png")));
}

void PreviewControllerTest::preservesBackendPreviewUrl()
{
    DirectoryModel model;
    PreviewController controller(&model);
    DirectoryEntry entry = previewEntry(QStringLiteral("/fixture/photo.png"));
    entry.filePreviewUrl = QUrl(QStringLiteral("file:///cache/preview.png"));

    QCOMPARE(controller.previewUrl(entry, false), entry.filePreviewUrl);
}

void PreviewControllerTest::suppressesRemoteAndDirectoryPreviews()
{
    DirectoryModel model;
    PreviewController controller(&model);
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
    PreviewController controller(&model);
    const DirectoryEntry entry = previewEntry(QStringLiteral("/fixture/document.txt"));

    QVERIFY(controller.previewUrl(entry, false).isEmpty());
}

void PreviewControllerTest::appliesOnlyCurrentPreviewGeneration()
{
    DirectoryModel model;
    DirectoryEntry entry = previewEntry(QStringLiteral("/fixture/photo.png"));
    model.applyEntries({entry}, 7);
    PreviewController controller(&model);

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

QTEST_GUILESS_MAIN(PreviewControllerTest)

#include "tst_preview_controller.moc"
