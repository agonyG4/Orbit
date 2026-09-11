#include <QImage>
#include <QMimeData>
#include <QTemporaryDir>
#include <QtTest>

#include "services/clipboard_service.h"
#include "services/file_uri_list.h"

using namespace Astrea::Explorer::Native::Services;

class ClipboardServiceTest final : public QObject
{
    Q_OBJECT

private slots:
    void canonicalFileUriListPreservesEncodingAndOrder();
    void createsFileUriListMimeData();
    void createsPngImageMimeData();
    void rejectsImageWithoutEncodedBytes();
    void rejectsPublishWithoutClipboard();
};

void ClipboardServiceTest::canonicalFileUriListPreservesEncodingAndOrder()
{
    const QStringList paths {
        QStringLiteral("/tmp/space name.txt"),
        QStringLiteral("/tmp/hash#percent%.txt"),
        QStringLiteral("/tmp/unicode-测试.txt"),
        QString(),
    };
    const QByteArray expected = QByteArrayLiteral(
        "file:///tmp/space%20name.txt\r\n"
        "file:///tmp/hash%23percent%25.txt\r\n"
        "file:///tmp/unicode-%E6%B5%8B%E8%AF%95.txt\r\n");

    QCOMPARE(fileUriListData(paths), expected);
    QByteArray separators = fileUriListData(paths);
    separators.replace(QByteArrayLiteral("\r\n"), QByteArray());
    QVERIFY(!separators.contains('\n'));
}

void ClipboardServiceTest::createsFileUriListMimeData()
{
    ClipboardService service;
    const QStringList paths {
        QStringLiteral("/tmp/space name.txt"),
        QStringLiteral("/tmp/hash#percent%.txt"),
        QStringLiteral("/tmp/unicode-测试.txt"),
    };

    const std::unique_ptr<QMimeData> mime = service.fileMimeData(paths);
    QVERIFY(mime != nullptr);
    const QList<QUrl> expectedUrls {
        QUrl(QStringLiteral("file:///tmp/space%20name.txt")),
        QUrl(QStringLiteral("file:///tmp/hash%23percent%25.txt")),
        QUrl(QStringLiteral("file:///tmp/unicode-%E6%B5%8B%E8%AF%95.txt")),
    };
    QCOMPARE(mime->urls(), expectedUrls);
    QVERIFY(mime->formats().contains(QStringLiteral("text/uri-list")));
    QCOMPARE(
        mime->data(QStringLiteral("text/uri-list")),
        fileUriListData(paths));
}

void ClipboardServiceTest::createsPngImageMimeData()
{
    ClipboardService service;
    QImage image(2, 1, QImage::Format_RGBA8888);
    image.fill(QColor(12, 34, 56, 255));

    const std::unique_ptr<QMimeData> mime = service.imageMimeData(image);
    QVERIFY(mime != nullptr);
    QVERIFY(mime->formats().contains(QStringLiteral("image/png")));
    QVERIFY(!mime->data(QStringLiteral("image/png")).isEmpty());
    QVERIFY(mime->hasImage());
    QCOMPARE(mime->imageData().value<QImage>().size(), image.size());
}

void ClipboardServiceTest::rejectsImageWithoutEncodedBytes()
{
    ClipboardService service;
    const std::unique_ptr<QMimeData> mime = service.imageMimeData(QImage());
    QVERIFY(mime == nullptr);
}

void ClipboardServiceTest::rejectsPublishWithoutClipboard()
{
    ClipboardService service;
    QVERIFY(!service.publishFilePaths({QStringLiteral("/tmp/file.txt")}));
    QVERIFY(!service.publishImage(QImage(1, 1, QImage::Format_RGBA8888)));
}

QTEST_GUILESS_MAIN(ClipboardServiceTest)

#include "tst_clipboard_service.moc"
