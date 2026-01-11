#include "qml/BeamBarItem.h"

#include <QtMath>

BeamBarItem::BeamBarItem(QObject *parent)
    : MeshItem(parent)
{
}

void BeamBarItem::setEmitterCount(int count)
{
    const int next = qMax(1, count);
    if (m_emitterCount == next)
        return;
    m_emitterCount = next;
    emit emitterCountChanged();
    notifyParent();
}

void BeamBarItem::setBaseColor(const QVector3D &color)
{
    if (m_baseColor == color)
        return;
    m_baseColor = color;
    emit baseColorChanged();
    notifyParent();
}

void BeamBarItem::setColor(const QVector3D &color)
{
    if (m_color == color)
        return;
    m_color = color;
    emit colorChanged();
    notifyParent();
}

void BeamBarItem::setIntensity(float intensity)
{
    if (qFuzzyCompare(m_intensity, intensity))
        return;
    m_intensity = intensity;
    emit intensityChanged();
    notifyParent();
}

void BeamBarItem::setRange(float range)
{
    if (qFuzzyCompare(m_range, range))
        return;
    m_range = range;
    emit rangeChanged();
    notifyParent();
}

void BeamBarItem::setBeamRadius(float radius)
{
    if (qFuzzyCompare(m_beamRadius, radius))
        return;
    m_beamRadius = radius;
    emit beamRadiusChanged();
    notifyParent();
}

void BeamBarItem::setCastShadows(bool cast)
{
    if (m_castShadows == cast)
        return;
    m_castShadows = cast;
    emit castShadowsChanged();
    notifyParent();
}

QVariantList BeamBarItem::emitterColors() const
{
    QVariantList list;
    list.reserve(m_emitterColors.size());
    for (const QVector3D &color : m_emitterColors)
        list.push_back(QVariant::fromValue(color));
    return list;
}

void BeamBarItem::setEmitterColors(const QVariantList &colors)
{
    QVector<QVector3D> next;
    next.reserve(colors.size());
    for (const QVariant &value : colors)
    {
        if (value.canConvert<QVector3D>())
            next.push_back(value.value<QVector3D>());
    }
    if (m_emitterColors == next)
        return;
    m_emitterColors = next;
    emit emitterColorsChanged();
    notifyParent();
}

QVariantList BeamBarItem::emitterIntensities() const
{
    QVariantList list;
    list.reserve(m_emitterIntensities.size());
    for (float intensity : m_emitterIntensities)
        list.push_back(intensity);
    return list;
}

void BeamBarItem::setEmitterIntensities(const QVariantList &intensities)
{
    QVector<float> next;
    next.reserve(intensities.size());
    for (const QVariant &value : intensities)
        next.push_back(float(value.toDouble()));
    if (m_emitterIntensities == next)
        return;
    m_emitterIntensities = next;
    emit emitterIntensitiesChanged();
    notifyParent();
}

Light BeamBarItem::toLight() const
{
    Light light;
    light.type = Light::Type::Spot;
    light.position = QVector3D(0.0f, 0.0f, 0.0f);
    light.direction = QVector3D(0.0f, -1.0f, 0.0f);
    light.color = m_color;
    light.intensity = m_intensity;
    light.range = m_range;
    const float coneAngle = qDegreesToRadians(1.0f);
    light.outerCone = coneAngle;
    light.innerCone = coneAngle * 0.8f;
    light.castShadows = m_castShadows;
    light.beamRadius = m_beamRadius;
    light.beamShape = Light::BeamShapeType::BeamShape;
    return light;
}
