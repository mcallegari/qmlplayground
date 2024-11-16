
cmake_minimum_required(VERSION 3.16)
project(qmlrhipipeline LANGUAGES CXX)

set(CMAKE_BUILD_TYPE "Debug")

find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick ShaderTools)
find_package(assimp REQUIRED)

qt_standard_project_setup()

qt_add_executable(qmlrhipipeline WIN32 MACOSX_BUNDLE
    main.cpp
    rhiqmlitem.cpp rhiqmlitem.h
)

set_target_properties(qmlrhipipeline PROPERTIES
    # Prevent name clash with build subdirectory on case-insensitive file systems
    OUTPUT_NAME qmlrhipipelineapp
)

target_link_libraries(qmlrhipipeline PRIVATE
    Qt6::Core
    Qt6::GuiPrivate
    Qt6::Qml
    Qt6::Quick
    assimp
)

qt_add_qml_module(qmlrhipipeline
    URI RhiQmlItem
    QML_FILES
        main.qml
    RESOURCE_PREFIX /
    NO_RESOURCE_TARGET_PATH
)

qt_add_shaders(qmlrhipipeline "qmlrhipipeline_shaders"
    PRECOMPILE
    OPTIMIZED
    PREFIX /
    BASE shaders
    FILES
        shaders/scene_pass.vert
        shaders/scene_pass.frag
)

install(TARGETS qmlrhipipeline
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_qml_app_script(
    TARGET qmlrhipipeline
    OUTPUT_SCRIPT deploy_script
    MACOS_BUNDLE_POST_BUILD
    NO_UNSUPPORTED_PLATFORM_ERROR
    DEPLOY_USER_QML_MODULES_ON_UNSUPPORTED_PLATFORM
)
install(SCRIPT ${deploy_script})
