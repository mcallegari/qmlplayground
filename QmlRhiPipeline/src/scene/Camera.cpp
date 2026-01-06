#include "scene/Camera.h"

void Camera::setPerspective(float fovYDegrees, float aspect, float nearPlane, float farPlane)
{
    m_fovY = fovYDegrees;
    m_aspect = aspect;
    m_near = nearPlane;
    m_far = farPlane;
    m_proj.setToIdentity();
    m_proj.perspective(fovYDegrees, aspect, nearPlane, farPlane);
}

void Camera::setPosition(const QVector3D &pos)
{
    m_pos = pos;
}

void Camera::lookAt(const QVector3D &target, const QVector3D &up)
{
    m_view.setToIdentity();
    m_view.lookAt(m_pos, target, up);
}
