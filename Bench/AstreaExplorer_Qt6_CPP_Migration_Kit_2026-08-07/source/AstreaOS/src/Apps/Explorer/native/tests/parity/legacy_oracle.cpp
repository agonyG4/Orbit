#include "parity/legacy_oracle.h"

#include <algorithm>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>

#include "parity/fixture_tree.h"

namespace Astrea::Explorer::Native::Parity {

namespace {

struct CommandResult
{
    int exitCode = -1;
    QByteArray stdoutData;
    QByteArray stderrData;
    QString error;
};

CommandResult runCommand(
    const QString &program,
    const QStringList &arguments,
    int timeoutMs = 10000)
{
    QProcess process;
    const QString canonicalProgram = program.contains(QLatin1Char('/'))
        ? QFileInfo(program).canonicalFilePath()
        : program;
    process.setProgram(canonicalProgram);
    process.setArguments(arguments);
    process.start();
    if (!process.waitForStarted(timeoutMs)) {
        return {-1, {}, {}, QStringLiteral("%1 (%2): %3")
                                   .arg(program, canonicalProgram, process.errorString())};
    }
    if (!process.waitForFinished(timeoutMs)) {
        process.kill();
        process.waitForFinished(1000);
        return {-1, process.readAllStandardOutput(), process.readAllStandardError(),
                QStringLiteral("command timed out")};
    }
    return {process.exitCode(), process.readAllStandardOutput(), process.readAllStandardError(), {}};
}

QString normalizePath(const QString &root, const QString &path)
{
    const QString relative = QDir(root).relativeFilePath(path);
    return relative == QStringLiteral(".")
        ? QStringLiteral("<root>")
        : QStringLiteral("<root>/%1").arg(relative);
}

EntrySnapshot normalizeEntry(
    const QString &root,
    const QJsonObject &object)
{
    const QString path = object.value(QStringLiteral("filePath")).toString();
    EntrySnapshot entry;
    entry.name = object.value(QStringLiteral("fileName")).toString();
    entry.relativePath = normalizePath(root, path);
    entry.isDirectory = object.value(QStringLiteral("fileIsDir")).toBool();
    entry.executable = object.value(QStringLiteral("fileExecutable")).toBool();
    entry.hidden = object.value(QStringLiteral("fileHidden")).toBool();
    entry.size = object.value(QStringLiteral("fileSize")).toInteger();
    entry.kind = object.value(QStringLiteral("fileKind")).toString();
    entry.hasPreview = !object.value(QStringLiteral("filePreviewUrl")).toString().isEmpty();
    return entry;
}

QVector<EntrySnapshot> normalizeArray(
    const QString &root,
    const QByteArray &payload)
{
    const QJsonDocument document = QJsonDocument::fromJson(payload);
    QVector<EntrySnapshot> entries;
    if (!document.isArray()) {
        return entries;
    }
    for (const QJsonValue &value : document.array()) {
        if (value.isObject()) {
            entries.append(normalizeEntry(root, value.toObject()));
        }
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

CommandResult runHelper(const QStringList &arguments)
{
    QStringList pythonArguments = {QString::fromUtf8(ASTREA_EXPLORER_PARITY_HELPER)};
    pythonArguments.append(arguments);
    return runCommand(QStringLiteral("python3"), pythonArguments);
}

QStringList recentsFromPayload(const QString &root, const QByteArray &payload)
{
    const QJsonDocument document = QJsonDocument::fromJson(payload);
    QVector<QPair<qint64, QString>> items;
    if (document.isArray()) {
        for (const QJsonValue &value : document.array()) {
            if (!value.isObject()) {
                continue;
            }
            const QJsonObject object = value.toObject();
            items.append({object.value(QStringLiteral("lastAccessed")).toInteger(),
                          normalizePath(root, object.value(QStringLiteral("filePath")).toString())});
        }
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

QStringList legacyFileOperations(const FixtureTree &fixture)
{
    const QString operations = QDir(fixture.operationsPath()).filePath(QStringLiteral("legacy"));
    const QString trashFiles = QDir(operations).filePath(QStringLiteral("trash/files"));
    const QString trashInfo = QDir(operations).filePath(QStringLiteral("trash/info"));
    QDir().mkpath(operations);
    QFile source(QDir(operations).filePath(QStringLiteral("source.txt")));
    if (!source.open(QIODevice::WriteOnly) || source.write("source") != 6) {
        return {QStringLiteral("setup:failed")};
    }
    source.close();

    const CommandResult rename = runCommand(
        QStringLiteral("python3"),
        {QString::fromUtf8(ASTREA_EXPLORER_PARITY_HELPER), QStringLiteral("rename"),
         source.fileName(), QStringLiteral("renamed.txt")});
    if (rename.exitCode != 0) {
        return {QStringLiteral("rename:failed")};
    }
    const QString renamed = QDir(operations).filePath(QStringLiteral("renamed.txt"));
    const CommandResult trash = runCommand(
        QStringLiteral("python3"),
        {QString::fromUtf8(ASTREA_EXPLORER_PARITY_HELPER), QStringLiteral("trash"), trashFiles,
         trashInfo, renamed});
    if (trash.exitCode != 0 || QFile::exists(renamed)
        || QDir(trashFiles).entryList(QDir::NoDotAndDotDot | QDir::AllEntries).isEmpty()) {
        return {QStringLiteral("rename:ok"), QStringLiteral("trash:failed")};
    }
    return {QStringLiteral("rename:ok"), QStringLiteral("trash:ok")};
}

} // namespace

ParitySnapshot LegacyOracle::capture(const FixtureTree &fixture) const
{
    ParitySnapshot snapshot;
    const QString backend = QString::fromUtf8(ASTREA_EXPLORER_PARITY_BACKEND);

    const CommandResult list = runCommand(
        backend,
        {QStringLiteral("list"), fixture.rootPath(), QStringLiteral("0"), QStringLiteral("name"),
         QStringLiteral("1"), QStringLiteral("1"), QStringLiteral("--preview-mode"),
         QStringLiteral("full")});
    if (!list.error.isEmpty() || list.exitCode != 0) {
        snapshot.error = QStringLiteral("legacy list: ")
            + (list.error.isEmpty() ? QString::fromUtf8(list.stderrData) : list.error);
        return snapshot;
    }
    snapshot.directoryEntries = normalizeArray(fixture.rootPath(), list.stdoutData);
    snapshot.previewStates = previewStates(snapshot.directoryEntries);

    const CommandResult search = runCommand(
        backend,
        {QStringLiteral("search"), fixture.rootPath(), QStringLiteral("search-target"),
         QStringLiteral("0"), QStringLiteral("name"), QStringLiteral("1"), QStringLiteral("1")});
    if (!search.error.isEmpty() || search.exitCode != 0) {
        snapshot.error = QStringLiteral("legacy search: ")
            + (search.error.isEmpty() ? QString::fromUtf8(search.stderrData) : search.error);
        return snapshot;
    }
    snapshot.searchEntries = normalizeArray(fixture.rootPath(), search.stdoutData);
    snapshot.searchQuery = QStringLiteral("search-target");
    snapshot.currentPath = QStringLiteral("<root>/Nested Folder");
    snapshot.history = {QStringLiteral("<root>"), QStringLiteral("<root>/Nested Folder")};
    snapshot.historyIndex = 1;

    for (const EntrySnapshot &entry : snapshot.directoryEntries) {
        if (entry.name == QStringLiteral("alpha.txt")
            || entry.name == QStringLiteral("space name.txt")) {
            snapshot.selectedFiles.append(entry.name);
        }
    }
    if (!snapshot.selectedFiles.isEmpty()) {
        snapshot.selectedFile = snapshot.selectedFiles.constLast();
    }

    const CommandResult device = runHelper(
        {QStringLiteral("network-mount-probe"), fixture.devicePath()});
    if (!device.error.isEmpty() || device.exitCode != 0) {
        snapshot.error = device.error.isEmpty() ? QString::fromUtf8(device.stderrData) : device.error;
        return snapshot;
    }
    snapshot.deviceState = normalizePath(
        fixture.rootPath(),
        QString::fromUtf8(device.stdoutData).trimmed());

    const QString finder = QDir(fixture.recentsPath()).filePath(QStringLiteral("finder.json"));
    const QString launch = QDir(fixture.recentsPath()).filePath(QStringLiteral("launch.json"));
    const QString xbel = QDir(fixture.recentsPath()).filePath(QStringLiteral("recently-used.xbel"));
    const CommandResult recents = runHelper(
        {QStringLiteral("merged-recents"), finder, launch, xbel, QStringLiteral("--limit"),
         QStringLiteral("60")});
    if (!recents.error.isEmpty() || recents.exitCode != 0) {
        snapshot.error = recents.error.isEmpty() ? QString::fromUtf8(recents.stderrData) : recents.error;
        return snapshot;
    }
    snapshot.recents = recentsFromPayload(fixture.rootPath(), recents.stdoutData);
    snapshot.fileOperationOutcomes = legacyFileOperations(fixture);
    return snapshot;
}

} // namespace Astrea::Explorer::Native::Parity
