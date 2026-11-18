#include "testrecorder.h"

#include "audiosystem.h"
#include "backend.h"
#include "chain.h"
#include "player.h"

TestRecorder::TestRecorder(AudioSystem *audio, Backend *backend, QObject *parent)
    : QObject(parent), m_audio(audio), m_backend(backend) {
    connect(m_audio, &AudioSystem::captured, this, &TestRecorder::captured);
    m_tick.setInterval(100);
    connect(&m_tick, &QTimer::timeout, this, [this] {
        emit elapsedChanged();
        if (elapsed() >= MaxSeconds)
            stop();
    });
}

TestRecorder::~TestRecorder() {
    if (m_state == "recording") {
        m_awaitingCapture = false;
        m_audio->stopCapture();
    }
}

double TestRecorder::elapsed() const {
    return m_state == "recording" ? std::min(MaxSeconds, m_clock.elapsed() / 1000.0) : 0;
}

double TestRecorder::position() const {
    return m_player ? double(m_player->position()) / Take::Rate : 0;
}

bool TestRecorder::playing() const {
    return m_player && m_player->playing();
}

void TestRecorder::setState(const QString &state) {
    if (state == m_state)
        return;
    m_state = state;
    emit stateChanged();
}

void TestRecorder::setError(const QString &error) {
    if (error == m_error)
        return;
    m_error = error;
    emit errorChanged();
}

Player *TestRecorder::player() {
    // Created on first play, so recording and the tests never open an audio
    // output.
    if (!m_player) {
        m_player = new Player(this);
        connect(m_player, &Player::playingChanged, this, &TestRecorder::playingChanged);
        connect(m_player, &Player::positionChanged, this, &TestRecorder::positionChanged);
        connect(m_player, &Player::failed, this, &TestRecorder::setError);
    }
    return m_player;
}

void TestRecorder::record() {
    if (m_state == "recording")
        return;
    if (!m_backend->running()) {
        setError(tr("Turn on the Vox Microphone to record a test."));
        setState(m_raw.isEmpty() ? "idle" : "ready");
        return;
    }
    if (!m_backend->microphoneConnected()) {
        setError(tr("Your microphone isn't connected."));
        return;
    }
    if (m_player)
        m_player->pause();
    // In compare mode the Vox Microphone carries the raw sound, which would
    // make both recordings the same.
    m_backend->setBypass(false);
    setError({});
    m_profileName = m_backend->config().current().name;
    m_audio->startCapture({m_backend->microphone(), Chain::SourceName});
    m_awaitingCapture = true;
    m_clock.start();
    m_tick.start();
    setState("recording");
    emit elapsedChanged();
}

void TestRecorder::stop() {
    if (m_state != "recording")
        return;
    m_tick.stop();
    m_audio->stopCapture();
}

void TestRecorder::toggleRecording() {
    if (m_state == "recording")
        stop();
    else
        record();
}

void TestRecorder::captured(const QList<QByteArray> &buffers) {
    if (!m_awaitingCapture)
        return;
    m_awaitingCapture = false;
    m_tick.stop();

    QVector<float> raw = Take::fromBytes(buffers.value(0)), vox = Take::fromBytes(buffers.value(1));
    if (raw.isEmpty() || vox.isEmpty()) {
        setError(raw.isEmpty() ? tr("Nothing came in from your microphone.")
                               : tr("Nothing came in from the Vox Microphone. Is it running?"));
        setState(m_raw.isEmpty() ? "idle" : "ready");
        return;
    }
    Take::align(raw, vox);
    if (raw.size() < Take::Rate) {
        setError(tr("That was too short. Record at least a second."));
        setState(m_raw.isEmpty() ? "idle" : "ready");
        return;
    }

    m_raw = raw;
    m_vox = vox;
    m_rawLevels = Take::levels(m_raw);
    m_voxLevels = Take::levels(m_vox);
    auto toList = [](const QVector<float> &curve) {
        QVariantList list;
        list.reserve(curve.size());
        for (float v : curve)
            list << v;
        return list;
    };
    m_rawCurve = toList(Take::curve(m_raw, CurvePoints));
    m_voxCurve = toList(Take::curve(m_vox, CurvePoints));
    m_track = 0;
    if (m_player)
        m_player->setTracks({m_raw, m_vox}, Take::Rate);
    emit takeChanged();
    emit trackChanged();
    emit positionChanged();
    setState("ready");
}

void TestRecorder::togglePlay() {
    if (m_state != "ready")
        return;
    if (playing())
        m_player->pause();
    else
        playTrack(m_track);
}

void TestRecorder::playTrack(int track) {
    if (m_state != "ready" || track < 0 || track > 1)
        return;
    Player *p = player();
    if (p->length() != m_raw.size())
        p->setTracks({m_raw, m_vox}, Take::Rate);
    if (track != m_track) {
        m_track = track;
        emit trackChanged();
    }
    p->setTrack(m_track);
    p->play();
}

void TestRecorder::switchTrack() {
    if (m_state != "ready")
        return;
    m_track = 1 - m_track;
    emit trackChanged();
    if (m_player)
        m_player->setTrack(m_track);
}

void TestRecorder::playFrom(int track, double fraction) {
    if (m_state != "ready")
        return;
    Player *p = player();
    if (p->length() != m_raw.size())
        p->setTracks({m_raw, m_vox}, Take::Rate);
    if (track >= 0 && track <= 1 && track != m_track) {
        m_track = track;
        emit trackChanged();
    }
    // The track is set before seeking, or the first block would come from
    // the other one.
    p->setTrack(m_track);
    p->seek(qint64(std::clamp(fraction, 0.0, 1.0) * m_raw.size()));
    p->play();
}

void TestRecorder::close() {
    if (m_state == "recording") {
        m_awaitingCapture = false;
        m_tick.stop();
        m_audio->stopCapture();
    }
    if (m_player)
        m_player->pause();
    setError({});
    setState("idle");
}
