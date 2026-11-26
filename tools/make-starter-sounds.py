#!/usr/bin/env python3
# Synthesises the soundboard's starter sounds into src/sounds/ as FLAC: an air
# horn, applause, a rimshot and a sad trombone. Made from scratch, so they carry
# no licence beyond the project's. Needs ffmpeg for the room echo, loudness
# normalisation and encoding.
# Usage: tools/make-starter-sounds.py
import math
import os
import random
import struct
import subprocess
import tempfile
import wave

RATE = 48000
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "src", "sounds")


def silence(seconds):
    return [0.0] * int(seconds * RATE)


def mix_into(buffer, sound, at_seconds, gain=1.0):
    start = int(at_seconds * RATE)
    if len(buffer) < start + len(sound):
        buffer.extend([0.0] * (start + len(sound) - len(buffer)))
    for i, v in enumerate(sound):
        buffer[start + i] += v * gain


def one_pole_lowpass(samples, cutoff):
    a = math.exp(-2 * math.pi * cutoff / RATE)
    out, y = [], 0.0
    for v in samples:
        y = (1 - a) * v + a * y
        out.append(y)
    return out


def highpass(samples, cutoff):
    low = one_pole_lowpass(samples, cutoff)
    return [v - l for v, l in zip(samples, low)]


def saw(phase):
    return 2.0 * (phase - math.floor(phase + 0.5))


def air_horn():
    # Three short blasts, then a long one: the stadium pattern.
    out = []
    pattern = [(0.0, 0.16), (0.22, 0.16), (0.44, 0.16), (0.68, 1.1)]
    voices = [233.08, 233.08 * 1.006, 233.08 * 0.994, 466.16, 466.16 * 1.004, 349.23]
    for start, length in pattern:
        n = int(length * RATE)
        phases = [random.random() for _ in voices]
        blast = []
        for i in range(n):
            t = i / RATE
            # The pitch sags a little as the can opens, then holds.
            bend = 1.0 - 0.03 * math.exp(-t / 0.04)
            v = 0.0
            for k, f in enumerate(voices):
                phases[k] += f * bend / RATE
                v += saw(phases[k]) * (0.6 if f > 400 else 1.0)
            attack = min(1.0, t / 0.008)
            release = min(1.0, (length - t) / 0.03)
            blast.append(math.tanh(1.8 * v / len(voices)) * attack * release)
        mix_into(out, one_pole_lowpass(blast, 3500), start)
    return out


def clap(brightness):
    n = int(0.06 * RATE)
    noise = [random.uniform(-1, 1) for _ in range(n)]
    shaped = highpass(one_pole_lowpass(noise, brightness), 700)
    # A real clap is a few quick reflections between the hands.
    out = [0.0] * n
    for delay, gain in ((0.0, 1.0), (0.004, 0.6), (0.009, 0.35)):
        d = int(delay * RATE)
        for i in range(n - d):
            out[i + d] += shaped[i] * gain * math.exp(-i / (0.011 * RATE))
    return out


def applause():
    length = 3.4
    out = silence(length)
    for _ in range(32):
        rate = random.uniform(3.6, 5.6)
        brightness = random.uniform(1800, 4200)
        gain = random.uniform(0.35, 1.0)
        t = random.uniform(0.0, 0.35)
        while t < length - 0.1:
            # Swell in, then thin out as people stop.
            envelope = min(1.0, t / 0.45) * min(1.0, (length - t) / 1.3)
            if random.random() < 0.97:
                mix_into(out, clap(brightness), t, gain * envelope * random.uniform(0.7, 1.0))
            t += 1.0 / rate * random.uniform(0.85, 1.15)
    return out


