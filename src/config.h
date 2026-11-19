#pragma once

#include <QJsonObject>
#include <QList>
#include <QString>
#include <QVariantMap>

struct Control {
    QString key;
    double min, max, step;
};

struct Profile {
    QString name;
    bool denoise = true;
    double gate = 0;         // RNNoise voice-activity threshold, percent
    double rumble = 80;      // high-pass corner, Hz
    double bass = 0;         // low shelf at 120 Hz, dB
    double mud = 0;          // bell at 300 Hz, dB
    double presence = 0;     // bell at 4 kHz, dB
    double air = 0;          // high shelf at 10 kHz, dB
    double compression = 0;  // percent
    double output = 0;       // dB
    bool effects = false;
    int autotuneNotes = 1;         // Chain::NoteSets: 0 robot, 1 major chord ... 5 every note
    int autotuneRoot = 9;          // 0 C ... 11 B; A by default
    double autotuneSpeed = 100;    // percent; 100 snaps at once, the robotic sound

    static const QList<Control> &controls();
    static QList<Profile> presets();

    QVariant value(const QString &key) const;
    // Clamps to the control's range; false for an unknown key.
    bool set(const QString &key, const QVariant &value);

    QVariantMap toMap() const;
    QJsonObject toJson() const;
    static Profile fromJson(const QJsonObject &object);

    bool operator==(const Profile &other) const = default;
};

struct Config {
    // A PipeWire node.name; empty until a microphone has been picked.
    QString microphone;
    int active = 0;
    QList<Profile> profiles;

    static Config defaults();
    static QString defaultPath();

    const Profile &current() const { return profiles.at(active); }
    Profile &current() { return profiles[active]; }
    // Profile names match case-insensitively; a number picks by position, from 1.
    int find(const QString &nameOrNumber) const;
    QString uniqueName(const QString &base) const;

    QJsonObject toJson() const;
    static Config fromJson(const QJsonObject &object);
    // A missing file gives the defaults; a broken one gives the defaults and
    // an error, so the caller can say so instead of silently losing profiles.
    static Config load(const QString &path, QString *error = nullptr);
    bool save(const QString &path, QString *error = nullptr) const;

    bool operator==(const Config &other) const = default;
};
