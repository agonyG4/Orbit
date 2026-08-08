#include <QCoreApplication>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTimer>
#include <QTimeZone>
#include <QtTest>

#include "backend/backend_transport.h"
#include "backend/backend_types.h"
#include "backend/fake_backend_client.h"
#include "backend/one_shot_cli_transport.h"
#include "backend/rust_backend_client.h"

using namespace Astrea::Explorer::Native::Backend;

class InMemoryTransport final : public BackendTransport
{
    Q_OBJECT

public:
    struct StartedRequest
    {
        BackendRequestId id {};
        QStringList arguments;
    };

    BackendRequestId start(const QStringList &arguments) override
    {
        const BackendRequestId id = allocateRequestId();
        startedRequests.append({id, arguments});
        return id;
    }

    void cancel(BackendRequestId requestId) override
    {
        cancelledRequests.append(requestId);
    }

    void succeed(BackendRequestId requestId, const QByteArray &payload)
    {
        emitCompleted(requestId, payload);
    }

    void fail(BackendRequestId requestId, const QString &code, const QString &message)
    {
        BackendTransportError error;
        error.requestId = requestId;
        error.code = code;
        error.message = message;
        emitFailed(requestId, error);
    }

    QVector<StartedRequest> startedRequests;
    QVector<BackendRequestId> cancelledRequests;
};

class BackendClientTest final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void decodesRoleCompatibleListPayload();
    void rejectsMalformedJsonPayload();
    void forwardsLegacyCliArgumentsForListAndSearch();
    void ignoresDuplicateTerminalEvents();
    void forwardsTransportFailuresExactlyOnce();
    void fakeBackendClientEmitsTypedSignalsWithoutProcessDependencies();
    void oneShotTransportMapsNonZeroExitToFailure();
    void oneShotTransportCancelsExactlyOnce();
    void oneShotTransportCapsOutput();
};

void BackendClientTest::initTestCase()
{
    qRegisterMetaType<BackendError>();
    qRegisterMetaType<QVector<DirectoryEntry>>();
}

void BackendClientTest::decodesRoleCompatibleListPayload()
{
    InMemoryTransport transport;
    RustBackendClient client(&transport);

    QSignalSpy readySpy(&client, &IRustBackendClient::listReady);
    QSignalSpy failedSpy(&client, &IRustBackendClient::failed);

    ListRequest request;
    request.path = QStringLiteral("/tmp/example");
    request.showHidden = true;
    request.sortField = QStringLiteral("date");
    request.sortAscending = false;
    request.foldersFirst = true;
    request.previews = true;

    const BackendRequestId requestId = client.list(request);
    QCOMPARE(requestId, BackendRequestId(1));
    QCOMPARE(transport.startedRequests.size(), 1);

    transport.succeed(
        requestId,
        QByteArrayLiteral(
            "[{\"fileName\":\"photo #1.png\",\"filePath\":\"/tmp/example/photo #1.png\","
            "\"fileUrl\":\"file:///tmp/example/photo%20%231.png\",\"fileIsDir\":false,"
            "\"fileExecutable\":false,\"fileHidden\":false,\"fileSize\":42,"
            "\"fileModified\":1723265945000,\"fileKind\":\"PNG\","
            "\"filePreviewUrl\":\"file:///tmp/cache/photo.png\",\"fileRemote\":false,"
            "\"fileMetadataLimited\":false,\"fileFilesystem\":\"ext4\"}]"));

    QTRY_COMPARE(readySpy.count(), 1);
    QCOMPARE(failedSpy.count(), 0);

    const QList<QVariant> signalArguments = readySpy.takeFirst();
    QCOMPARE(signalArguments.at(0).value<BackendRequestId>(), requestId);

    const QVector<DirectoryEntry> entries =
        signalArguments.at(1).value<QVector<DirectoryEntry>>();
    QCOMPARE(entries.size(), 1);
    QCOMPARE(entries.constFirst().fileName, QStringLiteral("photo #1.png"));
    QCOMPARE(entries.constFirst().filePath, QStringLiteral("/tmp/example/photo #1.png"));
    QCOMPARE(entries.constFirst().fileUrl, QUrl(QStringLiteral("file:///tmp/example/photo%20%231.png")));
    QCOMPARE(entries.constFirst().fileIsDir, false);
    QCOMPARE(entries.constFirst().fileExecutable, false);
    QCOMPARE(entries.constFirst().fileHidden, false);
    QCOMPARE(entries.constFirst().fileSize, 42);
    QCOMPARE(
        entries.constFirst().fileModified,
        QDateTime::fromMSecsSinceEpoch(1723265945000, QTimeZone::UTC));
    QCOMPARE(entries.constFirst().fileKind, QStringLiteral("PNG"));
    QCOMPARE(entries.constFirst().filePreviewUrl, QUrl(QStringLiteral("file:///tmp/cache/photo.png")));
    QCOMPARE(entries.constFirst().fileRemote, false);
    QCOMPARE(entries.constFirst().fileMetadataLimited, false);
    QCOMPARE(entries.constFirst().fileFilesystem, QStringLiteral("ext4"));
}

