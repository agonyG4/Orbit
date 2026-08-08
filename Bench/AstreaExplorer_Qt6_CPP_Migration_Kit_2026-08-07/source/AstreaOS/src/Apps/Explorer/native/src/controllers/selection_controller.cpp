#include "controllers/selection_controller.h"

#include <utility>

namespace Astrea::Explorer::Native::Backend {

SelectionController::SelectionController(DirectoryModel *model, QObject *parent)
    : QObject(parent)
    , m_model(model)
{
    Q_ASSERT(m_model != nullptr);
    connect(
        m_model,
        &QAbstractItemModel::modelReset,
        this,
        &SelectionController::reconcileAfterModelChange);
    connect(
        m_model,
        &QAbstractItemModel::rowsRemoved,
        this,
        &SelectionController::reconcileAfterModelChange);
    connect(
        m_model,
        &QAbstractItemModel::dataChanged,
        this,
        &SelectionController::reconcileAfterModelChange);
}

QString SelectionController::selectedFile() const
{
    return m_selectedFile;
}

QStringList SelectionController::selectedFiles() const
{
    return m_selectedFiles;
}

int SelectionController::lastSelectedIndex() const
{
    return m_lastSelectedIndex;
}

bool SelectionController::isSelected(const QString &name) const
{
    return !name.isEmpty() && m_selectedFiles.contains(name);
}

void SelectionController::clearSelection()
{
    applySelection({}, {}, {}, {}, {}, -1);
}

void SelectionController::selectAll()
{
    QStringList paths;
    QStringList names;
    paths.reserve(m_model->rowCount());
    names.reserve(m_model->rowCount());
    for (int row = 0; row < m_model->rowCount(); ++row) {
        paths.append(pathAt(row));
        names.append(nameAt(row));
    }

    const QString selectedPath = paths.isEmpty() ? QString() : paths.constLast();
    const QString selectedName = names.isEmpty() ? QString() : names.constLast();
    applySelection(
        paths,
        names,
        selectedPath,
        selectedName,
        m_anchorPath,
        m_lastSelectedIndex);
}

void SelectionController::selectByName(const QString &name)
{
    if (name.isEmpty()) {
        return;
    }

    const int index = [&]() {
        for (int row = 0; row < m_model->rowCount(); ++row) {
            if (nameAt(row) == name) {
                return row;
            }
        }
        return -1;
    }();
    const QString path = pathAt(index);
    applySelection({path}, {name}, path, name, path, index);
}

void SelectionController::handleSelection(
    const QString &name,
    int index,
    bool ctrlMode,
    bool shiftMode,
    bool preserveCurrentSelection)
{
    if (name.isEmpty()) {
        return;
    }
    if (preserveCurrentSelection && isSelected(name)) {
        return;
    }

    const QString path = pathAt(index);
    if (!ctrlMode && !shiftMode) {
        applySelection({path}, {name}, path, name, path, index);
        return;
    }

    if (ctrlMode) {
        QStringList paths = m_selectedPaths;
        QStringList names = m_selectedFiles;
        const int existingIndex = selectedPathIndex(path, name);
        if (existingIndex >= 0) {
            paths.removeAt(existingIndex);
            names.removeAt(existingIndex);
            QString selectedPath;
            QString selectedName;
            if (!names.isEmpty()) {
                selectedPath = paths.constLast();
                selectedName = names.constLast();
            }
            applySelection(paths, names, selectedPath, selectedName, path, index);
        } else {
            paths.append(path);
            names.append(name);
            applySelection(paths, names, path, name, path, index);
        }
        return;
    }

    if (shiftMode && m_lastSelectedIndex != -1 && index != -1) {
        const int minimum = qMin(m_lastSelectedIndex, index);
        const int maximum = qMax(m_lastSelectedIndex, index);
        QStringList paths;
        QStringList names;
        for (int row = minimum; row <= maximum; ++row) {
            paths.append(pathAt(row));
            names.append(nameAt(row));
        }
        applySelection(
            paths,
            names,
            path,
            name,
            m_anchorPath,
            m_lastSelectedIndex);
    }
}

void SelectionController::reconcileAfterModelChange()
{
    if (m_selectedPaths.isEmpty()) {
        if (!m_selectedFiles.isEmpty() || !m_selectedFile.isEmpty()
            || m_lastSelectedIndex != -1) {
            applySelection({}, {}, {}, {}, {}, -1);
        }
        return;
    }

    QStringList paths;
    QStringList names;
    paths.reserve(m_selectedPaths.size());
    names.reserve(m_selectedPaths.size());
    for (const QString &path : std::as_const(m_selectedPaths)) {
        const int index = indexForPath(path);
        if (index < 0) {
            continue;
        }
        paths.append(path);
        names.append(nameAt(index));
    }

    QString selectedPath = m_selectedPath;
    QString selectedName;
    if (indexForPath(selectedPath) < 0) {
        selectedPath = paths.isEmpty() ? QString() : paths.constLast();
    }
    if (!selectedPath.isEmpty()) {
        selectedName = nameAt(indexForPath(selectedPath));
    }

    QString anchorPath = m_anchorPath;
    int anchorIndex = indexForPath(anchorPath);
    if (anchorIndex < 0) {
        anchorPath.clear();
        anchorIndex = -1;
    }
    applySelection(paths, names, selectedPath, selectedName, anchorPath, anchorIndex);
}

QString SelectionController::pathAt(int index) const
{
    if (index < 0 || index >= m_model->rowCount()) {
        return {};
    }
    return m_model->data(m_model->index(index, 0), DirectoryModel::FilePathRole).toString();
}

QString SelectionController::nameAt(int index) const
{
    if (index < 0 || index >= m_model->rowCount()) {
        return {};
    }
    return m_model->data(m_model->index(index, 0), DirectoryModel::FileNameRole).toString();
}

int SelectionController::indexForPath(const QString &path) const
{
    if (path.isEmpty()) {
        return -1;
    }
    for (int row = 0; row < m_model->rowCount(); ++row) {
        if (pathAt(row) == path) {
            return row;
        }
    }
    return -1;
}

int SelectionController::selectedPathIndex(
    const QString &path,
    const QString &name) const
{
    if (!path.isEmpty()) {
        return m_selectedPaths.indexOf(path);
    }
    return m_selectedFiles.indexOf(name);
}

void SelectionController::applySelection(
    const QStringList &paths,
    const QStringList &names,
    const QString &selectedPath,
    const QString &selectedName,
    const QString &anchorPath,
    int anchorIndex)
{
    const bool fileChanged = m_selectedFile != selectedName;
    const bool filesChanged = m_selectedFiles != names;
    const bool indexChanged = m_lastSelectedIndex != anchorIndex;
    m_selectedPaths = paths;
    m_selectedFile = selectedName;
    m_selectedFiles = names;
    m_selectedPath = selectedPath;
    m_anchorPath = anchorPath;
    m_lastSelectedIndex = anchorIndex;

    if (fileChanged) {
        emit selectedFileChanged();
    }
    if (filesChanged) {
        emit selectedFilesChanged();
    }
    if (indexChanged) {
        emit lastSelectedIndexChanged();
    }
    if (fileChanged || filesChanged || indexChanged) {
        emit selectionChanged();
    }
}

} // namespace Astrea::Explorer::Native::Backend
