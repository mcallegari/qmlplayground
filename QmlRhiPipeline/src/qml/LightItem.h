#pragma once

#include <QtCore/QObject>
#include <QtGui/QVector2D>
#include <QtGui/QVector3D>
#include <QtCore/QSizeF>

#include "scene/Scene.h"

class LightItem : public QObject
{
    Q_OBJECT
    Q_PROPERTY(Type type READ type WRITE setType NOTIFY typeChanged)
    Q_PROPERTY(QVector3D position READ position WRITE setPosition NOTIFY positionChanged)
    Q_PROPERTY(QVector3D direction READ direction WRITE setDirection NOTIFY directionChanged)
    Q_PROPERTY(QVector3D color READ color WRITE setColor NOTIFY colorChanged)
    Q_PROPERTY(float intensity READ intensity WRITE setIntensity NOTIFY intensityChanged)
    Q_PROPERTY(float range READ range WRITE setRange NOTIFY rangeChanged)
    Q_PROPERTY(float coneAngle READ coneAngle WRITE setConeAngle NOTIFY coneAngleChanged)
    Q_PROPERTY(bool castShadows READ castShadows WRITE setCastShadows NOTIFY castShadowsChanged)
    Q_PROPERTY(int qualitySteps READ qualitySteps WRITE setQualitySteps NOTIFY qualityStepsChanged)
    Q_PROPERTY(QSizeF size READ size WRITE setSize NOTIFY sizeChanged)
    Q_PROPERTY(QString goboPath READ goboPath WRITE setGoboPath NOTIFY goboPathChanged)

public:
    enum Type {
        Ambient,
        Directional,
        Point,
        Spotlight,
        Area
    };
    Q_ENUM(Type)

    explicit LightItem(QObject *parent = nullptr);

    Type type() const { return m_type; }
    void setType(Type type);

    QVector3D position() const { return m_position; }
    void setPosition(const QVector3D &position);

    QVector3D direction() const { return m_direction; }
    void setDirection(const QVector3D &direction);

    QVector3D color() const { return m_color; }
    void setColor(const QVector3D &color);

    float intensity() const { return m_intensity; }
    void setIntensity(float intensity);

    float range() const { return m_range; }
    void setRange(float range);

    float coneAngle() const { return m_coneAngle; }
    void setConeAngle(float angle);

    bool castShadows() const { return m_castShadows; }
    void setCastShadows(bool cast);

    int qualitySteps() const { return m_qualitySteps; }
    void setQualitySteps(int steps);

    QSizeF size() const { return m_size; }
    void setSize(const QSizeF &size);

    QString goboPath() const { return m_goboPath; }
    void setGoboPath(const QString &path);

    Light toLight() const;

Q_SIGNALS:
    void typeChanged();
    void positionChanged();
    void directionChanged();
    void colorChanged();
    void intensityChanged();
    void rangeChanged();
    void coneAngleChanged();
    void castShadowsChanged();
    void qualityStepsChanged();
    void sizeChanged();
    void goboPathChanged();

private:
    void notifyParent();

    Type m_type = Spotlight;
    QVector3D m_position = QVector3D(0.0f, 0.0f, 0.0f);
    QVector3D m_direction = QVector3D(0.0f, -1.0f, 0.0f);
    QVector3D m_color = QVector3D(1.0f, 1.0f, 1.0f);
    float m_intensity = 1.0f;
    float m_range = 20.0f;
    float m_coneAngle = 25.0f;
    bool m_castShadows = true;
    int m_qualitySteps = 8;
    QSizeF m_size = QSizeF(1.0, 1.0);
    QString m_goboPath;
};
