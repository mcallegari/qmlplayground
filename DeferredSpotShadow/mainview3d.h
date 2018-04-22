#include <QObject>
#include <QQuickView>

#include <Qt3DCore/QEntity>
#include <Qt3DCore/QTransform>
#include <Qt3DRender/QSceneLoader>
#include <Qt3DRender/QMaterial>
#include <Qt3DRender/QLayer>
#include <Qt3DRender/QEffect>
#include <Qt3DRender/QGeometryRenderer>

using namespace Qt3DCore;
using namespace Qt3DRender;

typedef struct
{
    QVector3D m_extents;
    QVector3D m_center;
} BoundingVolume;

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
    /** The attached light index */
    unsigned int m_lightIndex;
    /** The bounding volume information */
    BoundingVolume m_volume;
    /** The selection box entity */
    QEntity *m_selectionBox;

} FixtureMesh;

class MainView3D : public QObject
{
    Q_OBJECT

    Q_PROPERTY(float ambientIntensity READ ambientIntensity WRITE setAmbientIntensity NOTIFY ambientIntensityChanged)

public:
    MainView3D(QQuickView *view, QObject *parent = 0);
    ~MainView3D() { }

    void resetItems();

    Q_INVOKABLE void sceneReady();
    Q_INVOKABLE void quadReady();

    Q_INVOKABLE void initializeFixture(quint32 fxID, QEntity *fxEntity, QComponent *picker, QSceneLoader *loader);

    Q_INVOKABLE void createFixtureItem();

    Q_INVOKABLE void setPanTilt(int panDegrees, int tiltDegrees);

    QVector3D lightPosition(quint32 fixtureID);

    /** Returns if the fixture with $fxID is currently selected */
    Q_INVOKABLE bool isFixtureSelected(quint32 fxID);
    /** Select/Deselect a fixture with the provided $fxID */
    Q_INVOKABLE void setFixtureSelection(quint32 fxID, bool enable);

    /** Get/Set the ambient light intensity */
    float ambientIntensity() const;
    void setAmbientIntensity(float ambientIntensity);

protected:
    /** First time 3D view variables initializations */
    void initialize3DProperties();

    /** Bounding box volume calculation methods */
    void getMeshCorners(QGeometryRenderer *mesh, QVector3D &minCorner, QVector3D &maxCorner);
    void addVolumes(FixtureMesh *meshRef, QVector3D minCorner, QVector3D maxCorner);

    /** Recursive method to get/set all the information of a scene */
    QEntity *inspectEntity(QEntity *entity, FixtureMesh *meshRef,
                           QLayer *layer, QEffect *effect,
                           bool calculateVolume, QVector3D translation);

signals:
    void ambientIntensityChanged(qreal ambientIntensity);

protected slots:
    /** @reimp */
    void slotRefreshView();

private:
    Qt3DCore::QTransform *getTransform(QEntity *entity);
    QMaterial *getMaterial(QEntity *entity);

    unsigned int getNewLightIndex();
    void updateLightPosition(FixtureMesh *meshRef);

private:
    /** Reference to the current view window.
     *  If the context is not detached, this is equal to $m_mainView,
     *  otherwise this is an indipendent view */
    QQuickView *m_view;

    /** Pre-cached QML components for quick item creation */
    QQmlComponent *m_fixtureComponent;
    QQmlComponent *m_selectionComponent;

    /** Reference to the Scene3D component */
    QQuickItem *m_scene3D;

    /** Reference to the scene root entity for items creation */
    QEntity *m_sceneRootEntity;

    /** Reference to the light pass entity and material for uniform updates */
    QEntity *m_quadEntity;
    QMaterial *m_quadMaterial;

    /** Map of QLC+ fixture IDs and QML Entity items */
    QMap<quint32, FixtureMesh*> m_entitiesMap;

    /** Cache of the loaded models against bounding volumes */
    QMap<QUrl, BoundingVolume> m_boundingVolumesMap;

    /** The list of the currently selected Fixture IDs */
    QList<quint32> m_selectedFixtures;

    /** Ambient light amount (0.0 - 1.0) */
    float m_ambientIntensity;
};

