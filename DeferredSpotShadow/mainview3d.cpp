#include <QDebug>
#include <QQuickItem>
#include <QQmlContext>
#include <QQmlComponent>

#include <Qt3DCore/QTransform>
#include <Qt3DRender/QGeometry>
#include <Qt3DRender/QAttribute>
#include <Qt3DRender/QBuffer>

#include "mainview3d.h"

MainView3D::MainView3D(QQuickView *view, QObject *parent)
    : QObject(parent)
    , m_view(view)
    , m_scene3D(NULL)
    , m_sceneRootEntity(NULL)
    , m_quadMaterial(NULL)
    , m_ambientIntensity(0.8)
{
    qRegisterMetaType<Qt3DCore::QEntity*>();

    m_fixtureComponent = new QQmlComponent(m_view->engine(), QUrl("qrc:/Fixture3DItem.qml"));
    if (m_fixtureComponent->isError())
        qDebug() << m_fixtureComponent->errors();

    m_selectionComponent = new QQmlComponent(m_view->engine(), QUrl("qrc:/SelectionEntity.qml"));
    if (m_selectionComponent->isError())
        qDebug() << m_selectionComponent->errors();
}

void MainView3D::initialize3DProperties()
{
    m_scene3D = qobject_cast<QQuickItem*>(m_view->rootObject()->findChild<QObject *>("scene3DItem"));

    qDebug() << Q_FUNC_INFO << m_scene3D;

    if (m_scene3D == NULL)
    {
        qDebug() << "Scene3DItem not found !";
        return;
    }

    m_sceneRootEntity = m_scene3D->findChild<QEntity *>("sceneRootEntity");
    if (m_sceneRootEntity == NULL)
    {
        qDebug() << "sceneRootEntity not found !";
        return;
    }

    m_quadEntity = m_scene3D->findChild<QEntity *>("quadEntity");
    if (m_quadEntity == NULL)
    {
        qDebug() << "quadEntity not found !";
        return;
    }

    m_quadMaterial = m_quadEntity->findChild<QMaterial *>("lightPassMaterial");
    if (m_quadMaterial == NULL)
    {
        qDebug() << "lightPassMaterial not found !";
        return;
    }

    qDebug() << m_sceneRootEntity << m_quadEntity << m_quadMaterial;
/*
    if (m_stageEntity == NULL)
    {
        QQmlComponent *stageComponent = new QQmlComponent(m_view->engine(), QUrl(m_stageResourceList.at(m_stageIndex)));
        if (stageComponent->isError())
            qDebug() << stageComponent->errors();

        m_stageEntity = qobject_cast<QEntity *>(stageComponent->create());
        m_stageEntity->setParent(m_sceneRootEntity);

        QLayer *sceneDeferredLayer = m_sceneRootEntity->property("deferredLayer").value<QLayer *>();
        QEffect *sceneEffect = m_sceneRootEntity->property("geometryPassEffect").value<QEffect *>();
        m_stageEntity->setProperty("sceneLayer", QVariant::fromValue(sceneDeferredLayer));
        m_stageEntity->setProperty("effect", QVariant::fromValue(sceneEffect));
    }
*/
}

void MainView3D::resetItems()
{
    qDebug() << "Resetting 3D items...";
    QMapIterator<quint32, FixtureMesh*> it(m_entitiesMap);
    while(it.hasNext())
    {
        it.next();
        FixtureMesh *e = it.value();
        delete e->m_rootItem;
        delete e->m_selectionBox;
    }

    m_entitiesMap.clear();
}

void MainView3D::sceneReady()
{
    qDebug() << "Scene entity ready";
}

void MainView3D::quadReady()
{
    qDebug() << "Quad material ready";

    QMetaObject::invokeMethod(this, "slotRefreshView", Qt::QueuedConnection);
}

void MainView3D::slotRefreshView()
{
    resetItems();

    initialize3DProperties();

    qDebug() << "Refreshing 3D view...";

    // automatically create one fixture
    createFixtureItem();
}

