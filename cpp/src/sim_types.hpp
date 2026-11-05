#pragma once

#include <raylib.h>
#include <raymath.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace itc {

// ============================================================================
// World Projection & Geographic Constants
// ============================================================================
struct GeoBounds {
    float lo0 = 30.0f;
    float lo1 = 59.0f;
    float la0 = 20.6f;
    float la1 = 41.0f;
    int cols = 435;
    int rows = 306;
    float ppd = 15.0f;
};

inline Vector2 wProj(float lon, float lat) {
    return {(lon - 38.6f) * 102.5f, (37.6f - lat) * 122.0f};
}

inline Vector2 wUnproj(float wx, float wy) {
    return {wx / 102.5f + 38.6f, 37.6f - wy / 122.0f};
}

inline std::string toMGRS(float wx, float wy) {
    Vector2 ll = wUnproj(wx, wy);
    int zone = (ll.x < 42.0f) ? 37 : (ll.x < 48.0f ? 38 : 39);
    char band = (ll.y < 32.0f) ? 'R' : 'S';
    const char* sq = (zone == 38 && band == 'S') ? "MB" : (zone == 38 ? "LB" : "KB");
    int e = std::abs(static_cast<int>( std::floor((ll.x - std::floor(ll.x)) * 99999.0f) )) % 100000;
    int n = std::abs(static_cast<int>( std::floor((ll.y - std::floor(ll.y)) * 99999.0f) )) % 100000;
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%d%c %s %05d %05d", zone, band, sq, e, n);
    return std::string(buf);
}

// ============================================================================
// OSM Buildings & Streets Binary Layout (matches extract_assets.py)
// ============================================================================
#pragma pack(push, 1)
struct OsmBuilding {
    float cx;
    float cy;
    float ww;
    float wh;
    float angle;
    float hNorm;
    uint8_t kind;
    uint8_t levels;
    uint8_t pad0;
    uint8_t pad1;
};

struct StreetSeg {
    float x1;
    float y1;
    float x2;
    float y2;
    uint32_t cls; // 0=major, 1=secondary, 2=residential, 3=alley
};
#pragma pack(pop)

static_assert(sizeof(OsmBuilding) == 28, "OsmBuilding binary stride must be 28 bytes");
static_assert(sizeof(StreetSeg) == 20, "StreetSeg binary stride must be 20 bytes");

struct City {
    std::string id;
    std::string name;
    std::string sect; // "sunni", "shia", "kurd", "mixed"
    std::string fobName;
    float lon = 0.0f;
    float lat = 0.0f;
    float x = 0.0f;
    float y = 0.0f;
    float pop = 0.1f;
    uint32_t bldOffset = 0;
    uint32_t bldCount = 0;
    uint32_t streetOffset = 0;
    uint32_t streetCount = 0;

    // Dynamic simulation state
    float control = 0.0f;      // 0..100% Coalition control
    float stability = 65.0f;   // 0..100% Civil stability
    float intel = 25.0f;       // 0..100% HUMINT/SIGINT coverage
    float insurgentActivity = 10.0f;
    float caches = 2.0f;
    bool iraqArmyPresent = true;
};

struct Highway {
    std::string name;
    std::string kind; // "msr" or "minor"
    std::vector<Vector2> pts;
};

struct Lake {
    std::string name;
    std::vector<Vector2> poly;
};

// ============================================================================
// 16x16 Grid 3D Base Building System
// ============================================================================
inline constexpr int BASE_GRID_N = 16;

struct BaseStructDef {
    const char* id;
    const char* name;
    const char* cat; // "Command", "Quarters", "Defense", "Aviation", "Logistics", "Medical", "Support"
    int cost;        // $M
    int w;           // Grid cells wide
    int d;           // Grid cells deep
    float h;         // Height in meters/grid units
    Color color;
    const char* bonusDesc;
    const char* modelFile; // Optional custom .glb override in cpp/assets/models/<modelFile>
};

