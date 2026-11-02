// ============================================================
//  Audio.cpp  -  用代码合成短促的 8-bit 音效
// ============================================================
#include "Audio.h"

#include <mmsystem.h>

#include <algorithm>
#include <cmath>
#include <cstring>

#pragma comment(lib, "winmm.lib")

namespace jubi {

namespace {

constexpr int kRate = 22050;

void put16(std::vector<BYTE>& v, uint16_t x) {
    v.push_back(static_cast<BYTE>(x & 0xFF));
    v.push_back(static_cast<BYTE>((x >> 8) & 0xFF));
}
void put32(std::vector<BYTE>& v, uint32_t x) {
    v.push_back(static_cast<BYTE>(x & 0xFF));
    v.push_back(static_cast<BYTE>((x >> 8) & 0xFF));
    v.push_back(static_cast<BYTE>((x >> 16) & 0xFF));
    v.push_back(static_cast<BYTE>((x >> 24) & 0xFF));
}

std::vector<BYTE> makeWav(const std::vector<float>& samples) {
    std::vector<BYTE> wav;
    const uint32_t dataSize = static_cast<uint32_t>(samples.size() * 2);
    wav.reserve(44 + dataSize);
    wav.push_back('R'); wav.push_back('I'); wav.push_back('F'); wav.push_back('F');
    put32(wav, 36 + dataSize);
    wav.push_back('W'); wav.push_back('A'); wav.push_back('V'); wav.push_back('E');
    wav.push_back('f'); wav.push_back('m'); wav.push_back('t'); wav.push_back(' ');
    put32(wav, 16);
    put16(wav, 1);
    put16(wav, 1);
    put32(wav, kRate);
    put32(wav, kRate * 2);
    put16(wav, 2);
    put16(wav, 16);
    wav.push_back('d'); wav.push_back('a'); wav.push_back('t'); wav.push_back('a');
    put32(wav, dataSize);
    for (float s : samples) {
        int v = static_cast<int>(std::lround(clampf(s, -1.0f, 1.0f) * 32767.0f));
        put16(wav, static_cast<uint16_t>(static_cast<int16_t>(v)));
    }
    return wav;
}

struct Synth {
    std::vector<float> buf;
    float length = 0.0f;

    explicit Synth(float seconds) : length(seconds) {
        buf.assign(static_cast<size_t>(seconds * kRate) + 2, 0.0f);
    }

    void tone(float start, float dur, float f0, float f1, float amp,
              int shape = 0, float attack = 0.004f, float decayPow = 1.8f) {
        int i0 = static_cast<int>(start * kRate);
        int n = static_cast<int>(dur * kRate);
        if (i0 < 0 || n <= 0) return;
        float phase = 0.0f;
        for (int i = 0; i < n; ++i) {
            int idx = i0 + i;
            if (idx < 0 || idx >= static_cast<int>(buf.size())) break;
            float t = static_cast<float>(i) / n;
            float f = f0 * std::pow(f1 / f0, t);
            phase += 2.0f * kPi * f / kRate;
            if (phase > 2.0f * kPi) phase -= 2.0f * kPi;
            float v = 0.0f;
            if (shape == 0) {
                v = std::sin(phase) >= 0.0f ? 1.0f : -1.0f; // square
            } else if (shape == 1) {
                v = 2.0f * std::fabs(2.0f * (phase / (2.0f * kPi)) - 1.0f) - 1.0f; // triangle
            } else {
                v = std::sin(phase); // sine
            }
            float env = std::min(1.0f, static_cast<float>(i) / (attack * kRate + 1.0f));
            env *= std::pow(1.0f - t, decayPow);
            buf[idx] += v * amp * env;
        }
    }

