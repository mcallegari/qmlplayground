
QT += 3dcore 3drender 3dinput 3dquick qml quick 3dquickextras

HEADERS += \

OTHER_FILES += \
    main.qml \
    DeferredRenderer.qml \
    FinalEffect.qml \
    SceneEffect.qml \
    AnimatedEntity.qml \
    SceneEntity.qml \
    ScreenQuadEntity.qml \
    GBuffer.qml

RESOURCES += \
    hundred-lights.qrc

SOURCES += \
    main.cpp
