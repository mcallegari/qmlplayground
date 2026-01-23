#include "scene/Camera.h"

void Camera::setPerspective(float fovYDegrees, float aspect, float nearPlane, float farPlane)
{
    if (m_fovY == fovYDegrees && m_aspect == aspect && m_near == nearPlane && m_far == farPlane)
    {
        return;
    }
    m_fovY = fovYDegrees;
    m_aspect = aspect;
    m_near = nearPlane;
    m_far = farPlane;
    m_proj.setToIdentity();
    m_proj.perspective(fovYDegrees, aspect, nearPlane, farPlane);
    m_dirty = true;
}

void Camera::setPosition(const QVector3D &pos)
{
    if (m_pos == pos)
        return;
    m_pos = pos;
    m_dirty = true;
}

void Camera::lookAt(const QVector3D &target, const QVector3D &up)
{
    QMatrix4x4 view;
    view.setToIdentity();
    view.lookAt(m_pos, target, up);
    if (view == m_view)
        return;
    m_view = view;
    m_dirty = true;
}
