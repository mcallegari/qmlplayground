#include "qml/ModelItem.h"

#include <QtQuick/QQuickItem>

ModelItem::ModelItem(QObject *parent)
    : QObject(parent)
{
}

void ModelItem::setPath(const QString &path)
{
    if (m_path == path)
        return;
    m_path = path;
    emit pathChanged();
    notifyParent();
}

void ModelItem::setPosition(const QVector3D &position)
{
    if (m_position == position)
        return;
    m_position = position;
    emit positionChanged();
    notifyParent();
}

void ModelItem::setRotationDegrees(const QVector3D &rotation)
{
    if (m_rotationDegrees == rotation)
        return;
    m_rotationDegrees = rotation;
    emit rotationDegreesChanged();
    notifyParent();
}

void ModelItem::setScale(const QVector3D &scale)
{
    if (m_scale == scale)
        return;
    m_scale = scale;
    emit scaleChanged();
    notifyParent();
}

void ModelItem::notifyParent()
{
    auto *item = qobject_cast<QQuickItem *>(parent());
    if (item)
        item->update();
}
