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
    Q_PROPERTY(QVector3D baseColor READ baseColor WRITE setBaseColor NOTIFY baseColorChanged)
    Q_PROPERTY(QVector3D emissiveColor READ emissiveColor WRITE setEmissiveColor NOTIFY emissiveColorChanged)

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
    QVector3D baseColor() const { return m_baseColor; }
    void setBaseColor(const QVector3D &color);
    QVector3D emissiveColor() const { return m_emissiveColor; }
    void setEmissiveColor(const QVector3D &color);

Q_SIGNALS:
    void positionChanged();
    void rotationDegreesChanged();
    void scaleChanged();
    void isSelectedChanged();
    void baseColorChanged();
    void emissiveColorChanged();

private:
    void notifyParent();

    QVector3D m_position = QVector3D(0.0f, 0.0f, 0.0f);
    QVector3D m_rotationDegrees = QVector3D(0.0f, 0.0f, 0.0f);
    QVector3D m_scale = QVector3D(1.0f, 1.0f, 1.0f);
    bool m_isSelected = false;
    QVector3D m_baseColor = QVector3D(0.7f, 0.7f, 0.7f);
    QVector3D m_emissiveColor = QVector3D(0.0f, 0.0f, 0.0f);
};
