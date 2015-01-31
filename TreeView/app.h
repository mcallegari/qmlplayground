#ifndef APP_H
#define APP_H

#include <QStringList>
#include <QObject>

#include "treemodel.h"

class Doc;

class App : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QVariant treeModel READ treeModel NOTIFY treeModelChanged)

public:
    App(QObject *parent = 0);

    QVariant treeModel();

signals:
    void treeModelChanged();

private:
    TreeModel *m_functionTree;
};

#endif // APP_H
