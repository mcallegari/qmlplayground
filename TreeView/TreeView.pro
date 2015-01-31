TEMPLATE = app

QT += qml quick svg

SOURCES += main.cpp \
    app.cpp \
    treemodel.cpp \
    treemodelitem.cpp

HEADERS += \
    app.h \
    treemodel.h
    treemodelitem.h

RESOURCES += qml.qrc

# Additional import path used to resolve QML modules in Qt Creator's code model
QML_IMPORT_PATH =