Qt3DCore::QTransform *MainView3D::getTransform(QEntity *entity)
{
    if (entity == NULL)
        return NULL;

    for (QComponent *component : entity->components()) // C++11
    {
        //qDebug() << component->metaObject()->className();
        Qt3DCore::QTransform *transform = qobject_cast<Qt3DCore::QTransform *>(component);
        if (transform)
            return transform;
    }

    return NULL;
}

QMaterial *MainView3D::getMaterial(QEntity *entity)
{
    if (entity == NULL)
        return NULL;

    for (QComponent *component : entity->components()) // C++11
    {
        //qDebug() << component->metaObject()->className();
        QMaterial *material = qobject_cast<QMaterial *>(component);
        if (material)
            return material;
    }

    return NULL;
}

unsigned int MainView3D::getNewLightIndex()
{
    unsigned int newIdx = 0;

    for (FixtureMesh *mesh : m_entitiesMap.values())
    {
        if (mesh->m_lightIndex >= newIdx)
            newIdx = mesh->m_lightIndex + 1;
    }

    return newIdx;
}

void MainView3D::updateLightPosition(FixtureMesh *meshRef)
{
    if (meshRef == NULL)
        return;

    QVector3D newLightPos = meshRef->m_rootTransform->translation();
    if (meshRef->m_armItem)
    {
        Qt3DCore::QTransform *armTransform = getTransform(meshRef->m_armItem);
        newLightPos += armTransform->translation();
    }
    if (meshRef->m_headItem)
    {
        Qt3DCore::QTransform *headTransform = getTransform(meshRef->m_headItem);
        newLightPos += headTransform->translation();
    }
    meshRef->m_rootItem->setProperty("lightPosition", newLightPos);
}

QVector3D MainView3D::lightPosition(quint32 fixtureID)
{
    FixtureMesh *meshRef = m_entitiesMap.value(fixtureID, NULL);
    if (meshRef == NULL)
        return QVector3D();

    return meshRef->m_rootItem->property("lightPosition").value<QVector3D>();
}

void MainView3D::getMeshCorners(QGeometryRenderer *mesh,
                                QVector3D &minCorner,
                                QVector3D &maxCorner)
{
    minCorner = QVector3D();
    maxCorner =  QVector3D();

    QGeometry *meshGeometry = mesh->geometry();

    if (!meshGeometry)
        return;

    Qt3DRender::QAttribute *vPosAttribute = nullptr;
    for (Qt3DRender::QAttribute *attribute : meshGeometry->attributes())
    {
        if (attribute->name() == Qt3DRender::QAttribute::defaultPositionAttributeName())
        {
            vPosAttribute = attribute;
            break;
        }
    }
    if (vPosAttribute)
    {
        const float *bufferPtr =
                reinterpret_cast<const float *>(vPosAttribute->buffer()->data().constData());
        uint stride = vPosAttribute->byteStride() / sizeof(float);
        uint offset = vPosAttribute->byteOffset() / sizeof(float);
        bufferPtr += offset;
        uint vertexCount = vPosAttribute->count();
        uint dataCount = vPosAttribute->buffer()->data().size() / sizeof(float);

        // Make sure we have valid data
        if (((vertexCount * stride) + offset) > dataCount)
            return;

        float minX = FLT_MAX;
        float minY = FLT_MAX;
        float minZ = FLT_MAX;
        float maxX = -FLT_MAX;
        float maxY = -FLT_MAX;
        float maxZ = -FLT_MAX;

        if (stride)
            stride = stride - 3; // Three floats per vertex

        for (uint i = 0; i < vertexCount; i++)
        {
            float xVal = *bufferPtr++;
            minX = qMin(xVal, minX);
            maxX = qMax(xVal, maxX);
            float yVal = *bufferPtr++;
            minY = qMin(yVal, minY);
            maxY = qMax(yVal, maxY);
            float zVal = *bufferPtr++;
            minZ = qMin(zVal, minZ);
            maxZ = qMax(zVal, maxZ);
            bufferPtr += stride;
        }

        minCorner = QVector3D(minX, minY, minZ);
        maxCorner = QVector3D(maxX, maxY, maxZ);
    }
}

