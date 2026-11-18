#include "player.h"

#include <QAudioDevice>
#include <QAudioSink>
#include <QMediaDevices>
#include <cmath>

// Short enough that switching tracks is heard at once, long enough not to
// stutter when the window is busy.
static constexpr double BufferSeconds = 0.08;

Player::Player(QObject *parent) : QObject(parent) {
    m_timer.setInterval(10);
    connect(&m_timer, &QTimer::timeout, this, &Player::pump);
}

Player::~Player() {
    stopSink();
}

void Player::setTracks(const QList<QVector<float>> &tracks, int rate) {
    pause();
    m_tracks = tracks;
    m_rate = rate;
    m_track = 0;
    m_next = 0;
    emit positionChanged();
}

qint64 Player::length() const {
    return m_tracks.isEmpty() ? 0 : m_tracks.first().size();
}

void Player::setTrack(int index) {
    if (index >= 0 && index < m_tracks.size())
        m_track = index;
}

qint64 Player::position() const {
    if (!m_sink)
        return m_next;
    const qint64 queued = (m_sink->bufferSize() - m_sink->bytesFree()) / std::max(1, m_format.bytesPerFrame());
    return std::clamp<qint64>(m_next - queued, 0, length());
}

QByteArray Player::stereo(const float *samples, qint64 count, QAudioFormat::SampleFormat format) {
    QByteArray block;
    if (format == QAudioFormat::Float) {
        block.resize(count * 2 * qsizetype(sizeof(float)));
        auto *out = reinterpret_cast<float *>(block.data());
        for (qint64 i = 0; i < count; ++i)
            out[2 * i] = out[2 * i + 1] = samples[i];
    } else {
        block.resize(count * 2 * qsizetype(sizeof(qint16)));
        auto *out = reinterpret_cast<qint16 *>(block.data());
        for (qint64 i = 0; i < count; ++i)
            out[2 * i] = out[2 * i + 1] = qint16(std::lround(std::clamp(samples[i], -1.0f, 1.0f) * 32767));
    }
    return block;
}

void Player::play() {
    if (m_playing || m_tracks.isEmpty())
        return;
    if (m_next >= length())
        m_next = 0;

    const QAudioDevice device = QMediaDevices::defaultAudioOutput();
    if (device.isNull()) {
        emit failed(tr("There are no speakers or headphones to play on."));
        return;
    }
    // Stereo with the take in both channels: Qt's PipeWire backend labels a
    // one-channel stream AUX0 rather than MONO, and PipeWire then plays it in
    // the left speaker only.
    m_format.setSampleRate(m_rate);
    m_format.setChannelConfig(QAudioFormat::ChannelConfigStereo);
    m_format.setSampleFormat(QAudioFormat::Float);
    if (!device.isFormatSupported(m_format))
        m_format.setSampleFormat(QAudioFormat::Int16);

    m_sink = new QAudioSink(device, m_format, this);
    m_sink->setBufferSize(qsizetype(m_rate * BufferSeconds) * m_format.bytesPerFrame());
    m_io = m_sink->start();
    if (!m_io) {
        stopSink();
        emit failed(tr("Couldn't play the recording."));
        return;
    }
    m_playing = true;
    emit playingChanged();
    pump();
    m_timer.start();
}

void Player::pause() {
    if (!m_playing)
        return;
    // Resume from what was heard, not from what was already buffered.
    m_next = position();
    stopSink();
    m_playing = false;
    emit playingChanged();
    emit positionChanged();
}

void Player::seek(qint64 sample) {
    const bool wasPlaying = m_playing;
    if (wasPlaying) {
        stopSink();
        m_playing = false;
    }
    m_next = std::clamp<qint64>(sample, 0, length());
    emit positionChanged();
    if (wasPlaying)
        play();
    else
        emit playingChanged();
}

void Player::pump() {
    if (!m_io || !m_sink)
        return;
    const QVector<float> &track = m_tracks.at(m_track);
    const int bytesPerFrame = m_format.bytesPerFrame();
    const qint64 count = std::min<qint64>(m_sink->bytesFree() / bytesPerFrame, track.size() - m_next);
    if (count > 0) {
        const qint64 written = m_io->write(stereo(track.constData() + m_next, count, m_format.sampleFormat()));
        m_next += std::max<qint64>(0, written) / bytesPerFrame;
    }
    emit positionChanged();
    if (m_next >= track.size() && m_sink->bytesFree() >= m_sink->bufferSize())
        finish();
}

void Player::finish() {
    stopSink();
    m_playing = false;
    m_next = 0;
    emit playingChanged();
    emit positionChanged();
}

void Player::stopSink() {
    m_timer.stop();
    if (m_sink) {
        m_sink->stop();
        m_sink->deleteLater();
    }
    m_sink = nullptr;
    m_io = nullptr;
}
