#include "qml/RhiQmlItem.h"

#include <QtCore/QDebug>
#include <QtCore/QElapsedTimer>
#include <QtCore/QTimer>
#include <QtCore/QMetaObject>
#include <QtCore/QVariant>
#include <QtCore/QLatin1String>
#include <QtMath>
#include <memory>
#include <QtGui/QMatrix4x4>
#include <QtGui/QGuiApplication>
#include <QtGui/QMouseEvent>
#include <QtGui/QQuaternion>
#include <QtGui/QKeyEvent>
#include <QtCore/QHash>

#include "core/RhiContext.h"
#include "core/RenderTargetCache.h"
#include "core/ShaderManager.h"
#include "renderer/DeferredRenderer.h"
#include "scene/AssimpLoader.h"
#include "qml/CameraItem.h"
#include "qml/LightItem.h"
#include "qml/HazerItem.h"
#include "qml/ModelItem.h"
#include "qml/StaticLightItem.h"
#include "qml/CubeItem.h"
#include "qml/SphereItem.h"
#include "qml/MeshItem.h"
#include "qml/MeshUtils.h"
#include "qml/PickingUtils.h"

namespace
{
using namespace RhiQmlUtils;

struct EmitterData
{
    QVector3D position;
    QVector3D direction;
    float diameter = 0.0f;
};

static bool isEmitterName(const QString &name)
{
    return name.contains(QLatin1String("emitter"), Qt::CaseInsensitive);
}

static QVector3D averageNormal(const Mesh &mesh)
{
    QVector3D sum(0.0f, 0.0f, 0.0f);
    for (const Vertex &v : mesh.vertices)
        sum += QVector3D(v.nx, v.ny, v.nz);
    if (sum.isNull())
        return QVector3D(0.0f, -1.0f, 0.0f);
    return sum.normalized();
}

static float maxPlaneDiameter(const Mesh &mesh, const QVector3D &localNormal)
{
    if (!mesh.boundsValid)
        return 0.0f;
    const QVector3D extents = mesh.boundsMax - mesh.boundsMin;
    const QVector3D col0 = mesh.modelMatrix.column(0).toVector3D();
    const QVector3D col1 = mesh.modelMatrix.column(1).toVector3D();
    const QVector3D col2 = mesh.modelMatrix.column(2).toVector3D();
    const QVector3D scaled(extents.x() * col0.length(),
                           extents.y() * col1.length(),
                           extents.z() * col2.length());
    const QVector3D absN(qAbs(localNormal.x()), qAbs(localNormal.y()), qAbs(localNormal.z()));
    if (absN.x() >= absN.y() && absN.x() >= absN.z())
        return qMax(scaled.y(), scaled.z());
    if (absN.y() >= absN.x() && absN.y() >= absN.z())
        return qMax(scaled.x(), scaled.z());
    return qMax(scaled.x(), scaled.y());
}

static QVector<EmitterData> collectEmitters(const QVector<Mesh> &meshes, int firstMesh, int meshCount)
{
    QVector<EmitterData> emitters;
    emitters.reserve(meshCount);
    for (int i = firstMesh; i < firstMesh + meshCount; ++i)
    {
        const Mesh &mesh = meshes[i];
        if (!isEmitterName(mesh.name))
            continue;
        if (!mesh.boundsValid)
            continue;
        const QVector3D localCenter = (mesh.boundsMin + mesh.boundsMax) * 0.5f;
        const QVector3D localNormal = averageNormal(mesh);
        const QVector3D worldPos = mesh.modelMatrix.map(localCenter);
        const QMatrix3x3 normalMat = mesh.modelMatrix.normalMatrix();
        const QVector3D worldNormal = QVector3D(
            normalMat(0, 0) * localNormal.x() + normalMat(0, 1) * localNormal.y() + normalMat(0, 2) * localNormal.z(),
            normalMat(1, 0) * localNormal.x() + normalMat(1, 1) * localNormal.y() + normalMat(1, 2) * localNormal.z(),
            normalMat(2, 0) * localNormal.x() + normalMat(2, 1) * localNormal.y() + normalMat(2, 2) * localNormal.z()
        ).normalized();
        const float diameter = maxPlaneDiameter(mesh, localNormal);
        emitters.push_back({ worldPos, worldNormal, diameter });
    }
    return emitters;
}

struct TransformInfo
{
    QMatrix4x4 matrix;
    QQuaternion rotation;
};

static TransformInfo makeTransform(const QVector3D &position,
                                   const QVector3D &rotationDegrees,
                                   const QVector3D &scale)
{
    TransformInfo info;
    info.matrix.translate(position);
    if (!rotationDegrees.isNull())
    {
        info.rotation = QQuaternion::fromEulerAngles(rotationDegrees);
        info.matrix.rotate(info.rotation);
    }
    if (scale != QVector3D(1.0f, 1.0f, 1.0f))
        info.matrix.scale(scale);
    return info;
}

static void applyTransform(QVector<Mesh> &meshes,
                           int firstMesh,
                           int meshCount,
                           const TransformInfo &transform,
                           const QVector3D &position,
                           bool setBase)
{
    for (int i = firstMesh; i < firstMesh + meshCount; ++i)
    {
        Mesh &mesh = meshes[i];
        if (setBase)
            mesh.baseModelMatrix = mesh.modelMatrix;
        mesh.modelMatrix = transform.matrix * mesh.baseModelMatrix;
        mesh.userOffset = position;
    }
}

static void applySelectionVisibility(QVector<Mesh> &meshes,
                                     int firstMesh,
                                     int meshCount,
                                     bool selected,
                                     bool selectable,
                                     bool visible)
{
    for (int i = firstMesh; i < firstMesh + meshCount; ++i)
    {
        Mesh &mesh = meshes[i];
        mesh.selected = selected;
        mesh.selectable = selectable;
        mesh.visible = visible;
    }
}

static void applyMaterial(QVector<Mesh> &meshes,
                          int firstMesh,
                          int meshCount,
                          const QVector3D &baseColor,
                          const QVector3D &emissiveColor,
                          float metalness,
                          float roughness)
{
    for (int i = firstMesh; i < firstMesh + meshCount; ++i)
    {
        Mesh &mesh = meshes[i];
        mesh.material.baseColor = baseColor;
        mesh.material.emissive = emissiveColor;
        mesh.material.metalness = metalness;
        mesh.material.roughness = roughness;
    }
}

template <typename RecordT>
static void syncSelectionVisibilityFromItem(RecordT &record,
                                            const MeshItem *item,
                                            QVector<Mesh> &meshes)
{
    const bool selected = item->isSelected();
    const bool selectable = item->selectable();
    const bool visible = item->visible();
    if (record.selected == selected && record.selectable == selectable && record.visible == visible)
        return;
    record.selected = selected;
    record.selectable = selectable;
    record.visible = visible;
    applySelectionVisibility(meshes, record.firstMesh, record.meshCount,
                             record.selected, record.selectable, record.visible);
}

template <typename RecordT>
static bool syncTransformFromItem(RecordT &record,
                                  const MeshItem *item,
                                  QVector<Mesh> &meshes)
{
    const QVector3D position = item->position();
    const QVector3D rotationDegrees = item->rotationDegrees();
    const QVector3D scale = item->scale();
    if (record.position == position
            && record.rotationDegrees == rotationDegrees
            && record.scale == scale)
        return false;
    record.position = position;
    record.rotationDegrees = rotationDegrees;
    record.scale = scale;
    const TransformInfo transform = makeTransform(record.position,
                                                  record.rotationDegrees,
                                                  record.scale);
    applyTransform(meshes, record.firstMesh, record.meshCount,
                   transform, record.position, false);
    return true;
}

template <typename RecordT>
static TransformInfo transformFromRecord(const RecordT &record)
{
    return makeTransform(record.position, record.rotationDegrees, record.scale);
}

static void hideEmitterMeshes(QVector<Mesh> &meshes, int firstMesh, int meshCount)
{
    for (int i = firstMesh; i < firstMesh + meshCount; ++i)
    {
        Mesh &mesh = meshes[i];
        if (isEmitterName(mesh.name))
        {
            mesh.visible = false;
            mesh.selectable = false;
        }
    }
}

template <typename RecordT, typename ItemT>
static ItemT *pickHitForRecords(const QVector<RecordT> &records, int meshIndex)
{
    for (const RecordT &record : records)
    {
        if (meshIndex >= record.firstMesh && meshIndex < record.firstMesh + record.meshCount)
            return const_cast<ItemT *>(record.item);
    }
    return nullptr;
}

class RhiQmlItemRenderer final : public QQuickRhiItemRenderer
{
public:
    enum DragType
    {
        DragBegin = 0,
        DragMove = 1,
        DragEnd = 2
    };

