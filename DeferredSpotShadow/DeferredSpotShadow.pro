TEMPLATE = app

QT += 3dcore 3drenderer 3dinput 3dquick qml quick

HEADERS += \


OTHER_FILES += \
    main.qml \
    DeferredRenderer.qml \
    FinalEffect.qml \
    SceneEffect.qml \
    SpotLightMesh.qml \
    GBuffer.qml

RESOURCES += \
    DeferredSpotShadow.qrc

SOURCES += \
    main.cpp
