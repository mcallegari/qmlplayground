#include <Qt3DCore/QEntity>
#include <Qt3DCore/QTransform>
#include <Qt3DRender/QSceneLoader>
#include <Qt3DRender/QLayer>
#include <Qt3DRender/QEffect>
#include <Qt3DRender/QMaterial>
#include <Qt3DRender/QSpotLight>

#include <QQuickView>

using namespace Qt3DCore;
using namespace Qt3DRender;

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

    Q_INVOKABLE void initializeFixture(quint32 fxID, QComponent *picker, QSceneLoader *loader,
                                       QSpotLight *light, QLayer *layer, QEffect *effect);

    Q_INVOKABLE void createMesh(QEntity *root, QLayer *layer, QEffect *effect);

    Q_INVOKABLE void setPanTilt(quint32 fxID, int panDegrees, int tiltDegrees);

private:
    Qt3DCore::QTransform *getTransform(QEntity *entity);
    QMaterial *getMaterial(QEntity *entity);

private:
    /** Reference to the current view window.
     *  If the context is not detached, this is equal to $m_mainView,
     *  otherwise this is an indipendent view */
    QQuickView *m_view;

    /** Map of QLC+ fixture IDs and QML Entity items */
    QMap<quint32, FixtureMesh*> m_entitiesMap;
};

