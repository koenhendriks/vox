#include "meter.h"

#include <cmath>

// 16 kHz mono is plenty for a level and keeps the pipe small; one block is
// 40 ms, so the meter updates 25 times a second.
static constexpr int Rate = 16000;
static constexpr qsizetype BlockBytes = Rate / 25 * qsizetype(sizeof(float));

Meter::Meter(QObject *parent) : QObject(parent) {
    connect(&m_process, &QProcess::readyReadStandardOutput, this, &Meter::read);
    // A source that stops delivering (unplugged, or Vox switched off) would
    // otherwise leave the meter frozen at its last level.
    m_silence.setInterval(300);
    m_silence.setSingleShot(true);
    connect(&m_silence, &QTimer::timeout, this, [this] {
        m_level = -90;
        emit levelChanged();
    });
}

Meter::~Meter() {
    stop();
}

void Meter::start(const QString &target) {
    if (target == m_target && m_process.state() != QProcess::NotRunning)
        return;
    stop();
    m_target = target;
    if (target.isEmpty())
        return;
    // dont-fallback keeps the meter on this source: if it's gone, PipeWire
    // would otherwise quietly meter the default microphone instead.
    m_process.start("pw-record", {"--target", target, "--rate", QString::number(Rate), "--channels", "1",
                                  "--format", "f32", "--raw",
                                  "-P", "{ node.dont-fallback = true node.dont-reconnect = true "
                                        "node.description = \"Vox level meter\" }",
                                  "-"});
    m_silence.start();
}

void Meter::stop() {
    m_target.clear();
    m_buffer.clear();
    m_silence.stop();
    if (m_process.state() != QProcess::NotRunning) {
        m_process.kill();
        m_process.waitForFinished(500);
    }
    if (m_level != -90) {
        m_level = -90;
        emit levelChanged();
    }
}

double Meter::rms(const float *samples, qsizetype count) {
    if (count <= 0)
        return -90;
    double sum = 0;
    for (qsizetype i = 0; i < count; ++i)
        sum += double(samples[i]) * samples[i];
    const double value = std::sqrt(sum / count);
    return value <= 0 ? -90 : std::max(-90.0, 20 * std::log10(value));
}

void Meter::read() {
    m_buffer += m_process.readAllStandardOutput();
    if (m_buffer.size() < BlockBytes)
        return;
    // Only the newest whole block matters; older audio is already history.
    const qsizetype blocks = m_buffer.size() / BlockBytes;
    const char *last = m_buffer.constData() + (blocks - 1) * BlockBytes;
    m_level = rms(reinterpret_cast<const float *>(last), BlockBytes / qsizetype(sizeof(float)));
    m_buffer.remove(0, blocks * BlockBytes);
    m_silence.start();
    emit levelChanged();
}
