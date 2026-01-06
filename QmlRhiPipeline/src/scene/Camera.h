#pragma once

#include <QtGui/QMatrix4x4>
#include <QtGui/QVector3D>

class Camera
{
public:
    void setPerspective(float fovYDegrees, float aspect, float nearPlane, float farPlane);
    void setPosition(const QVector3D &pos);
    void lookAt(const QVector3D &target, const QVector3D &up = QVector3D(0.0f, 1.0f, 0.0f));

    const QMatrix4x4 &viewMatrix() const { return m_view; }
    const QMatrix4x4 &projectionMatrix() const { return m_proj; }
    const QVector3D &position() const { return m_pos; }
    float nearPlane() const { return m_near; }
    float farPlane() const { return m_far; }
    float fovYDegrees() const { return m_fovY; }
    float aspect() const { return m_aspect; }

private:
    QMatrix4x4 m_view;
    QMatrix4x4 m_proj;
    QVector3D m_pos;
    float m_near = 0.1f;
    float m_far = 1000.0f;
    float m_fovY = 60.0f;
    float m_aspect = 1.0f;
};
