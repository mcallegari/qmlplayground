#pragma once

#include <QtCore/QMutex>
#include <QtCore/QPointer>
#include <QtGui/QImage>
#include <QtMultimedia/QMediaPlayer>

#include "qml/MeshItem.h"

class QVideoSink;
class QVideoFrame;

class VideoItem : public MeshItem
{
    Q_OBJECT
    Q_PROPERTY(QMediaPlayer *player READ player WRITE setPlayer NOTIFY playerChanged)
    Q_PROPERTY(int ledColumns READ ledColumns WRITE setLedColumns NOTIFY ledColumnsChanged)
    Q_PROPERTY(int ledRows READ ledRows WRITE setLedRows NOTIFY ledRowsChanged)

public:
    explicit VideoItem(QObject *parent = nullptr);

    MeshType type() const override { return MeshType::Video; }

    QMediaPlayer *player() const { return m_player; }
    void setPlayer(QMediaPlayer *player);

    int ledColumns() const { return m_ledColumns; }
    void setLedColumns(int columns);

    int ledRows() const { return m_ledRows; }
    void setLedRows(int rows);

    bool takeFrame(QImage &frame);

Q_SIGNALS:
    void playerChanged();
    void ledColumnsChanged();
    void ledRowsChanged();

private:
    void onVideoFrameChanged(const QVideoFrame &frame);
    QImage applyLedEffect(const QImage &image, int columns, int rows) const;

    QVideoSink *m_videoSink = nullptr;
    QPointer<QMediaPlayer> m_player;
    mutable QMutex m_frameMutex;
    QImage m_latestFrame;
    bool m_frameDirty = false;
    int m_ledColumns = 0;
    int m_ledRows = 0;
};
