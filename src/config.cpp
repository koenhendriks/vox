#include "config.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QStandardPaths>

const QList<Control> &Profile::controls() {
    static const QList<Control> list{
        {"gate", 0, 95, 5},
        {"rumble", 20, 200, 5},
        {"bass", -6, 12, 0.5},
        {"mud", -9, 6, 0.5},
        {"presence", -6, 9, 0.5},
        {"air", -6, 9, 0.5},
        {"compression", 0, 100, 5},
        {"output", -12, 12, 0.5},
        {"autotuneNotes", 0, 5, 1},
        {"autotuneRoot", 0, 11, 1},
        {"autotuneSpeed", 0, 100, 5},
    };
    return list;
}

// Starting points tuned on a USB condenser and a headset mic; every one can be
// edited, and they only come back when the config file is removed.
QList<Profile> Profile::presets() {
    Profile natural{"Natural", true, 0, 80, 2, -1, 1.5, 1, 30, 0};
    Profile broadcast{"Broadcast", true, 0, 70, 6, -3, 3, 2, 65, 2};
    Profile podcast{"Podcast", true, 0, 90, 3.5, -2.5, 2.5, 2, 50, 1};
    Profile meetings{"Meetings", true, 60, 120, 0, -3, 4, 1, 45, 1};
    return {natural, broadcast, podcast, meetings};
}

QVariant Profile::value(const QString &key) const {
    if (key == "name") return name;
    if (key == "denoise") return denoise;
    if (key == "gate") return gate;
    if (key == "rumble") return rumble;
    if (key == "bass") return bass;
    if (key == "mud") return mud;
    if (key == "presence") return presence;
    if (key == "air") return air;
    if (key == "compression") return compression;
    if (key == "output") return output;
    if (key == "effects") return effects;
    if (key == "autotuneNotes") return autotuneNotes;
    if (key == "autotuneRoot") return autotuneRoot;
    if (key == "autotuneSpeed") return autotuneSpeed;
    return {};
}

bool Profile::set(const QString &key, const QVariant &value) {
    if (key == "denoise" || key == "effects") {
        (key == "denoise" ? denoise : effects) = value.toBool();
        return true;
    }
    for (const auto &control : controls()) {
        if (control.key != key)
            continue;
        const double v = std::clamp(value.toDouble(), control.min, control.max);
        if (key == "autotuneNotes" || key == "autotuneRoot") {
            (key == "autotuneNotes" ? autotuneNotes : autotuneRoot) = int(std::lround(v));
            return true;
        }
        double *field = key == "gate" ? &gate : key == "rumble" ? &rumble : key == "bass" ? &bass
                      : key == "mud" ? &mud : key == "presence" ? &presence : key == "air" ? &air
                      : key == "compression" ? &compression : key == "autotuneSpeed" ? &autotuneSpeed
                      : &output;
        *field = v;
        return true;
    }
    return false;
}

QVariantMap Profile::toMap() const {
    return toJson().toVariantMap();
}

QJsonObject Profile::toJson() const {
    return {{"name", name},         {"denoise", denoise},   {"gate", gate},
            {"rumble", rumble},     {"bass", bass},         {"mud", mud},
            {"presence", presence}, {"air", air},           {"compression", compression},
            {"output", output},     {"effects", effects},   {"autotuneNotes", autotuneNotes},
            {"autotuneRoot", autotuneRoot},
            {"autotuneSpeed", autotuneSpeed}};
}

Profile Profile::fromJson(const QJsonObject &object) {
    Profile profile;
    profile.name = object.value("name").toString().trimmed();
    if (object.contains("denoise"))
        profile.denoise = object.value("denoise").toBool();
    // Profiles saved before effects existed load with them off.
    profile.effects = object.value("effects").toBool(false);
    for (const auto &control : controls())
        if (object.value(control.key).isDouble())
            profile.set(control.key, object.value(control.key).toDouble());
    return profile;
}

Config Config::defaults() {
    Config config;
    config.profiles = Profile::presets();
    return config;
}

QString Config::defaultPath() {
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + "/vox/config.json";
}

int Config::find(const QString &nameOrNumber) const {
    const QString wanted = nameOrNumber.trimmed();
    bool isNumber = false;
    const int number = wanted.toInt(&isNumber);
    if (isNumber)
        return number >= 1 && number <= profiles.size() ? number - 1 : -1;
    for (int i = 0; i < profiles.size(); ++i)
        if (profiles[i].name.compare(wanted, Qt::CaseInsensitive) == 0)
            return i;
    return -1;
}

QString Config::uniqueName(const QString &base) const {
    QString name = base;
    for (int n = 2; find(name) >= 0 || name.toInt() > 0; ++n)
        name = QString("%1 %2").arg(base).arg(n);
    return name;
}

QJsonObject Config::toJson() const {
    QJsonArray list;
    for (const auto &profile : profiles)
        list.append(profile.toJson());
    return {{"microphone", microphone}, {"active", active}, {"profiles", list}};
}

Config Config::fromJson(const QJsonObject &object) {
    Config config;
    config.microphone = object.value("microphone").toString();
    for (const auto &value : object.value("profiles").toArray()) {
        Profile profile = Profile::fromJson(value.toObject());
        if (!profile.name.isEmpty() && config.find(profile.name) < 0)
            config.profiles.append(profile);
    }
    if (config.profiles.isEmpty())
        config.profiles = Profile::presets();
    config.active = std::clamp(object.value("active").toInt(), 0, int(config.profiles.size()) - 1);
    return config;
}

Config Config::load(const QString &path, QString *error) {
    QFile file(path);
    if (!file.exists())
        return defaults();
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = file.errorString();
        return defaults();
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (!document.isObject()) {
        if (error)
            *error = parseError.errorString();
        return defaults();
    }
    return fromJson(document.object());
}

bool Config::save(const QString &path, QString *error) const {
    QDir().mkpath(QFileInfo(path).absolutePath());
    // QSaveFile writes beside the target and renames over it, so a crash or a
    // full disk never leaves a half-written profile list behind.
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error)
            *error = file.errorString();
        return false;
    }
    file.write(QJsonDocument(toJson()).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        if (error)
            *error = file.errorString();
        return false;
    }
    return true;
}
