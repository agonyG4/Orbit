#include "services/mime_apps_service.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLockFile>
#include <QSaveFile>
#include <QStandardPaths>

#include <algorithm>
#include <utility>

#include "services/desktop_file_id.h"

namespace Astrea::Explorer::Native::Services {
namespace {

QString defaultMimeAppsPath()
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::ConfigLocation))
        .filePath(QStringLiteral("mimeapps.list"));
}

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

} // namespace

MimeAppsService::MimeAppsService(QString filePath, int lockTimeoutMs)
    : m_filePath(filePath.trimmed().isEmpty() ? defaultMimeAppsPath() : std::move(filePath))
    , m_lockTimeoutMs(lockTimeoutMs)
{
}

QStringList MimeAppsService::readLines() const
{
    QFile file(m_filePath);
    if (!file.exists() || !file.open(QIODevice::ReadOnly)) {
        return {};
    }
    const QByteArray content = file.readAll();
    if (content.isEmpty()) {
        return {};
    }
    return QString::fromUtf8(content).split(QLatin1Char('\n'), Qt::KeepEmptyParts);
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
        const QStringList rawValues = line.mid(equals + 1).split(
            QLatin1Char(';'),
            Qt::SkipEmptyParts);
        return normalizeIds(rawValues);
    }
    return {};
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

QStringList MimeAppsService::defaultsForMime(const QString &mime) const
{
    return valuesFor(
        readLines(),
        QStringLiteral("[Default Applications]"),
        mime);
}

QStringList MimeAppsService::associationsForMime(const QString &mime) const
{
    return valuesFor(
        readLines(),
        QStringLiteral("[Added Associations]"),
        mime);
}

bool MimeAppsService::setDefault(const QString &mime, const QString &desktopId) const
{
    const QString normalizedMime = mime.trimmed();
    const QString normalizedId = DesktopFileId::normalize(desktopId);
    if (normalizedMime.isEmpty() || normalizedId.isEmpty()) {
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

    QStringList lines = readLines();
    QStringList defaults = valuesFor(
        lines,
        QStringLiteral("[Default Applications]"),
        normalizedMime);
    defaults.removeAll(normalizedId);
    defaults.prepend(normalizedId);

    QStringList associations = valuesFor(
        lines,
        QStringLiteral("[Added Associations]"),
        normalizedMime);
    associations.removeAll(normalizedId);
    associations.prepend(normalizedId);

    if (!updateValue(
            &lines,
            QStringLiteral("[Default Applications]"),
            normalizedMime,
            defaults)
        || !updateValue(
            &lines,
            QStringLiteral("[Added Associations]"),
            normalizedMime,
            associations)) {
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
