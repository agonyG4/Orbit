#include <QFile>
#include <QDir>
#include <QList>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQmlError>
#include <QUrl>
#include <QTimer>
#include <QtTest>

class WarningStore final : public QObject
{
    Q_OBJECT

public:
    QStringList messages;

public slots:
    void record(const QList<QQmlError> &errors)
    {
        for (const QQmlError &error : errors) {
            messages.append(error.toString());
        }
    }
};

class QmlWarningLifetimeTest final : public QObject
{
    Q_OBJECT

private slots:
    void warningHandlerDoesNotCaptureStackStorage();
    void postLoadWarningIsCollectedAfterEventLoopTurn();
};

void QmlWarningLifetimeTest::warningHandlerDoesNotCaptureStackStorage()
{
    const QString sourceRoot = qEnvironmentVariable(
        "ASTREA_EXPLORER_NATIVE_SOURCE_ROOT");
    QVERIFY2(!sourceRoot.isEmpty(), "ASTREA_EXPLORER_NATIVE_SOURCE_ROOT is required");
    QFile source(QDir(sourceRoot).filePath(QStringLiteral("explorer_application.cpp")));
    QVERIFY(source.open(QIODevice::ReadOnly));
    const QString implementation = QString::fromUtf8(source.readAll());

    QVERIFY(!implementation.contains(QStringLiteral("[&warnings]")));
    QVERIFY(implementation.contains(QStringLiteral("m_runtimeWarnings")));
    QVERIFY(implementation.contains(QStringLiteral("runtimeWarnings")));
}

void QmlWarningLifetimeTest::postLoadWarningIsCollectedAfterEventLoopTurn()
{
    WarningStore store;
    QQmlApplicationEngine engine;
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::warnings,
        &store,
        &WarningStore::record);

    engine.loadData(
        QByteArrayLiteral(
            "import QtQml 2.15\n"
            "QtObject { objectName: \"loaded\" }"),
        QUrl(QStringLiteral("qrc:/warning-lifetime-root.qml")));
    QVERIFY2(!engine.rootObjects().isEmpty(), "The post-load warning fixture did not load");

    QTimer::singleShot(0, &engine, [&engine]() {
        auto *lateComponent = new QQmlComponent(&engine, &engine);
        lateComponent->setData(
            QByteArrayLiteral(
                "import QtQml 2.15\n"
                "QtObject { property int invalid: unknownIdentifier }"),
            QUrl(QStringLiteral("qrc:/warning-lifetime-late.qml")));
        lateComponent->create();
    });

    QTRY_VERIFY_WITH_TIMEOUT(!store.messages.isEmpty(), 1000);
}

QTEST_MAIN(QmlWarningLifetimeTest)

#include "tst_qml_warning_lifetime.moc"
