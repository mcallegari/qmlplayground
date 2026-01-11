#pragma once

#include "qml/MeshItem.h"
#include "scene/Scene.h"

class StaticLightItem : public MeshItem
{
    Q_OBJECT
    Q_PROPERTY(QString path READ path WRITE setPath NOTIFY pathChanged)
    Q_PROPERTY(QVector3D color READ color WRITE setColor NOTIFY colorChanged)
    Q_PROPERTY(float intensity READ intensity WRITE setIntensity NOTIFY intensityChanged)
    Q_PROPERTY(float zoom READ zoom WRITE setZoom NOTIFY zoomChanged)

public:
    explicit StaticLightItem(QObject *parent = nullptr);

    MeshType type() const override { return MeshType::StaticLight; }

    QString path() const { return m_path; }
    void setPath(const QString &path);

    QVector3D color() const { return m_color; }
    void setColor(const QVector3D &color);

    float intensity() const { return m_intensity; }
    void setIntensity(float intensity);

    float zoom() const { return m_zoom; }
    void setZoom(float zoom);

    Light toLight() const;

Q_SIGNALS:
    void pathChanged();
    void colorChanged();
    void intensityChanged();
    void zoomChanged();

private:
    QString m_path;
    QVector3D m_color = QVector3D(1.0f, 1.0f, 1.0f);
    float m_intensity = 1.0f;
    float m_zoom = 25.0f;
};
