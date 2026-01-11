#pragma once

#include "qml/MeshItem.h"

class ModelItem : public MeshItem
{
    Q_OBJECT
    Q_PROPERTY(QString path READ path WRITE setPath NOTIFY pathChanged)

public:
    explicit ModelItem(QObject *parent = nullptr);

    MeshType type() const override { return MeshType::Model; }

    QString path() const { return m_path; }
    void setPath(const QString &path);

Q_SIGNALS:
    void pathChanged();

private:
    QString m_path;
};