    void initialize(QRhiCommandBuffer *cb) override
    {
        Q_UNUSED(cb);
        if (m_initialized)
            return;

        if (!m_rhiContext.initializeExternal(rhi()))
        {
            qWarning() << "RhiQmlItemRenderer: failed to initialize external RHI context";
            return;
        }

        m_targets = std::make_unique<RenderTargetCache>(rhi());
        m_shaders = std::make_unique<ShaderManager>(rhi());
        m_renderer.initialize(&m_rhiContext, m_targets.get(), m_shaders.get());
        m_initialized = true;
    }

    void synchronize(QQuickRhiItem *item) override
    {
        auto *qmlItem = static_cast<RhiQmlItem *>(item);
        struct LightTransform
        {
            QMatrix4x4 matrix;
            QQuaternion rotation;
        };
        QHash<const LightItem *, LightTransform> staticLightTransforms;
        QHash<const StaticLightItem *, LightTransform> staticLightItemTransforms;
        QHash<const StaticLightItem *, QVector<EmitterData>> staticLightEmitters;

        QVector<RhiQmlItem::PendingModel> models;
        QVector<QObject *> selectableItems;
        qmlItem->takePendingModels(models);
        for (const auto &entry : models)
        {
            const int beforeCount = m_scene.meshes().size();
            if (!m_loader.loadModel(entry.path, m_scene, true))
            {
                qWarning() << "RhiQmlItemRenderer: failed to load model" << entry.path;
                continue;
            }
            for (int i = beforeCount; i < m_scene.meshes().size(); ++i)
            {
                Mesh &mesh = m_scene.meshes()[i];
                mesh.baseModelMatrix = mesh.modelMatrix;
                QMatrix4x4 transform;
                transform.translate(entry.position);
                if (!entry.rotationDegrees.isNull())
                {
                    const QQuaternion rot = QQuaternion::fromEulerAngles(entry.rotationDegrees);
                    transform.rotate(rot);
                }
                if (entry.scale != QVector3D(1.0f, 1.0f, 1.0f))
                    transform.scale(entry.scale);
                mesh.modelMatrix = transform * mesh.baseModelMatrix;
                mesh.userOffset = entry.position;
                if (!mesh.vertices.isEmpty())
                {
                    QVector3D minV(mesh.vertices[0].px, mesh.vertices[0].py, mesh.vertices[0].pz);
                    QVector3D maxV = minV;
                    for (const Vertex &v : mesh.vertices)
                    {
                        minV.setX(qMin(minV.x(), v.px));
                        minV.setY(qMin(minV.y(), v.py));
                        minV.setZ(qMin(minV.z(), v.pz));
                        maxV.setX(qMax(maxV.x(), v.px));
                        maxV.setY(qMax(maxV.y(), v.py));
                        maxV.setZ(qMax(maxV.z(), v.pz));
                    }
                    const QVector3D center = (minV + maxV) * 0.5f;
                }
            }
        }

        QVector<Light> lights;
        qmlItem->takePendingLights(lights);
        for (const Light &light : lights)
            m_staticLights.push_back(light);

        const auto qmlModels = qmlItem->findChildren<ModelItem *>(QString(), Qt::FindChildrenRecursively);
        for (const ModelItem *modelItem : qmlModels)
        {
            if (modelItem->path().isEmpty())
                continue;
            if (modelItem->selectable())
                selectableItems.push_back(const_cast<ModelItem *>(modelItem));
            bool found = false;
            for (ModelRecord &record : m_qmlModels)
            {
                if (record.item != modelItem)
                    continue;
                found = true;
                if (record.path != modelItem->path())
                {
                    qWarning() << "RhiQmlItemRenderer: model path changed after load for" << modelItem->path();
                    break;
                }
                syncTransformFromItem(record, modelItem, m_scene.meshes());
                syncSelectionVisibilityFromItem(record, modelItem, m_scene.meshes());
                break;
            }
            if (found)
                continue;

            const int beforeCount = m_scene.meshes().size();
            if (!m_loader.loadModel(modelItem->path(), m_scene, true))
            {
                qWarning() << "RhiQmlItemRenderer: failed to load model" << modelItem->path();
                continue;
            }
            const int meshCount = m_scene.meshes().size() - beforeCount;
            ModelRecord record;
            record.item = modelItem;
            record.path = modelItem->path();
            record.position = modelItem->position();
            record.rotationDegrees = modelItem->rotationDegrees();
            record.scale = modelItem->scale();
            record.selected = modelItem->isSelected();
            record.selectable = modelItem->selectable();
            record.visible = modelItem->visible();
            record.firstMesh = beforeCount;
            record.meshCount = meshCount;

            const TransformInfo transform = transformFromRecord(record);
            applyTransform(m_scene.meshes(), record.firstMesh, record.meshCount,
                           transform, record.position, true);
            applySelectionVisibility(m_scene.meshes(), record.firstMesh, record.meshCount,
                                     record.selected, record.selectable, record.visible);
            m_qmlModels.push_back(record);
        }

        const auto qmlStaticLights = qmlItem->findChildren<StaticLightItem *>(QString(), Qt::FindChildrenRecursively);
        for (const StaticLightItem *staticLight : qmlStaticLights)
        {
            if (staticLight->path().isEmpty())
                continue;
            if (staticLight->selectable())
                selectableItems.push_back(const_cast<StaticLightItem *>(staticLight));
            bool found = false;
            for (StaticLightRecord &record : m_qmlStaticLights)
            {
                if (record.item != staticLight)
                    continue;
                found = true;
                if (record.path != staticLight->path())
                {
                    qWarning() << "RhiQmlItemRenderer: static light path changed after load for" << staticLight->path();
                    break;
                }
                syncTransformFromItem(record, staticLight, m_scene.meshes());
                syncSelectionVisibilityFromItem(record, staticLight, m_scene.meshes());
                hideEmitterMeshes(m_scene.meshes(), record.firstMesh, record.meshCount);
                staticLightEmitters.insert(staticLight,
                                           collectEmitters(m_scene.meshes(), record.firstMesh, record.meshCount));
                const TransformInfo transform = transformFromRecord(record);
                staticLightItemTransforms.insert(staticLight, { transform.matrix, transform.rotation });
                const auto lights = staticLight->findChildren<LightItem *>(QString(), Qt::FindChildrenRecursively);
                for (const LightItem *lightItem : lights)
                    staticLightTransforms.insert(lightItem, { transform.matrix, transform.rotation });
                break;
            }
            if (found)
                continue;

            const int beforeCount = m_scene.meshes().size();
            if (!m_loader.loadModel(staticLight->path(), m_scene, true))
            {
                qWarning() << "RhiQmlItemRenderer: failed to load static light model" << staticLight->path();
                continue;
            }
            const int meshCount = m_scene.meshes().size() - beforeCount;
            StaticLightRecord record;
            record.item = staticLight;
            record.path = staticLight->path();
            record.position = staticLight->position();
            record.rotationDegrees = staticLight->rotationDegrees();
            record.scale = staticLight->scale();
            record.selected = staticLight->isSelected();
            record.selectable = staticLight->selectable();
            record.visible = staticLight->visible();
            record.firstMesh = beforeCount;
            record.meshCount = meshCount;

            const TransformInfo transform = transformFromRecord(record);
            applyTransform(m_scene.meshes(), record.firstMesh, record.meshCount,
                           transform, record.position, true);
            applySelectionVisibility(m_scene.meshes(), record.firstMesh, record.meshCount,
                                     record.selected, record.selectable, record.visible);
            hideEmitterMeshes(m_scene.meshes(), record.firstMesh, record.meshCount);
            staticLightEmitters.insert(staticLight,
                                       collectEmitters(m_scene.meshes(), record.firstMesh, record.meshCount));
            staticLightItemTransforms.insert(staticLight, { transform.matrix, transform.rotation });
            const auto lights = staticLight->findChildren<LightItem *>(QString(), Qt::FindChildrenRecursively);
            for (const LightItem *lightItem : lights)
                staticLightTransforms.insert(lightItem, { transform.matrix, transform.rotation });

            m_qmlStaticLights.push_back(record);
        }

        const auto qmlCubes = qmlItem->findChildren<CubeItem *>(QString(), Qt::FindChildrenRecursively);
        for (const CubeItem *cubeItem : qmlCubes)
        {
            if (cubeItem->selectable())
                selectableItems.push_back(const_cast<CubeItem *>(cubeItem));
            bool found = false;
            for (CubeRecord &record : m_qmlCubes)
            {
                if (record.item != cubeItem)
                    continue;
                found = true;
                syncTransformFromItem(record, cubeItem, m_scene.meshes());
                const QVector3D baseColor = cubeItem->baseColor();
                const QVector3D emissiveColor = cubeItem->emissiveColor();
                const float metalness = cubeItem->metalness();
                const float roughness = cubeItem->roughness();
                if (record.baseColor != baseColor || record.emissiveColor != emissiveColor
                        || !qFuzzyCompare(record.metalness, metalness)
                        || !qFuzzyCompare(record.roughness, roughness))
                {
                    record.baseColor = baseColor;
                    record.emissiveColor = emissiveColor;
                    record.metalness = metalness;
                    record.roughness = roughness;
                    applyMaterial(m_scene.meshes(), record.firstMesh, record.meshCount,
                                  baseColor, emissiveColor, metalness, roughness);
                }
                syncSelectionVisibilityFromItem(record, cubeItem, m_scene.meshes());
                break;
            }
            if (found)
                continue;

            const int beforeCount = m_scene.meshes().size();
            m_scene.meshes().push_back(createUnitCubeMesh());
            const int meshCount = m_scene.meshes().size() - beforeCount;
            CubeRecord record;
            record.item = cubeItem;
            record.position = cubeItem->position();
            record.rotationDegrees = cubeItem->rotationDegrees();
            record.scale = cubeItem->scale();
            record.selected = cubeItem->isSelected();
            record.selectable = cubeItem->selectable();
            record.visible = cubeItem->visible();
            record.baseColor = cubeItem->baseColor();
            record.emissiveColor = cubeItem->emissiveColor();
            record.metalness = cubeItem->metalness();
            record.roughness = cubeItem->roughness();
            record.firstMesh = beforeCount;
            record.meshCount = meshCount;

            const TransformInfo transform = transformFromRecord(record);
            applyTransform(m_scene.meshes(), record.firstMesh, record.meshCount,
                           transform, record.position, true);
            applySelectionVisibility(m_scene.meshes(), record.firstMesh, record.meshCount,
                                     record.selected, record.selectable, record.visible);
            applyMaterial(m_scene.meshes(), record.firstMesh, record.meshCount,
                          record.baseColor, record.emissiveColor, record.metalness, record.roughness);
            m_qmlCubes.push_back(record);
        }

        const auto qmlSpheres = qmlItem->findChildren<SphereItem *>(QString(), Qt::FindChildrenRecursively);
        for (const SphereItem *sphereItem : qmlSpheres)
        {
            if (sphereItem->selectable())
                selectableItems.push_back(const_cast<SphereItem *>(sphereItem));
            bool found = false;
            for (SphereRecord &record : m_qmlSpheres)
            {
                if (record.item != sphereItem)
                    continue;
                found = true;
                syncTransformFromItem(record, sphereItem, m_scene.meshes());
                const QVector3D baseColor = sphereItem->baseColor();
                const QVector3D emissiveColor = sphereItem->emissiveColor();
                const float metalness = sphereItem->metalness();
                const float roughness = sphereItem->roughness();
                if (record.baseColor != baseColor || record.emissiveColor != emissiveColor
                        || !qFuzzyCompare(record.metalness, metalness)
                        || !qFuzzyCompare(record.roughness, roughness))
                {
                    record.baseColor = baseColor;
                    record.emissiveColor = emissiveColor;
                    record.metalness = metalness;
                    record.roughness = roughness;
                    applyMaterial(m_scene.meshes(), record.firstMesh, record.meshCount,
                                  baseColor, emissiveColor, metalness, roughness);
                }
                syncSelectionVisibilityFromItem(record, sphereItem, m_scene.meshes());
                break;
            }
            if (found)
                continue;

            const int beforeCount = m_scene.meshes().size();
            m_scene.meshes().push_back(createSphereMesh());
            const int meshCount = m_scene.meshes().size() - beforeCount;
            SphereRecord record;
            record.item = sphereItem;
            record.position = sphereItem->position();
            record.rotationDegrees = sphereItem->rotationDegrees();
            record.scale = sphereItem->scale();
            record.selected = sphereItem->isSelected();
            record.selectable = sphereItem->selectable();
            record.visible = sphereItem->visible();
            record.baseColor = sphereItem->baseColor();
            record.emissiveColor = sphereItem->emissiveColor();
            record.metalness = sphereItem->metalness();
            record.roughness = sphereItem->roughness();
            record.firstMesh = beforeCount;
            record.meshCount = meshCount;

            const TransformInfo transform = transformFromRecord(record);
            applyTransform(m_scene.meshes(), record.firstMesh, record.meshCount,
                           transform, record.position, true);
            applySelectionVisibility(m_scene.meshes(), record.firstMesh, record.meshCount,
                                     record.selected, record.selectable, record.visible);
            applyMaterial(m_scene.meshes(), record.firstMesh, record.meshCount,
                          record.baseColor, record.emissiveColor, record.metalness, record.roughness);
            m_qmlSpheres.push_back(record);
        }

        qmlItem->updateSelectableItems(selectableItems);

        m_scene.lights().clear();
        m_scene.lights() = m_staticLights;
        QVector3D ambientTotal = qmlItem->ambientLight() * qmlItem->ambientIntensity();
        for (auto it = staticLightItemTransforms.cbegin(); it != staticLightItemTransforms.cend(); ++it)
        {
            const StaticLightItem *itemPtr = it.key();
            const QVector<EmitterData> emitters = staticLightEmitters.value(itemPtr);
            if (!emitters.isEmpty())
            {
                for (const EmitterData &emitter : emitters)
                {
                    Light light = itemPtr->toLight();
                    light.position = emitter.position;
                    if (!emitter.direction.isNull())
                        light.direction = emitter.direction.normalized();
                    if (emitter.diameter > 0.0f)
                        light.beamRadius = emitter.diameter * 0.5f;
                    m_scene.lights().push_back(light);
                }
                continue;
            }

            Light light = itemPtr->toLight();
            QVector4D worldPos = it.value().matrix * QVector4D(light.position, 1.0f);
            light.position = worldPos.toVector3D();
            if (!light.direction.isNull())
                light.direction = it.value().rotation.rotatedVector(light.direction).normalized();
            m_scene.lights().push_back(light);
        }
        const auto qmlLights = qmlItem->findChildren<LightItem *>(QString(), Qt::FindChildrenRecursively);
        for (const LightItem *lightItem : qmlLights)
        {
            if (lightItem->type() == LightItem::Ambient)
            {
                ambientTotal += lightItem->color() * lightItem->intensity();
                continue;
            }
            Light light = lightItem->toLight();
            const auto it = staticLightTransforms.constFind(lightItem);
            if (it != staticLightTransforms.constEnd())
            {
                QVector4D worldPos = it->matrix * QVector4D(light.position, 1.0f);
                light.position = worldPos.toVector3D();
                if (!light.direction.isNull())
                    light.direction = it->rotation.rotatedVector(light.direction).normalized();
            }
            m_scene.lights().push_back(light);
        }

        const QSize size = qmlItem->effectiveColorBufferSize();
        const float aspect = size.height() > 0 ? float(size.width()) / float(size.height()) : 1.0f;
        const CameraItem *cameraItem = qmlItem->findChild<CameraItem *>(QString(), Qt::FindChildrenRecursively);
        if (cameraItem && !qmlItem->freeCameraEnabled())
        {
            m_scene.camera().setPosition(cameraItem->position());
            m_scene.camera().setPerspective(cameraItem->fov(), aspect, cameraItem->nearPlane(), cameraItem->farPlane());
            m_scene.camera().lookAt(cameraItem->target());
        }
        else
        {
            m_scene.camera().setPosition(qmlItem->cameraPosition());
            m_scene.camera().setPerspective(qmlItem->cameraFov(), aspect, 0.01f, 300.0f);
            m_scene.camera().lookAt(qmlItem->cameraTarget());
        }
        m_scene.setAmbientLight(ambientTotal);
        m_scene.setAmbientIntensity(1.0f);
        m_scene.setSmokeAmount(qmlItem->smokeAmount());
        m_scene.setBeamModel(static_cast<Scene::BeamModel>(qmlItem->beamModel()));
        m_scene.setBloomIntensity(qmlItem->bloomIntensity());
        m_scene.setBloomRadius(qmlItem->bloomRadius());
        m_scene.setTimeSeconds(qmlItem->smokeTimeSeconds());
        m_scene.setVolumetricEnabled(qmlItem->volumetricEnabled());
        m_scene.setShadowsEnabled(qmlItem->shadowsEnabled());
        m_scene.setSmokeNoiseEnabled(qmlItem->smokeNoiseEnabled());

        const HazerItem *hazer = qmlItem->findChild<HazerItem *>(QString(), Qt::FindChildrenRecursively);
        if (hazer && hazer->enabled())
        {
            m_scene.setHazeEnabled(true);
            m_scene.setHazePosition(hazer->position());
            m_scene.setHazeDirection(hazer->direction());
            m_scene.setHazeLength(hazer->length());
            m_scene.setHazeRadius(hazer->radius());
            m_scene.setHazeDensity(hazer->density());
        }
        else
        {
            m_scene.setHazeEnabled(false);
            m_scene.setHazeDensity(0.0f);
        }

        QVector3D selectedPos;
        bool hasSelected = computeSelectionCenter(selectedPos);

        ensureGizmoMeshes();

        bool skipPick = false;
        QVector<RhiQmlItem::DragRequest> drags;
        qmlItem->takePendingDragRequests(drags);
        for (const auto &drag : drags)
        {
            if (drag.type == DragBegin && hasSelected)
            {
                PickHit gizmoHit;
                const bool hitOk = pickSceneMesh(m_scene, m_rhiContext.rhi(), drag.normPos,
                                                 PickFilter::GizmosOnly, gizmoHit);
                if (hitOk)
                {
                    const Mesh &mesh = m_scene.meshes()[gizmoHit.meshIndex];
                    if (mesh.gizmoAxis >= 0)
                    {
                        const QVector3D axisDir = axisVector(mesh.gizmoAxis);
                        QVector3D rayOrigin;
                        QVector3D rayDir;
                        if (computeRay(m_scene, m_rhiContext.rhi(), drag.normPos, rayOrigin, rayDir))
                        {
                            m_axisDragActive = false;
                            m_rotateDragActive = false;
                            m_axisDrag = -1;
                            m_rotateAxis = -1;
                            buildDragSelection();
                            if (mesh.gizmoType == 2)
                            {
                                QVector3D hitPoint;
                                if (rayPlaneIntersection(rayOrigin, rayDir, selectedPos, axisDir, hitPoint))
                                {
                                    QVector3D startVec = hitPoint - selectedPos;
                                    startVec -= axisDir * QVector3D::dotProduct(startVec, axisDir);
                                    if (!startVec.isNull())
                                    {
                                        startVec.normalize();
                                        m_rotateDragActive = true;
                                        m_rotateAxis = mesh.gizmoAxis;
                                        m_rotateStartVec = startVec;
                                        m_dragOrigin = selectedPos;
                                        skipPick = true;
                                    }
                                }
                            }
                            else
                            {
                                m_axisDragActive = true;
                                m_axisDrag = mesh.gizmoAxis;
                                m_dragOrigin = selectedPos;
                                m_dragStartT = closestAxisT(rayOrigin, rayDir, selectedPos, axisDir);
                                skipPick = true;
                            }
                        }
                    }
                }
            }
            else if (drag.type == DragMove && m_axisDragActive && !m_dragSelection.isEmpty())
            {
                const QVector3D axisDir = axisVector(m_axisDrag);
                QVector3D rayOrigin;
                QVector3D rayDir;
                if (computeRay(m_scene, m_rhiContext.rhi(), drag.normPos, rayOrigin, rayDir))
                {
                    const float t = closestAxisT(rayOrigin, rayDir, m_dragOrigin, axisDir);
                    const float delta = t - m_dragStartT;
                    const QVector3D newCenter = m_dragOrigin + axisDir * delta;
                    selectedPos = newCenter;
                    hasSelected = true;
                    for (const SelectedTransform &entry : m_dragSelection)
                    {
                        const QVector3D newPos = entry.startPos + axisDir * delta;
                        QMetaObject::invokeMethod(qmlItem, "setObjectPosition", Qt::QueuedConnection,
                                                  Q_ARG(QObject *, entry.item),
                                                  Q_ARG(QVector3D, newPos));
                    }
                }
            }
            else if (drag.type == DragMove && m_rotateDragActive && !m_dragSelection.isEmpty())
            {
                const QVector3D axisDir = axisVector(m_rotateAxis);
                QVector3D rayOrigin;
                QVector3D rayDir;
                if (computeRay(m_scene, m_rhiContext.rhi(), drag.normPos, rayOrigin, rayDir))
                {
                    QVector3D hitPoint;
                    if (rayPlaneIntersection(rayOrigin, rayDir, m_dragOrigin, axisDir, hitPoint))
                    {
                        QVector3D currVec = hitPoint - m_dragOrigin;
                        currVec -= axisDir * QVector3D::dotProduct(currVec, axisDir);
                        if (!currVec.isNull())
                        {
                            currVec.normalize();
                            const float dot = QVector3D::dotProduct(m_rotateStartVec, currVec);
                            const QVector3D cross = QVector3D::crossProduct(m_rotateStartVec, currVec);
                            const float sign = QVector3D::dotProduct(axisDir, cross) >= 0.0f ? 1.0f : -1.0f;
                            const float angleRad = qAtan2(cross.length() * sign, qBound(-1.0f, dot, 1.0f));
                            const float angleDeg = qRadiansToDegrees(angleRad);
                            const QQuaternion rot = QQuaternion::fromAxisAndAngle(axisDir, angleDeg);
                            for (const SelectedTransform &entry : m_dragSelection)
                            {
                                QVector3D newRot = entry.startRot;
                                if (m_rotateAxis == 0)
                                    newRot.setX(entry.startRot.x() + angleDeg);
                                else if (m_rotateAxis == 1)
                                    newRot.setY(entry.startRot.y() + angleDeg);
                                else
                                    newRot.setZ(entry.startRot.z() + angleDeg);
                                const QVector3D offset = entry.startPos - m_dragOrigin;
                                const QVector3D newPos = m_dragOrigin + rot.rotatedVector(offset);
                                QMetaObject::invokeMethod(qmlItem, "setObjectPosition", Qt::QueuedConnection,
                                                          Q_ARG(QObject *, entry.item),
                                                          Q_ARG(QVector3D, newPos));
                                QMetaObject::invokeMethod(qmlItem, "setObjectRotation", Qt::QueuedConnection,
                                                          Q_ARG(QObject *, entry.item),
                                                          Q_ARG(QVector3D, newRot));
                            }
                            selectedPos = m_dragOrigin;
                        }
                    }
                }
            }
            else if (drag.type == DragEnd)
            {
                m_axisDragActive = false;
                m_rotateDragActive = false;
                m_axisDrag = -1;
                m_rotateAxis = -1;
                m_dragSelection.clear();
            }
        }

        updateGizmoMeshes(hasSelected ? selectedPos : QVector3D(),
                          m_scene.camera().position(), hasSelected);

        QVector<RhiQmlItem::PickRequest> picks;
        qmlItem->takePendingPickRequests(picks);
        if (!picks.isEmpty() && !skipPick)
        {
            for (const auto &pick : picks)
            {
                PickHit hit;
                const bool hitOk = pickSceneMesh(m_scene, m_rhiContext.rhi(), pick.normPos,
                                                 PickFilter::All, hit);

                QObject *hitItem = nullptr;
                if (hitOk)
                {
                    hitItem = pickHitForRecords<ModelRecord, ModelItem>(m_qmlModels, hit.meshIndex);
                    if (!hitItem)
                        hitItem = pickHitForRecords<StaticLightRecord, StaticLightItem>(m_qmlStaticLights, hit.meshIndex);
                    if (!hitItem)
                        hitItem = pickHitForRecords<CubeRecord, CubeItem>(m_qmlCubes, hit.meshIndex);
                    if (!hitItem)
                        hitItem = pickHitForRecords<SphereRecord, SphereItem>(m_qmlSpheres, hit.meshIndex);
                }

                QMetaObject::invokeMethod(qmlItem, "dispatchPickResult", Qt::QueuedConnection,
                                          Q_ARG(QObject *, hitItem),
                                          Q_ARG(QVector3D, hitOk ? hit.worldPos : QVector3D()),
                                          Q_ARG(bool, hitOk),
                                          Q_ARG(int, int(pick.modifiers)));
            }
        }
    }

