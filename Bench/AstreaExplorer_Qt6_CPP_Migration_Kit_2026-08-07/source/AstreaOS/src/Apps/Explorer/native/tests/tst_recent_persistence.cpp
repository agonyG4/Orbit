#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QQmlContext>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QScopeGuard>
#include <QTemporaryDir>
#include <QTimer>
#include <QUrl>
#include <QtTest>

namespace {

bool writeFile(const QString &path, const QByteArray &contents)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    return file.write(contents) == contents.size();
}

bool createProcessStubs(const QString &root)
{
    const QDir directory(root);
    if (!directory.mkpath(QStringLiteral("Quickshell/Io"))) {
        return false;
    }

    return writeFile(
               directory.filePath(QStringLiteral("Quickshell/qmldir")),
               QByteArrayLiteral(
                   "module Quickshell\n"
                   "singleton Quickshell 1.0 Quickshell.qml\n"))
        && writeFile(
               directory.filePath(QStringLiteral("Quickshell/Quickshell.qml")),
               QByteArrayLiteral(
                   "pragma Singleton\n"
                   "import QtQml 2.15\n"
                   "QtObject { function env(name) { return name === \"HOME\" ? \"/tmp\" : \"\" } }\n"))
        && writeFile(
               directory.filePath(QStringLiteral("Quickshell/Io/qmldir")),
               QByteArrayLiteral(
                   "module Quickshell.Io\n"
                   "Process 1.0 Process.qml\n"
                   "SplitParser 1.0 SplitParser.qml\n"))
        && writeFile(
               directory.filePath(QStringLiteral("Quickshell/Io/SplitParser.qml")),
               QByteArrayLiteral(
                   "import QtQml 2.15\n"
                   "QtObject { signal read(string data) }\n"))
        && writeFile(
               directory.filePath(QStringLiteral("Quickshell/Io/Process.qml")),
               QByteArrayLiteral(
                   "import QtQml 2.15\n"
                   "QtObject {\n"
                   "    property var command: []\n"
                   "    property bool running: false\n"
                   "    property bool stdinEnabled: false\n"
                   "    property QtObject stdout: SplitParser {}\n"
                   "    property var commandHistory: []\n"
                   "    signal started()\n"
                   "    signal exited(int exitCode)\n"
                   "    onCommandChanged: {\n"
                   "        if (command && command.length > 0) {\n"
                   "            var history = commandHistory.slice()\n"
                   "            history.push(command)\n"
                   "            commandHistory = history\n"
                   "        }\n"
                   "    }\n"
                   "    function complete(code, output) {\n"
                   "        if (output !== undefined && output !== null)\n"
                   "            stdout.read(output)\n"
                   "        running = false\n"
                   "        exited(code)\n"
                   "    }\n"
                   "}\n"));
}

class TestFileModel final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int count READ count CONSTANT)

public:
    int count() const { return 0; }

    Q_INVOKABLE QVariantMap get(int) const { return {}; }
};

class TestRecentApp final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString helperPath READ helperPath CONSTANT)
    Q_PROPERTY(QObject *fileModel READ fileModel CONSTANT)
    Q_PROPERTY(QString currentPath READ currentPath WRITE setCurrentPath)
    Q_PROPERTY(bool dialogActive READ dialogActive CONSTANT)

public:
    explicit TestRecentApp(TestFileModel *fileModel, QObject *parent = nullptr)
        : QObject(parent)
        , m_fileModel(fileModel)
    {
    }

    QString helperPath() const { return QStringLiteral("test-helper"); }
    QObject *fileModel() const { return m_fileModel; }
    QString currentPath() const { return m_currentPath; }
    void setCurrentPath(const QString &path) { m_currentPath = path; }
    bool dialogActive() const { return false; }

    Q_INVOKABLE bool isRecentPath(const QString &) const { return false; }
    Q_INVOKABLE bool isTrashPath(const QString &) const { return false; }
    Q_INVOKABLE QString fileUrlForPath(const QString &path) const
    {
        return QUrl::fromLocalFile(path).toString();
    }
    Q_INVOKABLE void refreshCurrentFolder() { ++m_refreshCount; }

private:
    TestFileModel *m_fileModel = nullptr;
    QString m_currentPath;
    int m_refreshCount = 0;
};