    void noise(float start, float dur, float amp, float decayPow = 2.0f) {
        int i0 = static_cast<int>(start * kRate);
        int n = static_cast<int>(dur * kRate);
        uint32_t seed = 0x1234567u;
        for (int i = 0; i < n; ++i) {
            int idx = i0 + i;
            if (idx < 0 || idx >= static_cast<int>(buf.size())) break;
            seed = seed * 1664525u + 1013904223u;
            float r = (static_cast<float>((seed >> 8) & 0xFFFF) / 32768.0f) - 1.0f;
            float t = static_cast<float>(i) / n;
            buf[idx] += r * amp * std::pow(1.0f - t, decayPow);
        }
    }

    void silence(float start, float dur) {
        int i0 = static_cast<int>(start * kRate);
        int n = static_cast<int>(dur * kRate);
        for (int i = 0; i < n; ++i) {
            int idx = i0 + i;
            if (idx >= 0 && idx < static_cast<int>(buf.size())) buf[idx] = 0.0f;
        }
    }
};

} // namespace

Audio::~Audio() { shutdown(); }

void Audio::build(Sfx sfx, const std::vector<BYTE>& wav) {
    sounds_[sfx] = wav;
}

void Audio::init() {
    if (initialized_) return;
    initialized_ = true;
    generateAll();
}

void Audio::shutdown() {
    if (!initialized_) return;
    PlaySoundW(nullptr, nullptr, 0);
    sounds_.clear();
    initialized_ = false;
}

void Audio::setMuted(bool muted) {
    muted_ = muted;
    if (muted_) PlaySoundW(nullptr, nullptr, 0);
}

void Audio::play(Sfx sfx) {
    if (!initialized_ || muted_) return;
    auto it = sounds_.find(sfx);
    if (it == sounds_.end() || it->second.empty()) return;
    PlaySoundW(reinterpret_cast<LPCWSTR>(it->second.data()), nullptr,
               SND_MEMORY | SND_ASYNC | SND_NODEFAULT);
}

void Audio::generateAll() {
    {
        Synth s(0.11f);
        s.tone(0.0f, 0.09f, 380.0f, 760.0f, 0.34f, 0, 0.004f, 1.4f);
        s.tone(0.02f, 0.08f, 760.0f, 900.0f, 0.16f, 2, 0.002f, 2.0f);
        build(Sfx::Jump, makeWav(s.buf));
    }
    {
        Synth s(0.16f);
        s.tone(0.0f, 0.13f, 600.0f, 1180.0f, 0.32f, 0, 0.003f, 1.5f);
        s.tone(0.04f, 0.11f, 900.0f, 1500.0f, 0.14f, 2, 0.002f, 2.0f);
        build(Sfx::DoubleJump, makeWav(s.buf));
    }
    {
        Synth s(0.09f);
        s.noise(0.0f, 0.055f, 0.22f, 2.4f);
        s.tone(0.0f, 0.07f, 150.0f, 70.0f, 0.22f, 2, 0.001f, 1.4f);
        build(Sfx::Land, makeWav(s.buf));
    }
    {
        Synth s(0.62f);
        s.tone(0.0f, 0.16f, 620.0f, 500.0f, 0.30f, 0, 0.002f, 1.0f);
        s.tone(0.13f, 0.20f, 460.0f, 330.0f, 0.28f, 0, 0.002f, 1.2f);
        s.tone(0.30f, 0.28f, 300.0f, 90.0f, 0.26f, 1, 0.002f, 1.6f);
        s.noise(0.28f, 0.30f, 0.10f, 2.0f);
        build(Sfx::Death, makeWav(s.buf));
    }
    {
        Synth s(0.36f);
        s.tone(0.0f, 0.12f, 880.0f, 880.0f, 0.26f, 1, 0.002f, 1.2f);
        s.tone(0.11f, 0.16f, 1320.0f, 1320.0f, 0.26f, 1, 0.002f, 1.4f);
        s.tone(0.22f, 0.13f, 1760.0f, 1760.0f, 0.20f, 2, 0.002f, 2.0f);
        build(Sfx::Checkpoint, makeWav(s.buf));
    }
    {
        Synth s(0.40f);
        s.tone(0.00f, 0.11f, 1046.0f, 1046.0f, 0.24f, 1, 0.002f, 1.4f);
        s.tone(0.08f, 0.11f, 1318.0f, 1318.0f, 0.24f, 1, 0.002f, 1.4f);
        s.tone(0.16f, 0.11f, 1568.0f, 1568.0f, 0.24f, 1, 0.002f, 1.4f);
        s.tone(0.24f, 0.15f, 2093.0f, 2093.0f, 0.20f, 2, 0.002f, 2.0f);
        build(Sfx::Feather, makeWav(s.buf));
    }
    {
        Synth s(0.26f);
        s.tone(0.0f, 0.10f, 220.0f, 720.0f, 0.30f, 1, 0.002f, 1.0f);
        s.tone(0.08f, 0.14f, 720.0f, 1200.0f, 0.22f, 0, 0.002f, 1.6f);
        build(Sfx::Bounce, makeWav(s.buf));
    }
    {
        Synth s(0.30f);
        s.noise(0.0f, 0.09f, 0.24f, 2.0f);
        s.noise(0.08f, 0.09f, 0.20f, 2.0f);
        s.noise(0.17f, 0.12f, 0.16f, 2.2f);
        s.tone(0.0f, 0.22f, 110.0f, 60.0f, 0.18f, 2, 0.001f, 1.6f);
        build(Sfx::Crumble, makeWav(s.buf));
    }
    {
        Synth s(0.14f);
        s.tone(0.0f, 0.11f, 1800.0f, 500.0f, 0.26f, 0, 0.001f, 1.2f);
        build(Sfx::Shoot, makeWav(s.buf));
    }
    {
        Synth s(1.30f);
        s.tone(0.00f, 0.15f, 523.0f, 523.0f, 0.26f, 1, 0.002f, 1.0f);
        s.tone(0.14f, 0.15f, 659.0f, 659.0f, 0.26f, 1, 0.002f, 1.0f);
        s.tone(0.28f, 0.15f, 784.0f, 784.0f, 0.26f, 1, 0.002f, 1.0f);
        s.tone(0.42f, 0.15f, 1046.0f, 1046.0f, 0.26f, 1, 0.002f, 1.0f);
        s.tone(0.58f, 0.28f, 784.0f, 784.0f, 0.24f, 0, 0.002f, 1.2f);
        s.tone(0.78f, 0.42f, 1046.0f, 1046.0f, 0.24f, 1, 0.002f, 1.4f);
        s.tone(0.86f, 0.40f, 1318.0f, 1318.0f, 0.18f, 2, 0.002f, 1.6f);
        build(Sfx::Win, makeWav(s.buf));
    }
    {
        Synth s(0.06f);
        s.tone(0.0f, 0.045f, 1200.0f, 1200.0f, 0.20f, 0, 0.001f, 2.0f);
        build(Sfx::MenuMove, makeWav(s.buf));
    }
    {
        Synth s(0.16f);
        s.tone(0.0f, 0.06f, 880.0f, 880.0f, 0.24f, 1, 0.001f, 1.4f);
        s.tone(0.055f, 0.09f, 1320.0f, 1320.0f, 0.22f, 1, 0.001f, 1.8f);
        build(Sfx::MenuSelect, makeWav(s.buf));
    }
    {
        Synth s(0.16f);
        s.tone(0.0f, 0.13f, 180.0f, 120.0f, 0.24f, 1, 0.002f, 1.0f);
        s.noise(0.0f, 0.05f, 0.08f, 2.0f);
        build(Sfx::Deny, makeWav(s.buf));
    }
    {
        Synth s(0.38f);
        s.noise(0.0f, 0.32f, 0.12f, 1.4f);
        s.tone(0.0f, 0.30f, 300.0f, 900.0f, 0.16f, 2, 0.01f, 1.2f);
        build(Sfx::Door, makeWav(s.buf));
    }
}

} // namespace jubi
