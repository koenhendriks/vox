#pragma once

#include <QByteArray>
#include <QObject>
#include <QProcess>
#include <QTimer>

// Follows the level of one PipeWire source by reading raw samples from
// pw-record, so the window can show what the microphone and Vox sound like.
class Meter : public QObject {
    Q_OBJECT

public:
    explicit Meter(QObject *parent = nullptr);
    ~Meter() override;

    void start(const QString &target);
    void stop();
    QString target() const { return m_target; }
    double level() const { return m_level; }

    // RMS in dBFS of mono 32-bit float samples, floored at -90.
    static double rms(const float *samples, qsizetype count);

signals:
    void levelChanged();

private:
    void read();

    QString m_target;
    QProcess m_process;
    QByteArray m_buffer;
    QTimer m_silence;
    double m_level = -90;
};
