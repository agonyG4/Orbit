#include <QFileInfo>
#include <QtTest>

#include "parity/fixture_tree.h"
#include "parity/legacy_oracle.h"
#include "parity/native_oracle.h"

class ShadowParityTest final : public QObject
{
    Q_OBJECT

private slots:
    void controlledFixtureMatchesLegacyAndNative();
};

void ShadowParityTest::controlledFixtureMatchesLegacyAndNative()
{
    QVERIFY2(
        QFileInfo::exists(QString::fromUtf8(ASTREA_EXPLORER_PARITY_BACKEND)),
        "Build explorer_backend before running the deterministic shadow-parity gate");
    QVERIFY2(
        QFileInfo::exists(QString::fromUtf8(ASTREA_EXPLORER_PARITY_HELPER)),
        "The legacy Explorer helper must remain available for parity");

    const std::unique_ptr<Astrea::Explorer::Native::Parity::FixtureTree> fixture =
        Astrea::Explorer::Native::Parity::FixtureTree::create();
    QVERIFY(fixture != nullptr);

    Astrea::Explorer::Native::Parity::LegacyOracle legacy;
    Astrea::Explorer::Native::Parity::NativeOracle native;
    const auto legacySnapshot = legacy.capture(*fixture);
    const auto nativeSnapshot = native.capture(*fixture);

    const QString difference = legacySnapshot.diff(nativeSnapshot);
    QVERIFY2(difference.isEmpty(), qPrintable(difference));
}

QTEST_MAIN(ShadowParityTest)

#include "tst_shadow_parity.moc"
