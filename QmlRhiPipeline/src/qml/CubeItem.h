#pragma once

#include <QtCore/QObject>
#include <QtGui/QVector3D>

class CubeItem : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVector3D position READ position WRITE setPosition NOTIFY positionChanged)
    Q_PROPERTY(QVector3D rotationDegrees READ rotationDegrees WRITE setRotationDegrees NOTIFY rotationDegreesChanged)
    Q_PROPERTY(QVector3D scale READ scale WRITE setScale NOTIFY scaleChanged)
    Q_PROPERTY(bool isSelected READ isSelected WRITE setIsSelected NOTIFY isSelectedChanged)

public:
    explicit CubeItem(QObject *parent = nullptr);

    QVector3D position() const { return m_position; }
    void setPosition(const QVector3D &position);

    QVector3D rotationDegrees() const { return m_rotationDegrees; }
    void setRotationDegrees(const QVector3D &rotation);

    QVector3D scale() const { return m_scale; }
    void setScale(const QVector3D &scale);
    bool isSelected() const { return m_isSelected; }
    void setIsSelected(bool selected);

Q_SIGNALS:
    void positionChanged();
    void rotationDegreesChanged();
    void scaleChanged();
    void isSelectedChanged();

private:
    void notifyParent();

    QVector3D m_position = QVector3D(0.0f, 0.0f, 0.0f);
    QVector3D m_rotationDegrees = QVector3D(0.0f, 0.0f, 0.0f);
    QVector3D m_scale = QVector3D(1.0f, 1.0f, 1.0f);
    bool m_isSelected = false;
};