    void render(QRhiCommandBuffer *cb) override
    {
        if (!m_initialized)
            return;
        QRhiRenderTarget *rt = renderTarget();
        if (!cb || !rt)
        {
            qWarning() << "RhiQmlItemRenderer: missing cb/rt" << cb << rt;
            return;
        }

        m_rhiContext.setExternalFrame(cb, rt);
        m_renderer.render(&m_scene);
        m_rhiContext.clearExternalFrame();
    }

private:
    bool m_initialized = false;
    RhiContext m_rhiContext;
    std::unique_ptr<RenderTargetCache> m_targets;
    std::unique_ptr<ShaderManager> m_shaders;
    DeferredRenderer m_renderer;
    Scene m_scene;
    AssimpLoader m_loader;
    QVector<Light> m_staticLights;
    struct ModelRecord
    {
        const ModelItem *item = nullptr;
        QString path;
        QVector3D position;
        QVector3D rotationDegrees;
        QVector3D scale;
        bool selected = false;
        bool selectable = true;
        bool visible = true;
        int firstMesh = 0;
        int meshCount = 0;
    };
    QVector<ModelRecord> m_qmlModels;
    struct StaticLightRecord
    {
        const StaticLightItem *item = nullptr;
        QString path;
        QVector3D position;
        QVector3D rotationDegrees;
        QVector3D scale;
        bool selected = false;
        bool selectable = true;
        bool visible = true;
        int firstMesh = 0;
        int meshCount = 0;
    };
    QVector<StaticLightRecord> m_qmlStaticLights;
    struct CubeRecord
    {
        const CubeItem *item = nullptr;
        QVector3D position;
        QVector3D rotationDegrees;
        QVector3D scale;
        QVector3D baseColor;
        QVector3D emissiveColor;
        float metalness = 0.0f;
        float roughness = 0.5f;
        bool selected = false;
        bool selectable = true;
        bool visible = true;
        int firstMesh = 0;
        int meshCount = 0;
    };
    QVector<CubeRecord> m_qmlCubes;
    struct SphereRecord
    {
        const SphereItem *item = nullptr;
        QVector3D position;
        QVector3D rotationDegrees;
        QVector3D scale;
        QVector3D baseColor;
        QVector3D emissiveColor;
        float metalness = 0.0f;
        float roughness = 0.5f;
        bool selected = false;
        bool selectable = true;
        bool visible = true;
        int firstMesh = 0;
        int meshCount = 0;
    };
    QVector<SphereRecord> m_qmlSpheres;
    struct GizmoPart
    {
        int meshIndex = -1;
        int axis = -1;
        int type = 0;
    };
    struct SelectedTransform
    {
        QObject *item = nullptr;
        QVector3D startPos;
        QVector3D startRot;
    };
    QVector<GizmoPart> m_gizmoParts;
    QVector<SelectedTransform> m_dragSelection;
    bool m_axisDragActive = false;
    int m_axisDrag = -1;
    bool m_rotateDragActive = false;
    int m_rotateAxis = -1;
    QVector3D m_dragOrigin;
    float m_dragStartT = 0.0f;
    QVector3D m_rotateStartVec;

