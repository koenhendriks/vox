#include "cli.h"

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QSaveFile>
#include <QStandardPaths>
#include <cstdio>
#include <unistd.h>

#include "chain.h"
#include "config.h"
#include "opendeck.h"
#include "pipewireaudio.h"

namespace {

QString tr(const char *text) {
    return QCoreApplication::translate("Cli", text);
}

void out(const QString &text) {
    std::fputs(qPrintable(text + '\n'), stdout);
}

int fail(const QString &message, bool json) {
    if (json)
        out(QJsonDocument(QJsonObject{{"ok", false}, {"error", message}}).toJson(QJsonDocument::Compact));
    std::fputs(qPrintable(message + '\n'), stderr);
    return 1;
}

void printJson(const QJsonObject &object) {
    out(QJsonDocument(object).toJson(QJsonDocument::Indented).trimmed());
}

struct Result {
    int code;
    QByteArray out, err;
};

Result execute(const QString &program, const QStringList &arguments, int timeout = 5000) {
    QProcess process;
    process.start(program, arguments);
    if (!process.waitForStarted(timeout))
        return {-1, {}, QString("%1 is not installed.").arg(program).toUtf8()};
    if (!process.waitForFinished(timeout)) {
        process.kill();
        return {-1, {}, QString("%1 didn't answer.").arg(program).toUtf8()};
    }
    return {process.exitStatus() == QProcess::NormalExit ? process.exitCode() : -1, process.readAllStandardOutput(),
            process.readAllStandardError()};
}

QString usage() {
    return tr("Usage: vox [command] [--json]\n"
              "Makes your microphone sound better, in every app, through the Vox Microphone.\n"
              "\n"
              "Window:\n"
              "  open                Open Vox (the default)\n"
              "  tray                Show Vox in the tray without opening the window\n"
              "\n"
              "Commands that need no display:\n"
              "  status              Whether Vox is running, and with which profile and microphone\n"
              "  list                List the profiles\n"
              "  use <profile>       Switch profile, by name or number, live\n"
              "  on | off            Start or stop the Vox Microphone, now and at login\n"
              "  default             Make the Vox Microphone the default microphone\n"
              "  effects [on|off]    Turn the profile's effects on or off; without either, toggle them\n"
              "  sounds              List the soundboard's sounds\n"
              "  opendeck install    Add the Vox plugin to OpenDeck, for Stream Deck keys\n"
              "  play <sound>        Play a sound into the Vox Microphone, by name or number\n"
              "  engine              Run the audio engine (the vox service does this)\n"
              "  skill [install]     Print the skill for coding agents, or install it\n"
              "\n"
              "Options:\n"
              "  --json              Answer in JSON\n"
              "  -h, --help          Show this help\n"
              "  -v, --version       Show the version");
}

QJsonObject profileJson(const Config &config, int index) {
    QJsonObject object = config.profiles[index].toJson();
    object.insert("number", index + 1);
    object.insert("active", index == config.active);
    return object;
}

// Status works without the service, so it reports what it can and leaves the
// rest false rather than failing.
int status(bool json) {
    QString error;
    const Config config = Config::load(Config::defaultPath(), &error);
    const Result show = execute("systemctl", {"--user", "show", Chain::ServiceName, "-p", "LoadState", "-p",
                                              "ActiveState", "-p", "UnitFileState"});
    const Result source = execute("pactl", {"get-default-source"});
    const Result sources = execute("pactl", {"-f", "json", "list", "sources"});
    const AudioStatus s = PipeWireAudio::parseStatus(show.out, source.out, sources.out);
    QString microphone = config.microphone;
    for (const auto &m : s.microphones)
        if (m.name == config.microphone)
            microphone = m.description;
    const QStringList missing = Chain::missingPackages();

    if (json) {
        QJsonObject object{{"running", s.running},
                           {"enabled", s.enabled},
                           {"failed", s.failed},
                           {"installed", s.serviceInstalled},
                           {"default", s.defaultSource == Chain::SourceName},
                           {"sourcePresent", s.sourcePresent},
                           {"source", Chain::SourceName},
                           {"profile", config.current().name},
                           {"microphone", config.microphone},
                           {"missingPackages", QJsonArray::fromStringList(missing)}};
        if (!error.isEmpty())
            object.insert("configError", error);
        printJson(object);
    } else {
        out(s.running ? tr("Vox is running.") : s.failed ? tr("Vox failed to start.") : tr("Vox is off."));
        if (s.running && !s.sourcePresent)
            out(tr("The Vox Microphone is missing; run vox off and vox on to bring it back."));
        out(tr("Profile: %1").arg(config.current().name));
        out(tr("Microphone: %1").arg(microphone.isEmpty() ? tr("not picked yet") : microphone));
        out(s.defaultSource == Chain::SourceName ? tr("The Vox Microphone is the default microphone.")
                                                 : tr("The Vox Microphone isn't the default microphone."));
        if (!missing.isEmpty())
            out(tr("Missing: %1").arg(missing.join(' ')));
        if (!error.isEmpty())
            out(tr("Config error: %1").arg(error));
    }
    return 0;
}

int sounds(bool json) {
    QString error;
    const Config config = Config::load(Config::defaultPath(), &error);
    if (!error.isEmpty())
        return fail(tr("Couldn't read %1: %2").arg(Config::defaultPath(), error), json);
    if (json) {
        QJsonArray array;
        for (int i = 0; i < config.sounds.size(); ++i)
            array.append(QJsonObject{{"number", i + 1}, {"name", config.sounds[i].name}});
        out(QJsonDocument(array).toJson(QJsonDocument::Indented).trimmed());
        return 0;
    }
    if (config.sounds.isEmpty())
        out(tr("The soundboard is empty; add sounds in the Vox window."));
    for (int i = 0; i < config.sounds.size(); ++i)
        out(QString("  %1. %2").arg(i + 1).arg(config.sounds[i].name));
    return 0;
}

// Plays like the window does: into the mix, with no fallback to the speakers,
// and to the user's own output as well when they hear sounds themselves.
// Waits for the sound to end, so a key binding can't stack up a queue.
int play(const QString &wanted, bool json) {
    if (wanted.isEmpty())
        return fail(tr("Say which sound: vox play <name or number>"), json);
    QString error;
    const Config config = Config::load(Config::defaultPath(), &error);
    if (!error.isEmpty())
        return fail(tr("Couldn't read %1: %2").arg(Config::defaultPath(), error), json);
    const int index = config.findSound(wanted);
    if (index < 0) {
        QStringList names;
        for (const auto &sound : config.sounds)
            names << sound.name;
        return fail(names.isEmpty() ? tr("The soundboard is empty; add sounds in the Vox window.")
                                    : tr("There's no sound called %1. The sounds are: %2").arg(wanted, names.join(", ")),
                    json);
    }
    const Sound &sound = config.sounds[index];
    const QString file = Config::defaultSoundsDirectory() + "/" + sound.file;
    if (!QFileInfo::exists(file))
        return fail(tr("The file for %1 is missing: %2").arg(sound.name, file), json);
    const QString volume = QString::number(config.soundVolume / 100.0, 'f', 3);
    QProcess mix, monitor;
    mix.start("pw-play", {"--target", Chain::MixName, "--volume", volume,
                          "-P", "{ node.dont-fallback = true node.dont-reconnect = true node.description = \"Vox soundboard\" }",
                          file});
    if (!mix.waitForStarted(3000))
        return fail(tr("pw-play is not installed."), json);
    if (config.soundMonitor)
        monitor.start("pw-play", {"--volume", volume, "-P", "{ node.description = \"Vox soundboard (you)\" }", file});
    mix.waitForFinished(-1);
    const bool ok = mix.exitStatus() == QProcess::NormalExit && mix.exitCode() == 0;
    if (!ok) {
        monitor.kill();
        monitor.waitForFinished(1000);
        return fail(tr("%1 couldn't play into the Vox Microphone. Turn it on with: vox on").arg(sound.name), json);
    }
    monitor.waitForFinished(5000);
    if (json)
        printJson({{"ok", true}, {"sound", sound.name}, {"number", index + 1}});
    return 0;
}

int list(bool json) {
    QString error;
    const Config config = Config::load(Config::defaultPath(), &error);
    if (!error.isEmpty())
        return fail(tr("Couldn't read %1: %2").arg(Config::defaultPath(), error), json);
    if (json) {
        QJsonArray array;
        for (int i = 0; i < config.profiles.size(); ++i)
            array.append(profileJson(config, i));
        out(QJsonDocument(array).toJson(QJsonDocument::Indented).trimmed());
        return 0;
    }
    for (int i = 0; i < config.profiles.size(); ++i)
        out(QString("%1 %2. %3").arg(i == config.active ? "*" : " ").arg(i + 1).arg(config.profiles[i].name));
    return 0;
}

int use(const QString &wanted, bool json) {
    if (wanted.isEmpty())
        return fail(tr("Say which profile: vox use <name or number>"), json);
    const QString path = Config::defaultPath();
    QString error;
    Config config = Config::load(path, &error);
    if (!error.isEmpty())
        return fail(tr("Couldn't read %1: %2").arg(path, error), json);
    const int index = config.find(wanted);
    if (index < 0) {
        QStringList names;
        for (const auto &profile : config.profiles)
            names << profile.name;
        return fail(tr("There's no profile called %1. The profiles are: %2").arg(wanted, names.join(", ")), json);
    }
    config.active = index;
    if (!config.save(path, &error))
        return fail(tr("Couldn't save %1: %2").arg(path, error), json);
    // When Vox isn't running this fails, and the engine picks the saved
    // profile up when it starts.
    const Result applied = execute("pw-cli", PipeWireAudio::applyArguments(
                                                 Chain::pod(Chain::params(config.current()))));
    if (json) {
        QJsonObject object = profileJson(config, index);
        object.insert("ok", true);
        // pw-cli exits 0 even when the node is missing, and says so on stderr.
        object.insert("live", applied.code == 0 && !applied.err.contains("Error"));
        printJson(object);
    } else {
        out(tr("Switched to %1.").arg(config.current().name));
    }
    return 0;
}

// Switches the active profile's effects; `how` is on, off or toggle.
int effects(const QString &how, bool json) {
    if (how != "on" && how != "off" && how != "toggle")
        return fail(tr("Say what to do: vox effects on, off or toggle"), json);
    const QString path = Config::defaultPath();
    QString error;
    Config config = Config::load(path, &error);
    if (!error.isEmpty())
        return fail(tr("Couldn't read %1: %2").arg(path, error), json);
    Profile &profile = config.current();
    profile.effects = how == "toggle" ? !profile.effects : how == "on";
    if (!config.save(path, &error))
        return fail(tr("Couldn't save %1: %2").arg(path, error), json);
    // As with `vox use`: harmless when Vox is off, which reads the saved
    // profile when it starts.
    execute("pw-cli", PipeWireAudio::applyArguments(Chain::pod(Chain::params(config.current()))));
    if (json)
        printJson({{"ok", true}, {"profile", profile.name}, {"effects", profile.effects}});
    else
        out(profile.effects ? tr("Effects are on for %1.").arg(profile.name) : tr("Effects are off for %1.").arg(profile.name));
    return 0;
}

// Copies the plugin, compiled into vox, into OpenDeck's plugins, replacing
// an older copy; OpenDeck picks it up when it next starts.
int installOpenDeckPlugin(bool json) {
    const QString config = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + "/opendeck";
    // OpenDeck makes its folder on its first start; a setup script can run
    // before that, so an installed OpenDeck is enough, and finds the plugin
    // waiting when it first starts.
    if (!QFileInfo::exists(config) && QStandardPaths::findExecutable("opendeck").isEmpty())
        return fail(tr("OpenDeck isn't installed here."), json);
    const QString target = config + "/plugins/" + OpenDeckPlugin::Uuid + ".sdPlugin";
    QDir(target).removeRecursively();
    QDirIterator files(":/opendeck", QDir::Files, QDirIterator::Subdirectories);
    while (files.hasNext()) {
        const QString source = files.next();
        const QString path = target + source.mid(QString(":/opendeck").size());
        QDir().mkpath(QFileInfo(path).absolutePath());
        if (!QFile::copy(source, path))
            return fail(tr("Couldn't write %1").arg(path), json);
        // Copies out of the resources are read-only; the launcher must run.
        const bool launcher = path.endsWith("/bin/vox-opendeck");
        QFile::setPermissions(path, QFile::ReadOwner | QFile::WriteOwner | QFile::ReadGroup | QFile::ReadOther
                                        | (launcher ? QFile::ExeOwner | QFile::ExeGroup | QFile::ExeOther
                                                    : QFile::Permissions()));
    }
    if (json)
        printJson({{"ok", true}, {"path", target}});
    else
        out(tr("Installed the Vox plugin into OpenDeck. Restart OpenDeck to load it."));
    return 0;
}

// Run by OpenDeck with the Stream Deck SDK's arguments: -port, -pluginUUID,
// -registerEvent and -info.
int runOpenDeckPlugin(const QStringList &arguments) {
    auto value = [&](const QString &name) { return arguments.value(arguments.indexOf(name) + 1); };
    const int port = value("-port").toInt();
    if (port <= 0)
        return fail(tr("vox opendeck is started by OpenDeck; to add the plugin, run: vox opendeck install"), false);
    CliVoxControl vox;
    OpenDeckPlugin plugin(&vox);
    QObject::connect(&plugin, &OpenDeckPlugin::disconnected, qApp, [] { QCoreApplication::exit(0); });
    plugin.connectTo(port, value("-registerEvent"), value("-pluginUUID"));
    return QCoreApplication::exec();
}

int service(bool on, bool json) {
    if (on) {
        const QStringList missing = Chain::missingPackages();
        if (!missing.isEmpty())
            return fail(tr("Vox needs %1. Install it with: omarchy-pkg-add %1").arg(missing.join(' ')), json);
    }
    const Result result = execute("systemctl", {"--user", on ? "enable" : "disable", "--now", Chain::ServiceName}, 15000);
    if (result.code != 0)
        return fail(QString::fromUtf8(result.err).trimmed(), json);
    if (json)
        printJson({{"ok", true}, {"running", on}});
    else
        out(on ? tr("The Vox Microphone is on, now and at login.") : tr("The Vox Microphone is off."));
    return 0;
}

int makeDefault(bool json) {
    const Result result = execute("pactl", {"set-default-source", Chain::SourceName});
    if (result.code != 0)
        return fail(QString::fromUtf8(result.err).trimmed(), json);
    if (json)
        printJson({{"ok", true}, {"default", Chain::SourceName}});
    else
        out(tr("The Vox Microphone is now the default microphone."));
    return 0;
}

// Run by vox.service: writes the PipeWire configuration for the saved profile
// and becomes the pipewire process that runs it, so systemd supervises the
// filter directly and stopping the service removes the Vox Microphone.
int engine() {
    const QStringList missing = Chain::missingPackages();
    if (!missing.isEmpty())
        return fail(tr("Vox needs %1. Install it with: omarchy-pkg-add %1").arg(missing.join(' ')), false);

    QString error;
    Config config = Config::load(Config::defaultPath(), &error);
    if (!error.isEmpty())
        std::fputs(qPrintable(tr("Couldn't read %1 (%2), so the first preset is used.")
                                  .arg(Config::defaultPath(), error) + '\n'), stderr);
    if (config.microphone.isEmpty()) {
        const QString source = QString::fromUtf8(execute("pactl", {"get-default-source"}).out).trimmed();
        if (source != Chain::SourceName)
            config.microphone = source;
    }

    const QString directory = QStandardPaths::writableLocation(QStandardPaths::GenericStateLocation) + "/vox";
    const QString path = directory + "/pipewire.conf";
    QDir().mkpath(directory);
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return fail(tr("Couldn't write %1: %2").arg(path, file.errorString()), false);
    file.write(Chain::configuration(config.current(), config.microphone).toUtf8());
    if (!file.commit())
        return fail(tr("Couldn't write %1: %2").arg(path, file.errorString()), false);

    const QString pipewire = QStandardPaths::findExecutable("pipewire");
    if (pipewire.isEmpty())
        return fail(tr("pipewire is not installed."), false);
    const QByteArray program = pipewire.toLocal8Bit(), conf = path.toLocal8Bit();
    std::fflush(stdout);
    std::fflush(stderr);
    execl(program.constData(), "pipewire", "-c", conf.constData(), static_cast<char *>(nullptr));
    return fail(tr("Couldn't start pipewire: %1").arg(QString::fromLocal8Bit(strerror(errno))), false);
}

QByteArray skillText() {
    QFile file(":/skill.md");
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

// The skill lives in ~/.agents/skills/vox for every agent, linked into
// ~/.claude/skills, the same way Hype installs its own.
int installSkill(bool json) {
    const QString home = QDir::homePath();
    const QString directory = home + "/.agents/skills/vox";
    QDir().mkpath(directory);
    QSaveFile file(directory + "/SKILL.md");
    if (!file.open(QIODevice::WriteOnly) || file.write(skillText()) < 0 || !file.commit())
        return fail(tr("Couldn't write %1: %2").arg(directory + "/SKILL.md", file.errorString()), json);

    const QString claudeSkills = home + "/.claude/skills";
    const QString link = claudeSkills + "/vox";
    QDir().mkpath(claudeSkills);
    const QFileInfo existing(link);
    if (existing.isSymLink() || !existing.exists()) {
        QFile::remove(link);
        QFile::link("../../.agents/skills/vox", link);
    }
    if (json)
        printJson({{"ok", true}, {"path", directory + "/SKILL.md"}});
    else
        out(tr("Installed the Vox skill in %1").arg(directory));
    return 0;
}

}

namespace Cli {

bool wantsWindow(const QStringList &arguments) {
    const QStringList rest = arguments.mid(1);
    return rest.isEmpty() || rest.first() == "open" || rest.first() == "tray";
}

int run(const QStringList &arguments) {
    QStringList rest = arguments.mid(1);
    const bool json = rest.removeAll("--json") > 0;
    if (rest.isEmpty() || rest.first() == "open" || rest.first() == "tray")
        return -1;
    const QString command = rest.takeFirst();

    if (command == "-h" || command == "--help" || command == "help") {
        out(usage());
        return 0;
    }
    if (command == "-v" || command == "--version") {
        out("vox " + QCoreApplication::applicationVersion());
        return 0;
    }
    if (command == "status")
        return status(json);
    if (command == "list")
        return list(json);
    if (command == "use")
        return use(rest.join(' '), json);
    if (command == "on" || command == "off")
        return service(command == "on", json);
    if (command == "effects")
        return effects(rest.value(0, "toggle"), json);
    if (command == "opendeck")
        return rest.value(0) == "install" ? installOpenDeckPlugin(json) : runOpenDeckPlugin(rest);
    if (command == "sounds")
        return sounds(json);
    if (command == "play")
        return play(rest.join(' '), json);
    if (command == "default")
        return makeDefault(json);
    if (command == "engine")
        return engine();
    if (command == "skill") {
        if (rest.value(0) == "install")
            return installSkill(json);
        std::fputs(skillText().constData(), stdout);
        return 0;
    }
    std::fputs(qPrintable(usage() + '\n'), stderr);
    return 1;
}

}
