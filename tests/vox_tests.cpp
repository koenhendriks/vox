#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtTest>

#include "backend.h"
#include "chain.h"
#include "config.h"
#include "fakeaudio.h"
#include "meter.h"
#include "pipewireaudio.h"
#include "theme.h"
#include "translator.h"

class VoxTests : public QObject {
    Q_OBJECT

private slots:
    void presetsHaveUniqueNamesAndValidRanges()
    {
        const auto presets = Profile::presets();
        QVERIFY(presets.size() >= 3);
        QStringList names;
        for (const auto &profile : presets) {
            QVERIFY(!names.contains(profile.name));
            names << profile.name;
            for (const auto &control : Profile::controls()) {
                const double value = profile.value(control.key).toDouble();
                QVERIFY2(value >= control.min && value <= control.max, qPrintable(profile.name + " " + control.key));
            }
        }
    }

    void profileClampsAndIgnoresUnknownKeys()
    {
        Profile profile;
        QVERIFY(profile.set("bass", 99));
        QCOMPARE(profile.bass, 12.0);
        QVERIFY(profile.set("output", -99));
        QCOMPARE(profile.output, -12.0);
        QVERIFY(!profile.set("volume", 3));
    }

    void configRoundTripsAndSavesAtomically()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath("nested/config.json");
        Config config = Config::defaults();
        config.microphone = "alsa_input.usb-studio";
        config.active = 2;
        config.profiles[2].bass = 4.5;
        QVERIFY(config.save(path));
        QString error;
        QCOMPARE(Config::load(path, &error), config);
        QVERIFY(error.isEmpty());
        QVERIFY(!QFile::exists(path + ".tmp"));
    }

    void brokenConfigIsReportedNotReplaced()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath("config.json");
        write(path, "{ not json");
        QString error;
        const Config config = Config::load(path, &error);
        QVERIFY(!error.isEmpty());
        QCOMPARE(config.profiles, Profile::presets());

        FakeAudio audio;
        audio.status = FakeAudio::sample();
        Backend backend(&audio, path, plugins(dir));
        QVERIFY(backend.problem().contains("couldn't be read"));
        backend.edit("bass", 3);
        backend.shutdown();
        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), QByteArray("{ not json"));
    }

    void findsProfilesByNameOrNumber()
    {
        const Config config = Config::defaults();
        QCOMPARE(config.find("broadcast"), 1);
        QCOMPARE(config.find("2"), 1);
        QCOMPARE(config.find("0"), -1);
        QCOMPARE(config.find("99"), -1);
        QCOMPARE(config.find("Nope"), -1);
        QCOMPARE(config.uniqueName("Podcast"), QString("Podcast 2"));
    }

    void compressorScalesWithAmount()
    {
        const auto off = Chain::compressor(0);
        QCOMPARE(off.ratio, 1.0);
        QCOMPARE(off.makeup, 0.0);
        const auto full = Chain::compressor(100);
        QCOMPARE(full.ratio, 6.0);
        QCOMPARE(full.threshold, -30.0);
        QVERIFY(full.makeup > 10 && full.makeup < 15);
    }

    void paramsRouteDenoiseAndBypass()
    {
        Profile profile;
        profile.denoise = false;
        auto value = [](const Chain::Params &params, const QString &name) {
            for (const auto &[key, v] : params)
                if (key == name)
                    return v;
            return -1.0;
        };
        const auto normal = Chain::params(profile);
        QCOMPARE(value(normal, "denoisemix:Gain 1"), 0.0);
        QCOMPARE(value(normal, "denoisemix:Gain 2"), 1.0);
        QCOMPARE(value(normal, "output:Gain 1"), 1.0);
        const auto bypass = Chain::params(profile, true);
        QCOMPARE(value(bypass, "output:Gain 1"), 0.0);
        QCOMPARE(value(bypass, "output:Gain 2"), 1.0);
        QCOMPARE(Chain::pod({{"bass:Gain", 1.5}}), QString(R"({ params = [ "bass:Gain" 1.5000 ] })"));
    }

    void configurationTargetsTheMicrophone()
    {
        Profile profile = Profile::presets().first();
        const QString conf = Chain::configuration(profile, "alsa_input.usb-studio");
        QVERIFY(conf.contains(R"(target.object = "alsa_input.usb-studio")"));
        QVERIFY(conf.contains(R"(node.name = "vox.microphone")"));
        QVERIFY(conf.contains(R"(node.name = "vox.capture")"));
        QVERIFY(conf.contains(R"("Gain" = 2.0000)"));
        QVERIFY(conf.contains("node.dont-fallback = true"));
        QVERIFY(!conf.contains(QRegularExpression("%\\d")));

        QVERIFY(!Chain::configuration(profile, {}).contains("target.object"));
        QVERIFY(Chain::configuration(profile, "evil\" } ]").contains(R"(target.object = "")"));
    }

    void reportsMissingPlugins()
    {
        QTemporaryDir dir;
        QCOMPARE(Chain::missingPackages({dir.path()}),
                 QStringList({"noise-suppression-for-voice", "swh-plugins"}));
        for (const auto &plugin : Chain::plugins())
            write(dir.filePath(plugin.file), "");
        QVERIFY(Chain::missingPackages({dir.path()}).isEmpty());
    }

    void parsesStatusAndSkipsMonitorsAndItself()
    {
        const QByteArray show = "LoadState=loaded\nActiveState=active\nUnitFileState=enabled\n";
        const QByteArray sources = R"([
            {"name": "alsa_output.speakers.monitor", "description": "Monitor of Speakers", "monitor_of_sink": "alsa_output.speakers", "properties": {"device.class": "monitor"}},
            {"name": "alsa_input.usb-studio", "description": "Studio USB Microphone", "monitor_of_sink": null, "properties": {"device.class": "sound"}},
            {"name": "vox.microphone", "description": "Vox Microphone", "monitor_of_sink": null, "properties": {}}
        ])";
        const AudioStatus status = PipeWireAudio::parseStatus(show, "vox.microphone\n", sources);
        QVERIFY(status.serviceInstalled && status.running && status.enabled && !status.failed);
        QCOMPARE(status.defaultSource, QString("vox.microphone"));
        QCOMPARE(status.microphones.size(), 1);
        QCOMPARE(status.microphones.first().name, QString("alsa_input.usb-studio"));

        const AudioStatus off = PipeWireAudio::parseStatus("LoadState=not-found\nActiveState=failed\n", "", "[]");
        QVERIFY(!off.serviceInstalled && !off.running && off.failed);
    }

    void meterMeasuresRms()
    {
        QCOMPARE(Meter::rms(nullptr, 0), -90.0);
        const float silence[4] = {0, 0, 0, 0};
        QCOMPARE(Meter::rms(silence, 4), -90.0);
        const float full[4] = {1, -1, 1, -1};
        QCOMPARE(Meter::rms(full, 4), 0.0);
        const float half[2] = {0.5f, -0.5f};
        QVERIFY(qAbs(Meter::rms(half, 2) + 6.0206) < 0.001);
    }

    void backendPicksTheDefaultMicrophone()
    {
        QTemporaryDir dir;
        FakeAudio audio;
        audio.status = FakeAudio::sample();
        audio.status.defaultSource = "alsa_input.pci-laptop";
        Backend backend(&audio, dir.filePath("config.json"), plugins(dir));
        QCOMPARE(backend.microphone(), QString("alsa_input.pci-laptop"));
        QVERIFY(backend.microphoneConnected());
    }

    void backendNeverPicksItselfAsMicrophone()
    {
        QTemporaryDir dir;
        FakeAudio audio;
        audio.status = FakeAudio::sample();
        audio.status.defaultSource = Chain::SourceName;
        Backend backend(&audio, dir.filePath("config.json"), plugins(dir));
        QCOMPARE(backend.microphone(), QString("alsa_input.usb-studio"));
    }

    void editsApplyLiveSaveAndUndo()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath("config.json");
        FakeAudio audio;
        audio.status = FakeAudio::sample();
        Backend backend(&audio, path, plugins(dir));
        const double before = backend.current().value("bass").toDouble();

        backend.beginEdit();
        backend.setValue("bass", 9);
        backend.setValue("bass", 10);
        QVERIFY(audio.applied.last().contains(R"("bass:Gain" 10.0000)"));
        QVERIFY(backend.canUndo());

        backend.undo();
        QCOMPARE(backend.current().value("bass").toDouble(), before);
        QVERIFY(backend.canRedo());
        backend.redo();
        QCOMPARE(backend.current().value("bass").toDouble(), 10.0);

        QTRY_VERIFY(QFile::exists(path));
        QTRY_COMPARE(Config::load(path).current().bass, 10.0);
    }

    void profilesCanBeCreatedRenamedAndDeleted()
    {
        QTemporaryDir dir;
        FakeAudio audio;
        audio.status = FakeAudio::sample();
        Backend backend(&audio, dir.filePath("config.json"), plugins(dir));
        const int count = backend.profileNames().size();

        const int index = backend.newProfile();
        QCOMPARE(backend.profileNames().size(), count + 1);
        QCOMPARE(backend.activeIndex(), index);
        QVERIFY(backend.renameProfile(index, "  ").length() > 0);
        QVERIFY(backend.renameProfile(index, "42").length() > 0);
        QVERIFY(backend.renameProfile(index, "broadcast").length() > 0);
        QCOMPARE(backend.renameProfile(index, "Late night"), QString());
        QCOMPARE(backend.profileNames().at(index), QString("Late night"));

        QVERIFY(backend.deleteProfile(index));
        QCOMPARE(backend.profileNames().size(), count);
        backend.undo();
        QCOMPARE(backend.profileNames().at(index), QString("Late night"));

        while (backend.profileNames().size() > 1)
            QVERIFY(backend.deleteProfile(0));
        QVERIFY(!backend.deleteProfile(0));
    }

    void switchingProfilesAppliesThem()
    {
        QTemporaryDir dir;
        FakeAudio audio;
        audio.status = FakeAudio::sample();
        Backend backend(&audio, dir.filePath("config.json"), plugins(dir));
        backend.selectProfile(1);
        const auto expected = Chain::pod(Chain::params(Profile::presets().at(1)));
        QCOMPARE(audio.applied.last(), expected);
    }

    void bypassIsAppliedAndUndoneOnShutdown()
    {
        QTemporaryDir dir;
        FakeAudio audio;
        audio.status = FakeAudio::sample();
        Backend backend(&audio, dir.filePath("config.json"), plugins(dir));
        backend.toggleBypass();
        QVERIFY(backend.bypass());
        QVERIFY(audio.applied.last().contains(R"("output:Gain 2" 1.0000)"));
        backend.shutdown();
        QVERIFY(!backend.bypass());
        QVERIFY(audio.applied.last().contains(R"("output:Gain 2" 0.0000)"));
    }

    void changingMicrophoneSavesThenRestarts()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath("config.json");
        FakeAudio audio;
        audio.status = FakeAudio::sample();
        audio.status.running = true;
        Backend backend(&audio, path, plugins(dir));
        backend.setMicrophone("alsa_input.pci-laptop");
        QCOMPARE(Config::load(path).microphone, QString("alsa_input.pci-laptop"));
        QCOMPARE(audio.restarts, 1);
    }

    void reportsADisconnectedMicrophone()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath("config.json");
        Config config = Config::defaults();
        config.microphone = "alsa_input.usb-unplugged";
        QVERIFY(config.save(path));
        FakeAudio audio;
        audio.status = FakeAudio::sample();
        Backend backend(&audio, path, plugins(dir));
        QVERIFY(!backend.microphoneConnected());
        QVERIFY(backend.problem().contains("isn't connected"));
    }

    void reportsMissingPluginsInTheWindow()
    {
        QTemporaryDir dir;
        FakeAudio audio;
        audio.status = FakeAudio::sample();
        Backend backend(&audio, dir.filePath("config.json"), {dir.filePath("nowhere")});
        QVERIFY(backend.problem().contains("swh-plugins"));
    }

    void followsChangesFromTheCommandLine()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath("config.json");
        QVERIFY(Config::defaults().save(path));
        FakeAudio audio;
        audio.status = FakeAudio::sample();
        Backend backend(&audio, path, plugins(dir));

        Config changed = Config::load(path);
        changed.active = 3;
        QVERIFY(changed.save(path));
        QTRY_COMPARE(backend.activeIndex(), 3);
        QCOMPARE(audio.applied.last(), Chain::pod(Chain::params(changed.current())));
    }

    void themeFallsBack()
    {
        QTemporaryDir dir;
        Theme theme(dir.path());
        QCOMPARE(theme.accent(), QString("#FFD60A"));
        QCOMPARE(theme.accentForeground(), QString("black"));
    }

    void themeReadsAccent()
    {
        QTemporaryDir dir;
        QDir(dir.path()).mkpath("theme");
        write(dir.path() + "/theme/colors.toml", "accent = \"#112233\"\n");
        Theme theme(dir.path());
        QCOMPARE(theme.accent(), QString("#112233"));
        QCOMPARE(theme.accentForeground(), QString("white"));
    }

    void themeFollowsHyprlandRounding()
    {
        QTemporaryDir dir;
        const QString response = dir.filePath("response.json"), command = dir.filePath("hyprctl");
        write(command, "#!/bin/sh\ncat '" + response.toUtf8() + "'\n");
        QVERIFY(QFile::setPermissions(command, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        write(response, "{\"int\":0}");

        Theme theme(dir.path(), nullptr, command);
        QCOMPARE(theme.radius(), 0);
        write(response, "{\"int\":6}");
        QTRY_COMPARE(theme.radius(), 6);
    }

    void translatorFallsBackToEnglish()
    {
        QTemporaryDir dir;
        Translator translator(dir.path() + "/language", dir.path());
        QCOMPARE(translator.language(), QString("en"));
        QVERIFY(translator.translate("Main", "Save").isNull());
    }

    void translatorFollowsTheLanguageFile()
    {
        QTemporaryDir dir;
        write(dir.path() + "/language", "pt-BR\n");
        write(dir.path() + "/pt.json", R"({"Save": "Guardar", "Open": "Abrir"})");
        write(dir.path() + "/pt-BR.json", R"({"Save": "Salvar"})");

        Translator translator(dir.path() + "/language", dir.path());
        QCOMPARE(translator.language(), QString("pt-BR"));
        QCOMPARE(translator.translate("Main", "Save"), QString("Salvar"));
        QCOMPARE(translator.translate("Main", "Open"), QString("Abrir"));
        QVERIFY(translator.translate("Main", "Quit").isNull());
    }

    void translatorPicksPluralForms()
    {
        QCOMPARE(Translator::pluralCategory("ru", 21), QString("one"));
        QCOMPARE(Translator::pluralCategory("ru", 22), QString("few"));
        QCOMPARE(Translator::pluralCategory("ar", 11), QString("many"));
        QCOMPARE(Translator::pluralCategory("ja", 1), QString("other"));
    }

    // Every shipped catalog parses, and every translation keeps the
    // placeholders of its English source, so .arg() fills the same slots.
    void catalogsKeepPlaceholders()
    {
        const QStringList catalogs = QDir(SRCDIR "../src/i18n").entryList({"*.json"});
        QVERIFY(catalogs.contains("nl.json"));
        static const QRegularExpression placeholder("%\\d");
        for (const auto &name : catalogs) {
            QFile file(SRCDIR "../src/i18n/" + name);
            QVERIFY(file.open(QIODevice::ReadOnly));
            QJsonParseError error;
            const QJsonObject catalog = QJsonDocument::fromJson(file.readAll(), &error).object();
            QVERIFY2(error.error == QJsonParseError::NoError, qPrintable(name + ": " + error.errorString()));
            for (auto it = catalog.begin(); it != catalog.end(); ++it) {
                if (!it.value().isString())
                    continue;
                QStringList source, translated;
                for (const auto &m : placeholder.globalMatch(it.key())) source << m.captured();
                for (const auto &m : placeholder.globalMatch(it.value().toString())) translated << m.captured();
                source.sort();
                translated.sort();
                source.removeDuplicates();
                translated.removeDuplicates();
                QVERIFY2(source == translated, qPrintable(name + ": " + it.key()));
            }
        }
    }

private:
    // A plugin directory with every LADSPA plugin Vox needs, so the window
    // reports the problem under test and not missing packages.
    static QStringList plugins(const QTemporaryDir &dir)
    {
        for (const auto &plugin : Chain::plugins())
            write(dir.filePath(plugin.file), "");
        return {dir.path()};
    }

    static void write(const QString &path, const QByteArray &contents)
    {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(contents);
    }
};

QTEST_MAIN(VoxTests)
#include "vox_tests.moc"
