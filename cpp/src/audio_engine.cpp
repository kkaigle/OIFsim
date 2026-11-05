#include "audio_engine.hpp"
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <vector>

namespace itc {

namespace {

Sound makeSynthSound(float durationSec, int sampleRate, auto&& sampleFn) {
    int frames = static_cast<int>(durationSec * sampleRate);
    std::vector<int16_t> pcm(frames);
    for (int i = 0; i < frames; ++i) {
        float t = static_cast<float>(i) / sampleRate;
        float v = sampleFn(t, durationSec, i);
        if (v > 1.0f) v = 1.0f;
        if (v < -1.0f) v = -1.0f;
        pcm[i] = static_cast<int16_t>(v * 32000.0f);
    }
    Wave w{};
    w.frameCount = static_cast<unsigned int>(frames);
    w.sampleRate = static_cast<unsigned int>(sampleRate);
    w.sampleSize = 16;
    w.channels = 1;
    w.data = pcm.data();
    return LoadSoundFromWave(w);
}

float hashNoise(int i) {
    uint32_t n = static_cast<uint32_t>(i);
    n = (n << 13U) ^ n;
    n = n * (n * n * 15731U + 789221U) + 1376312589U;
    return 1.0f - static_cast<float>(n & 0x7fffffffU) / 1073741824.0f;
}

} // namespace

void AudioEngine::init(const std::string& assetsRoot) {
    InitAudioDevice();
    if (!IsAudioDeviceReady()) return;
    initialized = true;

    const int sr = 44100;
    // Tactical UI click
    sfxClick = makeSynthSound(0.035f, sr, [](float t, float dur, int i) {
        float env = std::exp(-t * 95.0f);
        return (std::sin(2.0f * 3.1415926f * 1150.0f * t) * 0.6f + hashNoise(i) * 0.25f) * env * (1.0f - t / dur);
    });

    // SINCGARS radio squelch + beep
    sfxRadio = makeSynthSound(0.16f, sr, [](float t, float, int i) {
        if (t < 0.055f) {
            return std::sin(2.0f * 3.1415926f * 1850.0f * t) * 0.35f;
        }
        float env = std::exp(-(t - 0.055f) * 28.0f);
        return hashNoise(i) * 0.28f * env;
    });

    // Base grid structure placement thud
    sfxPlace = makeSynthSound(0.12f, sr, [](float t, float, int i) {
        float freq = 240.0f - t * 1100.0f;
        if (freq < 60.0f) freq = 60.0f;
        float env = std::exp(-t * 26.0f);
        return (std::sin(2.0f * 3.1415926f * freq * t) * 0.7f + hashNoise(i) * 0.2f) * env;
    });

    // JDAM / Artillery low-frequency impact boom
    sfxExplosion = makeSynthSound(0.65f, sr, [](float t, float, int i) {
        float env = std::exp(-t * 5.2f);
        float rumble = std::sin(2.0f * 3.1415926f * (72.0f * std::exp(-t * 3.0f)) * t);
        return (rumble * 0.65f + hashNoise(i) * 0.55f) * env;
    });

    // A-10C 30mm GAU-8 Avenger "BRRRT"
    sfxBrrrt = makeSynthSound(0.45f, sr, [](float t, float, int i) {
        float pulse = (std::sin(2.0f * 3.1415926f * 68.0f * t) > 0.0f) ? 1.0f : 0.25f;
        float saw = std::sin(2.0f * 3.1415926f * 136.0f * t) * 0.55f;
        float env = (t < 0.38f) ? 0.8f : std::exp(-(t - 0.38f) * 35.0f);
        return (saw + hashNoise(i) * 0.45f) * pulse * env;
    });

    // Tomahawk / MLRS launch whoosh
    sfxMissile = makeSynthSound(0.38f, sr, [](float t, float, int i) {
        float env = std::sin(3.1415926f * (t / 0.38f));
        return hashNoise(i) * 0.45f * env;
    });

    // Optional file overrides in cpp/assets/audio/
    auto tryOverride = [&](Sound& target, const char* filename) {
        std::string p = assetsRoot + "/audio/" + filename;
        if (std::filesystem::exists(p)) {
            UnloadSound(target);
            target = LoadSound(p.c_str());
        }
    };
    tryOverride(sfxClick, "click.wav");
    tryOverride(sfxRadio, "radio.wav");
    tryOverride(sfxPlace, "place.wav");
    tryOverride(sfxExplosion, "explosion.wav");
    tryOverride(sfxBrrrt, "brrrt.wav");
    tryOverride(sfxMissile, "missile.wav");
}

void AudioEngine::shutdown() {
    if (!initialized) return;
    UnloadSound(sfxClick);
    UnloadSound(sfxRadio);
    UnloadSound(sfxPlace);
    UnloadSound(sfxExplosion);
    UnloadSound(sfxBrrrt);
    UnloadSound(sfxMissile);
    CloseAudioDevice();
    initialized = false;
}

void AudioEngine::playClick()     { if (initialized && !muted) { SetSoundVolume(sfxClick, masterVolume * 0.6f); PlaySound(sfxClick); } }
void AudioEngine::playRadio()     { if (initialized && !muted) { SetSoundVolume(sfxRadio, masterVolume * 0.7f); PlaySound(sfxRadio); } }
void AudioEngine::playPlace()     { if (initialized && !muted) { SetSoundVolume(sfxPlace, masterVolume * 0.8f); PlaySound(sfxPlace); } }
void AudioEngine::playExplosion() { if (initialized && !muted) { SetSoundVolume(sfxExplosion, masterVolume * 0.9f); PlaySound(sfxExplosion); } }
void AudioEngine::playCasBurst()  { if (initialized && !muted) { SetSoundVolume(sfxBrrrt, masterVolume * 0.85f); PlaySound(sfxBrrrt); } }
void AudioEngine::playMissile()   { if (initialized && !muted) { SetSoundVolume(sfxMissile, masterVolume * 0.75f); PlaySound(sfxMissile); } }

} // namespace itc
