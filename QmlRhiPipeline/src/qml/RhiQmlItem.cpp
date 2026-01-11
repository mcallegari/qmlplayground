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
#include <QtCore/QSet>

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
#include "qml/MovingHeadItem.h"
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

struct MeshRecord
{
    const MeshItem *item = nullptr;
    MeshItem::MeshType type = MeshItem::MeshType::Model;
    QString path;
    QVector3D position;
    QVector3D rotationDegrees;
    QVector3D scale;
    QVector3D baseColor = QVector3D(0.7f, 0.7f, 0.7f);
    QVector3D emissiveColor = QVector3D(0.0f, 0.0f, 0.0f);
    float metalness = 0.0f;
    float roughness = 0.5f;
    float pan = 0.0f;
    float tilt = 0.0f;
    bool selected = false;
    bool selectable = true;
    bool visible = true;
    int firstMesh = 0;
    int meshCount = 0;
    int armMesh = -1;
    int headMesh = -1;
    QVector3D armPivot = QVector3D(0.0f, 0.0f, 0.0f);
    QVector3D headPivot = QVector3D(0.0f, 0.0f, 0.0f);
    bool pivotsValid = false;
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

static QMatrix4x4 rotateAroundPivot(const QVector3D &pivot, const QQuaternion &rotation)
{
    QMatrix4x4 mat;
    mat.translate(pivot);
    mat.rotate(rotation);
    mat.translate(-pivot);
    return mat;
}

enum class MovingHeadPart
{
    Base,
    Arm,
    Head,
    HeadChild,
    Other
};

static MovingHeadPart movingHeadPartForName(const QString &name)
{
    const QString lower = name.toLower();
    if (lower.contains(QLatin1String("emitter")) || lower.contains(QLatin1String("handle")))
        return MovingHeadPart::HeadChild;
    if (lower.contains(QLatin1String("head")))
        return MovingHeadPart::Head;
    if (lower.contains(QLatin1String("arm")))
        return MovingHeadPart::Arm;
    if (lower.contains(QLatin1String("base")))
        return MovingHeadPart::Base;
    return MovingHeadPart::Other;
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

static void applySelectionGroup(QVector<Mesh> &meshes,
                                int firstMesh,
                                int meshCount,
                                int groupId)
{
    for (int i = firstMesh; i < firstMesh + meshCount; ++i)
    {
        Mesh &mesh = meshes[i];
        mesh.selectionGroup = groupId;
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

template <typename RecordT, typename ItemT>
static void initCommonRecord(RecordT &record, const ItemT *item)
{
    record.position = item->position();
    record.rotationDegrees = item->rotationDegrees();
    record.scale = item->scale();
    record.selected = item->isSelected();
    record.selectable = item->selectable();
    record.visible = item->visible();
}

template <typename RecordT>
static TransformInfo applyCommonRecordTransforms(RecordT &record,
                                                 QVector<Mesh> &meshes,
                                                 bool setBase)
{
    const TransformInfo transform = transformFromRecord(record);
    applyTransform(meshes, record.firstMesh, record.meshCount, transform, record.position, setBase);
    applySelectionVisibility(meshes, record.firstMesh, record.meshCount,
                             record.selected, record.selectable, record.visible);
    applySelectionGroup(meshes, record.firstMesh, record.meshCount, record.firstMesh);
    return transform;
}

template <typename RecordT>
static void applyMovingHeadTransforms(RecordT &record,
                                      QVector<Mesh> &meshes,
                                      const TransformInfo &transform,
                                      const QVector3D &position,
                                      float panDegrees,
                                      float tiltDegrees,
                                      bool setBase)
{
    if (setBase)
    {
        for (int i = record.firstMesh; i < record.firstMesh + record.meshCount; ++i)
            meshes[i].baseModelMatrix = meshes[i].modelMatrix;
    }

    if (!record.pivotsValid)
    {
        record.armMesh = -1;
        record.headMesh = -1;
        for (int i = record.firstMesh; i < record.firstMesh + record.meshCount; ++i)
        {
            const MovingHeadPart part = movingHeadPartForName(meshes[i].name);
            if (part == MovingHeadPart::Arm && record.armMesh < 0)
                record.armMesh = i;
            if (part == MovingHeadPart::Head && record.headMesh < 0)
                record.headMesh = i;
        }
        if (record.armMesh >= 0)
            record.armPivot = meshes[record.armMesh].baseModelMatrix.column(3).toVector3D();
        else
            record.armPivot = QVector3D(0.0f, 0.0f, 0.0f);
        if (record.headMesh >= 0)
            record.headPivot = meshes[record.headMesh].baseModelMatrix.column(3).toVector3D();
        else
            record.headPivot = QVector3D(0.0f, 0.0f, 0.0f);
        record.pivotsValid = true;
    }

    const QQuaternion panRot = QQuaternion::fromAxisAndAngle(0.0f, 1.0f, 0.0f, panDegrees);
    const QVector3D tiltAxis = panRot.rotatedVector(QVector3D(1.0f, 0.0f, 0.0f));
    const QQuaternion tiltRot = QQuaternion::fromAxisAndAngle(tiltAxis, tiltDegrees);
    const QMatrix4x4 panMatrix = rotateAroundPivot(record.armPivot, panRot);
    const QVector3D headPivotRotated = (panMatrix * QVector4D(record.headPivot, 1.0f)).toVector3D();
    const QMatrix4x4 tiltMatrix = rotateAroundPivot(headPivotRotated, tiltRot);

    for (int i = record.firstMesh; i < record.firstMesh + record.meshCount; ++i)
    {
        Mesh &mesh = meshes[i];
        const MovingHeadPart part = movingHeadPartForName(mesh.name);
        QMatrix4x4 local = mesh.baseModelMatrix;
        if (part == MovingHeadPart::Arm)
            local = panMatrix * local;
        else if (part == MovingHeadPart::Head || part == MovingHeadPart::HeadChild)
            local = tiltMatrix * (panMatrix * local);
        mesh.modelMatrix = transform.matrix * local;
        mesh.userOffset = position;
    }
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

static MeshItem *pickHitForRecords(const QVector<MeshRecord> &records, int meshIndex)
{
    for (const auto &record : records)
    {
        if (meshIndex >= record.firstMesh && meshIndex < record.firstMesh + record.meshCount)
            return const_cast<MeshItem *>(record.item);
    }
    return nullptr;
}

static void pruneMissingRecords(QVector<MeshRecord> &records,
                                const QList<MeshItem *> &liveItems,
                                QVector<Mesh> &meshes)
{
    QSet<const MeshItem *> live;
    live.reserve(liveItems.size());
    for (const MeshItem *item : liveItems)
        live.insert(item);

    for (int i = records.size() - 1; i >= 0; --i)
    {
        const auto &record = records[i];
        if (live.contains(record.item))
            continue;
        applySelectionVisibility(meshes, record.firstMesh, record.meshCount, false, false, false);
        records.removeAt(i);
    }
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
        QHash<const MovingHeadItem *, QVector<EmitterData>> movingHeadEmitters;

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

        const auto qmlMeshItems = qmlItem->findChildren<MeshItem *>(QString(), Qt::FindChildrenRecursively);
        pruneMissingRecords(m_qmlMeshes, qmlMeshItems, m_scene.meshes());

        auto findRecord = [&](const MeshItem *item) -> MeshRecord * {
            for (MeshRecord &record : m_qmlMeshes)
            {
                if (record.item == item)
                    return &record;
            }
            return nullptr;
        };

        for (MeshItem *meshItem : qmlMeshItems)
        {
            const MeshItem::MeshType type = meshItem->type();
            MeshRecord *record = findRecord(meshItem);
            QString path;
            bool needsPath = false;

            if (type == MeshItem::MeshType::Model)
            {
                const ModelItem *modelItem = qobject_cast<ModelItem *>(meshItem);
                if (!modelItem)
                    continue;
                path = modelItem->path();
                needsPath = true;
            }
            else if (type == MeshItem::MeshType::StaticLight)
            {
                const StaticLightItem *staticLight = qobject_cast<StaticLightItem *>(meshItem);
                if (!staticLight)
                    continue;
                path = staticLight->path();
                needsPath = true;
            }
            else if (type == MeshItem::MeshType::MovingHead)
            {
                const MovingHeadItem *movingHead = qobject_cast<MovingHeadItem *>(meshItem);
                if (!movingHead)
                    continue;
                path = movingHead->path();
                needsPath = true;
            }

            if (needsPath && path.isEmpty())
                continue;

            if (meshItem->selectable())
                selectableItems.push_back(meshItem);

            if (record)
            {
                record->type = type;
                if (needsPath && record->path != path)
                {
                    if (type == MeshItem::MeshType::Model)
                        qWarning() << "RhiQmlItemRenderer: model path changed after load for" << path;
                    else if (type == MeshItem::MeshType::StaticLight)
                        qWarning() << "RhiQmlItemRenderer: static light path changed after load for" << path;
                    else if (type == MeshItem::MeshType::MovingHead)
                        qWarning() << "RhiQmlItemRenderer: moving head path changed after load for" << path;
                    continue;
                }
                applySelectionGroup(m_scene.meshes(), record->firstMesh, record->meshCount, record->firstMesh);

                if (type == MeshItem::MeshType::Model)
                {
                    syncTransformFromItem(*record, meshItem, m_scene.meshes());
                    syncSelectionVisibilityFromItem(*record, meshItem, m_scene.meshes());
                }
                else if (type == MeshItem::MeshType::StaticLight)
                {
                    syncTransformFromItem(*record, meshItem, m_scene.meshes());
                    syncSelectionVisibilityFromItem(*record, meshItem, m_scene.meshes());
                    hideEmitterMeshes(m_scene.meshes(), record->firstMesh, record->meshCount);
                    const StaticLightItem *staticLight = static_cast<const StaticLightItem *>(meshItem);
                    staticLightEmitters.insert(staticLight,
                                               collectEmitters(m_scene.meshes(), record->firstMesh, record->meshCount));
                    const TransformInfo transform = transformFromRecord(*record);
                    staticLightItemTransforms.insert(staticLight, { transform.matrix, transform.rotation });
                    const auto lights = staticLight->findChildren<LightItem *>(QString(), Qt::FindChildrenRecursively);
                    for (const LightItem *lightItem : lights)
                        staticLightTransforms.insert(lightItem, { transform.matrix, transform.rotation });
                }
                else if (type == MeshItem::MeshType::MovingHead)
                {
                    const MovingHeadItem *movingHead = static_cast<const MovingHeadItem *>(meshItem);
                    const bool transformDirty = syncTransformFromItem(*record, meshItem, m_scene.meshes());
                    syncSelectionVisibilityFromItem(*record, meshItem, m_scene.meshes());
                    const float pan = movingHead->pan();
                    const float tilt = movingHead->tilt();
                    if (transformDirty || !qFuzzyCompare(record->pan, pan) || !qFuzzyCompare(record->tilt, tilt))
                    {
                        record->pan = pan;
                        record->tilt = tilt;
                        const TransformInfo transform = transformFromRecord(*record);
                        applyMovingHeadTransforms(*record, m_scene.meshes(), transform,
                                                  record->position, record->pan, record->tilt, false);
                    }
                    hideEmitterMeshes(m_scene.meshes(), record->firstMesh, record->meshCount);
                    QVector<EmitterData> emitters = collectEmitters(m_scene.meshes(), record->firstMesh, record->meshCount);
                    if (record->headMesh >= 0)
                    {
                        const QVector3D axis = -m_scene.meshes()[record->headMesh].modelMatrix.column(1).toVector3D();
                        if (!axis.isNull())
                        {
                            const QVector3D dir = axis.normalized();
                            for (EmitterData &emitter : emitters)
                                emitter.direction = dir;
                        }
                    }
                    movingHeadEmitters.insert(movingHead, emitters);
                }
                else if (type == MeshItem::MeshType::Cube)
                {
                    const CubeItem *cubeItem = static_cast<const CubeItem *>(meshItem);
                    syncTransformFromItem(*record, meshItem, m_scene.meshes());
                    const QVector3D baseColor = cubeItem->baseColor();
                    const QVector3D emissiveColor = cubeItem->emissiveColor();
                    const float metalness = cubeItem->metalness();
                    const float roughness = cubeItem->roughness();
                    if (record->baseColor != baseColor || record->emissiveColor != emissiveColor
                            || !qFuzzyCompare(record->metalness, metalness)
                            || !qFuzzyCompare(record->roughness, roughness))
                    {
                        record->baseColor = baseColor;
                        record->emissiveColor = emissiveColor;
                        record->metalness = metalness;
                        record->roughness = roughness;
                        applyMaterial(m_scene.meshes(), record->firstMesh, record->meshCount,
                                      baseColor, emissiveColor, metalness, roughness);
                    }
                    syncSelectionVisibilityFromItem(*record, meshItem, m_scene.meshes());
                }
                else if (type == MeshItem::MeshType::Sphere)
                {
                    const SphereItem *sphereItem = static_cast<const SphereItem *>(meshItem);
                    syncTransformFromItem(*record, meshItem, m_scene.meshes());
                    const QVector3D baseColor = sphereItem->baseColor();
                    const QVector3D emissiveColor = sphereItem->emissiveColor();
                    const float metalness = sphereItem->metalness();
                    const float roughness = sphereItem->roughness();
                    if (record->baseColor != baseColor || record->emissiveColor != emissiveColor
                            || !qFuzzyCompare(record->metalness, metalness)
                            || !qFuzzyCompare(record->roughness, roughness))
                    {
                        record->baseColor = baseColor;
                        record->emissiveColor = emissiveColor;
                        record->metalness = metalness;
                        record->roughness = roughness;
                        applyMaterial(m_scene.meshes(), record->firstMesh, record->meshCount,
                                      baseColor, emissiveColor, metalness, roughness);
                    }
                    syncSelectionVisibilityFromItem(*record, meshItem, m_scene.meshes());
                }
                continue;
            }

            const int beforeCount = m_scene.meshes().size();
            bool created = false;

            if (type == MeshItem::MeshType::Model
                    || type == MeshItem::MeshType::StaticLight
                    || type == MeshItem::MeshType::MovingHead)
            {
                if (!m_loader.loadModel(path, m_scene, true))
                {
                    if (type == MeshItem::MeshType::Model)
                        qWarning() << "RhiQmlItemRenderer: failed to load model" << path;
                    else if (type == MeshItem::MeshType::StaticLight)
                        qWarning() << "RhiQmlItemRenderer: failed to load static light model" << path;
                    else if (type == MeshItem::MeshType::MovingHead)
                        qWarning() << "RhiQmlItemRenderer: failed to load moving head model" << path;
                    continue;
                }
                created = true;
            }
            else if (type == MeshItem::MeshType::Cube)
            {
                m_scene.meshes().push_back(createUnitCubeMesh());
                created = true;
            }
            else if (type == MeshItem::MeshType::Sphere)
            {
                m_scene.meshes().push_back(createSphereMesh());
                created = true;
            }

            if (!created)
                continue;

            const int meshCount = m_scene.meshes().size() - beforeCount;
            MeshRecord newRecord;
            newRecord.item = meshItem;
            newRecord.type = type;
            newRecord.path = path;
            initCommonRecord(newRecord, meshItem);
            newRecord.firstMesh = beforeCount;
            newRecord.meshCount = meshCount;

            if (type == MeshItem::MeshType::MovingHead)
            {
                const MovingHeadItem *movingHead = static_cast<const MovingHeadItem *>(meshItem);
                newRecord.pan = movingHead->pan();
                newRecord.tilt = movingHead->tilt();
                const TransformInfo transform = transformFromRecord(newRecord);
                applyMovingHeadTransforms(newRecord, m_scene.meshes(), transform,
                                          newRecord.position, newRecord.pan, newRecord.tilt, true);
                applySelectionVisibility(m_scene.meshes(), newRecord.firstMesh, newRecord.meshCount,
                                         newRecord.selected, newRecord.selectable, newRecord.visible);
                applySelectionGroup(m_scene.meshes(), newRecord.firstMesh, newRecord.meshCount, newRecord.firstMesh);
                hideEmitterMeshes(m_scene.meshes(), newRecord.firstMesh, newRecord.meshCount);
                QVector<EmitterData> emitters = collectEmitters(m_scene.meshes(), newRecord.firstMesh, newRecord.meshCount);
                if (newRecord.headMesh >= 0)
                {
                    const QVector3D axis = -m_scene.meshes()[newRecord.headMesh].modelMatrix.column(1).toVector3D();
                    if (!axis.isNull())
                    {
                        const QVector3D dir = axis.normalized();
                        for (EmitterData &emitter : emitters)
                            emitter.direction = dir;
                    }
                }
                movingHeadEmitters.insert(static_cast<const MovingHeadItem *>(meshItem), emitters);
            }
            else
            {
                const TransformInfo transform = applyCommonRecordTransforms(newRecord, m_scene.meshes(), true);
                if (type == MeshItem::MeshType::StaticLight)
                {
                    const StaticLightItem *staticLight = static_cast<const StaticLightItem *>(meshItem);
                    hideEmitterMeshes(m_scene.meshes(), newRecord.firstMesh, newRecord.meshCount);
                    staticLightEmitters.insert(staticLight,
                                               collectEmitters(m_scene.meshes(), newRecord.firstMesh, newRecord.meshCount));
                    staticLightItemTransforms.insert(staticLight, { transform.matrix, transform.rotation });
                    const auto lights = staticLight->findChildren<LightItem *>(QString(), Qt::FindChildrenRecursively);
                    for (const LightItem *lightItem : lights)
                        staticLightTransforms.insert(lightItem, { transform.matrix, transform.rotation });
                }
                else if (type == MeshItem::MeshType::Cube)
                {
                    const CubeItem *cubeItem = static_cast<const CubeItem *>(meshItem);
                    newRecord.baseColor = cubeItem->baseColor();
                    newRecord.emissiveColor = cubeItem->emissiveColor();
                    newRecord.metalness = cubeItem->metalness();
                    newRecord.roughness = cubeItem->roughness();
                    applyMaterial(m_scene.meshes(), newRecord.firstMesh, newRecord.meshCount,
                                  newRecord.baseColor, newRecord.emissiveColor,
                                  newRecord.metalness, newRecord.roughness);
                }
                else if (type == MeshItem::MeshType::Sphere)
                {
                    const SphereItem *sphereItem = static_cast<const SphereItem *>(meshItem);
                    newRecord.baseColor = sphereItem->baseColor();
                    newRecord.emissiveColor = sphereItem->emissiveColor();
                    newRecord.metalness = sphereItem->metalness();
                    newRecord.roughness = sphereItem->roughness();
                    applyMaterial(m_scene.meshes(), newRecord.firstMesh, newRecord.meshCount,
                                  newRecord.baseColor, newRecord.emissiveColor,
                                  newRecord.metalness, newRecord.roughness);
                }
            }

            m_qmlMeshes.push_back(newRecord);
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
        for (auto it = movingHeadEmitters.cbegin(); it != movingHeadEmitters.cend(); ++it)
        {
            const MovingHeadItem *itemPtr = it.key();
            const QVector<EmitterData> emitters = it.value();
            if (emitters.isEmpty())
                continue;
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
                    hitItem = pickHitForRecords(m_qmlMeshes, hit.meshIndex);
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
    QVector<MeshRecord> m_qmlMeshes;
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

        for (const MeshRecord &record : m_qmlMeshes)
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
        for (const MeshRecord &record : m_qmlMeshes)
        {
            if (!record.selected)
                continue;
            m_dragSelection.push_back({ const_cast<MeshItem *>(record.item),
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
        const float shaftRadius = axisLength * 0.06f;
        const float headSize = axisLength * 0.14f;
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
                headM.translate(pos + dir * (axisLength + headSize * 0.5f));
                headM.scale(headSize, headSize, headSize);
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
    else if (event->key() == Qt::Key_Q)
        m_moveDown = true;
    else if (event->key() == Qt::Key_E)
        m_moveUp = true;
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
    else if (event->key() == Qt::Key_Q)
        m_moveDown = false;
    else if (event->key() == Qt::Key_E)
        m_moveUp = false;
    else
        QQuickRhiItem::keyReleaseEvent(event);
}

void RhiQmlItem::mousePressEvent(QMouseEvent *event)
{
    forceActiveFocus(Qt::MouseFocusReason);
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
    const QVector3D up(0.0f, 1.0f, 0.0f);
    const float speed = m_moveSpeed * dtSeconds;

    if (m_moveForward)
        pos += fwd * speed;
    if (m_moveBackward)
        pos -= fwd * speed;
    if (m_moveLeft)
        pos -= right * speed;
    if (m_moveRight)
        pos += right * speed;
    if (m_moveUp)
        pos += up * speed;
    if (m_moveDown)
        pos -= up * speed;

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

void RhiQmlItem::removeSelectedItems()
{
    const auto meshItems = findChildren<MeshItem *>(QString(), Qt::FindChildrenRecursively);
    QVector<QObject *> toDelete;
    toDelete.reserve(meshItems.size());
    for (MeshItem *item : meshItems)
    {
        if (!item)
            continue;
        if (!item->isSelected())
            continue;
        toDelete.push_back(item);
    }

    if (toDelete.isEmpty())
        return;

    bool clearedSelection = false;
    for (QObject *item : toDelete)
    {
        item->setProperty("isSelected", false);
        m_selectableItems.remove(item);
        if (item == m_selectedItem)
        {
            m_selectedItem = nullptr;
            clearedSelection = true;
        }
        item->deleteLater();
    }

    if (clearedSelection)
        emit selectedItemChanged();
    update();
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