inline constexpr std::array<BaseStructDef, 15> BASE_STRUCT_CATALOG = {{
    {"toc",   "Tactical Ops Ctr (TOC)", "Command",   12, 2, 2, 1.4f, {90, 122, 82, 255},  "+15% Intel / C2 range",       "toc.glb"},
    {"sleep", "Barracks / Berthing",    "Quarters",   8, 2, 1, 1.0f, {138, 128, 104, 255},"+250 Cap / +Morale",          "sleep.glb"},
    {"hesco", "HESCO Bastion Wall",     "Defense",    2, 1, 1, 0.7f, {168, 149, 106, 255},"+4 Perimeter Defense",        "hesco.glb"},
    {"tower", "Guard Tower",            "Defense",    5, 1, 1, 2.6f, {110, 106, 94, 255}, "+10 Security / Early Warn",   "tower.glb"},
    {"heli",  "Helipad / LZ",           "Aviation",  18, 3, 3, 0.15f,{74, 82, 88, 255},   "Enables Air Assault / MEDEVAC","heli.glb"},
    {"motor", "Motorpool & Depot",      "Logistics", 14, 3, 2, 1.3f, {122, 108, 86, 255}, "+25% Vehicle Readiness",      "motor.glb"},
    {"ammo",  "Ammo Supply Pt (ASP)",   "Logistics", 15, 2, 2, 0.9f, {118, 98, 68, 255},  "+40% Ammo Storage",           "ammo.glb"},
    {"fuel",  "Fuel Point (FARP)",      "Logistics", 14, 2, 2, 1.1f, {108, 90, 68, 255},  "+40% Fuel Capacity",          "fuel.glb"},
    {"med",   "Role-2 Aid Station",     "Medical",   16, 2, 2, 1.2f, {140, 148, 138, 255},"-35% KIA from WIA",           "med.glb"},
    {"comms", "SATCOM / AN-TSC-93",     "Command",   10, 1, 1, 3.2f, {104, 132, 144, 255},"+20% Fire Support Response",  "comms.glb"},
    {"mort",  "120mm Mortar Pit",       "Defense",    9, 1, 1, 0.6f, {122, 114, 88, 255}, "Organic Indirect Fire",       "mort.glb"},
    {"gen",   "Prime Power Plant",      "Support",    7, 1, 2, 1.1f, {98, 106, 110, 255}, "Powers TOC/Comms/Hospital",   "gen.glb"},
    {"dfac",  "DFAC Dining Facility",   "Quarters",  10, 2, 2, 1.2f, {146, 132, 106, 255},"+8 Troop Morale / QoL",       "dfac.glb"},
    {"det",   "Detention Facility",     "Command",   11, 2, 2, 1.1f, {106, 98, 94, 255},  "+HUMINT from Detainees",      "det.glb"},
    {"uav",   "UAV Ground Control",     "Aviation",  20, 2, 2, 1.0f, {84, 104, 116, 255}, "Persistent ISR / MQ-1 Orbit", "uav.glb"},
}};

inline const BaseStructDef* findStructDef(const std::string& id) {
    for (const auto& def : BASE_STRUCT_CATALOG) {
        if (id == def.id) return &def;
    }
    return &BASE_STRUCT_CATALOG[0];
}

struct PlacedStructure {
    std::string kind;
    int gx = 0;  // Grid X [0..15]
    int gz = 0;  // Grid Z [0..15]
    int rot = 0; // 0 or 1 (90 deg rotation)
};

struct GridSnapResult {
    int gx = 0;
    int gz = 0;
    int w = 1;
    int d = 1;
};

inline GridSnapResult bldGridSnap(float u, float v, const std::string& kind, int rot) {
    const BaseStructDef* def = findStructDef(kind);
    int w = (rot % 2) ? def->d : def->w;
    int d = (rot % 2) ? def->w : def->d;
    int gx = std::clamp(static_cast<int>(std::floor((u + 0.5f) * BASE_GRID_N - w * 0.5f + 0.5f)), 0, BASE_GRID_N - w);
    int gz = std::clamp(static_cast<int>(std::floor((v + 0.5f) * BASE_GRID_N - d * 0.5f + 0.5f)), 0, BASE_GRID_N - d);
    return {gx, gz, w, d};
}

struct Base {
    int id = 0;
    std::string name;
    std::string cityId;
    float x = 0.0f;
    float y = 0.0f;
    int level = 1; // 1=COP, 2=FOB, 3=Super-FOB / Airbase
    float supply = 100.0f;
    float ammo = 100.0f;
    float fuel = 100.0f;
    float defense = 40.0f;
    float qol = 20.0f;
    int capacity = 1200;
    std::vector<PlacedStructure> layout;

