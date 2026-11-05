#include "asset_loader.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>

namespace itc {

namespace {

bool readBytes(const std::string& path, std::vector<uint8_t>& out) {
    std::ifstream ifs(path, std::ios::binary | std::ios::ate);
    if (!ifs) return false;
    auto sz = ifs.tellg();
    if (sz <= 0) return false;
    out.resize(static_cast<size_t>(sz));
    ifs.seekg(0, std::ios::beg);
    ifs.read(reinterpret_cast<char*>(out.data()), sz);
    return ifs.good();
}

struct BinReader {
    const uint8_t* data = nullptr;
    size_t size = 0;
    size_t offset = 0;

    template <typename T>
    T read() {
        T val{};
        if (offset + sizeof(T) <= size) {
            std::memcpy(&val, data + offset, sizeof(T));
            offset += sizeof(T);
        }
        return val;
    }

    std::string readStr() {
        uint32_t len = read<uint32_t>();
        if (offset + len > size) return {};
        std::string s(reinterpret_cast<const char*>(data + offset), len);
        offset += len;
        return s;
    }

    std::vector<Vector2> readPts() {
        uint32_t count = read<uint32_t>();
        std::vector<Vector2> pts;
        pts.reserve(count);
        for (uint32_t i = 0; i < count && offset + 8 <= size; ++i) {
            float x = read<float>();
            float y = read<float>();
            pts.push_back({x, y});
        }
        return pts;
    }
};

} // namespace

std::string TheaterAssets::resolveAssetsRoot() const {
    namespace fs = std::filesystem;
    std::vector<std::string> candidates = {
        "assets",
        "../assets",
        "cpp/assets",
#ifdef ITC_SOURCE_ASSETS_DIR
        ITC_SOURCE_ASSETS_DIR,
#endif
    };
    for (const auto& c : candidates) {
        if (fs::exists(fs::path(c) / "map" / "theater.bin")) {
            return c;
        }
    }
    return "assets";
}

bool TheaterAssets::loadTheaterBin(const std::string& path) {
    std::vector<uint8_t> buf;
    if (!readBytes(path, buf) || buf.size() < 16) return false;

    BinReader r{buf.data(), buf.size(), 0};
    char magic[4];
    std::memcpy(magic, buf.data(), 4);
    r.offset += 4;
    if (std::memcmp(magic, "ITCT", 4) != 0) return false;
    uint32_t version = r.read<uint32_t>();
    (void)version;

    iraqBorder = r.readPts();
    kuwaitBorder = r.readPts();
    gulfPoly = r.readPts();

    uint32_t nCities = r.read<uint32_t>();
    cities.clear();
    cities.reserve(nCities);
    for (uint32_t i = 0; i < nCities; ++i) {
        City c;
        c.id = r.readStr();
        c.name = r.readStr();
        c.sect = r.readStr();
        c.fobName = r.readStr();
        c.lon = r.read<float>();
        c.lat = r.read<float>();
        c.x = r.read<float>();
        c.y = r.read<float>();
        c.pop = r.read<float>();
        c.bldOffset = r.read<uint32_t>();
        c.bldCount = r.read<uint32_t>();
        c.streetOffset = r.read<uint32_t>();
        c.streetCount = r.read<uint32_t>();

        // Set initial sectarian & regional control
        if (c.sect == "kurd") {
            c.control = 78.0f;
            c.stability = 84.0f;
            c.intel = 65.0f;
            c.insurgentActivity = 4.0f;
            c.iraqArmyPresent = false;
        } else if (c.id == "ummqasr" || c.id == "basra") {
            c.control = 25.0f;
            c.stability = 60.0f;
            c.intel = 45.0f;
            c.insurgentActivity = 12.0f;
        } else {
            c.control = 0.0f;
            c.stability = 55.0f;
            c.intel = 22.0f;
            c.insurgentActivity = (c.sect == "sunni") ? 28.0f : 15.0f;
        }
        cities.push_back(std::move(c));
    }

    uint32_t nHw = r.read<uint32_t>();
    highways.clear();
    highways.reserve(nHw);
    for (uint32_t i = 0; i < nHw; ++i) {
        Highway hw;
        hw.name = r.readStr();
        hw.kind = r.readStr();
        hw.pts = r.readPts();
        highways.push_back(std::move(hw));
    }

    uint32_t nRiv = r.read<uint32_t>();
    rivers.clear();
    rivers.reserve(nRiv);
    for (uint32_t i = 0; i < nRiv; ++i) {
        rivers.push_back(r.readPts());
    }

    uint32_t nLakes = r.read<uint32_t>();
    lakes.clear();
    lakes.reserve(nLakes);
    for (uint32_t i = 0; i < nLakes; ++i) {
        Lake lk;
        lk.name = r.readStr();
        lk.poly = r.readPts();
        lakes.push_back(std::move(lk));
    }

    return true;
}

bool TheaterAssets::loadElevationBin(const std::string& path) {
    std::vector<uint8_t> buf;
    if (!readBytes(path, buf)) return false;
    size_t count = buf.size() / sizeof(int16_t);
    elevation.resize(count);
    std::memcpy(elevation.data(), buf.data(), count * sizeof(int16_t));
    return true;
}

float TheaterAssets::elevAtLonLat(float lon, float lat) const {
    if (elevation.empty()) return 0.0f;
    float fx = (lon - geo.lo0) * geo.ppd;
    float fy = (geo.la1 - lat) * geo.ppd;
    int x0 = std::clamp(static_cast<int>(std::floor(fx)), 0, geo.cols - 1);
    int y0 = std::clamp(static_cast<int>(std::floor(fy)), 0, geo.rows - 1);
    int x1 = std::min(x0 + 1, geo.cols - 1);
    int y1 = std::min(y0 + 1, geo.rows - 1);
    float tx = fx - std::floor(fx);
    float ty = fy - std::floor(fy);

    auto sample = [&](int x, int y) -> float {
        size_t idx = static_cast<size_t>(y) * geo.cols + x;
        return (idx < elevation.size()) ? static_cast<float>(elevation[idx]) : 0.0f;
    };

    float v00 = sample(x0, y0);
    float v10 = sample(x1, y0);
    float v01 = sample(x0, y1);
    float v11 = sample(x1, y1);
    float top = v00 * (1.0f - tx) + v10 * tx;
    float bot = v01 * (1.0f - tx) + v11 * tx;
    return top * (1.0f - ty) + bot * ty;
}

float TheaterAssets::elevAtWorld(float wx, float wy) const {
    Vector2 ll = wUnproj(wx, wy);
    return elevAtLonLat(ll.x, ll.y);
}

void TheaterAssets::buildMilitaryReliefTexture() {
    if (elevation.empty()) return;
    const int S = 3; // 3x supersampled relief texture (1305 x 918)
    const int W = geo.cols * S;
    const int H = geo.rows * S;

    std::vector<Color> pixels(static_cast<size_t>(W) * H);

    // Hypsometric military color ramp matching docs/index.html milBuild()
    struct Stop { float elev; float r, g, b; };
    static constexpr Stop ramp[] = {
        {-50.0f,  26.0f, 36.0f, 30.0f},
        {  0.0f,  34.0f, 44.0f, 34.0f},
        { 40.0f,  46.0f, 47.0f, 35.0f},
        {120.0f,  58.0f, 52.0f, 37.0f},
        {300.0f,  72.0f, 60.0f, 41.0f},
        {650.0f,  86.0f, 68.0f, 46.0f},
        {1200.0f, 98.0f, 77.0f, 53.0f},
        {2000.0f, 112.0f, 89.0f, 64.0f},
        {3200.0f, 132.0f, 110.0f, 86.0f}
    };

    auto sampleGrid = [&](float gx, float gy) -> float {
        int x0 = std::clamp(static_cast<int>(std::floor(gx)), 0, geo.cols - 1);
        int y0 = std::clamp(static_cast<int>(std::floor(gy)), 0, geo.rows - 1);
        int x1 = std::min(x0 + 1, geo.cols - 1);
        int y1 = std::min(y0 + 1, geo.rows - 1);
        float tx = gx - std::floor(gx);
        float ty = gy - std::floor(gy);
        float v00 = elevation[y0 * geo.cols + x0];
        float v10 = elevation[y0 * geo.cols + x1];
        float v01 = elevation[y1 * geo.cols + x0];
        float v11 = elevation[y1 * geo.cols + x1];
        return (v00 * (1.0f - tx) + v10 * tx) * (1.0f - ty) + (v01 * (1.0f - tx) + v11 * tx) * ty;
    };

    for (int py = 0; py < H; ++py) {
        float gy = static_cast<float>(py) / S;
        for (int px = 0; px < W; ++px) {
            float gx = static_cast<float>(px) / S;
            float e = sampleGrid(gx, gy);

            if (e <= 0.0f) {
                pixels[py * W + px] = {15, 29, 42, 255};
                continue;
            }

            // NW sun azimuth hillshade
            float dzdx = (sampleGrid(gx + 0.6f, gy) - sampleGrid(gx - 0.6f, gy)) / 12000.0f;
            float dzdy = (sampleGrid(gx, gy + 0.6f) - sampleGrid(gx, gy - 0.6f)) / 12000.0f;
            float shade = std::clamp(0.76f + (-dzdx * 0.707f - dzdy * 0.707f) * 3.8f, 0.42f, 1.28f);

            float r = ramp[8].r, g = ramp[8].g, b = ramp[8].b;
            for (size_t k = 0; k < 8; ++k) {
                if (e <= ramp[k + 1].elev) {
                    float t = std::clamp((e - ramp[k].elev) / (ramp[k + 1].elev - ramp[k].elev), 0.0f, 1.0f);
                    r = ramp[k].r + (ramp[k + 1].r - ramp[k].r) * t;
                    g = ramp[k].g + (ramp[k + 1].g - ramp[k].g) * t;
                    b = ramp[k].b + (ramp[k + 1].b - ramp[k].b) * t;
                    break;
                }
            }

            // Subtle 200m contour lines
            float cMod = std::fmod(e, 200.0f);
            if (e > 80.0f && cMod < 8.0f) {
                shade *= 0.88f;
            }

            pixels[py * W + px] = {
                static_cast<uint8_t>(std::clamp(r * shade, 0.0f, 255.0f)),
                static_cast<uint8_t>(std::clamp(g * shade, 0.0f, 255.0f)),
                static_cast<uint8_t>(std::clamp(b * shade, 0.0f, 255.0f)),
                255
            };
        }
    }

    Image img{};
    img.data = pixels.data();
    img.width = W;
    img.height = H;
    img.mipmaps = 1;
    img.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;

    milTexture = LoadTextureFromImage(img);
    SetTextureFilter(milTexture, TEXTURE_FILTER_BILINEAR);
    milLoaded = (milTexture.id != 0);
}

LoadedModelEntry TheaterAssets::loadGlbNormalized(const std::string& path) {
    LoadedModelEntry entry{};
    if (!std::filesystem::exists(path)) return entry;

    entry.model = LoadModel(path.c_str());
    if (entry.model.meshCount <= 0) return entry;

    entry.bounds = GetModelBoundingBox(entry.model);
    entry.size = {
         std::max(0.001f, entry.bounds.max.x - entry.bounds.min.x),
        std::max(0.001f, entry.bounds.max.y - entry.bounds.min.y),
        std::max(0.001f, entry.bounds.max.z - entry.bounds.min.z)
    };
    entry.center = {
        (entry.bounds.min.x + entry.bounds.max.x) * 0.5f,
        entry.bounds.min.y, // align bottom to Y=0 ground plane
        (entry.bounds.min.z + entry.bounds.max.z) * 0.5f
    };
    entry.valid = true;
    return entry;
}

bool TheaterAssets::loadAll() {
    assetsRoot = resolveAssetsRoot();
    TraceLog(LOG_INFO, "[ITC] Loading theater assets from: %s", assetsRoot.c_str());

    // 1. Theater geography
    if (!loadTheaterBin(assetsRoot + "/map/theater.bin")) {
        TraceLog(LOG_ERROR, "[ITC] Failed to load map/theater.bin");
        return false;
    }

    // 2. OSM Buildings (251,503 footprints)
    {
        std::vector<uint8_t> bbuf;
        if (readBytes(assetsRoot + "/map/osm_buildings.bin", bbuf)) {
            size_t count = bbuf.size() / sizeof(OsmBuilding);
            buildings.resize(count);
            std::memcpy(buildings.data(), bbuf.data(), count * sizeof(OsmBuilding));
            TraceLog(LOG_INFO, "[ITC] Loaded %zu OSM building footprints", count);
        }
    }

    // 3. OSM Streets (327,983 street segments)
    {
        std::vector<uint8_t> sbuf;
        if (readBytes(assetsRoot + "/map/osm_streets.bin", sbuf)) {
            size_t count = sbuf.size() / sizeof(StreetSeg);
            streets.resize(count);
            std::memcpy(streets.data(), sbuf.data(), count * sizeof(StreetSeg));
            TraceLog(LOG_INFO, "[ITC] Loaded %zu OSM street segments", count);
        }
    }

    // 4. Elevation & Hypsometric Relief Map
    if (loadElevationBin(assetsRoot + "/map/elevation.bin")) {
        buildMilitaryReliefTexture();
    }

    // 5. Satellite Map
    std::string satPath = assetsRoot + "/map/satellite.png";
    if (!std::filesystem::exists(satPath)) {
        satPath = assetsRoot + "/map/satellite.jpg";
    }
    if (std::filesystem::exists(satPath)) {
        satTexture = LoadTexture(satPath.c_str());
        SetTextureFilter(satTexture, TEXTURE_FILTER_BILINEAR);
        satLoaded = (satTexture.id != 0);
    }

    // 6. 3D GLB Models (Dequantized float32 glTF 2.0)
    std::string mdlDir = assetsRoot + "/models/";
    mdlTank    = loadGlbNormalized(mdlDir + "tank.glb");
    mdlIfv     = loadGlbNormalized(mdlDir + "ifv.glb");
    mdlHmmwv   = loadGlbNormalized(mdlDir + "hmmwv.glb");
    mdlBigRes  = loadGlbNormalized(mdlDir + "bigres.glb");
    mdlBigResL = loadGlbNormalized(mdlDir + "bigresL.glb");
    mdlRuin    = loadGlbNormalized(mdlDir + "ruin.glb");
    mdlHouse   = loadGlbNormalized(mdlDir + "house.glb");

    // Check for custom drop-in .glb models for the 15 base structures
    for (size_t i = 0; i < BASE_STRUCT_CATALOG.size(); ++i) {
        mdlBaseStructs[i] = loadGlbNormalized(mdlDir + BASE_STRUCT_CATALOG[i].modelFile);
    }

    unitCubeMesh = GenMeshCube(1.0f, 1.0f, 1.0f);
    defaultLitMat = LoadMaterialDefault();
    primitivesReady = true;

    return true;
}

void TheaterAssets::unloadAll() {
    if (satLoaded) UnloadTexture(satTexture);
    if (milLoaded) UnloadTexture(milTexture);
    auto unloadMdl = [](LoadedModelEntry& e) {
        if (e.valid) { UnloadModel(e.model); e.valid = false; }
    };
    unloadMdl(mdlTank);
    unloadMdl(mdlIfv);
    unloadMdl(mdlHmmwv);
    unloadMdl(mdlBigRes);
    unloadMdl(mdlBigResL);
    unloadMdl(mdlRuin);
    unloadMdl(mdlHouse);
    for (auto& m : mdlBaseStructs) unloadMdl(m);
}

void initCampaignScenario(SimState& sim, TheaterAssets& assets) {
    sim.bases.clear();
    sim.units.clear();

    auto findCity = [&](const std::string& id) -> const City* {
        for (const auto& c : assets.cities) {
            if (c.id == id) return &c;
        }
        return assets.cities.empty() ? nullptr : &assets.cities[0];
    };

    // Initial Coalition Forward Operating Bases & Staging Camps
    struct BaseSeed {
        const char* cityId;
        const char* name;
        int level;
        float dx, dy;
    };
    static constexpr BaseSeed baseSeeds[] = {
        {"baghdad",   "Camp Victory (BIAP)",   3, -2.2f,  1.4f},
        {"ummqasr",   "Camp Bucca / Staging",  2, -1.5f,  2.5f},
        {"nasiriyah", "Camp Adder (Tallil)",   2, -2.0f,  1.8f},
        {"fallujah",  "Camp Fallujah",         2,  1.8f,  1.2f},
        {"ramadi",    "Camp Blue Diamond",     2, -1.6f, -1.2f},
        {"balad",     "LSA Anaconda",          3,  1.5f, -1.4f},
        {"tikrit",    "FOB Speicher",          2, -1.4f, -1.6f},
        {"mosul",     "FOB Marez",             2, -1.8f,  1.5f},
        {"erbil",     "Erbil Air Base (JSOTF)",2,  1.4f, -1.2f},
        {"basra",     "Basra Air Station",     2, -1.8f,  1.4f},
    };

    int bId = 0;
    for (const auto& bs : baseSeeds) {
        const City* c = findCity(bs.cityId);
        if (!c) continue;
        Base b;
        b.id = bId++;
        b.name = bs.name;
        b.cityId = bs.cityId;
        b.x = c->x + bs.dx;
        b.y = c->y + bs.dy;
        b.level = bs.level;
        b.initDefaultLayout();
        sim.bases.push_back(std::move(b));
    }

    // Coalition & Iraqi Order of Battle (OIF 2003)
    struct UnitSeed {
        const char* name;
        const char* branch;
        UnitFaction faction;
        UnitCategory cat;
        float lon, lat;
        int pers;
        float cp;
    };
    static constexpr UnitSeed unitSeeds[] = {
        // Coalition V Corps & I MEF staged along Kuwait border & northern JSOTF
        {"3rd Infantry Div (1st BCT 'Raider')",  "USA",   UnitFaction::Coalition, UnitCategory::Armor,      47.10f, 29.85f, 4200, 96.0f},
        {"3rd Infantry Div (2nd BCT 'Spartan')", "USA",   UnitFaction::Coalition, UnitCategory::Armor,      47.35f, 29.75f, 4100, 95.0f},
        {"1st Marine Division (RCT-1)",          "USMC",  UnitFaction::Coalition, UnitCategory::Mechanized, 47.65f, 29.92f, 4500, 94.0f},
        {"1st Marine Division (RCT-5)",          "USMC",  UnitFaction::Coalition, UnitCategory::Mechanized, 47.82f, 29.88f, 4400, 94.0f},
        {"101st Airborne Div ('Screaming Eagles')","USA", UnitFaction::Coalition, UnitCategory::Aviation,   46.90f, 29.60f, 3800, 92.0f},
        {"82nd Airborne Div (2nd Bde)",          "USA",   UnitFaction::Coalition, UnitCategory::Infantry,   46.60f, 29.70f, 3200, 90.0f},
        {"1st UK Armoured Div ('Desert Rats')",  "UK",    UnitFaction::Coalition, UnitCategory::Armor,      47.92f, 30.02f, 4000, 93.0f},
        {"JSOC Task Force 20 / Delta-DEVGRU",    "SOCOM", UnitFaction::Coalition, UnitCategory::SpecialOps, 41.50f, 33.80f,  650, 98.0f},
        {"10th Special Forces Grp (Task Force Viking)","SOCOM",UnitFaction::Coalition,UnitCategory::SpecialOps,44.01f,36.19f, 900, 94.0f},
        {"11th Attack Helicopter Regt (AH-64D)", "USA",   UnitFaction::Coalition, UnitCategory::Aviation,   47.20f, 29.50f, 1100, 91.0f},
        {"4th Infantry Div ('Ironhorse')",       "USA",   UnitFaction::Coalition, UnitCategory::Armor,      47.50f, 29.40f, 4300, 96.0f},
        {"3rd ACR ('Brave Rifles')",             "USA",   UnitFaction::Coalition, UnitCategory::Mechanized, 46.80f, 29.95f, 3600, 91.0f},

        // Iraqi Republican Guard & Regular Army Corps
        {"RG Medina Armored Division",           "IRQ",   UnitFaction::IraqConventional, UnitCategory::Armor,      44.22f, 32.75f, 7500, 78.0f},
        {"RG Hammurabi Armored Division",        "IRQ",   UnitFaction::IraqConventional, UnitCategory::Armor,      43.95f, 33.38f, 7800, 80.0f},
        {"RG Baghdad Mechanized Division",       "IRQ",   UnitFaction::IraqConventional, UnitCategory::Mechanized, 45.65f, 32.55f, 6800, 72.0f},
        {"RG Nebuchadnezzar Infantry Div",       "IRQ",   UnitFaction::IraqConventional, UnitCategory::Infantry,   43.72f, 34.58f, 6500, 70.0f},
        {"RG Al-Nida Armored Division",          "IRQ",   UnitFaction::IraqConventional, UnitCategory::Armor,      44.62f, 33.68f, 7200, 75.0f},
        {"51st Mechanized Div (Basra Defense)",  "IRQ",   UnitFaction::IraqConventional, UnitCategory::Mechanized, 47.72f, 30.55f, 6000, 58.0f},
        {"11th Infantry Div (Nasiriyah)",        "IRQ",   UnitFaction::IraqConventional, UnitCategory::Infantry,   46.22f, 31.08f, 5400, 55.0f},
        {"Fedayeen Saddam Irregulars (Anbar)",   "IRQ",   UnitFaction::Insurgent,        UnitCategory::Infantry,   43.55f, 33.38f, 2400, 62.0f},
    };

    int uId = 1;
    for (const auto& us : unitSeeds) {
        Vector2 p = wProj(us.lon, us.lat);
        Unit u;
        u.id = uId++;
        u.name = us.name;
        u.branch = us.branch;
        u.faction = us.faction;
        u.category = us.cat;
        u.x = p.x;
        u.y = p.y;
        u.targetX = p.x;
        u.targetY = p.y;
        u.personnel = us.pers;
        u.combatPower = us.cp;
        u.heading = (us.faction == UnitFaction::Coalition) ? -1.57f : 1.57f;
        sim.units.push_back(std::move(u));
    }

    sim.pushLog("CENTCOM / CFLCC Theater Command initialized. Select Campaign Phase or issue unit orders.");
    sim.pushLog("Press [B] for 3D Tactical & Base Builder view, [C] for Campaign Phases, [F] for Fires.");
}

} // namespace itc
