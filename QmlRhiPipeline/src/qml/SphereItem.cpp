#include "qml/SphereItem.h"

SphereItem::SphereItem(QObject *parent)
    : MeshItem(parent)
{
}

void SphereItem::setBaseColor(const QVector3D &color)
{
    if (m_baseColor == color)
        return;
    m_baseColor = color;
    emit baseColorChanged();
    notifyParent();
}

void SphereItem::setEmissiveColor(const QVector3D &color)
{
    if (m_emissiveColor == color)
        return;
    m_emissiveColor = color;
    emit emissiveColorChanged();
    notifyParent();
}

void SphereItem::setMetalness(float metalness)
{
    if (qFuzzyCompare(m_metalness, metalness))
        return;
    m_metalness = metalness;
    emit metalnessChanged();
    notifyParent();
}

void SphereItem::setRoughness(float roughness)
{
    if (qFuzzyCompare(m_roughness, roughness))
        return;
    m_roughness = roughness;
    emit roughnessChanged();
    notifyParent();
}
