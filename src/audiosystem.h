#pragma once

#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>

struct Microphone {
    QString name;         // PipeWire node.name
    QString description;  // what the desktop calls it
    bool operator==(const Microphone &other) const = default;
};

struct AudioStatus {
    bool serviceInstalled = false;
    bool running = false;
    bool enabled = false;
    bool failed = false;
    // Whether the Vox Microphone itself is there, which a running service
    // doesn't guarantee.
    bool sourcePresent = false;
    QString defaultSource;
    QList<Microphone> microphones;
    bool operator==(const AudioStatus &other) const = default;
};

// Everything Vox asks of the desktop's audio, behind one interface so tests
// and the screenshot harness run against a fake instead of the user's devices.
class AudioSystem : public QObject {
    Q_OBJECT

public:
    using QObject::QObject;

    virtual void refresh() = 0;
    virtual void setRunning(bool running) = 0;
    virtual void restart() = 0;
    virtual void apply(const QString &pod) = 0;
    virtual void setDefaultSource(const QString &name) = 0;
    virtual void setListening(bool listening) = 0;
    virtual void setMetering(const QString &microphone, bool voxRunning) = 0;
    virtual void stopMetering() = 0;
    // Blocks until changes sent with apply() have reached PipeWire.
    virtual void finish() {}
    // Records each target at Take::Rate, mono 32-bit float, until stopCapture,
    // which emits captured() with one buffer per target in the same order.
    virtual void startCapture(const QStringList &targets) = 0;
    virtual void stopCapture() = 0;

signals:
    void statusChanged(const AudioStatus &status);
    void levels(double microphone, double vox);
    void listeningChanged(bool listening);
    void captured(const QList<QByteArray> &buffers);
    // A command failed in a way the user needs to know about.
    void failed(const QString &message);
};
