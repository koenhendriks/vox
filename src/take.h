#pragma once

#include <QByteArray>
#include <QVector>

// A test recording: the raw microphone and the Vox Microphone captured at the
// same time, lined up so both play the same word at the same position.
namespace Take {

inline constexpr int Rate = 48000;

struct Levels {
    double loudness = -90;  // RMS over the whole take, dBFS
    double peak = -90;      // dBFS
};

QVector<float> fromBytes(const QByteArray &bytes);

// How many samples `later` trails `earlier`, found by cross-correlating their
// envelopes within ±maxSeconds. Negative when `later` is actually ahead; 0
// when the two don't resemble each other enough to say.
qint64 lag(const QVector<float> &earlier, const QVector<float> &later, int rate = Rate, double maxSeconds = 0.4);

// Trims the leading samples that make `vox` trail `raw`, and both to one length.
void align(QVector<float> &raw, QVector<float> &vox, int rate = Rate);

// Peak level per bucket on a -60..0 dB scale mapped to 0..1, so quiet and
// loud takes both show the shape of the speech.
QVector<float> curve(const QVector<float> &samples, int buckets);

Levels levels(const QVector<float> &samples);

}
