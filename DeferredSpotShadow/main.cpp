
#include <QGuiApplication>
#include <QQmlContext>
#include <QQuickView>
#include <QQuickItem>
#include <QDebug>

#include "mainview3d.h"

int main(int argc, char* argv[])
{
    QGuiApplication app(argc, argv);

    QQuickView *m_view = new QQuickView();

    MainView3D *m_3DView = new MainView3D(m_view);
    m_view->rootContext()->setContextProperty("View3D", m_3DView);

    m_view->resize(1024, 1024);
    m_view->setResizeMode(QQuickView::SizeRootObjectToView);
    m_view->setSource(QUrl("qrc:/main.qml"));
    m_view->show();

    return app.exec();
}
