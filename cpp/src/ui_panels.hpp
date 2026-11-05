#pragma once

#include "asset_loader.hpp"
#include "audio_engine.hpp"
#include "sim_types.hpp"

namespace itc {

class UIPanels {
public:
    // Returns true if the mouse cursor is currently over any UI panel/button/modal
    bool isMouseOverUI(Vector2 mousePos, const SimState& sim, int screenW, int screenH) const;

    // Render all HUD bars, Left Rail modals, View Switcher, and Bottom Dock; handles button clicks
    void renderAndInteract(SimState& sim, TheaterAssets& assets, AudioEngine& audio, int screenW, int screenH);

    // Helper to advance campaign phase and apply strategic effects
    static void advanceCampaignPhase(SimState& sim, TheaterAssets& assets, AudioEngine& audio, CampaignPhase targetPhase);

private:
    bool drawTacButton(Rectangle rect, const char* label, bool active, Color accent = {120, 225, 145, 255}) const;
    void renderTopBar(SimState& sim, TheaterAssets& assets, AudioEngine& audio, int screenW);
    void renderLeftRail(SimState& sim, TheaterAssets& assets, AudioEngine& audio);
    void renderViewSwitcher(SimState& sim, AudioEngine& audio, int screenW);
    void renderLeftModal(SimState& sim, TheaterAssets& assets, AudioEngine& audio, int screenW, int screenH);
    void renderBottomDock(SimState& sim, TheaterAssets& assets, AudioEngine& audio, int screenW, int screenH);
};

} // namespace itc
