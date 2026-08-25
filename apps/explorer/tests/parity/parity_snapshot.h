#pragma once

#include <QStringList>
#include <QVector>

namespace Astrea::Explorer::Native::Parity {

struct EntrySnapshot
{
    QString name;
    QString relativePath;
    bool isDirectory = false;
    bool executable = false;
    bool hidden = false;
    qint64 size = 0;
    QString kind;
    bool hasPreview = false;
};

bool operator==(const EntrySnapshot &left, const EntrySnapshot &right);

struct ParitySnapshot
{
    QString error;
    QVector<EntrySnapshot> directoryEntries;
    QVector<EntrySnapshot> searchEntries;
    QString currentPath;
    QStringList history;
    int historyIndex = -1;
    QString searchQuery;
    QString selectedFile;
    QStringList selectedFiles;
    QStringList previewStates;
    QString deviceState;
    QStringList recents;
    QStringList fileOperationOutcomes;

    QString diff(const ParitySnapshot &other) const;
};

} // namespace Astrea::Explorer::Native::Parity
