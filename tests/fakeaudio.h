#pragma once

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

    void refresh() override { emit statusChanged(status); }
    void setRunning(bool running) override {
        status.running = status.enabled = running;
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
