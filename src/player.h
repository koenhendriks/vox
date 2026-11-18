#pragma once

#include <QAudioFormat>
#include <QObject>
#include <QTimer>
#include <QVector>

class QAudioSink;
class QIODevice;

// Plays one of several equally long tracks, and switches between them at the
// same position without a gap, so two versions of a take can be compared
// word for word. Audio is pushed in small blocks from the current track, so a
// switch is heard within one block.
class Player : public QObject {
    Q_OBJECT

public:
    explicit Player(QObject *parent = nullptr);
    ~Player() override;

    void setTracks(const QList<QVector<float>> &tracks, int rate);
    void setTrack(int index);
    int track() const { return m_track; }
    void play();
    void pause();
    void seek(qint64 sample);
    bool playing() const { return m_playing; }
    // The sample being heard now, not the one being sent to the device.
    qint64 position() const;
    qint64 length() const;

    // `count` mono samples as interleaved stereo frames in `format`.
    static QByteArray stereo(const float *samples, qint64 count, QAudioFormat::SampleFormat format);

signals:
    void playingChanged();
    void positionChanged();
    void failed(const QString &message);

private:
    void pump();
    void finish();
    void stopSink();

    QList<QVector<float>> m_tracks;
    int m_rate = 48000;
    int m_track = 0;
    qint64 m_next = 0;  // the next sample to send
    bool m_playing = false;
    QAudioFormat m_format;
    QAudioSink *m_sink = nullptr;
    QIODevice *m_io = nullptr;
    QTimer m_timer;
};
