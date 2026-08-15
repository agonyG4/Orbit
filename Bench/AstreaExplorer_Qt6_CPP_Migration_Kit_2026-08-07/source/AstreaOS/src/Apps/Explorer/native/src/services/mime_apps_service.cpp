#include "services/mime_apps_service.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLockFile>
#include <QSaveFile>

#include <algorithm>

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
    return values.isEmpty() ? QString() : values.join(QLatin1Char(';')) + QLatin1Char(';');
}

int sectionEnd(const QStringList &lines, int sectionStart)
{
    for (int index = sectionStart + 1; index < lines.size(); ++index) {
        const QString line = lines.at(index).trimmed();
        if (line.startsWith(QLatin1Char('[')) && line.endsWith(QLatin1Char(']'))) {
            return index;
        }
    }
    return lines.size();
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

void MimeAppsService::setCatalog(const DesktopApplicationCatalog *catalog)
{
    m_catalogService = catalog;
}

const DesktopApplicationCatalog::Snapshot &MimeAppsService::catalog() const
{
    if (m_catalogService != nullptr && m_catalogService->ready()) {
        return m_catalogService->snapshot();
    }
    if (!m_fallbackCatalogReady) {
        m_fallbackCatalog = DesktopApplicationCatalog::build(m_paths);
        m_fallbackCatalogReady = true;
    }
    return m_fallbackCatalog;
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

bool MimeAppsService::mimeMatches(const QStringList &mimeTypes, const QString &mime)
{
    for (const QString &candidate : mimeTypes) {
        if (candidate == mime || (candidate.endsWith(QStringLiteral("/*"))
                                  && mime.startsWith(candidate.left(candidate.size() - 1)))) {
            return true;
        }
    }
    return false;
}

bool MimeAppsService::isValidDesktopId(const QString &desktopId) const
{
    const QString normalizedId = DesktopFileId::normalize(desktopId);
    const auto found = catalog().constFind(normalizedId);
    return found != catalog().constEnd() && found.value().usable();
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
    QStringList catalogIds = catalog().keys();
    std::sort(catalogIds.begin(), catalogIds.end());
    for (const QString &id : catalogIds) {
        const auto entry = catalog().constFind(id);
        if (entry != catalog().constEnd() && mimeMatches(entry.value().mimeTypes, mime)) {
            associations.append(id);
        }
    }

    const QStringList paths = searchPaths();
    for (auto path = paths.crbegin(); path != paths.crend(); ++path) {
        const QStringList lines = readLines(*path);
        for (const QString &removed : valuesFor(
                 lines, QStringLiteral("[Removed Associations]"), mime)) {
            associations.removeAll(removed);
        }
        const QStringList defaults = validDesktopIds(valuesFor(
            lines, QStringLiteral("[Default Applications]"), mime));
        for (auto id = defaults.crbegin(); id != defaults.crend(); ++id) {
            associations.removeAll(*id);
            associations.prepend(*id);
        }
        const QStringList added = validDesktopIds(valuesFor(
            lines, QStringLiteral("[Added Associations]"), mime));
        for (auto id = added.crbegin(); id != added.crend(); ++id) {
            associations.removeAll(*id);
            associations.prepend(*id);
        }
    }
    return associations;
}

QStringList MimeAppsService::defaultsForMime(const QString &mime) const
{
    const QStringList effective = effectiveAssociations(mime);
    for (const QString &path : searchPaths()) {
        const QStringList defaults = validDesktopIds(valuesFor(
            readLines(path), QStringLiteral("[Default Applications]"), mime));
        QStringList usable;
        for (const QString &id : defaults) {
            if (effective.contains(id)) {
                usable.append(id);
            }
        }
        if (!usable.isEmpty()) {
            return usable;
        }
    }
    return effective;
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

    if (!updateValue(&lines, QStringLiteral("[Default Applications]"), normalizedMime, defaults)
        || !updateValue(&lines, QStringLiteral("[Added Associations]"), normalizedMime, associations)
        || !updateValue(&lines, QStringLiteral("[Removed Associations]"), normalizedMime, removed)) {
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