    QVector3D axisVector(int axis) const
    {
        switch (axis)
        {
        case 0: return QVector3D(1.0f, 0.0f, 0.0f);
        case 1: return QVector3D(0.0f, 1.0f, 0.0f);
        case 2: return QVector3D(0.0f, 0.0f, 1.0f);
        default: return QVector3D(0.0f, 0.0f, 0.0f);
        }
    }

    bool accumulateMeshBounds(const Mesh &mesh, QVector3D &minV, QVector3D &maxV, bool &hasBounds) const
    {
        QVector3D localMin = mesh.boundsMin;
        QVector3D localMax = mesh.boundsMax;
        if (!mesh.boundsValid)
        {
            if (mesh.vertices.isEmpty())
                return false;
            localMin = QVector3D(mesh.vertices[0].px, mesh.vertices[0].py, mesh.vertices[0].pz);
            localMax = localMin;
            for (const Vertex &v : mesh.vertices)
            {
                localMin.setX(qMin(localMin.x(), v.px));
                localMin.setY(qMin(localMin.y(), v.py));
                localMin.setZ(qMin(localMin.z(), v.pz));
                localMax.setX(qMax(localMax.x(), v.px));
                localMax.setY(qMax(localMax.y(), v.py));
                localMax.setZ(qMax(localMax.z(), v.pz));
            }
        }

        const QVector3D corners[8] = {
            QVector3D(localMin.x(), localMin.y(), localMin.z()),
            QVector3D(localMax.x(), localMin.y(), localMin.z()),
            QVector3D(localMin.x(), localMax.y(), localMin.z()),
            QVector3D(localMax.x(), localMax.y(), localMin.z()),
            QVector3D(localMin.x(), localMin.y(), localMax.z()),
            QVector3D(localMax.x(), localMin.y(), localMax.z()),
            QVector3D(localMin.x(), localMax.y(), localMax.z()),
            QVector3D(localMax.x(), localMax.y(), localMax.z())
        };

        for (const QVector3D &corner : corners)
        {
            const QVector3D world = mesh.modelMatrix.map(corner);
            if (!hasBounds)
            {
                minV = world;
                maxV = world;
                hasBounds = true;
            }
            else
            {
                minV.setX(qMin(minV.x(), world.x()));
                minV.setY(qMin(minV.y(), world.y()));
                minV.setZ(qMin(minV.z(), world.z()));
                maxV.setX(qMax(maxV.x(), world.x()));
                maxV.setY(qMax(maxV.y(), world.y()));
                maxV.setZ(qMax(maxV.z(), world.z()));
            }
        }
        return true;
    }

