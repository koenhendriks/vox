#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QTimer>
#include <QVariantList>
#include <QVector>

#include "take.h"

class AudioSystem;
class Backend;
class Player;

// Records the raw microphone and the Vox Microphone at the same moment, so
// both versions say exactly the same words, and plays them back with one
// playhead to switch between.
class TestRecorder : public QObject {
    Q_OBJECT
    // "idle", "recording" or "ready"
    Q_PROPERTY(QString state READ state NOTIFY stateChanged)
    Q_PROPERTY(double elapsed READ elapsed NOTIFY elapsedChanged)
    Q_PROPERTY(double maxSeconds READ maxSeconds CONSTANT)
    Q_PROPERTY(QString profileName READ profileName NOTIFY takeChanged)
    Q_PROPERTY(QVariantList rawCurve READ rawCurve NOTIFY takeChanged)
    Q_PROPERTY(QVariantList voxCurve READ voxCurve NOTIFY takeChanged)
    Q_PROPERTY(double rawLoudness READ rawLoudness NOTIFY takeChanged)
    Q_PROPERTY(double rawPeak READ rawPeak NOTIFY takeChanged)
    Q_PROPERTY(double voxLoudness READ voxLoudness NOTIFY takeChanged)
    Q_PROPERTY(double voxPeak READ voxPeak NOTIFY takeChanged)
    Q_PROPERTY(double duration READ duration NOTIFY takeChanged)
    Q_PROPERTY(double position READ position NOTIFY positionChanged)
    Q_PROPERTY(bool playing READ playing NOTIFY playingChanged)
    Q_PROPERTY(int track READ track NOTIFY trackChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)

public:
    static constexpr double MaxSeconds = 30;
    static constexpr int CurvePoints = 480;

    TestRecorder(AudioSystem *audio, Backend *backend, QObject *parent = nullptr);
    ~TestRecorder() override;

    QString state() const { return m_state; }
    double elapsed() const;
    double maxSeconds() const { return MaxSeconds; }
    QString profileName() const { return m_profileName; }
    QVariantList rawCurve() const { return m_rawCurve; }
    QVariantList voxCurve() const { return m_voxCurve; }
    double rawLoudness() const { return m_rawLevels.loudness; }
    double rawPeak() const { return m_rawLevels.peak; }
    double voxLoudness() const { return m_voxLevels.loudness; }
    double voxPeak() const { return m_voxLevels.peak; }
    double duration() const { return double(m_raw.size()) / Take::Rate; }
    double position() const;
    bool playing() const;
    int track() const { return m_track; }
    QString error() const { return m_error; }

    const QVector<float> &raw() const { return m_raw; }
    const QVector<float> &vox() const { return m_vox; }

    Q_INVOKABLE void record();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void toggleRecording();
    Q_INVOKABLE void togglePlay();
    // Plays `track` (0 raw, 1 Vox) from the current position.
    Q_INVOKABLE void playTrack(int track);
    Q_INVOKABLE void switchTrack();
    // Plays `track` from a point along the take, from 0 to 1.
    Q_INVOKABLE void playFrom(int track, double fraction);
    Q_INVOKABLE void close();

signals:
    void stateChanged();
    void elapsedChanged();
    void takeChanged();
    void positionChanged();
    void playingChanged();
    void trackChanged();
    void errorChanged();

private:
    void setState(const QString &state);
    void setError(const QString &error);
    void captured(const QList<QByteArray> &buffers);
    Player *player();

    AudioSystem *m_audio;
    Backend *m_backend;
    Player *m_player = nullptr;
    QString m_state = "idle";
    QString m_error;
    QString m_profileName;
    QElapsedTimer m_clock;
    QTimer m_tick;
    bool m_awaitingCapture = false;
    QVector<float> m_raw, m_vox;
    QVariantList m_rawCurve, m_voxCurve;
    Take::Levels m_rawLevels, m_voxLevels;
    int m_track = 1;
};
