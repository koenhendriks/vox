#pragma once

#include <QList>
#include <QPair>
#include <QString>
#include <QStringList>

#include "config.h"

// The PipeWire filter graph behind the Vox microphone. Every setting a profile
// holds is a control on a node in one fixed graph, so switching profiles or
// dragging a slider changes values on the running graph instead of rebuilding
// it, and apps using the Vox microphone never see it disappear.
namespace Chain {

inline constexpr auto SourceName = "vox.microphone";
inline constexpr auto CaptureName = "vox.capture";
// The processed voice and soundboard sounds both play into this sink; a
// loopback offers its mix to apps as the Vox Microphone.
inline constexpr auto MixName = "vox.mix";
inline constexpr auto ProcessedName = "vox.processed";
inline constexpr auto ServiceName = "vox.service";

using Params = QList<QPair<QString, double>>;

struct Compressor {
    double threshold, ratio, makeup;
};
Compressor compressor(double amount);
double retuneFilter(double speed);
// Which of the 12 notes, from C, auto-tune snaps to for a profile's notes
// setting and root. Fewer notes make bigger jumps: on a speaking voice, every
// note is barely audible and one note is a full robot.
QList<bool> noteMask(int notes, int root);

// The controls a profile sets, as "node:Port" pairs. Bypass routes the raw
// microphone straight to the output so the user can compare.
Params params(const Profile &profile, bool bypass = false);

// The `{ params = [ ... ] }` pod pw-cli takes to change them live.
QString pod(const Params &params);

// A whole `pipewire -c` configuration running the graph from `microphone`.
QString configuration(const Profile &profile, const QString &microphone);

struct Plugin {
    QString file, package;
    bool lv2;
};
const QList<Plugin> &plugins();
// Packages whose LADSPA or LV2 plugins can't be found where PipeWire looks.
QStringList missingPackages(const QStringList &searchPath = {});

}
