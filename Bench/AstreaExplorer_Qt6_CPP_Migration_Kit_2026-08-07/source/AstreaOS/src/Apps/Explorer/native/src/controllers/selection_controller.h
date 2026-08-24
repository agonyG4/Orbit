#pragma once

#include <QStringList>

#include "models/directory_model.h"

namespace Astrea::Explorer::Native::Backend {

class SelectionController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString selectedFile READ selectedFile NOTIFY selectedFileChanged)
    Q_PROPERTY(QStringList selectedFiles READ selectedFiles NOTIFY selectedFilesChanged)
    Q_PROPERTY(QStringList selectedPaths READ selectedPaths NOTIFY selectedPathsChanged)
    Q_PROPERTY(int lastSelectedIndex READ lastSelectedIndex NOTIFY lastSelectedIndexChanged)

public:
    explicit SelectionController(DirectoryModel *model, QObject *parent = nullptr);

    QString selectedFile() const;
    QStringList selectedFiles() const;
    QStringList selectedPaths() const;
    int lastSelectedIndex() const;

    Q_INVOKABLE bool isSelected(const QString &name) const;
    Q_INVOKABLE bool isPathSelected(const QString &path) const;
    Q_INVOKABLE void clearSelection();
    Q_INVOKABLE void removePaths(const QStringList &paths);
    Q_INVOKABLE void selectAll();
    Q_INVOKABLE void selectByName(const QString &name);
    Q_INVOKABLE void selectByPath(const QString &path);
    Q_INVOKABLE void prepareSelectionForDrag(const QString &name, int index);
    Q_INVOKABLE void handleSelection(
        const QString &name,
        int index,
        bool ctrlMode,
        bool shiftMode,
        bool preserveCurrentSelection);

signals:
    void selectedFileChanged();
    void selectedFilesChanged();
    void selectedPathsChanged();
    void lastSelectedIndexChanged();
    void selectionChanged();

private slots:
    void reconcileAfterModelChange();

private:
    QString pathAt(int index) const;
    QString nameAt(int index) const;
    int indexForPath(const QString &path) const;
    int selectedPathIndex(const QString &path, const QString &name) const;
    void applySelection(
        const QStringList &paths,
        const QStringList &names,
        const QString &selectedPath,
        const QString &selectedName,
        const QString &anchorPath,
        int anchorIndex);

    DirectoryModel *m_model = nullptr;
    QString m_selectedFile;
    QStringList m_selectedFiles;
    int m_lastSelectedIndex = -1;
    QStringList m_selectedPaths;
    QString m_selectedPath;
    QString m_anchorPath;
};

} // namespace Astrea::Explorer::Native::Backend
