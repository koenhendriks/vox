#pragma once

#include <QFile>

#include "audiosystem.h"
#include "chain.h"

// Stands in for PipeWire in tests and in the screenshot harness, with made-up
// devices, and records what Vox asked of it.
class FakeAudio : public AudioSystem {
    Q_OBJECT

public:
    using AudioSystem::AudioSystem;

    AudioStatus status;
    QStringList applied;
    int restarts = 0;
    bool listening = false;
    QString meteredMicrophone;
    QStringList captureTargets;
    QList<QByteArray> nextCapture;
    QStringList played;            // "id volume monitor", in order
    bool importFails = false;

    void refresh() override { emit statusChanged(status); }
    void setRunning(bool running) override {
        status.running = status.enabled = status.sourcePresent = running;
        refresh();
    }
    void restart() override { ++restarts; }
    void apply(const QString &pod) override { applied << pod; }
    void setDefaultSource(const QString &name) override {
        status.defaultSource = name;
        refresh();
    }
    void setListening(bool on) override {
        if (listening == on)
            return;
        listening = on;
        emit listeningChanged(on);
    }
    void setMetering(const QString &microphone, bool) override { meteredMicrophone = microphone; }
    void stopMetering() override { meteredMicrophone.clear(); }
    void startCapture(const QStringList &targets) override { captureTargets = targets; }
    void importSound(const QString &id, const QString &source, const QString &target) override {
        const bool ok = !importFails && QFile::copy(source, target);
        emit soundImported(id, ok, ok ? QString() : QString("not a sound"));
    }
    void playSound(const QString &id, const QString &, double volume, bool monitor) override {
        played << QString("%1 %2 %3").arg(id).arg(volume).arg(monitor);
    }
    void stopSound(const QString &id) override { emit soundFinished(id, true); }
    void stopCapture() override {
        captureTargets.clear();
        emit captured(nextCapture);
    }

    static AudioStatus sample() {
        AudioStatus s;
        s.serviceInstalled = true;
        s.defaultSource = "alsa_input.usb-studio";
        s.microphones = {{"alsa_input.usb-studio", "Studio USB Microphone"},
                         {"alsa_input.pci-laptop", "Built-in Microphone"}};
        return s;
    }
};
