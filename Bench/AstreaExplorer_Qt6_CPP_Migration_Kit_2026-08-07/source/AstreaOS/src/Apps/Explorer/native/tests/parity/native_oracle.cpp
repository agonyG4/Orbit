#include "parity/native_oracle.h"

#include <algorithm>

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "backend/one_shot_cli_transport.h"
#include "backend/rust_backend_client.h"
#include "controllers/navigation_controller.h"
#include "controllers/selection_controller.h"
#include "models/directory_model.h"
#include "parity/fixture_tree.h"
#include "services/directory_watch_service.h"

namespace Astrea::Explorer::Native::Parity {

namespace {

using namespace Astrea::Explorer::Native::Backend;

QString normalizePath(const QString &root, const QString &path)
{
    const QString relative = QDir(root).relativeFilePath(path);
    return relative == QStringLiteral(".")
        ? QStringLiteral("<root>")
        : QStringLiteral("<root>/%1").arg(relative);
}

EntrySnapshot normalizeEntry(
    const QString &root,
    const DirectoryEntry &entry)
{
    EntrySnapshot snapshot;
    snapshot.name = entry.fileName;
    snapshot.relativePath = normalizePath(root, entry.filePath);
    snapshot.isDirectory = entry.fileIsDir;
    snapshot.executable = entry.fileExecutable;
    snapshot.hidden = entry.fileHidden;
    snapshot.size = entry.fileSize;
    snapshot.kind = entry.fileKind;
    snapshot.hasPreview = !entry.filePreviewUrl.isEmpty();
    return snapshot;
}

QVector<EntrySnapshot> normalizeModel(
    const QString &root,
    const DirectoryModel &model)
{
    QVector<EntrySnapshot> entries;
    entries.reserve(model.rowCount());
    for (int row = 0; row < model.rowCount(); ++row) {
        const QModelIndex index = model.index(row, 0);
        DirectoryEntry entry;
        entry.fileName = model.data(index, DirectoryModel::FileNameRole).toString();
        entry.filePath = model.data(index, DirectoryModel::FilePathRole).toString();
        entry.fileIsDir = model.data(index, DirectoryModel::FileIsDirRole).toBool();
        entry.fileExecutable = model.data(index, DirectoryModel::FileExecutableRole).toBool();
        entry.fileHidden = model.data(index, DirectoryModel::FileHiddenRole).toBool();
        entry.fileSize = model.data(index, DirectoryModel::FileSizeRole).toLongLong();
        entry.fileKind = model.data(index, DirectoryModel::FileKindRole).toString();
        entry.filePreviewUrl = model.data(index, DirectoryModel::FilePreviewUrlRole).toUrl();
        entries.append(normalizeEntry(root, entry));
    }
    return entries;
}

QStringList previewStates(const QVector<EntrySnapshot> &entries)
{
    QStringList states;
    for (const EntrySnapshot &entry : entries) {
        states.append(QStringLiteral("%1:%2")
                          .arg(entry.relativePath, entry.hasPreview ? QStringLiteral("1")
                                                                      : QStringLiteral("0")));
    }
    return states;
}

bool waitForNavigation(NavigationController &navigation, int timeoutMs, QString *error)
{
    QElapsedTimer timer;
    timer.start();
    while (navigation.loading() && timer.elapsed() < timeoutMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 25);
    }
    if (navigation.loading()) {
        if (error != nullptr) {
            *error = QStringLiteral("native navigation timed out");
        }
        return false;
    }
    if (!navigation.loadError().isEmpty()) {
        if (error != nullptr) {
            *error = navigation.loadError();
        }
        return false;
    }
    return true;
}

class NativeSession final
{
public:
    NativeSession()
        : transport(options(), nullptr)
        , client(&transport)
        , model()
        , watcher()
        , navigation(&client, &model, &watcher)
        , selection(&model)
    {
    }

    static OneShotCliTransportOptions options()
    {
        OneShotCliTransportOptions result;
        result.backendProgram = QFileInfo(
                                   QString::fromUtf8(ASTREA_EXPLORER_PARITY_BACKEND))
                                   .canonicalFilePath();
        result.timeoutMs = 5000;
        result.maxStdoutBytes = 4 * 1024 * 1024;
        result.maxStderrBytes = 256 * 1024;
        return result;
    }

