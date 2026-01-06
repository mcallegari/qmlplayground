#include "qml/CameraItem.h"

#include <QtMath>
#include <QtQuick/QQuickItem>

CameraItem::CameraItem(QObject *parent)
    : QObject(parent)
{
}

void CameraItem::setPosition(const QVector3D &position)
{
    if (m_position == position)
        return;
    m_position = position;
    emit positionChanged();
    notifyParent();
}

void CameraItem::setTarget(const QVector3D &target)
{
    if (m_target == target)
        return;
    m_target = target;
    emit targetChanged();
    notifyParent();
}

void CameraItem::setFov(float fov)
{
    if (qFuzzyCompare(m_fov, fov))
        return;
    m_fov = fov;
    emit fovChanged();
    notifyParent();
}

void CameraItem::setNearPlane(float nearPlane)
{
    if (qFuzzyCompare(m_nearPlane, nearPlane))
        return;
    m_nearPlane = nearPlane;
    emit nearPlaneChanged();
    notifyParent();
}

void CameraItem::setFarPlane(float farPlane)
{
    if (qFuzzyCompare(m_farPlane, farPlane))
        return;
    m_farPlane = farPlane;
    emit farPlaneChanged();
    notifyParent();
}

void CameraItem::notifyParent()
{
    auto *item = qobject_cast<QQuickItem *>(parent());
    if (item)
        item->update();
}