void BackendClientTest::rejectsMalformedJsonPayload()
{
    InMemoryTransport transport;
    RustBackendClient client(&transport);

    QSignalSpy failedSpy(&client, &IRustBackendClient::failed);

    ListRequest request;
    request.path = QStringLiteral("/tmp/example");
    request.sortField = QStringLiteral("name");
    request.sortAscending = true;
    request.foldersFirst = true;
    request.previews = true;

    const BackendRequestId requestId = client.list(request);
    transport.succeed(requestId, QByteArrayLiteral("{not-json"));

    QTRY_COMPARE(failedSpy.count(), 1);
    const BackendError error = failedSpy.takeFirst().constFirst().value<BackendError>();
    QCOMPARE(error.requestId, requestId);
    QCOMPARE(error.code, QStringLiteral("decode_error"));
    QVERIFY(error.message.contains(QStringLiteral("JSON"), Qt::CaseInsensitive));
}

void BackendClientTest::forwardsLegacyCliArgumentsForListAndSearch()
{
    InMemoryTransport transport;
    RustBackendClient client(&transport);

    ListRequest listRequest;
    listRequest.path = QStringLiteral("/tmp/list path");
    listRequest.showHidden = true;
    listRequest.sortField = QStringLiteral("kind");
    listRequest.sortAscending = false;
    listRequest.foldersFirst = false;
    listRequest.previews = false;

    SearchRequest searchRequest;
    searchRequest.rootPath = QStringLiteral("/tmp/search root");
    searchRequest.query = QStringLiteral("Needle");
    searchRequest.showHidden = true;
    searchRequest.sortField = QStringLiteral("date");
    searchRequest.sortAscending = false;
    searchRequest.foldersFirst = true;

    const BackendRequestId listId = client.list(listRequest);
    const BackendRequestId searchId = client.search(searchRequest);

    QCOMPARE(listId, BackendRequestId(1));
    QCOMPARE(searchId, BackendRequestId(2));
    QCOMPARE(transport.startedRequests.size(), 2);
    QCOMPARE(
        transport.startedRequests.at(0).arguments,
        QStringList(
            {QStringLiteral("list"),
             QStringLiteral("/tmp/list path"),
             QStringLiteral("1"),
             QStringLiteral("kind"),
             QStringLiteral("0"),
             QStringLiteral("0"),
             QStringLiteral("--preview-mode"),
             QStringLiteral("none")}));
    QCOMPARE(
        transport.startedRequests.at(1).arguments,
        QStringList(
            {QStringLiteral("search"),
             QStringLiteral("/tmp/search root"),
             QStringLiteral("Needle"),
             QStringLiteral("1"),
             QStringLiteral("date"),
             QStringLiteral("0"),
             QStringLiteral("1")}));
}

void BackendClientTest::ignoresDuplicateTerminalEvents()
{
    InMemoryTransport transport;
    RustBackendClient client(&transport);

    QSignalSpy readySpy(&client, &IRustBackendClient::listReady);
    QSignalSpy failedSpy(&client, &IRustBackendClient::failed);

    ListRequest request;
    request.path = QStringLiteral("/tmp/example");
    request.sortField = QStringLiteral("name");
    request.sortAscending = true;
    request.foldersFirst = true;
    request.previews = true;

    const BackendRequestId requestId = client.list(request);
    transport.succeed(requestId, QByteArrayLiteral("[]"));
    transport.fail(requestId, QStringLiteral("backend_exit"), QStringLiteral("late failure"));
    transport.succeed(requestId, QByteArrayLiteral("[{\"fileName\":\"ignored\"}]"));

    QTRY_COMPARE(readySpy.count(), 1);
    QCOMPARE(failedSpy.count(), 0);
}

void BackendClientTest::forwardsTransportFailuresExactlyOnce()
{
    InMemoryTransport transport;
    RustBackendClient client(&transport);

    QSignalSpy failedSpy(&client, &IRustBackendClient::failed);

    SearchRequest request;
    request.rootPath = QStringLiteral("/tmp/example");
    request.query = QStringLiteral("needle");
    request.showHidden = false;
    request.sortField = QStringLiteral("name");
    request.sortAscending = true;
    request.foldersFirst = true;

    const BackendRequestId requestId = client.search(request);
    transport.fail(requestId, QStringLiteral("backend_exit"), QStringLiteral("exit code 7"));
    transport.fail(requestId, QStringLiteral("backend_exit"), QStringLiteral("duplicate"));

    QTRY_COMPARE(failedSpy.count(), 1);
    const BackendError error = failedSpy.takeFirst().constFirst().value<BackendError>();
    QCOMPARE(error.requestId, requestId);
    QCOMPARE(error.code, QStringLiteral("backend_exit"));
    QCOMPARE(error.message, QStringLiteral("exit code 7"));
}

