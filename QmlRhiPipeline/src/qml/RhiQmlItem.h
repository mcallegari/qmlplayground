#pragma once

#include <QtQuick/QQuickRhiItem>
#include <QtCore/QElapsedTimer>
#include <QtGui/QVector3D>
#include <QtCore/QVector>
#include <QtCore/QString>
#include <QtCore/QSizeF>

#include "scene/Scene.h"

class RhiQmlItem : public QQuickRhiItem
{
    Q_OBJECT
    Q_PROPERTY(QVector3D cameraPosition READ cameraPosition WRITE setCameraPosition NOTIFY cameraPositionChanged)
    Q_PROPERTY(QVector3D cameraTarget READ cameraTarget WRITE setCameraTarget NOTIFY cameraTargetChanged)
    Q_PROPERTY(float cameraFov READ cameraFov WRITE setCameraFov NOTIFY cameraFovChanged)
    Q_PROPERTY(QVector3D ambientLight READ ambientLight WRITE setAmbientLight NOTIFY ambientLightChanged)
    Q_PROPERTY(float ambientIntensity READ ambientIntensity WRITE setAmbientIntensity NOTIFY ambientIntensityChanged)
    Q_PROPERTY(bool freeCameraEnabled READ freeCameraEnabled WRITE setFreeCameraEnabled NOTIFY freeCameraEnabledChanged)
    Q_PROPERTY(float moveSpeed READ moveSpeed WRITE setMoveSpeed NOTIFY moveSpeedChanged)
    Q_PROPERTY(float lookSensitivity READ lookSensitivity WRITE setLookSensitivity NOTIFY lookSensitivityChanged)

public:
    explicit RhiQmlItem(QQuickItem *parent = nullptr);

    QVector3D cameraPosition() const { return m_cameraPosition; }
    void setCameraPosition(const QVector3D &pos);

    QVector3D cameraTarget() const { return m_cameraTarget; }
    void setCameraTarget(const QVector3D &target);

    float cameraFov() const { return m_cameraFov; }
    void setCameraFov(float fov);

    QVector3D ambientLight() const { return m_ambientLight; }
    void setAmbientLight(const QVector3D &ambient);
    float ambientIntensity() const { return m_ambientIntensity; }
    void setAmbientIntensity(float intensity);
    bool freeCameraEnabled() const { return m_freeCameraEnabled; }
    void setFreeCameraEnabled(bool enabled);
    float moveSpeed() const { return m_moveSpeed; }
    void setMoveSpeed(float speed);
    float lookSensitivity() const { return m_lookSensitivity; }
    void setLookSensitivity(float sensitivity);

    Q_INVOKABLE void addModel(const QString &path);
    Q_INVOKABLE void addModel(const QString &path, const QVector3D &position);
    Q_INVOKABLE void addModel(const QString &path,
                              const QVector3D &position,
                              const QVector3D &rotationDegrees,
                              const QVector3D &scale);
    Q_INVOKABLE void addPointLight(const QVector3D &position,
                                   const QVector3D &color,
                                   float intensity,
                                   float range);
    Q_INVOKABLE void addDirectionalLight(const QVector3D &direction,
                                         const QVector3D &color,
                                         float intensity);
    Q_INVOKABLE void addSpotLight(const QVector3D &position,
                                  const QVector3D &direction,
                                  const QVector3D &color,
                                  float intensity,
                                  float coneAngleDegrees);
    Q_INVOKABLE void addAreaLight(const QVector3D &position,
                                  const QVector3D &direction,
                                  const QVector3D &color,
                                  float intensity,
                                  const QSizeF &size);

    struct PendingModel {
        QString path;
        QVector3D position;
        QVector3D rotationDegrees;
        QVector3D scale = QVector3D(1.0f, 1.0f, 1.0f);
    };
    void takePendingModels(QVector<PendingModel> &out);
    void takePendingLights(QVector<Light> &out);

Q_SIGNALS:
    void cameraPositionChanged();
    void cameraTargetChanged();
    void cameraFovChanged();
    void ambientLightChanged();
    void ambientIntensityChanged();
    void freeCameraEnabledChanged();
    void moveSpeedChanged();
    void lookSensitivityChanged();

protected:
    QQuickRhiItemRenderer *createRenderer() override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;

private:
    void updateFreeCamera(float dtSeconds);
    void updateYawPitchFromDirection(const QVector3D &dir);
    QVector3D forwardVector() const;
    QVector3D rightVector() const;

    QVector3D m_cameraPosition = QVector3D(0.0f, 1.0f, 5.0f);
    QVector3D m_cameraTarget = QVector3D(0.0f, 0.0f, 0.0f);
    float m_cameraFov = 60.0f;
    QVector3D m_ambientLight = QVector3D(0.0f, 0.0f, 0.0f);
    float m_ambientIntensity = 1.0f;
    bool m_freeCameraEnabled = false;
    float m_moveSpeed = 5.0f;
    float m_lookSensitivity = 0.2f;
    bool m_moveForward = false;
    bool m_moveBackward = false;
    bool m_moveLeft = false;
    bool m_moveRight = false;
    bool m_looking = false;
    QPointF m_lastMousePos = QPointF(0.0, 0.0);
    float m_yawDeg = -90.0f;
    float m_pitchDeg = -10.0f;
    QElapsedTimer m_cameraTimer;
    QTimer *m_cameraTick = nullptr;

    QVector<PendingModel> m_pendingModels;
    QVector<Light> m_pendingLights;
};