void MainView3D::addVolumes(FixtureMesh *meshRef, QVector3D minCorner, QVector3D maxCorner)
{
    if (meshRef == NULL)
        return;

    // calculate the current bounding volume minimum/maximum positions
    float mminX = meshRef->m_volume.m_center.x() - (meshRef->m_volume.m_extents.x() / 2.0f);
    float mminY = meshRef->m_volume.m_center.y() - (meshRef->m_volume.m_extents.y() / 2.0f);
    float mminZ = meshRef->m_volume.m_center.z() - (meshRef->m_volume.m_extents.z() / 2.0f);
    float mmaxX = meshRef->m_volume.m_center.x() + (meshRef->m_volume.m_extents.x() / 2.0f);
    float mmaxY = meshRef->m_volume.m_center.y() + (meshRef->m_volume.m_extents.y() / 2.0f);
    float mmaxZ = meshRef->m_volume.m_center.z() + (meshRef->m_volume.m_extents.z() / 2.0f);

    //qDebug() << "volume pos" << mminX << mminY << mminZ << mmaxX << mmaxY << mmaxZ;

    // determine the minimum/maximum vertices positions between two volumes
    float vminX = qMin(mminX, minCorner.x());
    float vminY = qMin(mminY, minCorner.y());
    float vminZ = qMin(mminZ, minCorner.z());
    float vmaxX = qMax(mmaxX, maxCorner.x());
    float vmaxY = qMax(mmaxY, maxCorner.y());
    float vmaxZ = qMax(mmaxZ, maxCorner.z());

    //qDebug() << "vertices pos" << vminX << vminY << vminZ << vmaxX << vmaxY << vmaxZ;

    meshRef->m_volume.m_extents = QVector3D(vmaxX - vminX, vmaxY - vminY, vmaxZ - vminZ);
    meshRef->m_volume.m_center = QVector3D(vminX + meshRef->m_volume.m_extents.x() / 2.0f,
                                           vminY + meshRef->m_volume.m_extents.y() / 2.0f,
                                           vminZ + meshRef->m_volume.m_extents.z() / 2.0f);

    qDebug() << "-- extent" << meshRef->m_volume.m_extents << "-- center" << meshRef->m_volume.m_center;
}

QEntity *MainView3D::inspectEntity(QEntity *entity, FixtureMesh *meshRef,
                                   QLayer *layer, QEffect *effect,
                                   bool calculateVolume, QVector3D translation)
{
    if (entity == NULL)
        return NULL;

    QEntity *baseItem = NULL;
    QGeometryRenderer *geom = NULL;

    for (QComponent *component : entity->components()) // C++11
    {
        //qDebug() << component->metaObject()->className();

        QMaterial *material = qobject_cast<QMaterial *>(component);
        Qt3DCore::QTransform *transform = qobject_cast<Qt3DCore::QTransform *>(component);
        if (geom == NULL)
            geom = qobject_cast<QGeometryRenderer *>(component);

        if (material)
            material->setEffect(effect);

        if (transform)
            translation += transform->translation();
    }

    if (geom && calculateVolume)
    {
        QVector3D minCorner, maxCorner;
        getMeshCorners(geom, minCorner, maxCorner);
        minCorner += translation;
        maxCorner += translation;
        qDebug() << "Entity" << entity->objectName() << "translation:" << translation << ", min:" << minCorner << ", max:" << maxCorner;

        addVolumes(meshRef, minCorner, maxCorner);
    }

    for (QEntity *subEntity : entity->findChildren<QEntity *>(QString(), Qt::FindDirectChildrenOnly))
        baseItem = inspectEntity(subEntity, meshRef, layer, effect, calculateVolume, translation);

    entity->addComponent(layer);

    if (entity->objectName() == "base")
        baseItem = entity;
    else if (entity->objectName() == "arm")
        meshRef->m_armItem = entity;
    else if (entity->objectName() == "head")
        meshRef->m_headItem = entity;

    return baseItem;
}

