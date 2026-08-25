#include <QFileInfo>
#include <QtTest>

#include "parity/fixture_tree.h"
#include "parity/native_oracle.h"

class ShadowParityTest final : public QObject
{
    Q_OBJECT

private slots:
    void controlledFixtureExercisesNativeStack();
};

void ShadowParityTest::controlledFixtureExercisesNativeStack()
{
    QVERIFY2(
        QFileInfo::exists(QString::fromUtf8(ASTREA_EXPLORER_PARITY_BACKEND)),
        "Build explorer_backend before running the deterministic shadow-parity gate");
    const std::unique_ptr<Astrea::Explorer::Native::Parity::FixtureTree> fixture =
        Astrea::Explorer::Native::Parity::FixtureTree::create();
    QVERIFY(fixture != nullptr);

    Astrea::Explorer::Native::Parity::NativeOracle native;
    const auto snapshot = native.capture(*fixture);

    QVERIFY2(snapshot.error.isEmpty(), qPrintable(snapshot.error));
    QVERIFY(!snapshot.directoryEntries.isEmpty());
    QVERIFY(!snapshot.searchEntries.isEmpty());
    QCOMPARE(snapshot.currentPath, QStringLiteral("<root>/Nested Folder"));
    QCOMPARE(snapshot.searchQuery, QStringLiteral("search-target"));
    QCOMPARE(snapshot.selectedFile, QStringLiteral("space name.txt"));
    QVERIFY(snapshot.historyIndex >= 1);
}

QTEST_MAIN(ShadowParityTest)

#include "tst_shadow_parity.moc"
