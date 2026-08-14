#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

#include "services/mime_apps_service.h"

using Astrea::Explorer::Native::Services::MimeAppsService;

class MimeAppsServiceTest final : public QObject
{
    Q_OBJECT

private slots:
    void readsLiteralMimeKeysAndLists();
    void updatesDefaultAndAssociationsAtomically();
    void preservesUnrelatedAndMalformedContent();
    void emptyFileRoundTripsThroughFreshService();
};

void MimeAppsServiceTest::readsLiteralMimeKeysAndLists()
{
    QTemporaryDir fixture;
    QVERIFY(fixture.isValid());
    const QString path = fixture.filePath(QStringLiteral("mimeapps.list"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QVERIFY(file.write(QByteArrayLiteral(
        "[Default Applications]\n"
        "text/plain=org.example.Editor.desktop;org.example.Fallback.desktop;\n"
        "application/pdf=org.example.Pdf.desktop;\n"
        "\n"
        "[Added Associations]\n"
        "text/plain=org.example.Editor.desktop;\n")) > 0);
    file.close();

    MimeAppsService service(path);
    QCOMPARE(
        service.defaultsForMime(QStringLiteral("text/plain")),
        QStringList({QStringLiteral("org.example.Editor.desktop"),
                     QStringLiteral("org.example.Fallback.desktop")}));
    QCOMPARE(
        service.defaultsForMime(QStringLiteral("application/pdf")),
        QStringList({QStringLiteral("org.example.Pdf.desktop")}));
    QCOMPARE(
        service.associationsForMime(QStringLiteral("text/plain")),
        QStringList({QStringLiteral("org.example.Editor.desktop")}));
}

void MimeAppsServiceTest::updatesDefaultAndAssociationsAtomically()
{
    QTemporaryDir fixture;
    QVERIFY(fixture.isValid());
    const QString path = fixture.filePath(QStringLiteral("mimeapps.list"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QVERIFY(file.write(QByteArrayLiteral(
        "[Default Applications]\n"
        "text/plain=org.example.Fallback.desktop;org.example.Old.desktop;\n"
        "\n"
        "[Added Associations]\n"
        "text/plain=org.example.Old.desktop;\n")) > 0);
    file.close();

    MimeAppsService service(path);
    QVERIFY(service.setDefault(
        QStringLiteral("text/plain"),
        QStringLiteral("org.example.Editor")));

    QCOMPARE(
        service.defaultsForMime(QStringLiteral("text/plain")),
        QStringList({QStringLiteral("org.example.Editor.desktop"),
                     QStringLiteral("org.example.Fallback.desktop"),
                     QStringLiteral("org.example.Old.desktop")}));
    const QStringList associations = service.associationsForMime(QStringLiteral("text/plain"));
    QVERIFY(associations.contains(QStringLiteral("org.example.Editor.desktop")));
    QVERIFY(associations.contains(QStringLiteral("org.example.Old.desktop")));
    QVERIFY(!associations.contains(QStringLiteral("org.example.Editor")));

    MimeAppsService fresh(path);
    QCOMPARE(fresh.defaultsForMime(QStringLiteral("text/plain")).constFirst(),
             QStringLiteral("org.example.Editor.desktop"));
}

void MimeAppsServiceTest::preservesUnrelatedAndMalformedContent()
{
    QTemporaryDir fixture;
    QVERIFY(fixture.isValid());
    const QString path = fixture.filePath(QStringLiteral("mimeapps.list"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QVERIFY(file.write(QByteArrayLiteral(
        "# keep this comment\n"
        "malformed line\n"
        "[Default Applications]\n"
        "text/plain=org.example.Old.desktop;;\n"
        "not-an-assignment\n"
        "[Unrelated]\n"
        "answer=42\n")) > 0);
    file.close();

    MimeAppsService service(path);
    QVERIFY(service.setDefault(
        QStringLiteral("application/pdf"),
        QStringLiteral("org.example.Pdf.desktop")));

    QVERIFY(file.open(QIODevice::ReadOnly));
    const QString contents = QString::fromUtf8(file.readAll());
    QVERIFY(contents.contains(QStringLiteral("# keep this comment")));
    QVERIFY(contents.contains(QStringLiteral("malformed line")));
    QVERIFY(contents.contains(QStringLiteral("[Unrelated]")));
    QVERIFY(contents.contains(QStringLiteral("answer=42")));
    QCOMPARE(
        service.defaultsForMime(QStringLiteral("application/pdf")),
        QStringList({QStringLiteral("org.example.Pdf.desktop")}));
}

void MimeAppsServiceTest::emptyFileRoundTripsThroughFreshService()
{
    QTemporaryDir fixture;
    QVERIFY(fixture.isValid());
    const QString path = fixture.filePath(QStringLiteral("mimeapps.list"));
    MimeAppsService service(path);
    QVERIFY(service.setDefault(
        QStringLiteral("text/plain"),
        QStringLiteral("org.example.Editor.desktop")));

    MimeAppsService fresh(path);
    QCOMPARE(
        fresh.defaultsForMime(QStringLiteral("text/plain")),
        QStringList({QStringLiteral("org.example.Editor.desktop")}));
    QVERIFY(fresh.associationsForMime(QStringLiteral("text/plain"))
                .contains(QStringLiteral("org.example.Editor.desktop")));
}

QTEST_GUILESS_MAIN(MimeAppsServiceTest)

#include "tst_mime_apps_service.moc"
