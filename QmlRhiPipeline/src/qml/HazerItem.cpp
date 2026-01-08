#include "qml/HazerItem.h"

#include <QtQuick/QQuickItem>

HazerItem::HazerItem(QObject *parent)
    : QObject(parent)
{
}

void HazerItem::setPosition(const QVector3D &position)
{
    if (m_position == position)
        return;
    m_position = position;
    emit positionChanged();
    notifyParent();
}

void HazerItem::setDirection(const QVector3D &direction)
{
    if (m_direction == direction)
        return;
    m_direction = direction;
    emit directionChanged();
    notifyParent();
}

void HazerItem::setLength(float length)
{
    if (qFuzzyCompare(m_length, length))
        return;
    m_length = length;
    emit lengthChanged();
    notifyParent();
}

void HazerItem::setRadius(float radius)
{
    if (qFuzzyCompare(m_radius, radius))
        return;
    m_radius = radius;
    emit radiusChanged();
    notifyParent();
}

void HazerItem::setDensity(float density)
{
    if (qFuzzyCompare(m_density, density))
        return;
    m_density = density;
    emit densityChanged();
    notifyParent();
}

void HazerItem::setEnabled(bool enabled)
{
    if (m_enabled == enabled)
        return;
    m_enabled = enabled;
    emit enabledChanged();
    notifyParent();
}

void HazerItem::notifyParent()
{
    auto *item = qobject_cast<QQuickItem *>(parent());
    if (item)
        item->update();
}
