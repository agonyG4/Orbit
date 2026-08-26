#include <QFile>
#include <QtTest>

namespace {

QString appStatePath()
{
    return QStringLiteral(ASTREA_EXPLORER_RUNTIME_ROOT)
        + QStringLiteral("/Apps/Explorer/AppState.qml");
}

QString readAppState()
{
    QFile file(appStatePath());
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
    void recentPersistenceUsesNativeBoundary();
    void recentCompatibilityStateIsRetired();
};

void RecentPersistenceTest::recentPersistenceUsesNativeBoundary()
{
    const QString source = readAppState();
    QVERIFY2(!source.isEmpty(), qPrintable(appStatePath()));
    QVERIFY(source.contains(QStringLiteral("import Astrea.Explorer.Native 1.0")));
    QVERIFY(source.contains(QStringLiteral("readonly property QtObject nativeAppState: NativeAppState")));
    QVERIFY(source.contains(QStringLiteral("nativeAppState.loadRecent()")));
    QVERIFY(source.contains(QStringLiteral("nativeAppState.recordRecentAccess")));
    QVERIFY(!source.contains(QStringLiteral("RecentState")));
    QVERIFY(!source.contains(QStringLiteral("nativeNavigationActive")));
}

void RecentPersistenceTest::recentCompatibilityStateIsRetired()
{
    QVERIFY(!QFile::exists(
        QStringLiteral(ASTREA_EXPLORER_RUNTIME_ROOT)
        + QStringLiteral("/Apps/Explorer/state/RecentState.qml")));
}

QTEST_MAIN(RecentPersistenceTest)

#include "tst_recent_persistence.moc"