    bool canPlace(int gx, int gz, int w, int d, int ignoreIdx = -1) const {
        if (gx < 0 || gz < 0 || gx + w > BASE_GRID_N || gz + d > BASE_GRID_N) return false;
        for (size_t i = 0; i < layout.size(); ++i) {
            if (static_cast<int>(i) == ignoreIdx) continue;
            const auto& it = layout[i];
            const BaseStructDef* def = findStructDef(it.kind);
            int iw = (it.rot % 2) ? def->d : def->w;
            int idp = (it.rot % 2) ? def->w : def->d;
            if (gx < it.gx + iw && gx + w > it.gx && gz < it.gz + idp && gz + d > it.gz) {
                return false;
            }
        }
        return true;
    }

    void recalcStats() {
        defense = 25.0f + level * 15.0f;
        qol = 10.0f + level * 8.0f;
        capacity = 800 * level;
        for (const auto& s : layout) {
            if (s.kind == "hesco") defense += 4.0f;
            else if (s.kind == "tower") defense += 10.0f;
            else if (s.kind == "mort") defense += 12.0f;
            else if (s.kind == "sleep") { capacity += 250; qol += 4.0f; }
            else if (s.kind == "dfac") qol += 8.0f;
            else if (s.kind == "med") qol += 6.0f;
        }
        defense = std::min(100.0f, defense);
        qol = std::min(100.0f, qol);
    }

    void initDefaultLayout() {
        layout.clear();
        // Default perimeter corners & core TOC/Barracks/Motorpool/Helipad
        layout.push_back({"toc",   6, 6, 0});
        layout.push_back({"comms", 8, 6, 0});
        layout.push_back({"sleep", 4, 6, 0});
        layout.push_back({"sleep", 4, 8, 0});
        layout.push_back({"dfac",  6, 8, 0});
        layout.push_back({"med",   8, 8, 0});
        layout.push_back({"motor", 3, 10, 0});
        layout.push_back({"ammo",  10, 10, 0});
        layout.push_back({"fuel",  10, 7, 0});
        layout.push_back({"heli",  11, 3, 0});
        layout.push_back({"tower", 1, 1, 0});
        layout.push_back({"tower", 14, 1, 0});
        layout.push_back({"tower", 1, 14, 0});
        layout.push_back({"tower", 14, 14, 0});
        for (int g = 3; g <= 12; ++g) {
            layout.push_back({"hesco", g, 1, 0});
            layout.push_back({"hesco", g, 14, 0});
        }
        recalcStats();
    }
};

// ============================================================================
// Units (Coalition & Iraqi Conventional / Insurgent Forces)
// ============================================================================
enum class UnitFaction {
    Coalition,
    IraqConventional,
    Insurgent
};

enum class UnitCategory {
    Armor,      // M1A2 Abrams -> tank.glb
    Mechanized, // M2A3 Bradley / LAV-25 / Stryker -> ifv.glb
    Infantry,   // Motorized / Airborne / Marines -> hmmwv.glb
    Aviation,   // AH-64D Apache / UH-60 / A-10
    SpecialOps, // JSOC / SFODA / SEALs
    Artillery,  // M109A6 Paladin / MLRS
    Logistics   // Combat Service Support Convoy
};

struct Unit {
    int id = 0;
    std::string name;
    std::string branch; // "USA", "USMC", "USAF", "USN", "SOCOM", "UK", "IRQ"
    UnitFaction faction = UnitFaction::Coalition;
    UnitCategory category = UnitCategory::Mechanized;
    float x = 0.0f;
    float y = 0.0f;
    float targetX = 0.0f;
    float targetY = 0.0f;
    bool hasOrder = false;
    float heading = 0.0f; // radians
    float speed = 14.0f;  // world units per day
    float strength = 100.0f;
    float maxStrength = 100.0f;
    int personnel = 800;
    float morale = 92.0f;
    float supply = 100.0f;
    float combatPower = 85.0f;
    std::string posture = "Attack"; // "Attack", "Defend", "Patrol", "Clear", "Reserve"
    int assignedBaseId = -1;
};

// ============================================================================
// Campaign Phases & Compressed Mission-Type Fires System
// ============================================================================
enum class CampaignPhase : int {
    BuildUp = 0,        // Phase I: Kuwait Staging & Logistics Build-Up
    Ultimatum = 1,      // Phase II: 48-Hour Ultimatum & Cross-Border ISR
    ShockAndAwe = 2,    // Phase III: Strategic Air Campaign (Shock & Awe)
    GroundInvasion = 3, // Phase IV: OIF Cobra II Ground Invasion to Baghdad
    Insurgency = 4      // Phase V: Post-Regime Stability & Counter-Insurgency
};