void MainView3D::initializeFixture(quint32 fxID, QEntity *fxEntity, QComponent *picker, QSceneLoader *loader)
{
    bool calculateVolume = false;

    qDebug() << "Initialize fixture" << fxID;

    // The QSceneLoader instance is a component of an entity. The loaded scene
    // tree is added under this entity.
    QVector<QEntity *> entities = loader->entities();

    if (entities.isEmpty())
        return;

    // Technically there could be multiple entities referencing the scene loader
    // but sharing is discouraged, and in our case there will be one anyhow.
    QEntity *root = entities[0];

    qDebug() << "There are" << root->children().count() << "components in the loaded fixture";

    fxEntity->setParent(m_sceneRootEntity);

    QLayer *sceneDeferredLayer = m_sceneRootEntity->property("deferredLayer").value<QLayer *>();
    QEffect *sceneEffect = m_sceneRootEntity->property("geometryPassEffect").value<QEffect *>();

    QVector3D translation;
    FixtureMesh *meshRef = m_entitiesMap.value(fxID);
    meshRef->m_rootItem = fxEntity;
    meshRef->m_rootTransform = getTransform(meshRef->m_rootItem);

    // If this model has been already loaded, re-use the cached bounding volume
    if (m_boundingVolumesMap.contains(loader->source()))
        meshRef->m_volume = m_boundingVolumesMap[loader->source()];
    else
        calculateVolume = true;

    // Walk through the scene tree and add each mesh to the deferred pipeline.
    // If needed, calculate also the bounding volume */
    QEntity *baseItem = inspectEntity(root, meshRef, sceneDeferredLayer, sceneEffect, calculateVolume, translation);

    qDebug() << "Calculated volume" << meshRef->m_volume.m_extents << meshRef->m_volume.m_center;

    if (calculateVolume)
        m_boundingVolumesMap[loader->source()] = meshRef->m_volume;

    if (meshRef->m_armItem)
    {
        qDebug() << "Fixture" << fxID << "has an arm entity";
        Qt3DCore::QTransform *transform = getTransform(meshRef->m_armItem);
        if (transform != NULL)
            QMetaObject::invokeMethod(meshRef->m_rootItem, "bindPanTransform",
                    Q_ARG(QVariant, QVariant::fromValue(transform)),
                    Q_ARG(QVariant, 360.0));
    }

    if (meshRef->m_headItem)
    {
        qDebug() << "Fixture" << fxID << "has a head entity";
        Qt3DCore::QTransform *transform = getTransform(meshRef->m_headItem);

        if (baseItem != NULL)
        {
            // If there is a base item and a tilt channel,
            // this is either a moving head or a scanner
            if (transform != NULL)
                QMetaObject::invokeMethod(meshRef->m_rootItem, "bindTiltTransform",
                        Q_ARG(QVariant, QVariant::fromValue(transform)),
                        Q_ARG(QVariant, 270.0));
        }

        if (m_quadEntity)
        {
            QMetaObject::invokeMethod(m_quadMaterial, "addLight",
                    Q_ARG(QVariant, QVariant::fromValue(meshRef->m_rootItem)),
                    Q_ARG(QVariant, meshRef->m_lightIndex + 1),
                    Q_ARG(QVariant, QVariant::fromValue(meshRef->m_rootTransform)));
        }

        //meshRef->m_rootItem->setProperty("focusMinDegrees", focusMin);
        //meshRef->m_rootItem->setProperty("focusMaxDegrees", focusMax);
    }

    /* move the root mesh first */
    meshRef->m_rootTransform->setTranslation(QVector3D(meshRef->m_lightIndex * 1, 2, 0));

    /* recalculate the light position */
    if (meshRef->m_headItem)
        updateLightPosition(meshRef);

    /* Hook the object picker to the base entity */
    picker->setParent(meshRef->m_rootItem);
    meshRef->m_rootItem->addComponent(picker);

    QLayer *selectionLayer = m_sceneRootEntity->property("selectionLayer").value<QLayer *>();
    QGeometryRenderer *selectionMesh = m_sceneRootEntity->property("selectionMesh").value<QGeometryRenderer *>();

    meshRef->m_selectionBox = qobject_cast<QEntity *>(m_selectionComponent->create());
    meshRef->m_selectionBox->setParent(m_sceneRootEntity);

    meshRef->m_selectionBox->setProperty("selectionLayer", QVariant::fromValue(selectionLayer));
    meshRef->m_selectionBox->setProperty("geometryPassEffect", QVariant::fromValue(sceneEffect));
    meshRef->m_selectionBox->setProperty("selectionMesh", QVariant::fromValue(selectionMesh));

    meshRef->m_selectionBox->setProperty("extents", meshRef->m_volume.m_extents);
    meshRef->m_selectionBox->setProperty("center", meshRef->m_volume.m_center);

    if (meshRef->m_rootTransform != NULL)
        QMetaObject::invokeMethod(meshRef->m_selectionBox, "bindFixtureTransform",
                Q_ARG(QVariant, fxID),
                Q_ARG(QVariant, QVariant::fromValue(meshRef->m_rootTransform)));
}

