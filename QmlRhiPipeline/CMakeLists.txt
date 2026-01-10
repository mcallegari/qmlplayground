cmake_minimum_required(VERSION 3.20)
project(qmlrhipipeline LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui ShaderTools GuiPrivate Quick Qml Svg)
find_package(assimp QUIET)

add_executable(qmlrhipipeline
    src/main.cpp
    src/core/RhiContext.cpp
    src/core/RenderGraph.cpp
    src/core/RenderTargetCache.cpp
    src/core/ShaderManager.cpp
    src/qml/CameraItem.cpp
    src/qml/CubeItem.cpp
    src/qml/HazerItem.cpp
    src/qml/LightItem.cpp
    src/qml/MeshItem.cpp
    src/qml/MeshUtils.cpp
    src/qml/ModelItem.cpp
    src/qml/PickingUtils.cpp
    src/qml/RhiQmlItem.cpp
    src/qml/SphereItem.cpp
    src/renderer/DeferredRenderer.cpp
    src/renderer/PassDepth.cpp
    src/renderer/PassGBuffer.cpp
    src/renderer/PassLightCulling.cpp
    src/renderer/PassLighting.cpp
    src/renderer/PassShadow.cpp
    src/renderer/PassPost.cpp
    src/scene/AssimpLoader.cpp
    src/scene/Camera.cpp
    src/scene/Material.cpp
    src/scene/Mesh.cpp
    src/scene/Scene.cpp
)

target_include_directories(qmlrhipipeline PRIVATE src)

target_link_libraries(qmlrhipipeline PRIVATE Qt6::Core Qt6::Gui Qt6::GuiPrivate Qt6::Svg)
target_link_libraries(qmlrhipipeline PRIVATE Qt6::ShaderTools Qt6::Quick Qt6::Qml)

if(assimp_FOUND)
    target_link_libraries(qmlrhipipeline PRIVATE assimp::assimp)
else()
    target_compile_definitions(qmlrhipipeline PRIVATE RHIPIPELINE_NO_ASSIMP)
endif()

qt_add_shaders(qmlrhipipeline qmlrhipipeline_shaders
    PREFIX "/shaders"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/shaders"
    GLSL "430,310es"
    FILES
        shaders/gbuffer.vert
        shaders/gbuffer.frag
        shaders/gizmo.vert
        shaders/gizmo.frag
        shaders/lighting.vert
        shaders/lighting.frag
        shaders/lighting_d3d.frag
        shaders/lighting_metal.frag
        shaders/lighting_cull.frag
        shaders/post_bloom_downsample.frag
        shaders/post_bloom_upsample.frag
        shaders/post_combine.frag
        shaders/selection_box.vert
        shaders/selection_box.frag
        shaders/shadow.vert
        shaders/shadow.frag
        shaders/shadow_spot.frag
        shaders/tonemap.frag
)

qt_add_shaders(qmlrhipipeline qmlrhipipeline_compute_shaders
    PREFIX "/shaders"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/shaders"
    GLSL "310es,430"
    FILES
        shaders/light_cull.comp
)

qt_add_resources(qmlrhipipeline qmlrhipipeline_qml
    PREFIX "/"
    FILES
        qml/Main.qml
        qml/ViewGizmo.qml
)
