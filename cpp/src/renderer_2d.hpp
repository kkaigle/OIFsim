#pragma once

#include "asset_loader.hpp"
#include "sim_types.hpp"

namespace itc {

class Renderer2D {
public:
    Vector2 worldToScreen(float wx, float wy, const SimState& sim, int screenW, int screenH) const;
    Vector2 screenToWorld(float sx, float sy, const SimState& sim, int screenW, int screenH) const;

    void render(const SimState& sim, const TheaterAssets& assets, int screenW, int screenH);

    // Hit-testing helpers for mouse click selection on 2D map
    int pickCity(float sx, float sy, const SimState& sim, const TheaterAssets& assets, int screenW, int screenH) const;
    int pickUnit(float sx, float sy, const SimState& sim, int screenW, int screenH) const;
    int pickBase(float sx, float sy, const SimState& sim, int screenW, int screenH) const;

private:
    void drawCityStreetsAndBuildings(const City& c, const SimState& sim, const TheaterAssets& assets,
                                     float minWx, float maxWx, float minWy, float maxWy,
                                     int screenW, int screenH);
    void drawBaseCompound2D(const Base& b, bool selected, const SimState& sim, int screenW, int screenH);
    void drawUnitCounter(const Unit& u, bool selected, const SimState& sim, int screenW, int screenH);
    void drawDroneOverlay(const SimState& sim, const TheaterAssets& assets, int screenW, int screenH);
    void drawLaseOverlay(const SimState& sim, const TheaterAssets& assets, int screenW, int screenH);
};

} // namespace itc
