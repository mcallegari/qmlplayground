#include <QGuiApplication>
#include <QQmlEngine>
#include <QQmlContext>
#include <QQuickView>

#include "app.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    App myApp;

    QQuickView view;
    view.setResizeMode(QQuickView::SizeRootObjectToView);
    QQmlContext *ctxt = view.rootContext();
    ctxt->setContextProperty("myApp", &myApp);

    view.setSource(QUrl("qrc:TreeView.qml"));
    view.show();

    return app.exec();
}