QVariantMap recentItem(const QString &path, qint64 lastAccessed)
{
    return {
        {QStringLiteral("fileName"), QFileInfo(path).fileName()},
        {QStringLiteral("filePath"), path},
        {QStringLiteral("fileUrl"), QUrl::fromLocalFile(path).toString()},
        {QStringLiteral("fileIsDir"), false},
        {QStringLiteral("fileExecutable"), false},
        {QStringLiteral("fileHidden"), false},
        {QStringLiteral("fileSize"), 1},
        {QStringLiteral("fileModified"), QStringLiteral("old-filesystem-time")},
        {QStringLiteral("fileKind"), QStringLiteral("TXT")},
        {QStringLiteral("filePreviewUrl"), QString()},
        {QStringLiteral("lastAccessed"), lastAccessed},
        {QStringLiteral("recentSource"), QStringLiteral("finder")},
    };
}

QObject *propertyObject(QObject *object, const char *name)
{
    return object->property(name).value<QObject *>();
}

bool completeProcess(QObject *process, int code, const QString &output = {})
{
    return QMetaObject::invokeMethod(
        process,
        "complete",
        Q_ARG(QVariant, QVariant(code)),
        Q_ARG(QVariant, QVariant(output)));
}

QVariantList processCommands(QObject *process)
{
    return process->property("commandHistory").toList();
}

QJsonObject savedItems(QObject *process, int index)
{
    const QVariantList history = processCommands(process);
    if (index < 0 || index >= history.size()) {
        return {};
    }
    const QVariantList command = history.at(index).toList();
    if (command.size() < 5) {
        return {};
    }
    const QJsonDocument document = QJsonDocument::fromJson(command.at(4).toString().toUtf8());
    return document.isArray() && !document.array().isEmpty()
        ? document.array().at(0).toObject()
        : QJsonObject();
}

} // namespace

class RecentPersistenceTest final : public QObject
{
    Q_OBJECT

private slots:
    void saveGenerationsRejectLateOlderCompletion();
    void staleLoadCannotReplaceNewerInMemoryItems();
    void postLoadEventLoopKeepsGenerationGuardActive();
};

QObject *createRecentState(
    QTemporaryDir *stubs,
    TestRecentApp *app,
    QQmlEngine *engine)
{
    if (!stubs->isValid() || !createProcessStubs(stubs->path())) {
        return nullptr;
    }
    engine->addImportPath(stubs->path());
    const QString recentStatePath = QStringLiteral(ASTREA_EXPLORER_RUNTIME_ROOT)
        + QStringLiteral("/Apps/Explorer/state/RecentState.qml");
    QQmlComponent component(engine, QUrl::fromLocalFile(recentStatePath));
    if (!component.isReady()) {
        return nullptr;
    }
    return component.createWithInitialProperties({
        {QStringLiteral("app"), QVariant::fromValue(static_cast<QObject *>(app))},
    });
}

void RecentPersistenceTest::saveGenerationsRejectLateOlderCompletion()
{
    QTemporaryDir stubs;
    TestFileModel fileModel;
    TestRecentApp app(&fileModel);
    QQmlEngine engine;
    QObject *state = createRecentState(&stubs, &app, &engine);
    QVERIFY(state != nullptr);
    const auto cleanup = qScopeGuard([state]() { delete state; });

    const QVariantMap itemA = recentItem(QStringLiteral("/recent/A.txt"), 100);
    const QVariantMap itemB = recentItem(QStringLiteral("/recent/B.txt"), 200);
    state->setProperty("items", QVariantList {itemA});
    QVERIFY(QMetaObject::invokeMethod(state, "persist"));
    QCOMPARE(state->property("persistenceGeneration").toInt(), 1);
    QCOMPARE(state->property("saveGeneration").toInt(), 1);

    QObject *saveProcess = propertyObject(state, "saveProc");
    QVERIFY(saveProcess != nullptr);
    QCOMPARE(processCommands(saveProcess).size(), 1);
    QCOMPARE(savedItems(saveProcess, 0).value(QStringLiteral("filePath")).toString(),
             QStringLiteral("/recent/A.txt"));

    state->setProperty("items", QVariantList {itemB});
    QVERIFY(QMetaObject::invokeMethod(state, "persist"));
    QCOMPARE(state->property("persistenceGeneration").toInt(), 2);
    QCOMPARE(state->property("saveGeneration").toInt(), 1);

    QVERIFY(QMetaObject::invokeMethod(state, "startSave"));
    QCOMPARE(state->property("saveGeneration").toInt(), 2);
    QCOMPARE(processCommands(saveProcess).size(), 2);
    QCOMPARE(savedItems(saveProcess, 1).value(QStringLiteral("filePath")).toString(),
             QStringLiteral("/recent/B.txt"));

    QVERIFY(completeProcess(saveProcess, 0));
    QCOMPARE(state->property("saveGeneration").toInt(), 2);
    QCOMPARE(state->property("items").toList().constFirst().toMap().value(
                 QStringLiteral("filePath")).toString(), QStringLiteral("/recent/B.txt"));

    state->setProperty("saveGeneration", 1);
    QVERIFY(completeProcess(saveProcess, 0));
    QCOMPARE(state->property("saveGeneration").toInt(), 2);
    QCOMPARE(processCommands(saveProcess).size(), 3);
    QCOMPARE(savedItems(saveProcess, 2).value(QStringLiteral("filePath")).toString(),
             QStringLiteral("/recent/B.txt"));
}

