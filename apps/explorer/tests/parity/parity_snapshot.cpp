#include "parity/parity_snapshot.h"

namespace Astrea::Explorer::Native::Parity {

namespace {

QString describeEntry(const EntrySnapshot &entry)
{
    return QStringLiteral("%1|%2|dir=%3|exec=%4|hidden=%5|size=%6|kind=%7|preview=%8")
        .arg(entry.name, entry.relativePath)
        .arg(entry.isDirectory)
        .arg(entry.executable)
        .arg(entry.hidden)
        .arg(entry.size)
        .arg(entry.kind)
        .arg(entry.hasPreview);
}

QString compareEntries(
    const QVector<EntrySnapshot> &left,
    const QVector<EntrySnapshot> &right,
    const QString &label)
{
    if (left.size() != right.size()) {
        return QStringLiteral("%1 size mismatch: %2 != %3")
            .arg(label)
            .arg(left.size())
            .arg(right.size());
    }
    for (int i = 0; i < left.size(); ++i) {
        if (!(left.at(i) == right.at(i))) {
            return QStringLiteral("%1[%2] mismatch: %3 != %4")
                .arg(label)
                .arg(i)
                .arg(describeEntry(left.at(i)), describeEntry(right.at(i)));
        }
    }
    return {};
}

} // namespace

bool operator==(const EntrySnapshot &left, const EntrySnapshot &right)
{
    return left.name == right.name && left.relativePath == right.relativePath
        && left.isDirectory == right.isDirectory && left.executable == right.executable
        && left.hidden == right.hidden && left.size == right.size
        && left.kind == right.kind && left.hasPreview == right.hasPreview;
}

QString ParitySnapshot::diff(const ParitySnapshot &other) const
{
    if (error != other.error) {
        return QStringLiteral("oracle error mismatch: %1 != %2").arg(error, other.error);
    }
    const QString directoryDifference = compareEntries(
        directoryEntries,
        other.directoryEntries,
        QStringLiteral("directoryEntries"));
    if (!directoryDifference.isEmpty()) {
        return directoryDifference;
    }
    const QString searchDifference = compareEntries(
        searchEntries,
        other.searchEntries,
        QStringLiteral("searchEntries"));
    if (!searchDifference.isEmpty()) {
        return searchDifference;
    }
    if (currentPath != other.currentPath) {
        return QStringLiteral("currentPath mismatch: %1 != %2")
            .arg(currentPath, other.currentPath);
    }
    if (history != other.history || historyIndex != other.historyIndex) {
        return QStringLiteral("navigation history mismatch");
    }
    if (searchQuery != other.searchQuery) {
        return QStringLiteral("searchQuery mismatch");
    }
    if (selectedFile != other.selectedFile || selectedFiles != other.selectedFiles) {
        return QStringLiteral("selection mismatch");
    }
    if (previewStates != other.previewStates) {
        return QStringLiteral("previewStates mismatch");
    }
    if (deviceState != other.deviceState) {
        return QStringLiteral("deviceState mismatch");
    }
    if (recents != other.recents) {
        return QStringLiteral("recents mismatch");
    }
    if (fileOperationOutcomes != other.fileOperationOutcomes) {
        return QStringLiteral("fileOperationOutcomes mismatch");
    }
    return {};
}

} // namespace Astrea::Explorer::Native::Parity
