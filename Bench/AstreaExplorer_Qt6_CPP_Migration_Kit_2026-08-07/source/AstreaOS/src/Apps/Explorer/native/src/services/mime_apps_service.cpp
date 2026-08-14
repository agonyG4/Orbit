#include "services/mime_apps_service.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QLockFile>
#include <QSaveFile>
#include <QSettings>

#include <utility>

#include "services/desktop_file_id.h"

namespace Astrea::Explorer::Native::Services {
namespace {

QStringList normalizeIds(const QStringList &values)
{
    QStringList normalized;
    for (const QString &value : values) {
        const QString id = DesktopFileId::normalize(value);
        if (!id.isEmpty() && !normalized.contains(id)) {
            normalized.append(id);
        }
    }
    return normalized;
}

QString serializeIds(const QStringList &values)
{
    return values.isEmpty()
        ? QString()
        : values.join(QLatin1Char(';')) + QLatin1Char(';');
}

int sectionEnd(const QStringList &lines, int sectionStart)
{
    for (int index = sectionStart + 1; index < lines.size(); ++index) {
        const QString trimmed = lines.at(index).trimmed();
        if (trimmed.startsWith(QLatin1Char('['))
            && trimmed.endsWith(QLatin1Char(']'))) {
            return index;
        }
    }
    return lines.size();
}

bool desktopEntryCanHandle(const QString &path)
{
    QSettings settings(path, QSettings::IniFormat);
    settings.beginGroup(QStringLiteral("Desktop Entry"));
    const bool valid = settings.value(QStringLiteral("Type")).toString()
                           == QStringLiteral("Application")
        && !settings.value(QStringLiteral("Hidden"), false).toBool()
        && !settings.value(QStringLiteral("Exec")).toString().trimmed().isEmpty();
    settings.endGroup();
    return valid;
}

} // namespace

MimeAppsService::MimeAppsService(QString filePath, int lockTimeoutMs, XdgPaths paths)
    : m_filePathWasExplicit(!filePath.trimmed().isEmpty())
    , m_lockTimeoutMs(lockTimeoutMs)
    , m_paths(std::move(paths))
{
    m_filePath = m_filePathWasExplicit
        ? std::move(filePath)
        : QDir(m_paths.configHome).filePath(QStringLiteral("mimeapps.list"));
}

QStringList MimeAppsService::searchPaths() const
{
    return m_filePathWasExplicit ? QStringList {m_filePath} : m_paths.mimeAppsSearchPaths();
}

QStringList MimeAppsService::readLines(const QString &path) const
{
    QFile file(path);
    if (!file.exists() || !file.open(QIODevice::ReadOnly)) {
        return {};
    }
    const QByteArray content = file.readAll();
    return content.isEmpty()
        ? QStringList {}
        : QString::fromUtf8(content).split(QLatin1Char('\n'), Qt::KeepEmptyParts);
}

QStringList MimeAppsService::valuesFor(
    const QStringList &lines,
    const QString &section,
    const QString &mime) const
{
    int sectionStart = -1;
    for (int index = 0; index < lines.size(); ++index) {
        if (lines.at(index).trimmed() == section) {
            sectionStart = index;
            break;
        }
    }
    if (sectionStart < 0) {
        return {};
    }

    const int end = sectionEnd(lines, sectionStart);
    for (int index = sectionStart + 1; index < end; ++index) {
        const QString line = lines.at(index).trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#'))
            || line.startsWith(QLatin1Char(';'))) {
            continue;
        }
        const int equals = line.indexOf(QLatin1Char('='));
        if (equals < 0 || line.left(equals).trimmed() != mime) {
            continue;
        }
        return normalizeIds(line.mid(equals + 1).split(
            QLatin1Char(';'), Qt::SkipEmptyParts));
    }
    return {};
}

bool MimeAppsService::isValidDesktopId(const QString &desktopId) const
{
    const QString normalizedId = DesktopFileId::normalize(desktopId);
    if (normalizedId.isEmpty()) {
        return false;
    }

    for (const QString &root : m_paths.applicationRoots()) {
        if (!QFileInfo(root).isDir()) {
            continue;
        }
        QDirIterator iterator(root, {QStringLiteral("*.desktop")}, QDir::Files,
                              QDirIterator::Subdirectories);
        while (iterator.hasNext()) {
            const QString path = iterator.next();
            if (DesktopFileId::fromPath(path, {root}) == normalizedId
                && desktopEntryCanHandle(path)) {
                return true;
            }
        }
    }
    return false;
}

QStringList MimeAppsService::validDesktopIds(const QStringList &ids) const
{
    QStringList valid;
    for (const QString &id : normalizeIds(ids)) {
        if (isValidDesktopId(id)) {
            valid.append(id);
        }
    }
    return valid;
}