void MainView3D::createFixtureItem()
{
    if (m_quadEntity == NULL)
        initialize3DProperties();

    FixtureMesh *mesh = new FixtureMesh;
    mesh->m_rootItem = NULL;
    mesh->m_rootTransform = NULL;
    mesh->m_armItem = NULL;
    mesh->m_headItem = NULL;
    mesh->m_lightIndex = getNewLightIndex();
    mesh->m_selectionBox = NULL;
    m_entitiesMap[mesh->m_lightIndex] = mesh;

    qDebug() << "[MainView3D] Creating new fixture with ID" << mesh->m_lightIndex;

    QEntity *newItem = qobject_cast<QEntity *>(m_fixtureComponent->create());
    newItem->setProperty("fixtureID", mesh->m_lightIndex);
    newItem->setProperty("itemSource", "qrc:/moving_head.dae");
    newItem->setParent(m_sceneRootEntity);
}

void MainView3D::setPanTilt(int panDegrees, int tiltDegrees)
{
    for (quint32 fxID : m_selectedFixtures)
    {
        if (m_entitiesMap.contains(fxID) == false)
            continue;

        QEntity *fxItem = m_entitiesMap[fxID]->m_rootItem;

        QMetaObject::invokeMethod(fxItem, "setPosition",
                Q_ARG(QVariant, panDegrees),
                Q_ARG(QVariant, tiltDegrees));
    }
}

bool MainView3D::isFixtureSelected(quint32 fxID)
{
    return m_selectedFixtures.contains(fxID) ? true : false;
}

void MainView3D::setFixtureSelection(quint32 fxID, bool enable)
{
    qDebug() << "[View3D] fixture" << fxID << "selected:" << enable;

    if (m_selectedFixtures.contains(fxID))
    {
        if (enable == false)
            m_selectedFixtures.removeAll(fxID);
        else
            return;
    }
    else
    {
        if (enable)
            m_selectedFixtures.append(fxID);
        else
            return;
    }

    FixtureMesh *meshRef = m_entitiesMap.value(fxID, NULL);
    if (meshRef)
    {
        meshRef->m_rootItem->setProperty("isSelected", enable);
        meshRef->m_selectionBox->setProperty("isSelected", enable);
    }
}

float MainView3D::ambientIntensity() const
{
    return m_ambientIntensity;
}

void MainView3D::setAmbientIntensity(float ambientIntensity)
{
    if (m_ambientIntensity == ambientIntensity)
        return;

    m_ambientIntensity = ambientIntensity;
    emit ambientIntensityChanged(m_ambientIntensity);
}
