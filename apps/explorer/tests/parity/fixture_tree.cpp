#include "parity/fixture_tree.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

namespace Astrea::Explorer::Native::Parity {

class FixtureTree::Private
{
public:
    QTemporaryDir temporaryDirectory;
    QString rootPath;
    QString nestedPath;
    QString devicePath;
    QString operationsPath;
    QString recentsPath;
};

FixtureTree::FixtureTree()
    : m_private(std::make_unique<Private>())
{
}

FixtureTree::~FixtureTree() = default;

std::unique_ptr<FixtureTree> FixtureTree::create()
{
    auto fixture = std::unique_ptr<FixtureTree>(new FixtureTree());
    if (!fixture->m_private->temporaryDirectory.isValid()) {
        return nullptr;
    }

    const QString base = fixture->m_private->temporaryDirectory.path();
    fixture->m_private->rootPath = QDir(base).filePath(QStringLiteral("root"));
    fixture->m_private->nestedPath = QDir(fixture->m_private->rootPath).filePath(
        QStringLiteral("Nested Folder"));
    fixture->m_private->devicePath = QDir(fixture->m_private->rootPath).filePath(
        QStringLiteral("devices"));
    fixture->m_private->operationsPath = QDir(fixture->m_private->rootPath).filePath(
        QStringLiteral("operations"));
    fixture->m_private->recentsPath = QDir(fixture->m_private->rootPath).filePath(
        QStringLiteral("recents"));

    QDir().mkpath(fixture->m_private->nestedPath);
    QDir().mkpath(QDir(fixture->m_private->devicePath).filePath(QStringLiteral("USB Device")));
    QDir().mkpath(fixture->m_private->operationsPath);
    QDir().mkpath(fixture->m_private->recentsPath);

    const auto writeFile = [](const QString &path, const QByteArray &contents) {
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly)) {
            return false;
        }
        return file.write(contents) == contents.size();
    };

    const QStringList files = {
        QDir(fixture->m_private->rootPath).filePath(QStringLiteral("alpha.txt")),
        QDir(fixture->m_private->rootPath).filePath(QStringLiteral(".hidden.txt")),
        QDir(fixture->m_private->rootPath).filePath(QStringLiteral("space name.txt")),
        QDir(fixture->m_private->rootPath).filePath(QStringLiteral("unicode-ç.txt")),
        QDir(fixture->m_private->rootPath).filePath(QStringLiteral("photo.png")),
        QDir(fixture->m_private->rootPath).filePath(QStringLiteral("run.sh")),
        QDir(fixture->m_private->nestedPath).filePath(QStringLiteral("search-target.txt")),
        QDir(fixture->m_private->nestedPath).filePath(QStringLiteral("other.md")),
        QDir(fixture->m_private->operationsPath).filePath(QStringLiteral("source.txt")),
    };
    const QList<QByteArray> contents = {
        QByteArrayLiteral("alpha\n"),
        QByteArrayLiteral("hidden\n"),
        QByteArrayLiteral("space\n"),
        QByteArrayLiteral("unicode\n"),
        QByteArrayLiteral("not-a-real-png\n"),
        QByteArrayLiteral("#!/bin/sh\nprintf run\n"),
        QByteArrayLiteral("search target\n"),
        QByteArrayLiteral("other\n"),
        QByteArrayLiteral("operation source\n"),
    };
    for (int i = 0; i < files.size(); ++i) {
        if (!writeFile(files.at(i), contents.at(i))) {
            return nullptr;
        }
    }

    QFile::setPermissions(
        QDir(fixture->m_private->rootPath).filePath(QStringLiteral("run.sh")),
        QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);

    QJsonArray recentItems;
    recentItems.append(QJsonObject {
        {QStringLiteral("filePath"), files.at(0)},
        {QStringLiteral("lastAccessed"), 200},
    });
    recentItems.append(QJsonObject {
        {QStringLiteral("filePath"), files.at(2)},
        {QStringLiteral("lastAccessed"), 100},
    });
    return writeFile(
               QDir(fixture->m_private->recentsPath).filePath(QStringLiteral("finder.json")),
               QJsonDocument(recentItems).toJson(QJsonDocument::Compact))
        ? std::move(fixture)
        : nullptr;
}

QString FixtureTree::rootPath() const
{
    return m_private->rootPath;
}

QString FixtureTree::nestedPath() const
{
    return m_private->nestedPath;
}

QString FixtureTree::devicePath() const
{
    return m_private->devicePath;
}

QString FixtureTree::operationsPath() const
{
    return m_private->operationsPath;
}

QString FixtureTree::recentsPath() const
{
    return m_private->recentsPath;
}

} // namespace Astrea::Explorer::Native::Parity
