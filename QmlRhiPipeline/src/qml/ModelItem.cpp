#include "qml/ModelItem.h"

ModelItem::ModelItem(QObject *parent)
    : MeshItem(parent)
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
