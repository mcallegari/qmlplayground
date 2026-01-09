#include "qml/MeshItem.h"

#include <QtQuick/QQuickItem>

MeshItem::MeshItem(QObject *parent)
    : QObject(parent)
{
}

void MeshItem::setPosition(const QVector3D &position)
{
    if (m_position == position)
        return;
    m_position = position;
    emit positionChanged();
    notifyParent();
}

void MeshItem::setRotationDegrees(const QVector3D &rotation)
{
    if (m_rotationDegrees == rotation)
        return;
    m_rotationDegrees = rotation;
    emit rotationDegreesChanged();
    notifyParent();
}

void MeshItem::setScale(const QVector3D &scale)
{
    if (m_scale == scale)
        return;
    m_scale = scale;
    emit scaleChanged();
    notifyParent();
}

void MeshItem::setIsSelected(bool selected)
{
    if (m_isSelected == selected)
        return;
    m_isSelected = selected;
    emit isSelectedChanged();
    notifyParent();
}

void MeshItem::setSelectable(bool selectable)
{
    if (m_selectable == selectable)
        return;
    m_selectable = selectable;
    emit selectableChanged();
    if (!m_selectable && m_isSelected)
    {
        m_isSelected = false;
        emit isSelectedChanged();
    }
    notifyParent();
}

void MeshItem::setVisible(bool visible)
{
    if (m_visible == visible)
        return;
    m_visible = visible;
    emit visibleChanged();
    notifyParent();
}

void MeshItem::notifyParent()
{
    auto *item = qobject_cast<QQuickItem *>(parent());
    if (item)
        item->update();
}