    bool computeSelectionCenter(QVector3D &center)
    {
        QVector3D sum;
        int count = 0;

        for (const ModelRecord &record : m_qmlModels)
        {
            if (!record.selected)
                continue;
            QVector3D minV;
            QVector3D maxV;
            bool hasBounds = false;
            for (int i = record.firstMesh; i < record.firstMesh + record.meshCount; ++i)
            {
                const Mesh &mesh = m_scene.meshes()[i];
                if (!mesh.visible)
                    continue;
                if (!mesh.selectable)
                    continue;
                accumulateMeshBounds(mesh, minV, maxV, hasBounds);
            }
            if (hasBounds)
            {
                sum += (minV + maxV) * 0.5f;
                ++count;
            }
        }

        for (const StaticLightRecord &record : m_qmlStaticLights)
        {
            if (!record.selected)
                continue;
            QVector3D minV;
            QVector3D maxV;
            bool hasBounds = false;
            for (int i = record.firstMesh; i < record.firstMesh + record.meshCount; ++i)
            {
                const Mesh &mesh = m_scene.meshes()[i];
                if (!mesh.visible)
                    continue;
                if (!mesh.selectable)
                    continue;
                accumulateMeshBounds(mesh, minV, maxV, hasBounds);
            }
            if (hasBounds)
            {
                sum += (minV + maxV) * 0.5f;
                ++count;
            }
        }

        for (const CubeRecord &record : m_qmlCubes)
        {
            if (!record.selected)
                continue;
            QVector3D minV;
            QVector3D maxV;
            bool hasBounds = false;
            for (int i = record.firstMesh; i < record.firstMesh + record.meshCount; ++i)
            {
                const Mesh &mesh = m_scene.meshes()[i];
                if (!mesh.visible)
                    continue;
                if (!mesh.selectable)
                    continue;
                accumulateMeshBounds(mesh, minV, maxV, hasBounds);
            }
            if (hasBounds)
            {
                sum += (minV + maxV) * 0.5f;
                ++count;
            }
        }

        for (const SphereRecord &record : m_qmlSpheres)
        {
            if (!record.selected)
                continue;
            QVector3D minV;
            QVector3D maxV;
            bool hasBounds = false;
            for (int i = record.firstMesh; i < record.firstMesh + record.meshCount; ++i)
            {
                const Mesh &mesh = m_scene.meshes()[i];
                if (!mesh.visible)
                    continue;
                if (!mesh.selectable)
                    continue;
                accumulateMeshBounds(mesh, minV, maxV, hasBounds);
            }
            if (hasBounds)
            {
                sum += (minV + maxV) * 0.5f;
                ++count;
            }
        }

        if (count == 0)
            return false;
        center = sum / float(count);
        return true;
    }

