---
name: vox
description: Control Vox, the Omarchy app that makes the user's microphone sound better in every app through a virtual "Vox Microphone". Use when asked to switch voice profiles, turn the improved microphone on or off, make it the default microphone, or check whether it's running.
---

# Vox

Vox runs the user's microphone through noise removal, EQ, a compressor and a
limiter, and offers the result to every app as the PipeWire source
`vox.microphone` ("Vox Microphone"). It runs as the systemd user service
`vox.service`. Profiles hold the settings; switching is live and never drops
the microphone out of a call.

Every command takes `--json`, exits 0 on success and 1 on failure, and writes
errors to stderr.

| Command | What it does |
| --- | --- |
| `vox status` | Running, enabled, default, active profile, microphone, missing packages |
| `vox list` | Profiles, numbered from 1, with their settings and which is active |
| `vox use <name or number>` | Switch profile live (and for the next start) |
| `vox on` / `vox off` | Start or stop the Vox Microphone, now and at login |
| `vox default` | Make the Vox Microphone the default microphone |
| `vox effects [on\|off]` | Turn the active profile's effects (auto-tune) on or off; without either, toggle them |
| `vox opendeck install` | Add the Vox plugin to OpenDeck for Stream Deck keys; OpenDeck must restart to load it |
| `vox sounds` | The soundboard's sounds, numbered from 1 |
| `vox play <name or number>` | Play a sound into the Vox Microphone, so the call hears it; waits until it ends |
| `vox open` | Open the window (needs a display); hands over to a running Vox |
| `vox tray` | Show the tray icon without the window (runs at login) |

Profiles are stored in `~/.config/vox/config.json`. Prefer `vox use` over
editing it, and never edit `~/.local/state/vox/pipewire.conf`, which the
service rewrites on every start.

If `vox status --json` lists `missingPackages`, install them with
`omarchy-pkg-add <packages>` before `vox on`. If the service failed, read
`journalctl --user -u vox`.