    OneShotCliTransport transport;
    RustBackendClient client;
    DirectoryModel model;
    DirectoryWatchService watcher;
    NavigationController navigation;
    SelectionController selection;
};

QStringList nativeRecents(const FixtureTree &fixture)
{
    QFile file(QDir(fixture.recentsPath()).filePath(QStringLiteral("finder.json")));
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    QVector<QPair<qint64, QString>> items;
    for (const QJsonValue &value : document.array()) {
        const QJsonObject object = value.toObject();
        items.append({object.value(QStringLiteral("lastAccessed")).toInteger(),
                      normalizePath(fixture.rootPath(),
                                    object.value(QStringLiteral("filePath")).toString())});
    }
    std::sort(items.begin(), items.end(), [](const auto &left, const auto &right) {
        return left.first > right.first;
    });
    QStringList result;
    for (const auto &item : items) {
        result.append(item.second);
    }
    return result;
}

QStringList nativeFileOperations(const FixtureTree &fixture)
{
    const QString operations = QDir(fixture.operationsPath()).filePath(QStringLiteral("native"));
    const QString trashFiles = QDir(operations).filePath(QStringLiteral("trash/files"));
    const QString trashInfo = QDir(operations).filePath(QStringLiteral("trash/info"));
    QDir().mkpath(operations);
    QFile source(QDir(operations).filePath(QStringLiteral("source.txt")));
    if (!source.open(QIODevice::WriteOnly) || source.write("source") != 6) {
        return {QStringLiteral("setup:failed")};
    }
    source.close();

    const QString renamed = QDir(operations).filePath(QStringLiteral("renamed.txt"));
    if (!QFile::rename(source.fileName(), renamed)) {
        return {QStringLiteral("rename:failed")};
    }
    QDir().mkpath(trashFiles);
    QDir().mkpath(trashInfo);
    const QString trashed = QDir(trashFiles).filePath(QStringLiteral("renamed.txt"));
    if (!QFile::rename(renamed, trashed) || QFile::exists(renamed)
        || !QFile::exists(trashed)) {
        return {QStringLiteral("rename:ok"), QStringLiteral("trash:failed")};
    }
    return {QStringLiteral("rename:ok"), QStringLiteral("trash:ok")};
}

QString nativeDeviceState(const FixtureTree &fixture, QString *error)
{
    NativeSession session;
    const BackendRequestId requestId = session.navigation.navigateTo(fixture.devicePath());
    if (requestId == 0 || !waitForNavigation(session.navigation, 5000, error)) {
        return {};
    }
    if (session.model.rowCount() == 0) {
        if (error != nullptr) {
            *error = QStringLiteral("native device fixture was empty");
        }
        return {};
    }
    return normalizePath(
        fixture.rootPath(),
        session.model.data(
            session.model.index(0, 0),
            DirectoryModel::FilePathRole)
            .toString());
}

} // namespace

ParitySnapshot NativeOracle::capture(const FixtureTree &fixture) const
{
    ParitySnapshot snapshot;
    NativeSession session;
    QString error;

    const BackendRequestId rootRequest = session.navigation.navigateTo(fixture.rootPath());
    if (rootRequest == 0 || !waitForNavigation(session.navigation, 5000, &error)) {
        snapshot.error = QStringLiteral("native root: ") + error;
        return snapshot;
    }

    const QVector<EntrySnapshot> rootEntries = normalizeModel(fixture.rootPath(), session.model);
    snapshot.directoryEntries = rootEntries;
    snapshot.previewStates = previewStates(rootEntries);
    session.selection.selectByName(QStringLiteral("alpha.txt"));
    int spaceIndex = -1;
    for (int row = 0; row < session.model.rowCount(); ++row) {
        if (session.model
                .data(session.model.index(row, 0), DirectoryModel::FileNameRole)
                .toString()
            == QStringLiteral("space name.txt")) {
            spaceIndex = row;
            break;
        }
    }
    session.selection.handleSelection(
        QStringLiteral("space name.txt"),
        spaceIndex,
        true,
        false,
        false);
    snapshot.selectedFile = session.selection.selectedFile();
    snapshot.selectedFiles = session.selection.selectedFiles();

    const BackendRequestId nestedRequest = session.navigation.navigateTo(fixture.nestedPath());
    if (nestedRequest == 0 || !waitForNavigation(session.navigation, 5000, &error)) {
        snapshot.error = QStringLiteral("native nested: ") + error;
        return snapshot;
    }
    snapshot.currentPath = normalizePath(
        fixture.rootPath(),
        session.navigation.currentPath());
    snapshot.history = {
        normalizePath(fixture.rootPath(), fixture.rootPath()),
        normalizePath(fixture.rootPath(), fixture.nestedPath()),
    };
    snapshot.historyIndex = session.navigation.historyIndex();

    const BackendRequestId searchRequest = session.navigation.submitSearch(
        fixture.rootPath(),
        QStringLiteral("search-target"));
    if (searchRequest == 0 || !waitForNavigation(session.navigation, 5000, &error)) {
        snapshot.error = QStringLiteral("native search: ") + error;
        return snapshot;
    }
    snapshot.searchEntries = normalizeModel(fixture.rootPath(), session.model);
    snapshot.searchQuery = session.navigation.searchQuery();

    snapshot.deviceState = nativeDeviceState(fixture, &error);
    if (!error.isEmpty()) {
        snapshot.error = error;
        return snapshot;
    }
    snapshot.recents = nativeRecents(fixture);
    snapshot.fileOperationOutcomes = nativeFileOperations(fixture);
    return snapshot;
}

} // namespace Astrea::Explorer::Native::Parity