struct CampaignPhaseInfo {
    CampaignPhase phase;
    const char* code;
    const char* title;
    const char* subtitle;
    const char* description;
    int budgetGrant;
};

inline constexpr std::array<CampaignPhaseInfo, 5> CAMPAIGN_PHASES = {{
    {CampaignPhase::BuildUp,        "PHASE I",   "BUILD-UP IN KUWAIT",     "CFLCC Staging & Logistics Prep",
     "Stage V Corps & I MEF in northern Kuwait, build forward logistics bases, and pre-position fuel/ammo.", 150},
    {CampaignPhase::Ultimatum,      "PHASE II",  "48-HOUR ULTIMATUM",      "Presidential Ultimatum & SOF Infiltration",
     "48-hour deadline issued to Saddam Hussein. JSOC, CIA Jawbreaker, and Special Forces infiltrate western/northern Iraq.", 100},
    {CampaignPhase::ShockAndAwe,    "PHASE III", "SHOCK & AWE AIR BLITZ",  "Strategic Decapitation & SEAD",
     "Unleash Tomahawk TLAM salvos, B-2 Spirit stealth strikes, and coalition air supremacy across Baghdad and Iraqi C2.", 250},
    {CampaignPhase::GroundInvasion, "PHASE IV",  "GROUND INVASION",        "V Corps & I MEF Drive on Baghdad",
     "Cross the berm! 3rd ID attacks up Karbala Gap while 1st Marine Division pushes through Nasiriyah and Route 6.", 300},
    {CampaignPhase::Insurgency,     "PHASE V",   "STABILITY & COIN OPS",   "Counter-Insurgency & Reconstruction",
     "Regime toppled. Transition to FOB construction, civil reconstruction, HUMINT networks, and counter-IED operations.", 200},
}};

// Compressed Fires Menu: Call by Mission Type only (automatically dispatches best available asset)
struct FireMissionType {
    const char* id;
    const char* name;
    const char* badge;
    const char* desc;
    int cost;
    float radius;
    float damage;
    float flightSeconds;
    Color color;
};

inline constexpr std::array<FireMissionType, 9> FIRES_MISSION_TYPES = {{
    {"cas",    "CLOSE AIR SUPPORT (CAS)",  "AIR",    "A-10C / AH-64D gun & Hellfire runs on tactical targets", 4,  8.0f,  28.0f, 2.5f, {255, 165, 60, 255}},
    {"jdam",   "PRECISION JDAM STRIKE",    "STRIKE", "F-15E / F/A-18 GBU-31 2,000lb GPS-guided strike",        8,  10.0f, 48.0f, 3.2f, {255, 95,  55, 255}},
    {"tlam",   "TOMAHAWK CRUISE SALVO",    "DEEP",   "BGM-109 TLAM deep strategic strike from 5th Fleet",      15, 14.0f, 70.0f, 4.5f, {255, 65,  65, 255}},
    {"arty",   "155MM ARTILLERY BARRAGE",  "FIRES",  "M109A6 Paladin HE/Excalibur battery fire mission",       3,  9.0f,  24.0f, 2.0f, {245, 200, 70, 255}},
    {"mlrs",   "M270 MLRS / HIMARS",       "FIRES",  "GMLRS rocket salvo for counter-battery & area targets",  7,  13.0f, 42.0f, 2.8f, {255, 140, 50, 255}},
    {"mortar", "120MM MORTAR FIRE",        "ORGANIC","Immediate FOB/Battalion organic mortar suppression",     1,  5.5f,  14.0f, 1.4f, {220, 210, 95, 255}},
    {"sead",   "SEAD / HARM PACKAGE",      "AIR",    "F-16CJ Wild Weasel suppression of enemy air defenses",   6,  16.0f, 35.0f, 2.6f, {120, 200, 255, 255}},
    {"uav",    "MQ-1 PREDATOR ISR ORBIT",  "ISR",    "Loitering electro-optical/IR recon & Hellfire overwatch",3,  22.0f, 5.0f,  1.5f, {90,  230, 175, 255}},
    {"sof",    "JSOC DIRECT ACTION RAID",  "SOF",    "Night helicopter raid to capture HVTs & seize caches",   10, 7.0f,  32.0f, 2.2f, {175, 145, 255, 255}},
}};

struct ActiveFireMission {
    std::string typeId;
    std::string name;
    Vector2 origin;
    Vector2 target;
    float progress = 0.0f; // 0..1
    float speed = 0.4f;
    float radius = 8.0f;
    float damage = 25.0f;
    Color color = ORANGE;
};

