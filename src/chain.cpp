#include "chain.h"

#include <QDir>
#include <QFileInfo>
#include <QMap>
#include <cmath>

namespace Chain {

// One amount drives the whole compressor so it stays a single slider: the
// threshold drops and the ratio climbs together, and make-up gain wins back
// about half of what the compressor takes at the threshold, so turning it up
// sounds denser rather than quieter.
Compressor compressor(double amount) {
    const double a = std::clamp(amount, 0.0, 100.0) / 100.0;
    Compressor c;
    c.ratio = 1.0 + 5.0 * a;
    c.threshold = -6.0 - 24.0 * a;
    c.makeup = -c.threshold * (1.0 - 1.0 / c.ratio) * 0.5;
    return c;
}

// x42-autotune's filter is how long it takes to settle on a note: its
// minimum, 0.02, jumps at once, which is the over-the-top sound.
double retuneFilter(double speed) {
    return 0.02 + (1.0 - std::clamp(speed, 0.0, 100.0) / 100.0) * 0.28;
}

QList<bool> noteMask(int notes, int root) {
    // In the order the window lists them, wildest first.
    static const QList<QList<int>> sets{
        {0},                         // robot: one note
        {0, 4, 7},                   // major chord
        {0, 3, 7},                   // minor chord
        {0, 2, 4, 5, 7, 9, 11},      // major key
        {0, 2, 3, 5, 7, 8, 10},      // natural minor key
        {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11},
    };
    const QList<int> &set = sets[std::clamp(notes, 0, int(sets.size()) - 1)];
    QList<bool> mask(12, false);
    for (int interval : set)
        mask[(((root + interval) % 12) + 12) % 12] = true;
    return mask;
}

Params params(const Profile &p, bool bypass) {
    const Compressor c = compressor(p.compression);
    Params notes;
    const QList<bool> mask = noteMask(p.autotuneNotes, p.autotuneRoot);
    for (int i = 0; i < 12; ++i)
        notes.append({QString("autotune:m%1").arg(i, 2, 10, QChar('0')), mask[i] ? 1.0 : 0.0});
    return notes + Params{
        {"denoise:VAD Threshold (%)", p.gate},
        {"denoisemix:Gain 1", p.denoise ? 1.0 : 0.0},
        {"denoisemix:Gain 2", p.denoise ? 0.0 : 1.0},
        {"rumble:Freq", p.rumble},
        {"autotune:filter", retuneFilter(p.autotuneSpeed)},
        {"autotune:fastmode", p.autotuneSpeed >= 50 ? 1.0 : 0.0},
        {"effectsmix:Gain 1", p.effects ? 1.0 : 0.0},
        {"effectsmix:Gain 2", p.effects ? 0.0 : 1.0},
        {"bass:Gain", p.bass},
        {"mud:Gain", p.mud},
        {"presence:Gain", p.presence},
        {"air:Gain", p.air},
        {"compressor:Threshold level (dB)", c.threshold},
        {"compressor:Ratio (1:n)", c.ratio},
        {"compressor:Makeup gain (dB)", c.makeup},
        {"limiter:Input gain (dB)", p.output},
        {"output:Gain 1", bypass ? 0.0 : 1.0},
        {"output:Gain 2", bypass ? 1.0 : 0.0},
    };
}

static QString number(double value) {
    return QString::number(value, 'f', 4);
}

QString pod(const Params &params) {
    QStringList parts;
    for (const auto &[name, value] : params)
        parts << QString("\"%1\" %2").arg(name, number(value));
    return "{ params = [ " + parts.join(' ') + " ] }";
}

// PipeWire's SPA-JSON has no escapes we can rely on across versions, so a
// value that would break out of its quotes is dropped rather than escaped.
static QString quoted(const QString &value) {
    if (value.contains('"') || value.contains('\\') || value.contains('\n'))
        return "\"\"";
    return '"' + value + '"';
}

QString configuration(const Profile &profile, const QString &microphone) {
    // Controls that profiles don't change.
    Params fixed{
        {"rumble:Q", 0.707},
        {"bass:Freq", 120}, {"bass:Q", 0.707},
        {"mud:Freq", 300}, {"mud:Q", 1.0},
        {"presence:Freq", 4000}, {"presence:Q", 0.8},
        {"air:Freq", 10000}, {"air:Q", 0.707},
        {"denoise:VAD Grace Period (ms)", 200},
        {"denoise:Retroactive VAD Grace (ms)", 0},
        {"compressor:RMS/peak", 0.5},
        {"compressor:Attack time (ms)", 10},
        {"compressor:Release time (ms)", 150},
        {"compressor:Knee radius (dB)", 6},
        {"limiter:Limit (dB)", -1},
        {"limiter:Release time (s)", 0.08},
        // Auto mode, full correction: the plugin picks the nearest note in
        // the key itself, with no MIDI input.
        {"autotune:mode", 0},
        {"autotune:corr", 1.0},
        {"autotune:tuning", 440},
    };
    QMap<QString, QStringList> controls;
    for (const auto &[name, value] : fixed + params(profile)) {
        const QString node = name.section(':', 0, 0), port = name.section(':', 1);
        controls[node] << QString("\"%1\" = %2").arg(port, number(value));
    }
    auto control = [&](const QString &node) {
        return controls.contains(node) ? " control = { " + controls[node].join(' ') + " }" : QString();
    };

    QString target;
    if (!microphone.isEmpty())
        target = "                target.object = " + quoted(microphone) + "\n";

    return QString(R"conf(# Written by `vox engine` from ~/.config/vox/config.json; edits are overwritten.
context.properties = { log.level = 0 }
context.spa-libs = {
    audio.convert.* = audioconvert/libspa-audioconvert
    support.*       = support/libspa-support
}
context.modules = [
    { name = libpipewire-module-rt args = { } flags = [ ifexists nofail ] }
    { name = libpipewire-module-protocol-native }
    { name = libpipewire-module-client-node }
    { name = libpipewire-module-adapter }
    { name = libpipewire-module-filter-chain
        args = {
            node.description = "Vox Microphone"
            media.name = "Vox Microphone"
            audio.channels = 1
            audio.position = [ MONO ]
            filter.graph = {
                nodes = [
                    { type = builtin name = split label = copy }
                    { type = ladspa name = denoise plugin = librnnoise_ladspa label = noise_suppressor_mono%1 }
                    { type = builtin name = denoisemix label = mixer%2 }
                    { type = builtin name = rumble label = bq_highpass%3 }
                    { type = lv2 name = autotune plugin = "http://gareus.org/oss/lv2/fat1"%14 }
                    { type = builtin name = effectsmix label = mixer%15 }
                    { type = builtin name = bass label = bq_lowshelf%4 }
                    { type = builtin name = mud label = bq_peaking%5 }
                    { type = builtin name = presence label = bq_peaking%6 }
                    { type = builtin name = air label = bq_highshelf%7 }
                    { type = ladspa name = compressor plugin = sc4m_1916 label = sc4m%8 }
                    { type = ladspa name = limiter plugin = fast_lookahead_limiter_1913 label = fastLookaheadLimiter%9 }
                    { type = builtin name = output label = mixer%10 }
                ]
                links = [
                    { output = "split:Out" input = "denoise:Input" }
                    { output = "denoise:Output" input = "denoisemix:In 1" }
                    { output = "split:Out" input = "denoisemix:In 2" }
                    { output = "denoisemix:Out" input = "rumble:In" }
                    { output = "rumble:Out" input = "autotune:in" }
                    { output = "autotune:out" input = "effectsmix:In 1" }
                    { output = "rumble:Out" input = "effectsmix:In 2" }
                    { output = "effectsmix:Out" input = "bass:In" }
                    { output = "bass:Out" input = "mud:In" }
                    { output = "mud:Out" input = "presence:In" }
                    { output = "presence:Out" input = "air:In" }
                    { output = "air:Out" input = "compressor:Input" }
                    { output = "compressor:Output" input = "limiter:Input 1" }
                    { output = "limiter:Output 1" input = "output:In 1" }
                    { output = "split:Out" input = "output:In 2" }
                ]
                inputs = [ "split:In" ]
                outputs = [ "output:Out" ]
            }
            capture.props = {
                node.name = "%11"
                node.description = "Vox (input)"
                node.passive = true
                # Without this, an unplugged microphone would make WirePlumber
                # fall back to the default source, which may be Vox itself.
                node.dont-fallback = true
                # With dont-fallback alone, WirePlumber destroys a stream whose
                # target is gone, and the filter chain goes with it: a USB mic
                # that resets for a second took the Vox Microphone away for
                # good. Linger makes WirePlumber wait and relink it instead.
                node.linger = true
%12            }
            playback.props = {
                node.name = "%13"
                node.description = "Vox Microphone"
                media.class = Audio/Source
                device.icon-name = "audio-input-microphone"
            }
        }
    }
]
)conf")
        .arg(control("denoise"), control("denoisemix"), control("rumble"), control("bass"), control("mud"),
             control("presence"), control("air"), control("compressor"), control("limiter"))
        .arg(control("output"), CaptureName, target, SourceName)
        .arg(control("autotune"), control("effectsmix"));
}

const QList<Plugin> &plugins() {
    static const QList<Plugin> list{
        {"librnnoise_ladspa.so", "noise-suppression-for-voice", false},
        {"sc4m_1916.so", "swh-plugins", false},
        {"fast_lookahead_limiter_1913.so", "swh-plugins", false},
        {"fat1.lv2/manifest.ttl", "x42-plugins-lv2", true},
    };
    return list;
}

QStringList missingPackages(const QStringList &searchPath) {
    // The same places PipeWire's LADSPA loader and lilv look.
    QStringList ladspa = searchPath, lv2 = searchPath;
    if (searchPath.isEmpty()) {
        ladspa = qEnvironmentVariable("LADSPA_PATH").split(':', Qt::SkipEmptyParts);
        ladspa << "/usr/lib64/ladspa" << "/usr/lib/ladspa";
        lv2 = qEnvironmentVariable("LV2_PATH").split(':', Qt::SkipEmptyParts);
        lv2 << QDir::homePath() + "/.lv2" << "/usr/local/lib/lv2" << "/usr/lib/lv2";
    }
    QStringList missing;
    for (const auto &plugin : plugins()) {
        bool found = false;
        for (const auto &directory : plugin.lv2 ? lv2 : ladspa)
            found = found || QFileInfo::exists(directory + "/" + plugin.file);
        if (!found && !missing.contains(plugin.package))
            missing << plugin.package;
    }
    return missing;
}

}