    void buildDragSelection()
    {
        m_dragSelection.clear();
        for (const ModelRecord &record : m_qmlModels)
        {
            if (!record.selected)
                continue;
            m_dragSelection.push_back({ const_cast<ModelItem *>(record.item),
                                        record.position,
                                        record.rotationDegrees });
        }
        for (const StaticLightRecord &record : m_qmlStaticLights)
        {
            if (!record.selected)
                continue;
            m_dragSelection.push_back({ const_cast<StaticLightItem *>(record.item),
                                        record.position,
                                        record.rotationDegrees });
        }
        for (const CubeRecord &record : m_qmlCubes)
        {
            if (!record.selected)
                continue;
            m_dragSelection.push_back({ const_cast<CubeItem *>(record.item),
                                        record.position,
                                        record.rotationDegrees });
        }
        for (const SphereRecord &record : m_qmlSpheres)
        {
            if (!record.selected)
                continue;
            m_dragSelection.push_back({ const_cast<SphereItem *>(record.item),
                                        record.position,
                                        record.rotationDegrees });
        }
    }

    void ensureGizmoMeshes()
    {
        if (!m_gizmoParts.isEmpty())
            return;
        const QVector3D colors[3] = {
            QVector3D(1.0f, 0.1f, 0.1f),
            QVector3D(0.1f, 1.0f, 0.1f),
            QVector3D(0.1f, 0.3f, 1.0f)
        };
        const float arcRadius = 1.0f;
        const float arcThickness = 0.03f;
        const float arcSpan = float(M_PI) * 0.5f;
        const int arcSegments = 24;
        const int arcSides = 10;
        for (int axis = 0; axis < 3; ++axis)
        {
            for (int part = 0; part < 2; ++part)
            {
                Mesh mesh = createUnitCubeMesh();
                mesh.selectable = false;
                mesh.gizmoAxis = axis;
                mesh.gizmoType = part;
                mesh.material.baseColor = colors[axis];
                mesh.material.emissive = colors[axis] * 2.0f;
                const int meshIndex = m_scene.meshes().size();
                m_scene.meshes().push_back(mesh);
                m_gizmoParts.push_back({ meshIndex, axis, part });
            }
            Mesh arc = createArcMesh(arcRadius, arcThickness, 0.0f, arcSpan,
                                     arcSegments, arcSides);
            arc.selectable = false;
            arc.gizmoAxis = axis;
            arc.gizmoType = 2;
            arc.material.baseColor = colors[axis];
            arc.material.emissive = colors[axis] * 2.0f;
            const int arcIndex = m_scene.meshes().size();
            m_scene.meshes().push_back(arc);
            m_gizmoParts.push_back({ arcIndex, axis, 2 });
        }
    }

    void updateGizmoMeshes(const QVector3D &pos, const QVector3D &cameraPos, bool visible)
    {
        const float distance = (cameraPos - pos).length();
        const float axisLength = qMax(0.2f, distance * 0.2f);
        const float shaftRadius = axisLength * 0.025f;
        const float headLength = axisLength * 0.2f;
        const float headRadius = axisLength * 0.06f;
        const float arcRadius = axisLength * 0.5f;
        const float arcScale = arcRadius;

        for (const GizmoPart &part : m_gizmoParts)
        {
            Mesh &mesh = m_scene.meshes()[part.meshIndex];
            if (!visible)
            {
                QMatrix4x4 hidden;
                hidden.setToIdentity();
                hidden.scale(0.0f);
                mesh.modelMatrix = hidden;
                continue;
            }

            const int axis = part.axis;
            const QVector3D dir = axisVector(axis);
            if (part.type == 0)
            {
                QMatrix4x4 shaftM;
                shaftM.translate(pos + dir * (axisLength * 0.5f));
                if (axis == 0)
                    shaftM.scale(axisLength, shaftRadius, shaftRadius);
                else if (axis == 1)
                    shaftM.scale(shaftRadius, axisLength, shaftRadius);
                else
                    shaftM.scale(shaftRadius, shaftRadius, axisLength);
                mesh.modelMatrix = shaftM;
            }
            else if (part.type == 1)
            {
                QMatrix4x4 headM;
                headM.translate(pos + dir * (axisLength + headLength * 0.5f));
                if (axis == 0)
                    headM.scale(headLength, headRadius, headRadius);
                else if (axis == 1)
                    headM.scale(headRadius, headLength, headRadius);
                else
                    headM.scale(headRadius, headRadius, headLength);
                mesh.modelMatrix = headM;
            }
            else
            {
                QVector3D u(1.0f, 0.0f, 0.0f);
                QVector3D v(0.0f, 1.0f, 0.0f);
                QVector3D axisDir = dir;
                if (axis == 0) {
                    axisDir = QVector3D(1.0f, 0.0f, 0.0f);
                    u = QVector3D(0.0f, 1.0f, 0.0f);
                    v = QVector3D(0.0f, 0.0f, 1.0f);
                } else if (axis == 1) {
                    axisDir = QVector3D(0.0f, 1.0f, 0.0f);
                    u = QVector3D(1.0f, 0.0f, 0.0f);
                    v = QVector3D(0.0f, 0.0f, 1.0f);
                } else {
                    axisDir = QVector3D(0.0f, 0.0f, 1.0f);
                    u = QVector3D(1.0f, 0.0f, 0.0f);
                    v = QVector3D(0.0f, 1.0f, 0.0f);
                }

                QMatrix4x4 arcM;
                arcM.setToIdentity();
                arcM.setColumn(0, QVector4D(u * arcScale, 0.0f));
                arcM.setColumn(1, QVector4D(v * arcScale, 0.0f));
                arcM.setColumn(2, QVector4D(axisDir * arcScale, 0.0f));
                arcM.setColumn(3, QVector4D(pos, 1.0f));
                mesh.modelMatrix = arcM;
            }
        }
    }
};

} // namespace

RhiQmlItem::RhiQmlItem(QQuickItem *parent)
    : QQuickRhiItem(parent)
{
    setAcceptedMouseButtons(Qt::LeftButton | Qt::RightButton);
    setFlag(ItemIsFocusScope, true);
    m_cameraTick = new QTimer(this);
    m_cameraTick->setInterval(16);
    connect(m_cameraTick, &QTimer::timeout, this, [this]()
    {
        if (!m_freeCameraEnabled)
            return;
        if (!m_cameraTimer.isValid())
            m_cameraTimer.start();
        const qint64 ms = m_cameraTimer.restart();
        const float dt = ms > 0 ? float(ms) / 1000.0f : 0.0f;
        if (dt > 0.0f)
            updateFreeCamera(dt);
    });
    m_smokeTick = new QTimer(this);
    m_smokeTick->setInterval(16);
    connect(m_smokeTick, &QTimer::timeout, this, [this]()
    {
        if (m_smokeAmount <= 0.0f || !m_smokeNoiseEnabled)
            return;
        if (!m_smokeTimer.isValid())
            m_smokeTimer.start();
        update();
    });
}

void RhiQmlItem::setCameraPosition(const QVector3D &pos)
{
    if (m_cameraPosition == pos)
        return;
    m_cameraPosition = pos;
    emit cameraPositionChanged();
    update();
}

void RhiQmlItem::setCameraTarget(const QVector3D &target)
{
    if (m_cameraTarget == target)
        return;
    m_cameraTarget = target;
    emit cameraTargetChanged();
    update();
}

void RhiQmlItem::setCameraFov(float fov)
{
    if (qFuzzyCompare(m_cameraFov, fov))
        return;
    m_cameraFov = fov;
    emit cameraFovChanged();
    update();
}

