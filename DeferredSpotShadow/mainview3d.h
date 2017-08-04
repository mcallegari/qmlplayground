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

    unsigned int m_lightIndex;
} FixtureMesh;

class MainView3D : public QObject
{
    Q_OBJECT

    Q_PROPERTY(int lightsCount READ lightsCount WRITE setLightsCount NOTIFY lightsCountChanged)

public:
    MainView3D(QQuickView *view, QObject *parent = 0);
    ~MainView3D() { }

    Q_INVOKABLE void initializeFixture(quint32 fxID, QComponent *picker, QSceneLoader *loader, QLayer *layer, QEffect *effect);

    Q_INVOKABLE void createMesh(QEntity *root, QLayer *layer, QEffect *effect, QEntity *quadEntity);

    Q_INVOKABLE void setPanTilt(quint32 fxID, int panDegrees, int tiltDegrees);

    int lightsCount() const;
    void setLightsCount(int lightsCount);

signals:
    void lightsCountChanged(int lightsCount);

private:
    Qt3DCore::QTransform *getTransform(QEntity *entity);
    QMaterial *getMaterial(QEntity *entity);

    unsigned int getNewLightIndex();

private:
    /** Reference to the current view window.
     *  If the context is not detached, this is equal to $m_mainView,
     *  otherwise this is an indipendent view */
    QQuickView *m_view;

    QEntity *m_quadEntity;

    /** Map of QLC+ fixture IDs and QML Entity items */
    QMap<quint32, FixtureMesh*> m_entitiesMap;

    int m_lightsCount;
};

