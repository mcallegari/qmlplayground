#include "qml/RhiQmlItem.h"

#include <QtCore/QDebug>
#include <QtCore/QElapsedTimer>
#include <QtCore/QTimer>
#include <QtMath>
#include <memory>
#include <QtGui/QMatrix4x4>
#include <QtGui/QMouseEvent>
#include <QtGui/QQuaternion>
#include <QtGui/QKeyEvent>

#include "core/RhiContext.h"
#include "core/RenderTargetCache.h"
#include "core/ShaderManager.h"
#include "renderer/DeferredRenderer.h"
#include "scene/AssimpLoader.h"
#include "qml/CameraItem.h"
#include "qml/LightItem.h"
#include "qml/ModelItem.h"
#include "qml/CubeItem.h"

namespace
{

Mesh createUnitCubeMesh()
{
    Mesh mesh;
    const float h = 0.5f;
    mesh.vertices = {
        // +Z
        { -h, -h,  h,  0.0f, 0.0f, 1.0f,  0.0f, 0.0f },
        {  h, -h,  h,  0.0f, 0.0f, 1.0f,  1.0f, 0.0f },
        {  h,  h,  h,  0.0f, 0.0f, 1.0f,  1.0f, 1.0f },
        { -h,  h,  h,  0.0f, 0.0f, 1.0f,  0.0f, 1.0f },
        // -Z
        {  h, -h, -h,  0.0f, 0.0f, -1.0f, 0.0f, 0.0f },
        { -h, -h, -h,  0.0f, 0.0f, -1.0f, 1.0f, 0.0f },
        { -h,  h, -h,  0.0f, 0.0f, -1.0f, 1.0f, 1.0f },
        {  h,  h, -h,  0.0f, 0.0f, -1.0f, 0.0f, 1.0f },
        // +X
        {  h, -h,  h,  1.0f, 0.0f, 0.0f,  0.0f, 0.0f },
        {  h, -h, -h,  1.0f, 0.0f, 0.0f,  1.0f, 0.0f },
        {  h,  h, -h,  1.0f, 0.0f, 0.0f,  1.0f, 1.0f },
        {  h,  h,  h,  1.0f, 0.0f, 0.0f,  0.0f, 1.0f },
        // -X
        { -h, -h, -h, -1.0f, 0.0f, 0.0f,  0.0f, 0.0f },
        { -h, -h,  h, -1.0f, 0.0f, 0.0f,  1.0f, 0.0f },
        { -h,  h,  h, -1.0f, 0.0f, 0.0f,  1.0f, 1.0f },
        { -h,  h, -h, -1.0f, 0.0f, 0.0f,  0.0f, 1.0f },
        // +Y
        { -h,  h,  h,  0.0f, 1.0f, 0.0f,  0.0f, 0.0f },
        {  h,  h,  h,  0.0f, 1.0f, 0.0f,  1.0f, 0.0f },
        {  h,  h, -h,  0.0f, 1.0f, 0.0f,  1.0f, 1.0f },
        { -h,  h, -h,  0.0f, 1.0f, 0.0f,  0.0f, 1.0f },
        // -Y
        { -h, -h, -h,  0.0f, -1.0f, 0.0f, 0.0f, 0.0f },
        {  h, -h, -h,  0.0f, -1.0f, 0.0f, 1.0f, 0.0f },
        {  h, -h,  h,  0.0f, -1.0f, 0.0f, 1.0f, 1.0f },
        { -h, -h,  h,  0.0f, -1.0f, 0.0f, 0.0f, 1.0f }
    };
    mesh.indices = {
        0, 1, 2, 0, 2, 3,
        4, 5, 6, 4, 6, 7,
        8, 9, 10, 8, 10, 11,
        12, 13, 14, 12, 14, 15,
        16, 17, 18, 16, 18, 19,
        20, 21, 22, 20, 22, 23
    };
    mesh.indexCount = mesh.indices.size();
    mesh.modelMatrix.setToIdentity();
    mesh.baseModelMatrix = mesh.modelMatrix;
    mesh.boundsMin = QVector3D(-h, -h, -h);
    mesh.boundsMax = QVector3D(h, h, h);
    mesh.boundsValid = true;
    return mesh;
}

class RhiQmlItemRenderer final : public QQuickRhiItemRenderer
{
public:
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

        QVector<RhiQmlItem::PendingModel> models;
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
                if (record.position != modelItem->position()
                        || record.rotationDegrees != modelItem->rotationDegrees()
                        || record.scale != modelItem->scale())
                        {
                    record.position = modelItem->position();
                    record.rotationDegrees = modelItem->rotationDegrees();
                    record.scale = modelItem->scale();
                    QMatrix4x4 transform;
                    transform.translate(record.position);
                    if (!record.rotationDegrees.isNull())
                    {
                        const QQuaternion rot = QQuaternion::fromEulerAngles(record.rotationDegrees);
                        transform.rotate(rot);
                    }
                    if (record.scale != QVector3D(1.0f, 1.0f, 1.0f))
                        transform.scale(record.scale);
                    for (int i = record.firstMesh; i < record.firstMesh + record.meshCount; ++i)
                    {
                        Mesh &mesh = m_scene.meshes()[i];
                        mesh.modelMatrix = transform * mesh.baseModelMatrix;
                        mesh.userOffset = record.position;
                    }
                }
                const bool selected = modelItem->isSelected();
                if (record.selected != selected)
                {
                    record.selected = selected;
                    for (int i = record.firstMesh; i < record.firstMesh + record.meshCount; ++i)
                        m_scene.meshes()[i].selected = selected;
                }
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
            record.firstMesh = beforeCount;
            record.meshCount = meshCount;

