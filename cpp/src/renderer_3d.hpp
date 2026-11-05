#pragma once

#include "asset_loader.hpp"
#include "sim_types.hpp"

namespace itc {

class Renderer3D {
public:
    // Compound size in 3D world units when viewing/building a FOB
    static constexpr float BASE_WORLD_SPAN = 6.4f;

    Camera3D buildCamera(const SimState& sim, const TheaterAssets& assets) const;

    // Raycast mouse cursor onto the active base's 16x16 3D grid and update sim.hoverGx / hoverGz
    bool updateBaseGridHover(SimState& sim, const TheaterAssets& assets, Vector2 mousePos) const;

    void render(const SimState& sim, const TheaterAssets& assets, int screenW, int screenH);

private:
    void drawModelScaled(const LoadedModelEntry& entry, Vector3 worldPos, Vector3 targetSize, float yawDeg, Color tint) const;
    void drawStructure3D(const BaseStructDef& def, size_t catalogIdx, Vector3 cellCenter, float cellW, float cellD,
                         int rot, Color tintOverride, bool wireframeOnly, const TheaterAssets& assets) const;
    void drawBaseCompound3D(const Base& b, bool isSelected, const SimState& sim, const TheaterAssets& assets) const;
    void drawCity3D(const City& c, const SimState& sim, const TheaterAssets& assets) const;
    void drawUnit3D(const Unit& u, bool isSelected, const TheaterAssets& assets) const;
};

} // namespace itc

