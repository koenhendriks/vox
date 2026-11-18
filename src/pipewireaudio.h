#pragma once

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

    static QStringList applyArguments(const QString &pod);
    static AudioStatus parseStatus(const QByteArray &systemctlShow, const QByteArray &defaultSource,
                                   const QByteArray &sourcesJson);

private:
    void run(const QString &program, const QStringList &arguments,
             std::function<void(int code, const QByteArray &out, const QByteArray &err)> done);
    void applyNext();

    QString m_pendingPod;
    bool m_applying = false;
    bool m_refreshing = false;
    QList<QProcess *> m_captures;
    QList<QByteArray> m_captureData;
    QProcess m_loopback;
    Meter m_micMeter, m_voxMeter;
};
