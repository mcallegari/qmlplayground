#pragma once

#include "qml/MeshItem.h"
#include "scene/Scene.h"

class MovingHeadItem : public MeshItem
{
    Q_OBJECT
    Q_PROPERTY(QString path READ path WRITE setPath NOTIFY pathChanged)
    Q_PROPERTY(QVector3D color READ color WRITE setColor NOTIFY colorChanged)
    Q_PROPERTY(float intensity READ intensity WRITE setIntensity NOTIFY intensityChanged)
    Q_PROPERTY(float zoom READ zoom WRITE setZoom NOTIFY zoomChanged)
    Q_PROPERTY(float pan READ pan WRITE setPan NOTIFY panChanged)
    Q_PROPERTY(float tilt READ tilt WRITE setTilt NOTIFY tiltChanged)
    Q_PROPERTY(QString goboPath READ goboPath WRITE setGoboPath NOTIFY goboPathChanged)

public:
    explicit MovingHeadItem(QObject *parent = nullptr);

    MeshType type() const override { return MeshType::MovingHead; }

    QString path() const { return m_path; }
    void setPath(const QString &path);

    QVector3D color() const { return m_color; }
    void setColor(const QVector3D &color);

    float intensity() const { return m_intensity; }
    void setIntensity(float intensity);

    float zoom() const { return m_zoom; }
    void setZoom(float zoom);

    float pan() const { return m_pan; }
    void setPan(float panDegrees);

    float tilt() const { return m_tilt; }
    void setTilt(float tiltDegrees);

    QString goboPath() const { return m_goboPath; }
    void setGoboPath(const QString &path);

    Light toLight() const;

Q_SIGNALS:
    void pathChanged();
    void colorChanged();
    void intensityChanged();
    void zoomChanged();
    void panChanged();
    void tiltChanged();
    void goboPathChanged();

private:
    QString m_path;
    QVector3D m_color = QVector3D(1.0f, 1.0f, 1.0f);
    float m_intensity = 1.0f;
    float m_zoom = 25.0f;
    float m_pan = 0.0f;
    float m_tilt = 0.0f;
    QString m_goboPath;
};