            QMatrix4x4 transform;
            transform.translate(record.position);
            if (!record.rotationDegrees.isNull())
            {
                const QQuaternion rot = QQuaternion::fromEulerAngles(record.rotationDegrees);
                transform.rotate(rot);
            }
            if (record.scale != QVector3D(1.0f, 1.0f, 1.0f))
                transform.scale(record.scale);

            for (int i = beforeCount; i < m_scene.meshes().size(); ++i)
            {
                Mesh &mesh = m_scene.meshes()[i];
                mesh.baseModelMatrix = mesh.modelMatrix;
                mesh.modelMatrix = transform * mesh.baseModelMatrix;
                mesh.userOffset = record.position;
                mesh.selected = record.selected;
            }
            m_qmlModels.push_back(record);
        }

        const auto qmlCubes = qmlItem->findChildren<CubeItem *>(QString(), Qt::FindChildrenRecursively);
        for (const CubeItem *cubeItem : qmlCubes)
        {
            bool found = false;
            for (CubeRecord &record : m_qmlCubes)
            {
                if (record.item != cubeItem)
                    continue;
                found = true;
                if (record.position != cubeItem->position()
                        || record.rotationDegrees != cubeItem->rotationDegrees()
                        || record.scale != cubeItem->scale())
                {
                    record.position = cubeItem->position();
                    record.rotationDegrees = cubeItem->rotationDegrees();
                    record.scale = cubeItem->scale();
                    QMatrix4x4 transform;
                    transform.translate(record.position);
                    if (!record.rotationDegrees.isNull())
                    {
                        const QQuaternion rot = QQuaternion::fromEulerAngles(record.rotationDegrees);
                        transform.rotate(rot);
                    }
                    if (record.scale != QVector3D(1.0f, 1.0f, 1.0f))
                        transform.scale(record.scale);
                    for (int i = record.firstMesh; i < record.firstMesh + record.meshCount; ++i)
                    {
                        Mesh &mesh = m_scene.meshes()[i];
                        mesh.modelMatrix = transform * mesh.baseModelMatrix;
                        mesh.userOffset = record.position;
                    }
                }
                const QVector3D baseColor = cubeItem->baseColor();
                const QVector3D emissiveColor = cubeItem->emissiveColor();
                if (record.baseColor != baseColor || record.emissiveColor != emissiveColor)
                {
                    record.baseColor = baseColor;
                    record.emissiveColor = emissiveColor;
                    for (int i = record.firstMesh; i < record.firstMesh + record.meshCount; ++i)
                    {
                        Mesh &mesh = m_scene.meshes()[i];
                        mesh.material.baseColor = baseColor;
                        mesh.material.emissive = emissiveColor;
                    }
                }
                const bool selected = cubeItem->isSelected();
                if (record.selected != selected)
                {
                    record.selected = selected;
                    for (int i = record.firstMesh; i < record.firstMesh + record.meshCount; ++i)
                        m_scene.meshes()[i].selected = selected;
                }
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
            record.baseColor = cubeItem->baseColor();
            record.emissiveColor = cubeItem->emissiveColor();
            record.firstMesh = beforeCount;
            record.meshCount = meshCount;

            QMatrix4x4 transform;
            transform.translate(record.position);
            if (!record.rotationDegrees.isNull())
            {
                const QQuaternion rot = QQuaternion::fromEulerAngles(record.rotationDegrees);
                transform.rotate(rot);
            }
            if (record.scale != QVector3D(1.0f, 1.0f, 1.0f))
                transform.scale(record.scale);

            for (int i = beforeCount; i < m_scene.meshes().size(); ++i)
            {
                Mesh &mesh = m_scene.meshes()[i];
                mesh.baseModelMatrix = mesh.modelMatrix;
                mesh.modelMatrix = transform * mesh.baseModelMatrix;
                mesh.userOffset = record.position;
                mesh.selected = record.selected;
                mesh.material.baseColor = record.baseColor;
                mesh.material.emissive = record.emissiveColor;
            }
            m_qmlCubes.push_back(record);
        }

        m_scene.lights().clear();
        m_scene.lights() = m_staticLights;
        QVector3D ambientTotal = qmlItem->ambientLight() * qmlItem->ambientIntensity();
        const auto qmlLights = qmlItem->findChildren<LightItem *>(QString(), Qt::FindChildrenRecursively);
        for (const LightItem *lightItem : qmlLights)
        {
            if (lightItem->type() == LightItem::Ambient)
            {
                ambientTotal += lightItem->color() * lightItem->intensity();
                continue;
            }
            m_scene.lights().push_back(lightItem->toLight());
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
        int firstMesh = 0;
        int meshCount = 0;
    };
    QVector<ModelRecord> m_qmlModels;
    struct CubeRecord
    {
        const CubeItem *item = nullptr;
        QVector3D position;
        QVector3D rotationDegrees;
        QVector3D scale;
        QVector3D baseColor;
        QVector3D emissiveColor;
        bool selected = false;
        int firstMesh = 0;
        int meshCount = 0;
    };
    QVector<CubeRecord> m_qmlCubes;
};

} // namespace

RhiQmlItem::RhiQmlItem(QQuickItem *parent)
    : QQuickRhiItem(parent)
{
    setAcceptedMouseButtons(Qt::RightButton);
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

QQuickRhiItemRenderer *RhiQmlItem::createRenderer()
{
    return new RhiQmlItemRenderer();
}