void RhiQmlItem::zoomAlongView(float delta)
{
    const QVector3D dir = (m_freeCameraEnabled ? forwardVector() : (m_cameraTarget - m_cameraPosition).normalized());
    if (dir.isNull())
        return;
    m_cameraPosition += dir * delta;
    if (!m_freeCameraEnabled)
        m_cameraTarget += dir * delta;
    emit cameraPositionChanged();
    emit cameraTargetChanged();
    update();
}

void RhiQmlItem::setAmbientLight(const QVector3D &ambient)
{
    if (m_ambientLight == ambient)
        return;
    m_ambientLight = ambient;
    emit ambientLightChanged();
    update();
}

void RhiQmlItem::setAmbientIntensity(float intensity)
{
    if (qFuzzyCompare(m_ambientIntensity, intensity))
        return;
    m_ambientIntensity = intensity;
    emit ambientIntensityChanged();
    update();
}

void RhiQmlItem::setSmokeAmount(float amount)
{
    if (qFuzzyCompare(m_smokeAmount, amount))
        return;
    m_smokeAmount = amount;
    emit smokeAmountChanged();
    updateSmokeTicker();
    update();
}

void RhiQmlItem::setBeamModel(BeamModel mode)
{
    if (m_beamModel == mode)
        return;
    m_beamModel = mode;
    emit beamModelChanged();
    update();
}

void RhiQmlItem::setBloomIntensity(float intensity)
{
    if (qFuzzyCompare(m_bloomIntensity, intensity))
        return;
    m_bloomIntensity = intensity;
    emit bloomIntensityChanged();
    update();
}

void RhiQmlItem::setBloomRadius(float radius)
{
    if (qFuzzyCompare(m_bloomRadius, radius))
        return;
    m_bloomRadius = radius;
    emit bloomRadiusChanged();
    update();
}

void RhiQmlItem::setVolumetricEnabled(bool enabled)
{
    if (m_volumetricEnabled == enabled)
        return;
    m_volumetricEnabled = enabled;
    emit volumetricEnabledChanged();
    update();
}

void RhiQmlItem::setShadowsEnabled(bool enabled)
{
    if (m_shadowsEnabled == enabled)
        return;
    m_shadowsEnabled = enabled;
    emit shadowsEnabledChanged();
    update();
}

void RhiQmlItem::setSmokeNoiseEnabled(bool enabled)
{
    if (m_smokeNoiseEnabled == enabled)
        return;
    m_smokeNoiseEnabled = enabled;
    emit smokeNoiseEnabledChanged();
    updateSmokeTicker();
    update();
}

void RhiQmlItem::setFreeCameraEnabled(bool enabled)
{
    if (m_freeCameraEnabled == enabled)
        return;
    m_freeCameraEnabled = enabled;
    if (m_freeCameraEnabled)
    {
        const QVector3D dir = (m_cameraTarget - m_cameraPosition).normalized();
        updateYawPitchFromDirection(dir);
        m_cameraTimer.restart();
        m_cameraTick->start();
    }
    else
    {
        m_cameraTick->stop();
    }
    emit freeCameraEnabledChanged();
    update();
}

void RhiQmlItem::setMoveSpeed(float speed)
{
    if (qFuzzyCompare(m_moveSpeed, speed))
        return;
    m_moveSpeed = speed;
    emit moveSpeedChanged();
}

void RhiQmlItem::setLookSensitivity(float sensitivity)
{
    if (qFuzzyCompare(m_lookSensitivity, sensitivity))
        return;
    m_lookSensitivity = sensitivity;
    emit lookSensitivityChanged();
}

float RhiQmlItem::smokeTimeSeconds() const
{
    if (!m_smokeTimer.isValid())
        return 0.0f;
    return float(m_smokeTimer.elapsed()) / 1000.0f;
}

void RhiQmlItem::updateSmokeTicker()
{
    if (m_smokeAmount > 0.0f && m_smokeNoiseEnabled)
    {
        if (!m_smokeTimer.isValid())
            m_smokeTimer.start();
        if (!m_smokeTick->isActive())
            m_smokeTick->start();
    }
    else
    {
        if (m_smokeTick->isActive())
            m_smokeTick->stop();
    }
}

void RhiQmlItem::keyPressEvent(QKeyEvent *event)
{
    if (!m_freeCameraEnabled)
    {
        QQuickRhiItem::keyPressEvent(event);
        return;
    }
    if (event->key() == Qt::Key_W)
        m_moveForward = true;
    else if (event->key() == Qt::Key_S)
        m_moveBackward = true;
    else if (event->key() == Qt::Key_A)
        m_moveLeft = true;
    else if (event->key() == Qt::Key_D)
        m_moveRight = true;
    else
        QQuickRhiItem::keyPressEvent(event);
}

void RhiQmlItem::keyReleaseEvent(QKeyEvent *event)
{
    if (!m_freeCameraEnabled)
    {
        QQuickRhiItem::keyReleaseEvent(event);
        return;
    }
    if (event->key() == Qt::Key_W)
        m_moveForward = false;
    else if (event->key() == Qt::Key_S)
        m_moveBackward = false;
    else if (event->key() == Qt::Key_A)
        m_moveLeft = false;
    else if (event->key() == Qt::Key_D)
        m_moveRight = false;
    else
        QQuickRhiItem::keyReleaseEvent(event);
}

void RhiQmlItem::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::MiddleButton)
    {
        m_panning = true;
        m_lastPanPos = event->position();
        event->accept();
        return;
    }
    if (event->button() == Qt::LeftButton)
    {
        const float w = width();
        const float h = height();
        if (w > 0.0f && h > 0.0f)
        {
            const QPointF pos = event->position();
            const QPointF normPos(pos.x() / w, pos.y() / h);
            m_pendingPickRequests.push_back({ normPos, QGuiApplication::keyboardModifiers() });
            m_pendingDragRequests.push_back({ normPos, 0 });
            update();
        }
        m_leftDown = true;
        event->accept();
        return;
    }

    if (!m_freeCameraEnabled || event->button() != Qt::RightButton)
    {
        QQuickRhiItem::mousePressEvent(event);
        return;
    }
    m_looking = true;
    m_lastMousePos = event->position();
    event->accept();
}

void RhiQmlItem::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::MiddleButton)
    {
        m_panning = false;
        event->accept();
        return;
    }
    if (event->button() == Qt::LeftButton)
    {
        const float w = width();
        const float h = height();
        if (w > 0.0f && h > 0.0f)
        {
            const QPointF pos = event->position();
            const QPointF normPos(pos.x() / w, pos.y() / h);
            m_pendingDragRequests.push_back({ normPos, 2 });
            update();
        }
        m_leftDown = false;
        event->accept();
        return;
    }
    if (!m_freeCameraEnabled || event->button() != Qt::RightButton)
    {
        QQuickRhiItem::mouseReleaseEvent(event);
        return;
    }
    m_looking = false;
    event->accept();
}

void RhiQmlItem::mouseMoveEvent(QMouseEvent *event)
{
    if (m_panning)
    {
        const QPointF pos = event->position();
        const QPointF delta = pos - m_lastPanPos;
        m_lastPanPos = pos;

        QVector3D forward = m_freeCameraEnabled
                ? forwardVector()
                : (m_cameraTarget - m_cameraPosition).normalized();
        if (forward.isNull())
            forward = QVector3D(0.0f, 0.0f, -1.0f);
        const QVector3D right = QVector3D::crossProduct(forward, QVector3D(0.0f, 1.0f, 0.0f)).normalized();
        const QVector3D up = QVector3D::crossProduct(right, forward).normalized();
        const float dist = (m_cameraTarget - m_cameraPosition).length();
        const float scale = qMax(0.001f, dist) * 0.006f;
        const QVector3D pan = (-right * float(delta.x()) + up * float(delta.y())) * scale;
        m_cameraPosition += pan;
        m_cameraTarget += pan;
        emit cameraPositionChanged();
        emit cameraTargetChanged();
        update();
        event->accept();
        return;
    }
    if (m_leftDown)
    {
        const float w = width();
        const float h = height();
        if (w > 0.0f && h > 0.0f)
        {
            const QPointF pos = event->position();
            const QPointF normPos(pos.x() / w, pos.y() / h);
            m_pendingDragRequests.push_back({ normPos, 1 });
            update();
        }
    }
    if (!m_freeCameraEnabled || !m_looking)
    {
        QQuickRhiItem::mouseMoveEvent(event);
        return;
    }
    const QPointF pos = event->position();
    const QPointF delta = pos - m_lastMousePos;
    m_lastMousePos = pos;
    m_yawDeg += float(delta.x()) * m_lookSensitivity;
    m_pitchDeg -= float(delta.y()) * m_lookSensitivity;
    m_pitchDeg = qBound(-89.0f, m_pitchDeg, 89.0f);
    const QVector3D fwd = forwardVector();
    m_cameraTarget = m_cameraPosition + fwd;
    emit cameraTargetChanged();
    update();
    event->accept();
}