struct ExplosionFx {
    Vector2 pos;
    float age = 0.0f;
    float maxAge = 1.6f;
    float maxRadius = 10.0f;
    Color color = ORANGE;
};

// ============================================================================
// View Modes, UI Left Rail Modals, Bottom Dock Tabs, and Keybindings
// ============================================================================
enum class ViewMode : int {
    Map2D = 0,
    Tac3D = 1,
    Drone = 2,
    Lase  = 3
};

enum class LeftModal : int {
    None = 0,
    Campaign = 1,
    Fires = 2,
    AOs = 3,
    Logistics = 4,
    Ops = 5,
    Advisors = 6
};

enum class BottomTab : int {
    Forces = 0,
    Bases = 1,
    Intel = 2,
    TechROE = 3,
    Settings = 4
};

struct KeyBindings {
    int panUp       = KEY_W;
    int panDown     = KEY_S;
    int panLeft     = KEY_A;
    int panRight    = KEY_D;
    int rotLeft     = KEY_Q;
    int rotRight    = KEY_E;
    int pauseToggle = KEY_SPACE;
    int speedDown   = KEY_LEFT_BRACKET;
    int speedUp     = KEY_RIGHT_BRACKET;
    int campaignMenu= KEY_C;
    int firesMenu   = KEY_F;
    int toggle3D    = KEY_B;
    int toggleDrone = KEY_V;
    int toggleLase  = KEY_G;
    int rotateStruct= KEY_R;
    int mapModeToggle = KEY_M;

    // For interactive key rebinding in Settings tab
    int rebindingSlot = -1;
};

// ============================================================================
// Master Simulation & Camera State
// ============================================================================
struct SimState {
    // Time & Campaign
    int year = 2003;
    int month = 3;
    int day = 19;
    int hour = 5;
    float dayAccumulator = 0.0f;
    int simSpeed = 1; // 0=Paused, 1=1x, 2=2x, 4=4x
    CampaignPhase phase = CampaignPhase::BuildUp;

    // Strategic metrics
    int budget = 420;            // $M available
    float publicSupport = 74.0f; // % US public support
    float congressSupport = 78.0f;
    float coalitionMorale = 91.0f;
    int coalitionKIA = 0;
    int coalitionWIA = 0;
    int enemyNeutralized = 0;

    // Map & 3D Camera
    ViewMode viewMode = ViewMode::Map2D;
    bool satelliteMap = false; // false = Hypsometric Tactical Map, true = Satellite Imagery
    bool showGrid = true;
    bool showLabels = true;
    bool showUnits = true;
    bool show3DBuildings = true;

    // 2D / 3D focal point in world space (Baghdad is ~590, 523)
    float camX = 590.4f;
    float camY = 523.4f;
    float zoom = 2.4f;       // 2D pixels per world unit (0.4 to 45.0)
    float camYaw = 0.35f;    // 3D orbit yaw (radians)
    float camPitch = 0.62f;  // 3D orbit pitch (radians)
    float camDist = 28.0f;   // 3D camera distance in world units

    // Selections & Interactive Modes
    int selectedCityIdx = 6; // Baghdad default
    int selectedUnitId = -1;
    int selectedBaseIdx = 0; // Camp Victory default
    std::string activeFireMissionId; // Non-empty when targeting a fire mission on the map

    // 3D / 2D Base Builder Placement State
    bool baseBuilderActive = false;
    std::string buildStructId = "toc";
    int buildRot = 0; // 0 or 1
    bool demolishMode = false;
    int hoverGx = -1;
    int hoverGz = -1;
    int hoverW = 1;
    int hoverD = 1;
    bool hoverValid = false;

    // UI state
    LeftModal leftModal = LeftModal::None;
    BottomTab bottomTab = BottomTab::Forces;
    bool bottomDockExpanded = true;
    KeyBindings keys;

    // Entities
    std::vector<Base> bases;
    std::vector<Unit> units;
    std::vector<ActiveFireMission> activeFires;
    std::vector<ExplosionFx> explosions;
    std::vector<std::string> tickerLog;

    void pushLog(const std::string& msg) {
        char prefix[32];
        std::snprintf(prefix, sizeof(prefix), "[%04d-%02d-%02d %02d00Z] ", year, month, day, hour);
        tickerLog.insert(tickerLog.begin(), std::string(prefix) + msg);
        if (tickerLog.size() > 40) tickerLog.pop_back();
    }
};

} // namespace itc
