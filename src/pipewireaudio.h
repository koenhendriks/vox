#pragma once

#include <QHash>
#include <QProcess>
#include <QStringList>

#include "audiosystem.h"
#include "meter.h"

// The real audio system: systemd runs the Vox engine as a user service, and
// the PipeWire and PulseAudio command-line tools do the rest.
class PipeWireAudio : public AudioSystem {
    Q_OBJECT

public:
    explicit PipeWireAudio(QObject *parent = nullptr);
    ~PipeWireAudio() override;

    void refresh() override;
    void setRunning(bool running) override;
    void restart() override;
    void apply(const QString &pod) override;
    void setDefaultSource(const QString &name) override;
    void setListening(bool listening) override;
    void setMetering(const QString &microphone, bool voxRunning) override;
    void stopMetering() override;
    void finish() override;
    void startCapture(const QStringList &targets) override;
    void stopCapture() override;
    void importSound(const QString &id, const QString &source, const QString &target) override;
    void playSound(const QString &id, const QString &file, double volume, bool monitor) override;
    void stopSound(const QString &id) override;

    static QStringList applyArguments(const QString &pod);
    // ffmpeg's arguments to turn any audio file into a sound: silence
    // trimmed, loudness normalised, mono 48 kHz FLAC.
    static QStringList importArguments(const QString &source, const QString &target);
    static AudioStatus parseStatus(const QByteArray &systemctlShow, const QByteArray &defaultSource,
                                   const QByteArray &sourcesJson);

private:
    void run(const QString &program, const QStringList &arguments,
             std::function<void(int code, const QByteArray &out, const QByteArray &err)> done);
    void applyNext();

    QString m_pendingPod;
    bool m_applying = false;
    bool m_refreshing = false;
    struct Playing {
        QProcess *mix = nullptr;
        QProcess *monitor = nullptr;
        bool stopped = false;
    };
    QHash<QString, Playing> m_playing;
    QList<QProcess *> m_captures;
    QList<QByteArray> m_captureData;
    QProcess m_loopback;
    Meter m_micMeter, m_voxMeter;
};