void RhiQmlItem::updateFreeCamera(float dtSeconds)
{
    QVector3D pos = m_cameraPosition;
    const QVector3D fwd = forwardVector();
    const QVector3D right = rightVector();
    const float speed = m_moveSpeed * dtSeconds;

    if (m_moveForward)
        pos += fwd * speed;
    if (m_moveBackward)
        pos -= fwd * speed;
    if (m_moveLeft)
        pos -= right * speed;
    if (m_moveRight)
        pos += right * speed;

    if (pos != m_cameraPosition)
    {
        m_cameraPosition = pos;
        m_cameraTarget = m_cameraPosition + fwd;
        emit cameraPositionChanged();
        emit cameraTargetChanged();
        update();
    }
}

void RhiQmlItem::updateYawPitchFromDirection(const QVector3D &dir)
{
    const QVector3D nd = dir.normalized();
    m_pitchDeg = qRadiansToDegrees(std::asin(nd.y()));
    m_yawDeg = qRadiansToDegrees(std::atan2(nd.z(), nd.x()));
}

QVector3D RhiQmlItem::forwardVector() const
{
    const float yaw = qDegreesToRadians(m_yawDeg);
    const float pitch = qDegreesToRadians(m_pitchDeg);
    QVector3D f(std::cos(yaw) * std::cos(pitch),
                std::sin(pitch),
                std::sin(yaw) * std::cos(pitch));
    return f.normalized();
}

QVector3D RhiQmlItem::rightVector() const
{
    const QVector3D fwd = forwardVector();
    const QVector3D up(0.0f, 1.0f, 0.0f);
    return QVector3D::crossProduct(fwd, up).normalized();
}

void RhiQmlItem::addModel(const QString &path)
{
    if (path.isEmpty())
        return;
    addModel(path, QVector3D(0.0f, 0.0f, 0.0f), QVector3D(0.0f, 0.0f, 0.0f), QVector3D(1.0f, 1.0f, 1.0f));
}

void RhiQmlItem::addModel(const QString &path, const QVector3D &position)
{
    if (path.isEmpty())
        return;
    addModel(path, position, QVector3D(0.0f, 0.0f, 0.0f), QVector3D(1.0f, 1.0f, 1.0f));
}

void RhiQmlItem::addModel(const QString &path,
                          const QVector3D &position,
                          const QVector3D &rotationDegrees,
                          const QVector3D &scale)
{
    if (path.isEmpty())
        return;
    PendingModel model;
    model.path = path;
    model.position = position;
    model.rotationDegrees = rotationDegrees;
    model.scale = scale;
    m_pendingModels.push_back(model);
    update();
}

void RhiQmlItem::addPointLight(const QVector3D &position,
                               const QVector3D &color,
                               float intensity,
                               float range)
{
    Light light;
    light.type = Light::Type::Point;
    light.position = position;
    light.color = color;
    light.intensity = intensity;
    light.range = range;
    light.castShadows = true;
    m_pendingLights.push_back(light);
    update();
}

void RhiQmlItem::addDirectionalLight(const QVector3D &direction,
                                     const QVector3D &color,
                                     float intensity)
{
    Light light;
    light.type = Light::Type::Directional;
    light.direction = direction;
    light.color = color;
    light.intensity = intensity;
    light.castShadows = true;
    m_pendingLights.push_back(light);
    update();
}

void RhiQmlItem::addSpotLight(const QVector3D &position,
                              const QVector3D &direction,
                              const QVector3D &color,
                              float intensity,
                              float coneAngleDegrees)
{
    Light light;
    light.type = Light::Type::Spot;
    light.position = position;
    light.direction = direction;
    light.color = color;
    light.intensity = intensity;
    light.range = 20.0f;
    const float halfAngle = qDegreesToRadians(coneAngleDegrees * 0.5f);
    light.innerCone = halfAngle * 0.8f;
    light.outerCone = halfAngle;
    light.castShadows = true;
    m_pendingLights.push_back(light);
    update();
}

void RhiQmlItem::addAreaLight(const QVector3D &position,
                              const QVector3D &direction,
                              const QVector3D &color,
                              float intensity,
                              const QSizeF &size)
{
    Light light;
    light.type = Light::Type::Area;
    light.position = position;
    light.direction = direction;
    light.color = color;
    light.intensity = intensity;
    light.range = qMax(size.width(), size.height());
    light.areaSize = QVector2D(float(size.width()), float(size.height()));
    light.castShadows = false;
    m_pendingLights.push_back(light);
    update();
}

void RhiQmlItem::takePendingModels(QVector<PendingModel> &out)
{
    out = std::move(m_pendingModels);
    m_pendingModels.clear();
}

void RhiQmlItem::takePendingLights(QVector<Light> &out)
{
    out = std::move(m_pendingLights);
    m_pendingLights.clear();
}

void RhiQmlItem::takePendingPickRequests(QVector<PickRequest> &out)
{
    out = std::move(m_pendingPickRequests);
    m_pendingPickRequests.clear();
}

void RhiQmlItem::takePendingDragRequests(QVector<DragRequest> &out)
{
    out = std::move(m_pendingDragRequests);
    m_pendingDragRequests.clear();
}

void RhiQmlItem::dispatchPickResult(QObject *item, const QVector3D &worldPos, bool hit, int modifiers)
{
    emit meshPicked(item, worldPos, hit, modifiers);
}

void RhiQmlItem::handlePick(QObject *item, bool hit, int modifiers)
{
    const bool multi = (modifiers & Qt::ShiftModifier) != 0;
    bool canSelect = item && item->property("isSelected").isValid();
    if (canSelect && item->property("selectable").isValid())
        canSelect = item->property("selectable").toBool();

    if (!multi && (!hit || canSelect))
    {
        for (QObject *entry : m_selectableItems)
        {
            if (!entry)
                continue;
            if (entry->property("selectable").isValid() && !entry->property("selectable").toBool())
                continue;
            entry->setProperty("isSelected", false);
        }
    }

    if (item && hit && canSelect)
    {
        if (multi)
            item->setProperty("isSelected", !item->property("isSelected").toBool());
        else
            item->setProperty("isSelected", true);
        setSelectedItem(item);
    }
    else if (!multi && !hit)
    {
        setSelectedItem(nullptr);
    }
}

void RhiQmlItem::updateSelectableItems(const QVector<QObject *> &items)
{
    m_selectableItems.clear();
    for (QObject *item : items)
    {
        if (!item)
            continue;
        m_selectableItems.insert(item);
    }
}

void RhiQmlItem::setCameraDirection(const QVector3D &dir)
{
    if (dir.isNull())
        return;
    updateYawPitchFromDirection(dir);
    m_cameraTarget = m_cameraPosition + forwardVector();
    emit cameraTargetChanged();
    update();
}

void RhiQmlItem::rotateFreeCamera(float yawDelta, float pitchDelta)
{
    if (!m_freeCameraEnabled)
        return;
    m_yawDeg += yawDelta;
    m_pitchDeg += pitchDelta;
    m_pitchDeg = qBound(-89.0f, m_pitchDeg, 89.0f);
    const QVector3D fwd = forwardVector();
    m_cameraTarget = m_cameraPosition + fwd;
    emit cameraTargetChanged();
    update();
}

void RhiQmlItem::setSelectedItem(QObject *item)
{
    if (m_selectedItem == item)
        return;
    m_selectedItem = item;
    emit selectedItemChanged();
}

void RhiQmlItem::setObjectPosition(QObject *item, const QVector3D &pos)
{
    if (!item)
        return;
    item->setProperty("position", QVariant::fromValue(pos));
}

void RhiQmlItem::setObjectRotation(QObject *item, const QVector3D &rotation)
{
    if (!item)
        return;
    item->setProperty("rotationDegrees", QVariant::fromValue(rotation));
}

QQuickRhiItemRenderer *RhiQmlItem::createRenderer()
{
    return new RhiQmlItemRenderer();
}