QStringList MimeAppsService::effectiveAssociations(const QString &mime) const
{
    QStringList associations;
    const QStringList paths = searchPaths();
    for (auto path = paths.crbegin(); path != paths.crend(); ++path) {
        const QStringList lines = readLines(*path);
        for (const QString &removed : valuesFor(
                 lines, QStringLiteral("[Removed Associations]"), mime)) {
            associations.removeAll(removed);
        }
        const QStringList added = valuesFor(
            lines, QStringLiteral("[Added Associations]"), mime);
        for (auto addedId = added.crbegin(); addedId != added.crend(); ++addedId) {
            associations.removeAll(*addedId);
            associations.prepend(*addedId);
        }
    }
    return validDesktopIds(associations);
}

QStringList MimeAppsService::defaultsForMime(const QString &mime) const
{
    for (const QString &path : searchPaths()) {
        const QStringList defaults = validDesktopIds(valuesFor(
            readLines(path), QStringLiteral("[Default Applications]"), mime));
        if (!defaults.isEmpty()) {
            return defaults;
        }
    }
    return effectiveAssociations(mime);
}

QStringList MimeAppsService::associationsForMime(const QString &mime) const
{
    return effectiveAssociations(mime);
}

bool MimeAppsService::updateValue(
    QStringList *lines,
    const QString &section,
    const QString &mime,
    const QStringList &values) const
{
    if (lines == nullptr || mime.trimmed().isEmpty()) {
        return false;
    }

    int sectionStart = -1;
    for (int index = 0; index < lines->size(); ++index) {
        if (lines->at(index).trimmed() == section) {
            sectionStart = index;
            break;
        }
    }

    const QString replacement = mime + QLatin1Char('=') + serializeIds(values);
    if (sectionStart < 0) {
        if (!lines->isEmpty() && !lines->constLast().isEmpty()) {
            lines->append(QString());
        }
        lines->append(section);
        lines->append(replacement);
        return true;
    }

    const int end = sectionEnd(*lines, sectionStart);
    for (int index = sectionStart + 1; index < end; ++index) {
        const QString line = lines->at(index).trimmed();
        const int equals = line.indexOf(QLatin1Char('='));
        if (equals >= 0 && line.left(equals).trimmed() == mime) {
            (*lines)[index] = replacement;
            return true;
        }
    }

    lines->insert(end, replacement);
    return true;
}

bool MimeAppsService::setDefault(const QString &mime, const QString &desktopId) const
{
    const QString normalizedMime = mime.trimmed();
    const QString normalizedId = DesktopFileId::normalize(desktopId);
    if (normalizedMime.isEmpty() || normalizedId.isEmpty() || !isValidDesktopId(normalizedId)) {
        return false;
    }

    const QFileInfo fileInfo(m_filePath);
    if (!QDir().mkpath(fileInfo.absolutePath())) {
        return false;
    }

    QLockFile lock(m_filePath + QStringLiteral(".lock"));
    lock.setStaleLockTime(10000);
    if (!lock.tryLock(m_lockTimeoutMs)) {
        return false;
    }

    QStringList lines = readLines(m_filePath);
    QStringList defaults = valuesFor(
        lines, QStringLiteral("[Default Applications]"), normalizedMime);
    defaults.removeAll(normalizedId);
    defaults.prepend(normalizedId);

    QStringList associations = valuesFor(
        lines, QStringLiteral("[Added Associations]"), normalizedMime);
    associations.removeAll(normalizedId);
    associations.prepend(normalizedId);

    QStringList removed = valuesFor(
        lines, QStringLiteral("[Removed Associations]"), normalizedMime);
    removed.removeAll(normalizedId);

    if (!updateValue(
            &lines,
            QStringLiteral("[Default Applications]"),
            normalizedMime,
            defaults)
        || !updateValue(
            &lines,
            QStringLiteral("[Added Associations]"),
            normalizedMime,
            associations)
        || !updateValue(
            &lines,
            QStringLiteral("[Removed Associations]"),
            normalizedMime,
            removed)) {
        return false;
    }

    QSaveFile output(m_filePath);
    if (!output.open(QIODevice::WriteOnly)) {
        return false;
    }
    const QByteArray content = (lines.join(QLatin1Char('\n'))
        + (lines.isEmpty() || !lines.constLast().isEmpty() ? QStringLiteral("\n") : QString()))
                                   .toUtf8();
    if (output.write(content) != content.size()) {
        return false;
    }
    return output.commit();
}

QString MimeAppsService::filePath() const
{
    return m_filePath;
}

} // namespace Astrea::Explorer::Native::Services
