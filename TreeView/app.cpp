#include <QQmlContext>
#include <QDebug>

#include "app.h"
#include "treemodel.h"

App::App(QObject *parent)
    : QObject(parent)
{
    m_functionTree = new TreeModel(this);
    QStringList treeColumns;
    treeColumns << "capital" << "currency";
    m_functionTree->setColumnNames(treeColumns);
#if 0
    for (int i = 0; i < 10; i++)
    {
        QStringList vars;
        vars << "King's landing" << "Coins";
        m_functionTree->addItem(QString("Entry %1").arg(i + 1), vars);
    }
#endif

#if 1
    QStringList vars;

    vars = (QStringList() << "London" << "GBP");
    m_functionTree->addItem("United Kingdom", vars, "World");

    vars = (QStringList() << "Washington" << "USD");
    m_functionTree->addItem("United States", vars, "World/America");
    vars = (QStringList() << "Buenos Aires" << "ARS");
    m_functionTree->addItem("Argentina", vars, "World/America");

    vars = (QStringList() << "Cairo" << "EGP");
    m_functionTree->addItem("Egypt", vars, "World/Africa");

    vars = (QStringList() << "King's Landing" << "Coins");
    m_functionTree->addItem("Westeros", vars);
#endif

    m_functionTree->printTree();
}

QVariant App::treeModel()
{
    return QVariant::fromValue(m_functionTree);
}