def tom(start_freq, end_freq, length):
    n = int(length * RATE)
    phase, out = 0.0, []
    for i in range(n):
        t = i / RATE
        f = end_freq + (start_freq - end_freq) * math.exp(-t / 0.05)
        phase += f / RATE
        # Faded out at the end, or the cut-off is heard as a click.
        body = math.sin(2 * math.pi * phase) * math.exp(-t / (length * 0.35)) * min(1.0, (length - t) / 0.04)
        stick = random.uniform(-1, 1) * math.exp(-t / 0.004) * 0.5
        out.append(body + stick)
    return out


def cymbal(length):
    n = int(length * RATE)
    noise = [random.uniform(-1, 1) for _ in range(n)]
    bright = highpass(noise, 5500)
    return [v * math.exp(-i / (0.32 * RATE)) * 1.4 for i, v in enumerate(bright)]


def rimshot():
    out = []
    mix_into(out, tom(220, 150, 0.22), 0.0, 0.9)
    mix_into(out, tom(160, 105, 0.26), 0.17, 0.95)
    mix_into(out, tom(160, 105, 0.26), 0.43, 0.6)
    mix_into(out, cymbal(1.6), 0.43, 0.7)
    return out


def sad_trombone():
    out = []
    notes = [(196.00, 0.42), (185.00, 0.42), (174.61, 0.42), (164.81, 1.5)]
    t0 = 0.0
    for index, (freq, length) in enumerate(notes):
        n = int(length * RATE)
        phase, note = 0.0, []
        last = index == len(notes) - 1
        for i in range(n):
            t = i / RATE
            vibrato = 1 + (0.012 * math.sin(2 * math.pi * 5.2 * t) * min(1.0, t / 0.4) if last else 0)
            # The slide droops into each note a little flat, like a real one.
            droop = 1 - 0.02 * math.exp(-t / 0.06)
            phase += freq * vibrato * droop / RATE
            v = saw(phase) + 0.5 * saw(2 * phase + 0.1)
            attack = min(1.0, t / 0.04)
            release = min(1.0, (length - t) / (0.35 if last else 0.06))
            note.append(v * attack * release)
        # A wah: the bell opens on each note, then closes.
        out_note, y = [], 0.0
        for i, v in enumerate(note):
            t = i / RATE
            cutoff = 450 + 1700 * math.sin(math.pi * min(1.0, t / length)) ** (0.7 if last else 1.3)
            a = math.exp(-2 * math.pi * cutoff / RATE)
            y = (1 - a) * v + a * y
            out_note.append(y)
        mix_into(out, out_note, t0)
        t0 += length - 0.02
    return out


def write(name, samples, echo):
    peak = max(abs(v) for v in samples) or 1.0
    with tempfile.TemporaryDirectory() as tmp:
        path = os.path.join(tmp, name + ".wav")
        with wave.open(path, "wb") as w:
            w.setnchannels(1)
            w.setsampwidth(2)
            w.setframerate(RATE)
            w.writeframes(b"".join(struct.pack("<h", int(v / peak * 0.9 * 32767)) for v in samples))
        # loudnorm emits one huge block on short input, which the FLAC encoder
        # refuses, so the audio is cut back into ordinary blocks after it.
        filters = (["aecho=0.8:0.55:37|71:0.28|0.16"] if echo else []) + [
            "loudnorm=I=-16:TP=-1.5:LRA=11", f"aresample={RATE}", "asetnsamples=n=4096"]
        target = os.path.join(OUT, name + ".flac")
        subprocess.run(["ffmpeg", "-hide_banner", "-loglevel", "error", "-y", "-i", path,
                        "-af", ",".join(filters), "-ar", str(RATE), "-ac", "1", target], check=True)
        print(f"{target}: {len(samples) / RATE:.1f} s")


if __name__ == "__main__":
    random.seed(7)
    os.makedirs(OUT, exist_ok=True)
    write("air-horn", air_horn(), echo=False)
    write("applause", applause(), echo=True)
    write("rimshot", rimshot(), echo=True)
    write("sad-trombone", sad_trombone(), echo=False)