void RecentPersistenceTest::staleLoadCannotReplaceNewerInMemoryItems()
{
    QTemporaryDir stubs;
    TestFileModel fileModel;
    TestRecentApp app(&fileModel);
    QQmlEngine engine;
    QObject *state = createRecentState(&stubs, &app, &engine);
    QVERIFY(state != nullptr);
    const auto cleanup = qScopeGuard([state]() { delete state; });

    const QVariantMap itemA = recentItem(QStringLiteral("/recent/A.txt"), 100);
    const QVariantMap itemB = recentItem(QStringLiteral("/recent/B.txt"), 200);
    state->setProperty("items", QVariantList {itemA});
    QVERIFY(QMetaObject::invokeMethod(state, "load"));
    QCOMPARE(state->property("loadGeneration").toInt(), 0);

    state->setProperty("items", QVariantList {itemB});
    QVERIFY(QMetaObject::invokeMethod(state, "persist"));
    QCOMPARE(state->property("persistenceGeneration").toInt(), 1);

    QObject *loadProcess = propertyObject(state, "loadProc");
    QVERIFY(loadProcess != nullptr);
    const QByteArray staleSnapshot = QJsonDocument::fromVariant(
        QVariantList {itemA}).toJson(QJsonDocument::Compact);
    QVERIFY(completeProcess(loadProcess, 0, QString::fromUtf8(staleSnapshot)));

    const QVariantList items = state->property("items").toList();
    QCOMPARE(items.size(), 1);
    QCOMPARE(items.constFirst().toMap().value(QStringLiteral("filePath")).toString(),
             QStringLiteral("/recent/B.txt"));
}

void RecentPersistenceTest::postLoadEventLoopKeepsGenerationGuardActive()
{
    QTemporaryDir stubs;
    TestFileModel fileModel;
    TestRecentApp app(&fileModel);
    QQmlEngine engine;
    QObject *state = createRecentState(&stubs, &app, &engine);
    QVERIFY(state != nullptr);
    const auto cleanup = qScopeGuard([state]() { delete state; });

    const QVariantMap itemA = recentItem(QStringLiteral("/recent/A.txt"), 100);
    const QVariantMap itemB = recentItem(QStringLiteral("/recent/B.txt"), 200);
    state->setProperty("items", QVariantList {itemA});
    QVERIFY(QMetaObject::invokeMethod(state, "load"));
    state->setProperty("items", QVariantList {itemB});
    QVERIFY(QMetaObject::invokeMethod(state, "persist"));

    QObject *loadProcess = propertyObject(state, "loadProc");
    QVERIFY(loadProcess != nullptr);
    const QByteArray staleSnapshot = QJsonDocument::fromVariant(
        QVariantList {itemA}).toJson(QJsonDocument::Compact);
    QTimer::singleShot(0, loadProcess, [loadProcess, staleSnapshot]() {
        completeProcess(loadProcess, 0, QString::fromUtf8(staleSnapshot));
    });

    QTRY_COMPARE(state->property("persistenceGeneration").toInt(), 1);
    const QVariantList items = state->property("items").toList();
    QCOMPARE(items.size(), 1);
    QCOMPARE(items.constFirst().toMap().value(QStringLiteral("filePath")).toString(),
             QStringLiteral("/recent/B.txt"));
}

QTEST_GUILESS_MAIN(RecentPersistenceTest)

#include "tst_recent_persistence.moc"
