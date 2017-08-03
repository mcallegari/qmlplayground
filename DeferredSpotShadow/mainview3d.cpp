#include <QQmlComponent>
#include <QQmlProperty>

#include "mainview3d.h"

MainView3D::MainView3D(QQuickView *view, QObject *parent)
    : QObject(parent)
    , m_view(view)
{
    //qRegisterMetaType<Qt3DCore::QEntity*>();
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

void MainView3D::initializeFixture(quint32 fxID, QComponent *picker, QSceneLoader *loader,
                                   QLayer *layer, QEffect *effect)
{
    // The QSceneLoader instance is a component of an entity. The loaded scene
    // tree is added under this entity.
    QVector<QEntity *> entities = loader->entities();

    if (entities.isEmpty())
        return;

    // Technically there could be multiple entities referencing the scene loader
    // but sharing is discouraged, and in our case there will be one anyhow.
    QEntity *root = entities[0];

    qDebug() << "There are" << root->children().count() << "submeshes in the loaded fixture";

    FixtureMesh *meshRef = m_entitiesMap.value(fxID);

    QEntity *baseItem = root->findChild<QEntity *>("base");
    meshRef->m_armItem = root->findChild<QEntity *>("arm");
    meshRef->m_headItem = root->findChild<QEntity *>("head");

    if (baseItem != NULL)
    {
        meshRef->m_rootTransform = getTransform(baseItem);
        QMaterial *material = getMaterial(baseItem);
        if (material != NULL)
            material->setEffect(effect);
    }

    if (meshRef->m_armItem != NULL)
    {
        Qt3DCore::QTransform *transform = getTransform(meshRef->m_armItem);
        if (transform != NULL)
            QMetaObject::invokeMethod(loader, "bindPanTransform",
                    Q_ARG(QVariant, QVariant::fromValue(transform)),
                    Q_ARG(QVariant, 360));
        QMaterial *material = getMaterial(meshRef->m_armItem);
        if (material != NULL)
            material->setEffect(effect);
        meshRef->m_armItem->addComponent(layer);
    }

    if (meshRef->m_headItem != NULL)
    {
        qDebug() << "Fixture" << fxID << "has an head entity !";
        Qt3DCore::QTransform *transform = getTransform(meshRef->m_headItem);

        if (baseItem != NULL)
        {
            /* If there is a base item and a tilt channel,
             * this is either a moving head or a scanner */
            if (transform != NULL)
                QMetaObject::invokeMethod(loader, "bindTiltTransform",
                        Q_ARG(QVariant, QVariant::fromValue(transform)),
                        Q_ARG(QVariant, 270));
            QMaterial *material = getMaterial(meshRef->m_headItem);
            if (material != NULL)
                material->setEffect(effect);
            meshRef->m_headItem->addComponent(layer);

            QQmlComponent *lightComponent = new QQmlComponent(m_view->engine(), QUrl("qrc:/FixtureSpotLight.qml"));
            if (lightComponent->isError())
                qDebug() << lightComponent->errors();

            QEntity *newLightItem = qobject_cast<QEntity *>(lightComponent->create());

            if (newLightItem == NULL)
            {
                qWarning() << "Error during light component creation";
                return;
            }

            newLightItem->setParent(m_quadEntity);
            newLightItem->setProperty("transform", QVariant::fromValue(transform));

            QLayer *quadLayer = qvariant_cast<QLayer *>(m_quadEntity->property("layer"));
            newLightItem->addComponent(quadLayer);
        }
        else
        {
            meshRef->m_rootTransform = transform;
            baseItem = meshRef->m_headItem;
        }
    }

    baseItem->addComponent(layer);

    /* Hook the object picker to the base entity */
    picker->setParent(baseItem);
    baseItem->addComponent(picker);
}

void MainView3D::createMesh(QEntity *root, QLayer *layer, QEffect *effect, QEntity *quadEntity)
{
    QQmlComponent *fixtureComponent;
    fixtureComponent = new QQmlComponent(m_view->engine(), QUrl("qrc:/Fixture3DItem.qml"));
    if (fixtureComponent->isError())
        qDebug() << fixtureComponent->errors();

    QEntity *newFixtureItem = qobject_cast<QEntity *>(fixtureComponent->create());

    if (newFixtureItem == NULL)
    {
        qWarning() << "Error during fixture component creation";
        return;
    }

    m_quadEntity = quadEntity;

    newFixtureItem->setProperty("itemSource", "qrc:/moving_head.dae");
    newFixtureItem->setProperty("layer", QVariant::fromValue(layer));
    newFixtureItem->setProperty("effect", QVariant::fromValue(effect));
    newFixtureItem->setParent(root);

    FixtureMesh *mesh = new FixtureMesh;
    mesh->m_rootItem = newFixtureItem;
    mesh->m_rootTransform = NULL;
    mesh->m_armItem = NULL;
    mesh->m_headItem = NULL;
    m_entitiesMap[0] = mesh;
}

void MainView3D::setPanTilt(quint32 fxID, int panDegrees, int tiltDegrees)
{
    if (m_entitiesMap.contains(fxID) == false)
        return;

    QEntity *fxItem = m_entitiesMap[fxID]->m_rootItem;

    QMetaObject::invokeMethod(fxItem, "setPosition",
            Q_ARG(QVariant, panDegrees),
            Q_ARG(QVariant, tiltDegrees));
}
