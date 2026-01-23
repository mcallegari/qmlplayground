#include <QtGui/QGuiApplication>
#include <QtCore/QCoreApplication>
#include <QtQml/QQmlApplicationEngine>
#include <QtQml/qqml.h>
#include <QtCore/QPointer>
#include <QtCore/QTimer>

#include "qml/CameraItem.h"
#include "qml/LightItem.h"
#include "qml/MeshItem.h"
#include "qml/MovingHeadItem.h"
#include "qml/ModelItem.h"
#include "qml/CubeItem.h"
#include "qml/BeamBarItem.h"
#include "qml/SphereItem.h"
#include "qml/StaticLightItem.h"
#include "qml/HazerItem.h"
#include "qml/RhiQmlItem.h"
#include "qml/PixelBarItem.h"
#include "qml/VideoItem.h"

#define RHIPIPELINE_CPP_SCENE_DEMO 0

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);

    qmlRegisterType<RhiQmlItem>("RhiQmlItem", 1, 0, "RhiQmlItem");
    qmlRegisterUncreatableType<RhiQmlItem>("RhiQmlItem", 1, 0, "Scene", "Scene enums only");
    qmlRegisterType<LightItem>("RhiQmlItem", 1, 0, "Light");
    qmlRegisterType<HazerItem>("RhiQmlItem", 1, 0, "Hazer");
    qmlRegisterType<CameraItem>("RhiQmlItem", 1, 0, "Camera");
    qmlRegisterType<ModelItem>("RhiQmlItem", 1, 0, "Model");
    qmlRegisterType<MovingHeadItem>("RhiQmlItem", 1, 0, "MovingHeadLight");
    qmlRegisterType<CubeItem>("RhiQmlItem", 1, 0, "Cube");
    qmlRegisterType<SphereItem>("RhiQmlItem", 1, 0, "Sphere");
    qmlRegisterType<StaticLightItem>("RhiQmlItem", 1, 0, "StaticLight");
    qmlRegisterType<PixelBarItem>("RhiQmlItem", 1, 0, "PixelBar");
    qmlRegisterType<BeamBarItem>("RhiQmlItem", 1, 0, "BeamBar");
    qmlRegisterType<VideoItem>("RhiQmlItem", 1, 0, "VideoItem");

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

#if RHIPIPELINE_CPP_SCENE_DEMO
    QTimer::singleShot(0, [&engine]()
    {
        if (engine.rootObjects().isEmpty())
            return;
        QObject *root = engine.rootObjects().first();
        auto *renderer = root->findChild<RhiQmlItem *>(QStringLiteral("sceneRenderer"));
        if (!renderer)
            return;

        auto *spot = new LightItem(renderer);
        spot->setType(LightItem::Spotlight);
        spot->setPosition(QVector3D(-2.0f, 4.0f, 1.0f));
        spot->setDirection(QVector3D(0.4f, -1.0f, -0.2f));
        spot->setColor(QVector3D(0.8f, 0.9f, 1.0f));
        spot->setIntensity(4.0f);
        spot->setRange(100.0f);
        spot->setConeAngle(25.0f);
        spot->setQualitySteps(40);
        spot->setBeamRadius(0.2f);
        spot->setGoboPath(QStringLiteral("gobos/gobo00024.svg"));
        spot->setCastShadows(true);

        auto *led = new CubeItem(renderer);
        led->setPosition(QVector3D(0.0f, -1.6f, 2.0f));
        led->setScale(QVector3D(0.2f, 0.05f, 0.2f));
        led->setBaseColor(QVector3D(0.05f, 0.05f, 0.05f));
        led->setEmissiveColor(QVector3D(2.0f, 0.2f, 0.2f));

        auto *pulse = new QTimer(renderer);
        QPointer<LightItem> spotPtr = spot;
        QPointer<CubeItem> ledPtr = led;
        QObject::connect(pulse, &QTimer::timeout, renderer, [spotPtr, ledPtr, pulse]()
        {
            static float phase = 0.0f;
            phase += 0.08f;
            const float s = (qSin(phase) * 0.5f + 0.5f);
            if (spotPtr)
                spotPtr->setIntensity(2.0f + s * 4.0f);
            if (ledPtr)
                ledPtr->setEmissiveColor(QVector3D(1.5f + s * 2.0f, 0.2f, 0.2f));
            if (!spotPtr && !ledPtr)
                pulse->stop();
        });
        pulse->start(33);

        QTimer::singleShot(10000, spot, &QObject::deleteLater);
    });
#endif

    return app.exec();
}
