#include "qml/StaticLightItem.h"

#include <QtMath>

StaticLightItem::StaticLightItem(QObject *parent)
    : MeshItem(parent)
{
    m_path = QStringLiteral(FIXTURE_MESH_PATH "/par.glb");
}

void StaticLightItem::setPath(const QString &path)
{
    if (m_path == path)
        return;
    m_path = path;
    emit pathChanged();
    notifyParent();
}

void StaticLightItem::setColor(const QVector3D &color)
{
    if (m_color == color)
        return;
    m_color = color;
    emit colorChanged();
    notifyParent();
}

void StaticLightItem::setIntensity(float intensity)
{
    if (qFuzzyCompare(m_intensity, intensity))
        return;
    m_intensity = intensity;
    emit intensityChanged();
    notifyParent();
}

void StaticLightItem::setZoom(float zoom)
{
    if (qFuzzyCompare(m_zoom, zoom))
        return;
    m_zoom = zoom;
    emit zoomChanged();
    notifyParent();
}

void StaticLightItem::setGoboPath(const QString &path)
{
    if (m_goboPath == path)
        return;
    m_goboPath = path;
    emit goboPathChanged();
    notifyParent();
}

Light StaticLightItem::toLight() const
{
    Light light;
    light.type = Light::Type::Spot;
    light.position = QVector3D(0.0f, 0.0f, 0.0f);
    light.direction = QVector3D(0.0f, -1.0f, 0.0f);
    light.color = m_color;
    light.intensity = m_intensity;
    light.range = 20.0f;
    const float coneAngle = qDegreesToRadians(m_zoom);
    light.outerCone = coneAngle;
    light.innerCone = coneAngle * 0.8f;
    light.castShadows = true;
    light.goboPath = m_goboPath;
    return light;
}
