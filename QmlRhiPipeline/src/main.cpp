#include <QtGui/QGuiApplication>
#include <QtCore/QCoreApplication>
#include <QtQml/QQmlApplicationEngine>
#include <QtQml/qqml.h>

#include "qml/CameraItem.h"
#include "qml/LightItem.h"
#include "qml/ModelItem.h"
#include "qml/CubeItem.h"
#include "qml/RhiQmlItem.h"

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);

    qmlRegisterType<RhiQmlItem>("RhiQmlItem", 1, 0, "RhiQmlItem");
    qmlRegisterUncreatableType<RhiQmlItem>("RhiQmlItem", 1, 0, "Scene", "Scene enums only");
    qmlRegisterType<LightItem>("RhiQmlItem", 1, 0, "Light");
    qmlRegisterType<CameraItem>("RhiQmlItem", 1, 0, "Camera");
    qmlRegisterType<ModelItem>("RhiQmlItem", 1, 0, "Model");
    qmlRegisterType<CubeItem>("RhiQmlItem", 1, 0, "Cube");

    QQmlApplicationEngine engine;
    QUrl url(QStringLiteral("qrc:/qml/Main.qml"));
    if (argc > 1)
        url = QUrl::fromLocalFile(QString::fromLocal8Bit(argv[1]));

    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [url](QObject *obj, const QUrl &objUrl)
                     {
                         if (!obj && objUrl == url)
                             QCoreApplication::exit(-1);
                     }, Qt::QueuedConnection);
    engine.load(url);

    return app.exec();
}
