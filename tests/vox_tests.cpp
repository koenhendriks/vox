#include <QDir>
#include <QFile>
#include <QFileInfo>
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
#include "player.h"
#include "take.h"
#include "testrecorder.h"
#include "theme.h"
#include "translator.h"
#include "instance.h"
#include "trayicon.h"
#include <QImage>
#include <QSignalSpy>

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
        // Without linger, WirePlumber destroys the capture stream when the
        // mic disappears, and the Vox Microphone with it.
        QVERIFY(conf.contains("node.linger = true"));
        QVERIFY(!conf.contains(QRegularExpression("%\\d")));

        QVERIFY(!Chain::configuration(profile, {}).contains("target.object"));
        QVERIFY(Chain::configuration(profile, "evil\" } ]").contains(R"(target.object = "")"));
    }

    void effectsAreOffForOldProfilesAndRoundTrip()
    {
        const Profile old = Profile::fromJson(QJsonObject{{"name", "Old"}, {"bass", 3}});
        QVERIFY(!old.effects);
        QCOMPARE(old.autotuneNotes, 1);
        QCOMPARE(old.autotuneRoot, 9);
        QCOMPARE(old.autotuneSpeed, 100.0);

        Profile profile;
        profile.name = "Robot";
        QVERIFY(profile.set("effects", true));
        QVERIFY(profile.set("autotuneNotes", 4.4));
        QVERIFY(profile.set("autotuneRoot", 30));
        QVERIFY(profile.set("autotuneSpeed", 140));
        QCOMPARE(profile.autotuneNotes, 4);
        QCOMPARE(profile.autotuneRoot, 11);
        QCOMPARE(profile.autotuneSpeed, 100.0);
        QCOMPARE(Profile::fromJson(profile.toJson()), profile);
    }

    void effectsSwitchTheAutotuneInLive()
    {
        Profile profile;
        auto value = [](const Chain::Params &params, const QString &name) {
            for (const auto &[key, v] : params)
                if (key == name)
                    return v;
            return -1.0;
        };
        QCOMPARE(value(Chain::params(profile), "effectsmix:Gain 1"), 0.0);
        QCOMPARE(value(Chain::params(profile), "effectsmix:Gain 2"), 1.0);
        profile.effects = true;
        profile.autotuneNotes = 1;   // major chord
        profile.autotuneRoot = 9;    // A: A, C# and E
        profile.autotuneSpeed = 100;
        const auto on = Chain::params(profile);
        QCOMPARE(value(on, "effectsmix:Gain 1"), 1.0);
        QCOMPARE(value(on, "effectsmix:Gain 2"), 0.0);
        QStringList enabled;
        for (int i = 0; i < 12; ++i)
            if (value(on, QString("autotune:m%1").arg(i, 2, 10, QChar('0'))) == 1.0)
                enabled << QString::number(i);
        QCOMPARE(enabled, QStringList({"1", "4", "9"}));
        QCOMPARE(value(on, "autotune:filter"), 0.02);
        QCOMPARE(value(on, "autotune:fastmode"), 1.0);
        profile.autotuneSpeed = 0;
        QVERIFY(qAbs(value(Chain::params(profile), "autotune:filter") - 0.30) < 1e-9);
        QCOMPARE(value(Chain::params(profile), "autotune:fastmode"), 0.0);
    }

    void noteMasksGetSmallerTowardsTheRobot()
    {
        auto count = [](const QList<bool> &mask) { return int(std::count(mask.begin(), mask.end(), true)); };
        QCOMPARE(count(Chain::noteMask(0, 9)), 1);
        QVERIFY(Chain::noteMask(0, 9)[9]);
        QCOMPARE(count(Chain::noteMask(1, 0)), 3);
        QCOMPARE(count(Chain::noteMask(3, 0)), 7);
        QCOMPARE(count(Chain::noteMask(5, 7)), 12);
        // C minor chord: C, Eb and G.
        const QList<bool> cMinor = Chain::noteMask(2, 0);
        QVERIFY(cMinor[0] && cMinor[3] && cMinor[7]);
        // Out-of-range settings fall back to the nearest set rather than crash.
        QCOMPARE(count(Chain::noteMask(99, 0)), 12);
    }

    void configurationRoutesThroughTheAutotune()
    {
        const QString conf = Chain::configuration(Profile::presets().first(), "alsa_input.usb-studio");
        const QString node = QStringLiteral("type = lv2 name = autotune plugin = \"http://gareus.org/oss/lv2/fat1\" control");
        QVERIFY(conf.contains(node));
        QVERIFY(conf.contains(R"({ output = "rumble:Out" input = "autotune:in" })"));
        QVERIFY(conf.contains(R"({ output = "autotune:out" input = "effectsmix:In 1" })"));
        QVERIFY(conf.contains(R"({ output = "rumble:Out" input = "effectsmix:In 2" })"));
        QVERIFY(conf.contains(R"({ output = "effectsmix:Out" input = "bass:In" })"));
        QVERIFY(!conf.contains(R"({ output = "rumble:Out" input = "bass:In" })"));
        QVERIFY(conf.contains(R"("mode" = 0.0000)"));
    }

    void reportsMissingPlugins()
    {
        QTemporaryDir dir;
        QCOMPARE(Chain::missingPackages({dir.path()}),
                 QStringList({"noise-suppression-for-voice", "swh-plugins", "x42-plugins-lv2"}));
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
        QVERIFY(status.sourcePresent);

        const AudioStatus off = PipeWireAudio::parseStatus("LoadState=not-found\nActiveState=failed\n", "", "[]");
        QVERIFY(!off.serviceInstalled && !off.running && off.failed);
        QVERIFY(!off.sourcePresent);
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

    void reportsAMissingVoxMicrophone()
    {
        QTemporaryDir dir;
        FakeAudio audio;
        audio.status = FakeAudio::sample();
        audio.status.running = true;
        audio.status.sourcePresent = false;
        Backend backend(&audio, dir.filePath("config.json"), plugins(dir));
        // The engine may still be starting on the first look.
        QVERIFY(!backend.sourceMissing());
        backend.refresh();
        QVERIFY(backend.sourceMissing());
        QVERIFY(backend.problem().contains("Vox Microphone is gone"));

        audio.status.sourcePresent = true;
        backend.refresh();
        QVERIFY(!backend.sourceMissing());
        QVERIFY(!backend.problem().contains("gone"));
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

    void alignmentFindsTheDelay()
    {
        // Bursts of noise with gaps, like speech, and a copy that trails it
        // by 37 ms and has its level changed, like Vox's output.
        const QVector<float> speech = syllables(3 * Take::Rate);
        const qint64 delay = Take::Rate * 37 / 1000;
        QVector<float> late(delay, 0.0f);
        for (float s : speech)
            late << s * 2.5f;
        QVERIFY(qAbs(Take::lag(speech, late) - delay) <= Take::Rate / 1000);
        QVERIFY(qAbs(Take::lag(late, speech) + delay) <= Take::Rate / 1000);

        QVector<float> raw = speech, vox = late;
        Take::align(raw, vox);
        QCOMPARE(raw.size(), vox.size());
        QVERIFY(qAbs(Take::lag(raw, vox)) <= Take::Rate / 1000);
    }

    void alignmentLeavesUnrelatedTakesAlone()
    {
        const QVector<float> silence(Take::Rate, 0.0f);
        QCOMPARE(Take::lag(silence, syllables(Take::Rate)), qint64(0));
    }

    void curveAndLevelsUseDecibels()
    {
        QVector<float> samples(1000, 0.0f);
        for (int i = 500; i < 1000; ++i)
            samples[i] = (i % 2 ? 0.5f : -0.5f);
        const QVector<float> curve = Take::curve(samples, 2);
        QCOMPARE(curve.size(), 2);
        QCOMPARE(curve[0], 0.0f);
        QVERIFY(qAbs(curve[1] - float((60 - 6.0206) / 60)) < 0.001f);

        const Take::Levels levels = Take::levels(samples);
        QVERIFY(qAbs(levels.peak + 6.0206) < 0.001);
        QVERIFY(qAbs(levels.loudness + 9.0309) < 0.001);
        QCOMPARE(Take::levels({}).peak, -90.0);
    }

    void playerPlaysMonoTakesInBothChannels()
    {
        const float samples[3] = {0.5f, -0.25f, 2.0f};
        const QByteArray floats = Player::stereo(samples, 3, QAudioFormat::Float);
        QCOMPARE(floats.size(), qsizetype(6 * sizeof(float)));
        const auto *f = reinterpret_cast<const float *>(floats.constData());
        for (int i = 0; i < 3; ++i) {
            QCOMPARE(f[2 * i], samples[i]);
            QCOMPARE(f[2 * i + 1], samples[i]);
        }
        const QByteArray ints = Player::stereo(samples, 3, QAudioFormat::Int16);
        const auto *n = reinterpret_cast<const qint16 *>(ints.constData());
        QCOMPARE(n[0], qint16(16384));
        QCOMPARE(n[1], qint16(16384));
        QCOMPARE(n[4], qint16(32767));
        QCOMPARE(n[5], qint16(32767));
    }

    void testRecordingNeedsVoxRunning()
    {
        QTemporaryDir dir;
        FakeAudio audio;
        audio.status = FakeAudio::sample();
        Backend backend(&audio, dir.filePath("config.json"), plugins(dir));
        TestRecorder recorder(&audio, &backend);
        recorder.record();
        QCOMPARE(recorder.state(), QString("idle"));
        QVERIFY(recorder.error().contains("Turn on"));
        QVERIFY(audio.captureTargets.isEmpty());
    }

    void testRecordingCapturesBothAndLinesThemUp()
    {
        QTemporaryDir dir;
        FakeAudio audio;
        audio.status = FakeAudio::sample();
        audio.status.running = true;
        Backend backend(&audio, dir.filePath("config.json"), plugins(dir));
        backend.selectProfile(1);
        backend.toggleBypass();
        TestRecorder recorder(&audio, &backend);

        recorder.record();
        QCOMPARE(recorder.state(), QString("recording"));
        QCOMPARE(audio.captureTargets, QStringList({"alsa_input.usb-studio", QString(Chain::SourceName)}));
        QVERIFY2(!backend.bypass(), "compare mode would make both recordings raw");

        const QVector<float> speech = syllables(2 * Take::Rate);
        QVector<float> vox(Take::Rate / 50, 0.0f);
        for (float s : speech)
            vox << s * 2;
        audio.nextCapture = {bytes(speech), bytes(vox)};
        recorder.stop();

        QCOMPARE(recorder.state(), QString("ready"));
        QCOMPARE(recorder.profileName(), backend.profileNames().at(1));
        QCOMPARE(recorder.raw().size(), recorder.vox().size());
        QVERIFY(recorder.duration() > 1.9);
        QCOMPARE(recorder.rawCurve().size(), TestRecorder::CurvePoints);
        QVERIFY(recorder.voxPeak() > recorder.rawPeak());
        QCOMPARE(recorder.track(), 0);
        recorder.switchTrack();
        QCOMPARE(recorder.track(), 1);

        recorder.close();
        QCOMPARE(recorder.state(), QString("idle"));
    }

    void testRecordingReportsSilenceFromVox()
    {
        QTemporaryDir dir;
        FakeAudio audio;
        audio.status = FakeAudio::sample();
        audio.status.running = true;
        Backend backend(&audio, dir.filePath("config.json"), plugins(dir));
        TestRecorder recorder(&audio, &backend);
        recorder.record();
        audio.nextCapture = {bytes(syllables(Take::Rate * 2)), QByteArray()};
        recorder.stop();
        QCOMPARE(recorder.state(), QString("idle"));
        QVERIFY(recorder.error().contains("Vox Microphone"));
    }

    void testRecordingRejectsTooShortTakes()
    {
        QTemporaryDir dir;
        FakeAudio audio;
        audio.status = FakeAudio::sample();
        audio.status.running = true;
        Backend backend(&audio, dir.filePath("config.json"), plugins(dir));
        TestRecorder recorder(&audio, &backend);
        recorder.record();
        const QVector<float> blip = syllables(Take::Rate / 4);
        audio.nextCapture = {bytes(blip), bytes(blip)};
        recorder.stop();
        QCOMPARE(recorder.state(), QString("idle"));
        QVERIFY(recorder.error().contains("too short"));
    }

    void trayIconsFollowStateAndAccent()
    {
        QTemporaryDir dir;
        TrayIcons icons(dir.path());
        const QUrl on = icons.source("on", "#112233");
        const QUrl off = icons.source("off", "#112233");
        QVERIFY(on.isLocalFile() && QFile::exists(on.toLocalFile()));
        QVERIFY(on != off);
        QVERIFY(icons.source("on", "#445566") != on);
        QCOMPARE(icons.source("on", "#112233"), on);

        // The middle of the mic capsule carries the color of the state.
        const QImage onImage(on.toLocalFile()), offImage(off.toLocalFile());
        QCOMPARE(onImage.size(), QSize(64, 64));
        QCOMPARE(QColor(onImage.pixel(32, 20)).name(), QString("#112233"));
        QCOMPARE(QColor(offImage.pixel(32, 30)).name(), QString("#9b9ba3"));
        QCOMPARE(qAlpha(onImage.pixel(2, 2)), 0);
        // The slash crosses the icon only when Vox is off.
        QCOMPARE(qAlpha(onImage.pixel(45, 47)), 0);
        QVERIFY(qAlpha(offImage.pixel(45, 47)) > 200);
        // Comparing shows a hollow mic.
        const QImage compare(icons.source("compare", "#112233").toLocalFile());
        QCOMPARE(qAlpha(compare.pixel(32, 20)), 0);
        QVERIFY(qAlpha(compare.pixel(25, 20)) > 200);
    }

    void secondInstanceHandsOverItsCommand()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath("instance.sock");
        Instance first(path);
        QVERIFY(!first.send("open"));
        QVERIFY(first.listen());

        QSignalSpy spy(&first, &Instance::command);
        Instance second(path);
        QVERIFY(second.send("open"));
        QVERIFY(spy.wait(2000));
        QCOMPARE(spy.first().first().toString(), QString("open"));
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
    // Deterministic noise bursts of uneven length with gaps between them.
    static QVector<float> syllables(qsizetype length)
    {
        QVector<float> samples(length, 0.0f);
        quint32 seed = 12345;
        qsizetype i = 0;
        int burst = 0;
        while (i < length) {
            const qsizetype on = Take::Rate * (80 + (burst * 37) % 170) / 1000;
            const qsizetype off = Take::Rate * (40 + (burst * 53) % 120) / 1000;
            for (qsizetype j = 0; j < on && i < length; ++j, ++i) {
                seed = seed * 1664525u + 1013904223u;
                samples[i] = (float(seed >> 8) / float(1 << 24) - 0.5f) * 0.4f;
            }
            i += off;
            ++burst;
        }
        return samples;
    }

    static QByteArray bytes(const QVector<float> &samples)
    {
        return QByteArray(reinterpret_cast<const char *>(samples.constData()), samples.size() * sizeof(float));
    }

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
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(contents);
    }
};

QTEST_MAIN(VoxTests)
#include "vox_tests.moc"
