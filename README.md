# Vox

[![CI](https://github.com/koenhendriks/vox/actions/workflows/ci.yml/badge.svg)](https://github.com/koenhendriks/vox/actions/workflows/ci.yml)
[![AUR](https://img.shields.io/aur/version/vox?label=AUR)](https://aur.archlinux.org/packages/vox)

![Vox with a test recording of the Broadcast profile](docs/screenshot.png)

Vox makes your microphone sound better in every app. It runs your voice
through noise removal, a tone EQ, a compressor and a limiter, and offers the
result to the whole desktop as the **Vox Microphone**. Pick it in Teams, Meet,
Discord, OBS or your browser, or make it the default microphone so every app
uses it without asking.

Profiles hold the settings. Switching profiles happens live, so the Vox
Microphone never drops out of a call.

For when a call needs it: **Effects** adds an over-the-top auto-tune that
snaps your voice to the notes of a key, and the **Soundboard** plays an air
horn, applause or any sound you add straight into the call.

Vox is built for [Omarchy](https://omarchy.org): it follows the Omarchy theme
and language live, sits in the Omarchy bar's tray, and installs as an Arch
package. It should work on any Arch Linux desktop with PipeWire and
WirePlumber.

## Install

**Arch Linux and Omarchy:** install [`vox`](https://aur.archlinux.org/packages/vox)
from the AUR with any AUR helper:

```sh
yay -S vox
```

**From a clone**, to run what's on `main` or your own changes:

```sh
git clone https://github.com/koenhendriks/vox.git
cd vox
bin/install
```

`bin/install` builds the working tree into the `vox` package with `makepkg`,
pulling in what it needs from the Arch repositories, including `swh-plugins`
(compressor and limiter), `noise-suppression-for-voice` (RNNoise) and
`x42-plugins-lv2` (auto-tune).

Then open **Vox** from the app launcher, press **Turn on**, and either pick
"Vox Microphone" in your apps or press **Make default**.

## Tray

Vox sits in the system tray, starting at login. The icon is a solid
microphone in the theme's accent while the Vox Microphone is on, grey and
crossed out while it's off, and hollow while comparing with the raw
microphone; hover it to see the profile and whether Vox is the default
microphone. Its menu turns Vox on or off, switches profiles in one click,
turns effects on or off, and makes Vox the default microphone. Click the icon
to open or hide the window, middle-click it to turn Vox on or off. Closing the window, or Q, leaves Vox in
the tray.

Omarchy's bar puts new tray icons behind its arrow; right-click the arrow and
pin Vox to keep it in sight. To stop the tray starting at login, copy
`/etc/xdg/autostart/vox-tray.desktop` to `~/.config/autostart/` and add
`Hidden=true`.

## Effects

Switch **Effects** on, in its card, with E, or from the tray menu, and Vox runs
your voice through an auto-tune that snaps every note to one of a few notes.
The fewer notes, the bigger the jumps, so pick how wild it gets:

| Notes | What it sounds like |
| --- | --- |
| Robot: one note | Every syllable on the same note, a full robot |
| Major or minor chord | Your voice jumps between three notes, as if it sings (the default) |
| Major or minor key | Seven notes; melodic, less extreme |
| Every note | Barely audible on a speaking voice; for singing |

**Root note** sets the note the chord, key or robot is built on. At a retune
speed of 100% it jumps between notes at once; lower speeds glide into each
note. Effects belong to the profile, so a profile for fun and one for
meetings each remember theirs, and switching never drops the Vox Microphone
out of a call.

## Soundboard

The soundboard sits under the profiles. Click a sound to play it into the Vox
Microphone, so everyone in the call hears it; click it again, or press S, to
stop. Ctrl+1 to 9 plays the first nine, and the tray menu has them too, for
when the window is closed. It starts with an air horn, applause, a rimshot and
a sad trombone, synthesised for Vox.

Drop audio files onto the soundboard, or press **+**, to add your own: any
format ffmpeg reads, even the sound track of a video. Vox converts a copy into
`~/.local/share/vox/sounds` at the same loudness as the others and never
touches your file. To swap a sound's audio but keep its name, place and
Ctrl+number, drop a file onto that sound, or right-click it and pick
**Replace…**. Right-click also renames or removes a sound; Ctrl+Z undoes any
of these. **⋯** sets the volume and whether you hear the sounds
yourself.

Sounds join your voice after noise removal and auto-tune, which would
otherwise mangle them. To get there they play into an output called **Vox
Soundboard**, which shows up in your sound settings; anything you play into
it is heard in the call.

## Test recording

Press **Record a test** (or T), say a few sentences, and press it again. Vox
records your raw microphone and the Vox Microphone at the same moment, so both
takes hold exactly the same words, and lines them up to take out Vox's own
processing delay. The player draws both as level curves with one playhead:
click a curve to hear that version from that point, or press Tab while it
plays to switch between raw and Vox without losing your place. Recordings
stay in memory and stop after 30 seconds.

## How it works

PipeWire runs a filter chain as the systemd user service `vox.service`:

```text
your mic → RNNoise → rumble filter → [auto-tune] → bass → mud → presence → air → compressor → limiter ┐
                                                                                 soundboard sounds ┴→ Vox Soundboard → Vox Microphone
```

The service starts with PipeWire at login and keeps running when the window
is closed. Every setting is a control on the running graph, so the window and
`vox use` change it in place with `pw-cli`, without restarting anything.

| Control | What it does |
| --- | --- |
| Noise removal | RNNoise removes fans, typing and room noise |
| Voice gate | Mutes the gaps between words (RNNoise voice detection) |
| Rumble filter | High-pass filter against desk, traffic and air-conditioning rumble |
| Bass | Low shelf at 120 Hz for a deeper, warmer voice |
| Mud | Bell at 300 Hz; cut it to sound less boxy |
| Presence | Bell at 4 kHz; brings words forward |
| Air | High shelf at 10 kHz |
| Compression | One knob for threshold, ratio and make-up gain |
| Output | Gain into a limiter that stops at -1 dBFS |
| Effects | Switches the auto-tune in or out of the chain |
| Notes | Which notes auto-tune snaps to: one, a chord, a key, or every note |
| Root note | The note the chord, key or robot is built on |
| Retune speed | How fast it snaps; 100% jumps at once, for the robotic sound |

The presets are Natural, Broadcast, Podcast and Meetings. Edit them, or press
N to make a new profile from the current one.

## Keyboard

| Key | Action |
| --- | --- |
| Space | Compare with your raw microphone |
| L | Listen to yourself (use headphones) |
| E | Turn effects on or off |
| Ctrl+1 to 9 | Play a sound |
| S | Stop all sounds |
| T | Record a test, or stop recording |
| P | Play or pause the test |
| Tab | Switch between raw and Vox in the test |
| Esc | Close the test |
| O | Turn the Vox Microphone on or off |
| D | Make Vox the default microphone |
| 1 to 9 | Switch profile |
| Up / Down | Previous or next profile |
| N | New profile from this one |
| R or F2 | Rename profile |
| Delete | Delete profile |
| Ctrl+Z / Ctrl+Shift+Z | Undo / redo |
| ? | Show every shortcut |
| Q | Close the window (Vox stays in the tray) |

## Command line

Every command takes `--json`, exits 0 or 1, and writes errors to stderr.

```sh
vox tray              # show Vox in the tray without a window
vox status            # running, enabled, default, profile, microphone
vox list              # the profiles, numbered
vox use broadcast     # switch profile live, by name or number
vox on                # start the Vox Microphone, now and at login
vox off
vox default           # make it the default microphone
vox sounds            # the soundboard's sounds, numbered
vox play "air horn"   # play a sound into the call, by name or number
vox skill install     # install a skill so coding agents can drive Vox
```

`vox use` and `vox play` make handy key bindings. On Omarchy, add them to
`~/.config/hypr/bindings.lua`:

```lua
o.bind("SUPER + ALT + V", "Vox broadcast", "vox use broadcast")
o.bind("SUPER + ALT + H", "Air horn", "vox play 'air horn'")
```

## Files

| Path | What's there |
| --- | --- |
| `~/.config/vox/config.json` | Your profiles and microphone |
| `~/.local/state/vox/pipewire.conf` | The filter graph, rewritten on every start |
| `~/.local/share/vox/sounds` | The soundboard's sounds, converted copies |
| `/usr/lib/systemd/user/vox.service` | The service that runs the Vox Microphone |
| `/etc/xdg/autostart/vox-tray.desktop` | Starts the tray at login |

## Troubleshooting

- **Vox says it couldn't start.** `journalctl --user -u vox` shows why.
  `vox status` lists missing packages, if any.
- **An app doesn't list the Vox Microphone.** Check that Vox is on, then
  restart the app; some apps only read the device list when they start.
- **You hear an echo with Listen.** Listening plays your voice back live;
  use headphones.
- **The tray icon is missing.** In Omarchy, hover the arrow in the bar; new
  icons start behind it.

## Uninstall

```sh
vox off                                    # stop the Vox Microphone and remove it from login
sudo pacman -R vox
rm -rf ~/.config/vox ~/.local/state/vox ~/.local/share/vox   # profiles, graph and sounds
```

## Privacy

Vox works entirely on your machine and never connects to the network. Test
recordings live in memory and are gone when you close the test or quit Vox.

## Development

Vox is C++ and Qt Quick, built with qmake6 and make, following the way
Omarchy's own apps are made.

```sh
bin/build      # build/vox
bin/test       # Qt Test, offscreen
bin/install    # build and install the package, so Vox is in the app launcher
```

The logic lives in C++ under `src/` (`chain.cpp` writes the PipeWire graph,
`backend.cpp` drives it), the window in `src/*.qml`, and translations in
`src/i18n/` as JSON keyed by the English text. The tests run against a fake
audio system, so they need no microphone or PipeWire.

Bug reports and pull requests are welcome.

## Releasing

1. Bump `VERSION` in `vox.pro` and commit. The PKGBUILD's `pkgver` follows by
   itself: `pkgver()` reads the tags, and the release pins it in the AUR copy.
2. Tag and push: `git tag -m "Vox 0.4.0" v0.4.0 && git push origin main v0.4.0`.

`release.yml` checks that the tag matches `vox.pro`, runs CI, and builds the
source tarball. It then builds the AUR package from that tarball exactly as an
AUR user would (tests included), and publishes a GitHub release with the
tarball, its SHA-256 checksum and notes listing the commits since the previous
tag (`tools/release-notes.sh`). Last, it pushes the PKGBUILD generated by
`tools/aur-pkgbuild.sh` and its `.SRCINFO` to the AUR, using the
`AUR_SSH_PRIVATE_KEY` repository secret.

## Credits

- [RNNoise](https://github.com/werman/noise-suppression-for-voice) for noise
  removal, as a LADSPA plugin by Werman.
- [SWH plugins](https://github.com/swh/ladspa) by Steve Harris for the
  compressor and limiter.
- [x42-autotune](https://x42-plugins.com/x42/x42-autotune) by Robin Gareus for
  the auto-tune effect.
- [PipeWire](https://pipewire.org) for the filter chain.
- [FFmpeg](https://ffmpeg.org) for converting sounds; the starter sounds are
  made by `tools/make-starter-sounds.py`.
- The theme, translator and build scaffolding follow Omacut and Monologue by
  David Heinemeier Hansson.

## License

[MIT](LICENSE)