void BackendClientTest::fakeBackendClientEmitsTypedSignalsWithoutProcessDependencies()
{
    FakeRustBackendClient client;
    QSignalSpy readySpy(&client, &IRustBackendClient::listReady);

    ListRequest request;
    request.path = QStringLiteral("/tmp/example");
    request.sortField = QStringLiteral("name");
    request.sortAscending = true;
    request.foldersFirst = true;
    request.previews = true;

    const BackendRequestId requestId = client.list(request);

    DirectoryEntry entry;
    entry.fileName = QStringLiteral("typed.txt");
    entry.filePath = QStringLiteral("/tmp/example/typed.txt");
    entry.fileUrl = QUrl(QStringLiteral("file:///tmp/example/typed.txt"));
    entry.fileKind = QStringLiteral("TXT");

    client.completeList(requestId, {entry});

    QTRY_COMPARE(readySpy.count(), 1);
    const QList<QVariant> signalArguments = readySpy.takeFirst();
    QCOMPARE(signalArguments.at(0).value<BackendRequestId>(), requestId);
    const QVector<DirectoryEntry> entries =
        signalArguments.at(1).value<QVector<DirectoryEntry>>();
    QCOMPARE(entries.size(), 1);
    QCOMPARE(entries.constFirst().fileName, QStringLiteral("typed.txt"));
}

void BackendClientTest::oneShotTransportMapsNonZeroExitToFailure()
{
    const QString python = QStandardPaths::findExecutable(QStringLiteral("python3"));
    QVERIFY2(!python.isEmpty(), "python3 must be available for transport tests");

    OneShotCliTransportOptions options;
    options.backendProgram = python;
    options.timeoutMs = 3000;
    options.maxStdoutBytes = 4096;
    options.maxStderrBytes = 4096;
    OneShotCliTransport transport(options);

    QSignalSpy completedSpy(&transport, &BackendTransport::completed);
    QSignalSpy failedSpy(&transport, &BackendTransport::failed);

    const BackendRequestId requestId = transport.start(
        {QStringLiteral("-c"),
         QStringLiteral("import sys; sys.stderr.write('boom\\n'); sys.exit(7)")});

    QTRY_COMPARE(failedSpy.count(), 1);
    QCOMPARE(completedSpy.count(), 0);

    const QList<QVariant> signalArguments = failedSpy.takeFirst();
    QCOMPARE(signalArguments.at(0).value<BackendRequestId>(), requestId);
    const BackendTransportError error =
        signalArguments.at(1).value<BackendTransportError>();
    QCOMPARE(error.requestId, requestId);
    QCOMPARE(error.code, QStringLiteral("backend_exit"));
    QVERIFY(error.message.contains(QStringLiteral("7")));
    QVERIFY(error.message.contains(QStringLiteral("boom")));
}

void BackendClientTest::oneShotTransportCancelsExactlyOnce()
{
    const QString python = QStandardPaths::findExecutable(QStringLiteral("python3"));
    QVERIFY2(!python.isEmpty(), "python3 must be available for transport tests");

    OneShotCliTransportOptions options;
    options.backendProgram = python;
    options.timeoutMs = 5000;
    options.maxStdoutBytes = 4096;
    options.maxStderrBytes = 4096;
    OneShotCliTransport transport(options);

    QSignalSpy completedSpy(&transport, &BackendTransport::completed);
    QSignalSpy failedSpy(&transport, &BackendTransport::failed);

    const BackendRequestId requestId = transport.start(
        {QStringLiteral("-c"),
         QStringLiteral("import time; time.sleep(10)")});
    transport.cancel(requestId);

    QTRY_COMPARE(failedSpy.count(), 1);
    QCOMPARE(completedSpy.count(), 0);

    const BackendTransportError error =
        failedSpy.takeFirst().at(1).value<BackendTransportError>();
    QCOMPARE(error.requestId, requestId);
    QCOMPARE(error.code, QStringLiteral("cancelled"));
}

void BackendClientTest::oneShotTransportCapsOutput()
{
    const QString python = QStandardPaths::findExecutable(QStringLiteral("python3"));
    QVERIFY2(!python.isEmpty(), "python3 must be available for transport tests");

    OneShotCliTransportOptions options;
    options.backendProgram = python;
    options.timeoutMs = 3000;
    options.maxStdoutBytes = 8;
    options.maxStderrBytes = 1024;
    OneShotCliTransport transport(options);

    QSignalSpy completedSpy(&transport, &BackendTransport::completed);
    QSignalSpy failedSpy(&transport, &BackendTransport::failed);

    const BackendRequestId requestId = transport.start(
        {QStringLiteral("-c"),
         QStringLiteral("print('0123456789abcdef')")});

    QTRY_COMPARE(failedSpy.count(), 1);
    QCOMPARE(completedSpy.count(), 0);

    const BackendTransportError error =
        failedSpy.takeFirst().at(1).value<BackendTransportError>();
    QCOMPARE(error.requestId, requestId);
    QCOMPARE(error.code, QStringLiteral("output_limit_exceeded"));
}

QTEST_MAIN(BackendClientTest)

#include "tst_backend_client.moc"
