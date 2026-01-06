#include "qml/CubeItem.h"

#include <QtQuick/QQuickItem>

CubeItem::CubeItem(QObject *parent)
    : QObject(parent)
{
}

void CubeItem::setPosition(const QVector3D &position)
{
    if (m_position == position)
        return;
    m_position = position;
    emit positionChanged();
    notifyParent();
}

void CubeItem::setRotationDegrees(const QVector3D &rotation)
{
    if (m_rotationDegrees == rotation)
        return;
    m_rotationDegrees = rotation;
    emit rotationDegreesChanged();
    notifyParent();
}

void CubeItem::setScale(const QVector3D &scale)
{
    if (m_scale == scale)
        return;
    m_scale = scale;
    emit scaleChanged();
    notifyParent();
}

void CubeItem::setIsSelected(bool selected)
{
    if (m_isSelected == selected)
        return;
    m_isSelected = selected;
    emit isSelectedChanged();
    notifyParent();
}

void CubeItem::setBaseColor(const QVector3D &color)
{
    if (m_baseColor == color)
        return;
    m_baseColor = color;
    emit baseColorChanged();
    notifyParent();
}

void CubeItem::setEmissiveColor(const QVector3D &color)
{
    if (m_emissiveColor == color)
        return;
    m_emissiveColor = color;
    emit emissiveColorChanged();
    notifyParent();
}

void CubeItem::notifyParent()
{
    auto *item = qobject_cast<QQuickItem *>(parent());
    if (item)
        item->update();
}
