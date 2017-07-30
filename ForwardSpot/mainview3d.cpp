
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

void MainView3D::initializeFixture(quint32 fxID, QComponent *picker, Qt3DRender::QSceneLoader *loader, QSpotLight *light)
{
    FixtureMesh *meshRef = new FixtureMesh;
    meshRef->m_rootItem = NULL;
    meshRef->m_rootTransform = NULL;
    meshRef->m_armItem = NULL;
    meshRef->m_headItem = NULL;
    m_entitiesMap[fxID] = meshRef;

    // The QSceneLoader instance is a component of an entity. The loaded scene
    // tree is added under this entity.
    QVector<QEntity *> entities = loader->entities();

    if (entities.isEmpty())
        return;

    // Technically there could be multiple entities referencing the scene loader
    // but sharing is discouraged, and in our case there will be one anyhow.
    QEntity *root = entities[0];

    qDebug() << "There are" << root->children().count() << "submeshes in the loaded fixture";

    QEntity *baseItem = root->findChild<QEntity *>("base");
    meshRef->m_armItem = root->findChild<QEntity *>("arm");
    meshRef->m_headItem = root->findChild<QEntity *>("head");

    if (baseItem != NULL)
    {
        meshRef->m_rootTransform = getTransform(baseItem);
    }
    if (meshRef->m_armItem != NULL)
    {
        Qt3DCore::QTransform *transform = getTransform(meshRef->m_armItem);
        if (transform != NULL)
            QMetaObject::invokeMethod(loader, "bindPanTransform",
                    Q_ARG(QVariant, QVariant::fromValue(transform)),
                    Q_ARG(QVariant, 360));
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

            meshRef->m_headItem->addComponent(light);
        }
        else
        {
            meshRef->m_rootTransform = transform;
            baseItem = meshRef->m_headItem;
        }
    }


    //meshRef->m_rootTransform->setTranslation(QVector3D(0.0, 1.0, 0.0));

    /* Hook the object picker to the base entity */
    picker->setParent(baseItem);
    baseItem->addComponent(picker);
}
