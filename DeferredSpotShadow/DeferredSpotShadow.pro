
SOURCE += main.cpp

QT += qml quick 3dcore 3drender 3dinput 3dquick 3dextras 3dquickextras

OTHER_FILES += \
    main.qml \
    DeferredRenderer.qml \
    FinalEffect.qml \
    Fixture3DItem.qml \
    FixtureSpotLight.qml \
    SceneEffect.qml \
    SceneEntity.qml \
    ScreenQuadEntity.qml \
    GBuffer.qml

HEADERS += mainview3d.h

SOURCES += main.cpp mainview3d.cpp

RESOURCES += DeferredSpotShadow.qrc
