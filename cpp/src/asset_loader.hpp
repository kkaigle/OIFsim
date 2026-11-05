#pragma once

#include "sim_types.hpp"
#include <array>
#include <string>
#include <unordered_map>
#include <vector>

namespace itc {

struct LoadedModelEntry {
    Model model{};
    BoundingBox bounds{};
    Vector3 size{1.0f, 1.0f, 1.0f};
    Vector3 center{0.0f, 0.0f, 0.0f};
    bool valid = false;
};

class TheaterAssets {
public:
    GeoBounds geo{};
    std::string assetsRoot;

    // Geographic polygons & lines
    std::vector<Vector2> iraqBorder;
    std::vector<Vector2> kuwaitBorder;
    std::vector<Vector2> gulfPoly;
    std::vector<City> cities;
    std::vector<Highway> highways;
    std::vector<std::vector<Vector2>> rivers;
    std::vector<Lake> lakes;

    // 251,503 OSM Buildings & 327,983 Street Segments
    std::vector<OsmBuilding> buildings;
    std::vector<StreetSeg> streets;

    // Elevation heightmap (435 x 306 int16 meters)
    std::vector<int16_t> elevation;

    // Map Textures (Satellite + Hypsometric Military Relief with hillshade)
    Texture2D satTexture{};
    Texture2D milTexture{};
    bool satLoaded = false;
    bool milLoaded = false;

    // 3D GLB Models (Vehicles, Urban Buildings, and Custom Base Structure Overrides)
    LoadedModelEntry mdlTank{};
    LoadedModelEntry mdlIfv{};
    LoadedModelEntry mdlHmmwv{};
    LoadedModelEntry mdlBigRes{};
    LoadedModelEntry mdlBigResL{};
    LoadedModelEntry mdlRuin{};
    LoadedModelEntry mdlHouse{};
    std::array<LoadedModelEntry, 15> mdlBaseStructs{};

    // Cube & Cylinder meshes for fast batched 3D rendering
    Mesh unitCubeMesh{};
    Material defaultLitMat{};
    bool primitivesReady = false;

    bool loadAll();
    void unloadAll();

    // Sample terrain elevation in meters at world coordinates (wx, wy)
    float elevAtLonLat(float lon, float lat) const;
    float elevAtWorld(float wx, float wy) const;

private:
    std::string resolveAssetsRoot() const;
    bool loadTheaterBin(const std::string& path);
    bool loadElevationBin(const std::string& path);
    void buildMilitaryReliefTexture();
    LoadedModelEntry loadGlbNormalized(const std::string& path);
};

// Initialize starting campaign forces & bases
void initCampaignScenario(SimState& sim, TheaterAssets& assets);

} // namespace itc

