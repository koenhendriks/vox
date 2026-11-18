#include "take.h"

#include <cmath>
#include <cstring>

namespace Take {

QVector<float> fromBytes(const QByteArray &bytes) {
    QVector<float> samples(bytes.size() / qsizetype(sizeof(float)));
    std::memcpy(samples.data(), bytes.constData(), samples.size() * sizeof(float));
    return samples;
}

static double toDb(double amplitude) {
    return amplitude <= 0 ? -90.0 : std::max(-90.0, 20 * std::log10(amplitude));
}

// Mean absolute value per millisecond, minus its average, so correlation
// follows the rhythm of the speech rather than the EQ or the level.
static QVector<double> envelope(const QVector<float> &samples, int block) {
    QVector<double> env(samples.size() / block);
    double mean = 0;
    for (qsizetype i = 0; i < env.size(); ++i) {
        double sum = 0;
        for (int j = 0; j < block; ++j)
            sum += std::fabs(samples[i * block + j]);
        env[i] = sum / block;
        mean += env[i];
    }
    if (!env.isEmpty())
        mean /= env.size();
    for (auto &value : env)
        value -= mean;
    return env;
}

qint64 lag(const QVector<float> &earlier, const QVector<float> &later, int rate, double maxSeconds) {
    const int block = std::max(1, rate / 1000);
    const QVector<double> a = envelope(earlier, block), b = envelope(later, block);
    const qsizetype maxLag = qsizetype(maxSeconds * 1000);
    double best = 0;
    qsizetype bestLag = 0;
    for (qsizetype shift = -maxLag; shift <= maxLag; ++shift) {
        double sum = 0, energyA = 0, energyB = 0;
        for (qsizetype i = std::max<qsizetype>(0, -shift); i < a.size() && i + shift < b.size(); ++i) {
            const double x = a[i], y = b[i + shift];
            sum += x * y;
            energyA += x * x;
            energyB += y * y;
        }
        if (energyA <= 0 || energyB <= 0)
            continue;
        const double correlation = sum / std::sqrt(energyA * energyB);
        if (correlation > best) {
            best = correlation;
            bestLag = shift;
        }
    }
    // Below this the takes don't share enough speech to trust the match, and
    // leaving them unshifted is better than shifting them wrongly.
    return best < 0.3 ? 0 : bestLag * block;
}

void align(QVector<float> &raw, QVector<float> &vox, int rate) {
    const qint64 shift = lag(raw, vox, rate);
    if (shift > 0)
        vox.remove(0, std::min<qsizetype>(shift, vox.size()));
    else if (shift < 0)
        raw.remove(0, std::min<qsizetype>(-shift, raw.size()));
    const qsizetype length = std::min(raw.size(), vox.size());
    raw.resize(length);
    vox.resize(length);
}

QVector<float> curve(const QVector<float> &samples, int buckets) {
    QVector<float> result(buckets, 0.0f);
    if (samples.isEmpty() || buckets <= 0)
        return result;
    for (int b = 0; b < buckets; ++b) {
        const qsizetype from = samples.size() * b / buckets;
        const qsizetype to = std::max(from + 1, samples.size() * (b + 1) / buckets);
        float peak = 0;
        for (qsizetype i = from; i < to && i < samples.size(); ++i)
            peak = std::max(peak, std::fabs(samples[i]));
        result[b] = float(std::clamp((toDb(peak) + 60.0) / 60.0, 0.0, 1.0));
    }
    return result;
}

Levels levels(const QVector<float> &samples) {
    Levels result;
    if (samples.isEmpty())
        return result;
    double sum = 0;
    float peak = 0;
    for (float s : samples) {
        sum += double(s) * s;
        peak = std::max(peak, std::fabs(s));
    }
    result.loudness = toDb(std::sqrt(sum / samples.size()));
    result.peak = toDb(peak);
    return result;
}

}
