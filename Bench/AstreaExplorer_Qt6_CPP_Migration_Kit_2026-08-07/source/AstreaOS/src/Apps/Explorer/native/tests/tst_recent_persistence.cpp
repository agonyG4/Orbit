#include <QFile>
#include <QScopeGuard>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QtTest>

namespace {

QString recentStatePath()
{
    return QStringLiteral(ASTREA_EXPLORER_RUNTIME_ROOT)
        + QStringLiteral("/Apps/Explorer/state/RecentState.qml");
}

QString readRecentState()
{
    QFile file(recentStatePath());
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return QString::fromUtf8(file.readAll());
}

} // namespace

class RecentPersistenceTest final : public QObject
{
    Q_OBJECT

private slots:
    void nativeShimHasNoQmlProcessDependency();
    void nativeShimLoadsAndExposesCompatibilityContract();
};

void RecentPersistenceTest::nativeShimHasNoQmlProcessDependency()
{
    const QString source = readRecentState();
    QVERIFY2(!source.isEmpty(), qPrintable(recentStatePath()));
    QVERIFY(source.contains(QStringLiteral("nativeOwned")));
    QVERIFY(source.contains(QStringLiteral("nativeAppState.loadRecent()")));
    QVERIFY(source.contains(QStringLiteral("nativeAppState.recordRecentAccess")));
    QVERIFY(!source.contains(QStringLiteral("Quickshell")));
    QVERIFY(!source.contains(QStringLiteral("Process")));
}

void RecentPersistenceTest::nativeShimLoadsAndExposesCompatibilityContract()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, QUrl::fromLocalFile(recentStatePath()));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));

    QObject *state = component.create();
    QVERIFY2(state != nullptr, qPrintable(component.errorString()));
    const auto cleanup = qScopeGuard([state]() { delete state; });

    QVERIFY(state->property("nativeOwned").toBool());
    QVERIFY(QMetaObject::invokeMethod(
        state,
        "recordAccess",
        Q_ARG(QVariant, QVariant(QStringLiteral("/recent/example.txt"))),
        Q_ARG(QVariant, QVariant(false)),
        Q_ARG(QVariant, QVariant(QString()))));
}

QTEST_MAIN(RecentPersistenceTest)

#include "tst_recent_persistence.moc"
