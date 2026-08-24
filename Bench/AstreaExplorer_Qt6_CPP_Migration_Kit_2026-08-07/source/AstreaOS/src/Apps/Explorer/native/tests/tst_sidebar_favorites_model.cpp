#include <QSignalSpy>
#include <QtTest>

#include "models/sidebar_favorites_model.h"

using namespace Astrea::Explorer::Native::Backend;

class SidebarFavoritesModelTest final : public QObject
{
    Q_OBJECT

private slots:
    void exposesFavoriteRoles();
    void movesRowsWithQtMoveSignals();
    void commitsOnlyAfterExplicitCommit();
    void cancelsBackToOriginalOrder();
};

QVariantMap favorite(const QString &path, const QString &label)
{
    return {
        {QStringLiteral("path"), path},
        {QStringLiteral("label"), label},
        {QStringLiteral("icon"), QStringLiteral("inode-directory")},
    };
}

void SidebarFavoritesModelTest::exposesFavoriteRoles()
{
    SidebarFavoritesModel model;
    model.setItems({favorite(QStringLiteral("/one"), QStringLiteral("One"))});

    const QHash<int, QByteArray> roles = model.roleNames();
    QCOMPARE(roles.value(SidebarFavoritesModel::PathRole), QByteArrayLiteral("path"));
    QCOMPARE(roles.value(SidebarFavoritesModel::LabelRole), QByteArrayLiteral("label"));
    QCOMPARE(roles.value(SidebarFavoritesModel::IconRole), QByteArrayLiteral("icon"));
    QCOMPARE(
        model.data(model.index(0, 0), SidebarFavoritesModel::PathRole).toString(),
        QStringLiteral("/one"));
}

void SidebarFavoritesModelTest::movesRowsWithQtMoveSignals()
{
    SidebarFavoritesModel model;
    model.setItems({favorite(QStringLiteral("/one"), QStringLiteral("One")),
                    favorite(QStringLiteral("/two"), QStringLiteral("Two")),
                    favorite(QStringLiteral("/three"), QStringLiteral("Three"))});
    QSignalSpy movedSpy(&model, &QAbstractItemModel::rowsMoved);

    QVERIFY(model.beginDrag(QStringLiteral("/one")));
    QVERIFY(model.moveFavorite(QStringLiteral("/one"), 2));
    QCOMPARE(model.indexOfPath(QStringLiteral("/one")), 2);
    QCOMPARE(movedSpy.count(), 1);
    QCOMPARE(movedSpy.constFirst().at(1).toInt(), 0);
    QCOMPARE(movedSpy.constFirst().at(2).toInt(), 0);
    QCOMPARE(movedSpy.constFirst().at(4).toInt(), 3);
}

void SidebarFavoritesModelTest::commitsOnlyAfterExplicitCommit()
{
    SidebarFavoritesModel model;
    model.setItems({favorite(QStringLiteral("/one"), QStringLiteral("One")),
                    favorite(QStringLiteral("/two"), QStringLiteral("Two"))});

    QVERIFY(model.beginDrag(QStringLiteral("/two")));
    QVERIFY(model.moveFavorite(QStringLiteral("/two"), 0));
    QCOMPARE(model.items().constFirst().toMap().value(QStringLiteral("path")).toString(),
             QStringLiteral("/two"));
    QCOMPARE(model.commitDrag().constFirst().toMap().value(QStringLiteral("path")).toString(),
             QStringLiteral("/two"));
    QVERIFY(!model.dragActive());
}

void SidebarFavoritesModelTest::cancelsBackToOriginalOrder()
{
    SidebarFavoritesModel model;
    model.setItems({favorite(QStringLiteral("/one"), QStringLiteral("One")),
                    favorite(QStringLiteral("/two"), QStringLiteral("Two"))});
    QSignalSpy resetSpy(&model, &QAbstractItemModel::modelReset);

    QVERIFY(model.beginDrag(QStringLiteral("/one")));
    QVERIFY(model.moveFavorite(QStringLiteral("/one"), 1));
    model.cancelDrag();

    QCOMPARE(model.indexOfPath(QStringLiteral("/one")), 0);
    QCOMPARE(model.indexOfPath(QStringLiteral("/two")), 1);
    QVERIFY(!model.dragActive());
    QCOMPARE(resetSpy.count(), 1);
}

QTEST_GUILESS_MAIN(SidebarFavoritesModelTest)

#include "tst_sidebar_favorites_model.moc"
