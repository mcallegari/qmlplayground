#include <Qt3DCore/QEntity>
#include <Qt3DCore/QTransform>
#include <Qt3DRender/QSceneLoader>

#include <QQuickView>

using namespace Qt3DCore;

typedef struct
{
    /** Reference to the fixture root item, for hierarchy walk and function calls */
    QEntity *m_rootItem;
    /** Reference to the root item tranform component, to perform translations/rotations */
    Qt3DCore::QTransform *m_rootTransform;
    /** Reference to the arm entity used by moving heads */
    QEntity *m_armItem;
    /** Reference to the head entity used by moving heads */
    QEntity *m_headItem;

} FixtureMesh;

class MainView3D : public QObject
{
    Q_OBJECT

public:
    MainView3D(QQuickView *view, QObject *parent = 0);
    ~MainView3D() { }

    Q_INVOKABLE void initializeFixture(quint32 fxID, QComponent *picker, Qt3DRender::QSceneLoader *loader);

private:
    Qt3DCore::QTransform *getTransform(QEntity *entity);

private:
    /** Reference to the current view window.
     *  If the context is not detached, this is equal to $m_mainView,
     *  otherwise this is an indipendent view */
    QQuickView *m_view;

    /** Map of QLC+ fixture IDs and QML Entity items */
    QMap<quint32, FixtureMesh*> m_entitiesMap;
};

