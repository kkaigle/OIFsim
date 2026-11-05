#pragma once

#include <raylib.h>
#include <string>

namespace itc {

class AudioEngine {
public:
    bool initialized = false;
    bool muted = false;
    float masterVolume = 0.75f;

    Sound sfxClick{};
    Sound sfxRadio{};
    Sound sfxPlace{};
    Sound sfxExplosion{};
    Sound sfxBrrrt{};
    Sound sfxMissile{};

    void init(const std::string& assetsRoot);
    void shutdown();

    void playClick();
    void playRadio();
    void playPlace();
    void playExplosion();
    void playCasBurst();
    void playMissile();
};

} // namespace itc

