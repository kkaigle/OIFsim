#include "ui_panels.hpp"
#include <algorithm>
#include <cstdio>

namespace itc {

namespace {

const char* keyName(int key) {
    switch (key) {
        case KEY_W: return "W";
        case KEY_A: return "A";
        case KEY_S: return "S";
        case KEY_D: return "D";
        case KEY_Q: return "Q";
        case KEY_E: return "E";
        case KEY_R: return "R";
        case KEY_C: return "C";
        case KEY_F: return "F";
        case KEY_B: return "B";
        case KEY_V: return "V";
        case KEY_G: return "G";
        case KEY_M: return "M";
        case KEY_SPACE: return "SPACE";
        case KEY_LEFT_BRACKET: return "[";
        case KEY_RIGHT_BRACKET: return "]";
        case KEY_UP: return "UP";
        case KEY_DOWN: return "DOWN";
        case KEY_LEFT: return "LEFT";
        case KEY_RIGHT: return "RIGHT";
        default: {
            static char buf[16];
            if (key >= 32 && key <= 90) {
                std::snprintf(buf, sizeof(buf), "%c", static_cast<char>(key));
                return buf;
            }
            std::snprintf(buf, sizeof(buf), "KEY_%d", key);
            return buf;
        }
    }
}

} // namespace

bool UIPanels::isMouseOverUI(Vector2 mp, const SimState& sim, int screenW, int screenH) const {
    // Top bar
    if (mp.y <= 38.0f) return true;
    // Left rail
    if (mp.x <= 135.0f && mp.y <= 310.0f) return true;
    // Top-right view switcher
    if (mp.x >= screenW - 390.0f && mp.y <= 82.0f) return true;
    // Left modal open
    if (sim.leftModal != LeftModal::None && mp.x >= 140.0f && mp.x <= 560.0f && mp.y >= 46.0f && mp.y <= 530.0f) {
        return true;
    }
    // Bottom dock
    float dockH = sim.bottomDockExpanded ? 215.0f : 34.0f;
    if (mp.y >= screenH - dockH) return true;
    return false;
}

bool UIPanels::drawTacButton(Rectangle r, const char* label, bool active, Color accent) const {
    Vector2 mp = GetMousePosition();
    bool hovered = CheckCollisionPointRec(mp, r);
    Color bg = active ? Color{34, 58, 42, 245}
             : hovered ? Color{32, 42, 36, 240}
                       : Color{20, 26, 22, 230};
    Color border = active ? accent : (hovered ? Color{140, 175, 145, 230} : Color{62, 78, 66, 210});
    DrawRectangleRec(r, bg);
    DrawRectangleLinesEx(r, active ? 1.8f : 1.0f, border);
    Color txtCol = active ? accent : Color{215, 222, 210, 245};
    DrawText(label, static_cast<int>(r.x + 8), static_cast<int>(r.y + (r.height - 11) * 0.5f), 11, txtCol);
    return hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

void UIPanels::advanceCampaignPhase(SimState& sim, TheaterAssets& assets, AudioEngine& audio, CampaignPhase targetPhase) {
    int curIdx = static_cast<int>(sim.phase);
    int tgtIdx = static_cast<int>(targetPhase);
    if (tgtIdx <= curIdx || tgtIdx >= static_cast<int>(CAMPAIGN_PHASES.size())) return;

    sim.phase = targetPhase;
    const auto& info = CAMPAIGN_PHASES[tgtIdx];
    sim.budget += info.budgetGrant;
    audio.playRadio();

    sim.pushLog(std::string("CAMPAIGN PHASE ADVANCED -> ") + info.code + ": " + info.title +
                " (+" + std::to_string(info.budgetGrant) + "M Budget)");

    // Strategic phase effects on theater units & Iraqi forces
    if (targetPhase == CampaignPhase::Ultimatum) {
        for (auto& c : assets.cities) {
            c.intel = std::min(100.0f, c.intel + 15.0f);
        }
    } else if (targetPhase == CampaignPhase::ShockAndAwe) {
        audio.playMissile();
        // Automatically launch initial Tomahawk & Stealth strike wave on Baghdad & Tikrit regime targets
        for (const auto& c : assets.cities) {
            if (c.id == "baghdad" || c.id == "tikrit" || c.id == "mosul" || c.id == "basra") {
                ActiveFireMission fm;
                fm.typeId = "tlam";
                fm.name = "SHOCK & AWE TLAM SALVO";
                fm.origin = wProj(48.8f, 29.0f); // 5th Fleet Persian Gulf
                fm.target = {c.x, c.y};
                fm.speed = 0.35f;
                fm.radius = 14.0f;
                fm.damage = 45.0f;
                fm.color = {255, 85, 65, 255};
                sim.activeFires.push_back(fm);
            }
        }
    } else if (targetPhase == CampaignPhase::GroundInvasion) {
        // Order V Corps & I MEF divisions across the Kuwait berm toward Nasiriyah, Najaf, Karbala, and Basra!
        const char* objCities[] = {"basra", "nasiriyah", "najaf", "karbala", "baghdad"};
        size_t objIdx = 0;
        for (auto& u : sim.units) {
            if (u.faction == UnitFaction::Coalition && u.y > 600.0f) {
                const char* cid = objCities[objIdx % 5];
                objIdx++;
                for (const auto& c : assets.cities) {
                    if (c.id == cid) {
                        u.targetX = c.x + ((objIdx % 3) - 1) * 2.5f;
                        u.targetY = c.y + ((objIdx % 2) ? 1.8f : -1.8f);
                        u.hasOrder = true;
                        break;
                    }
                }
            }
        }
        sim.pushLog("V CORPS & I MEF CROSSING THE BERM — Cobra II Ground Invasion underway!");
    } else if (targetPhase == CampaignPhase::Insurgency) {
        for (auto& c : assets.cities) {
            c.control = std::max(c.control, 65.0f);
            if (c.sect == "sunni") c.insurgentActivity += 25.0f;
        }
        sim.pushLog("Regime toppled. Coalition transitioning to Phase V Stability & Counter-Insurgency (COIN).");
    }
}

void UIPanels::renderTopBar(SimState& sim, TheaterAssets& assets, AudioEngine& audio, int screenW) {
    (void)assets;
    DrawRectangle(0, 0, screenW, 38, {14, 19, 16, 245});
    DrawLine(0, 38, screenW, 38, {58, 76, 62, 230});

    // Title & DTG
    DrawText("IRAQ THEATER COMMAND // C++20 NATIVE", 12, 6, 12, {125, 235, 150, 255});
    char dtg[64];
    std::snprintf(dtg, sizeof(dtg), "DTG: %04d-%02d-%02d %02d00Z", sim.year, sim.month, sim.day, sim.hour);
    DrawText(dtg, 12, 21, 11, {180, 195, 175, 240});

    // Campaign Phase Badge (clickable to open Campaign Modal)
    const auto& pInfo = CAMPAIGN_PHASES[static_cast<int>(sim.phase)];
    std::string pLabel = std::string(pInfo.code) + ": " + pInfo.title;
    if (drawTacButton({280, 5, 235, 28}, pLabel.c_str(), sim.leftModal == LeftModal::Campaign, {245, 200, 80, 255})) {
        audio.playClick();
        sim.leftModal = (sim.leftModal == LeftModal::Campaign) ? LeftModal::None : LeftModal::Campaign;
    }

    // Strategic Metrics
    char statsBuf[160];
    std::snprintf(statsBuf, sizeof(statsBuf), "BUDGET: $%dM   |   SUPPORT: %.0f%%   |   MORALE: %.0f%%   |   KIA/WIA: %d/%d",
                  sim.budget, sim.publicSupport, sim.coalitionMorale, sim.coalitionKIA, sim.coalitionWIA);
    DrawText(statsBuf, 530, 13, 12, {220, 225, 205, 245});

    // Speed Controls on Right
    float sx = static_cast<float>(screenW - 215);
    const char* spdLabels[4] = {"PAUSE", "1X", "2X", "4X"};
    const int spdVals[4] = {0, 1, 2, 4};
    for (int i = 0; i < 4; ++i) {
        if (drawTacButton({sx + i * 50.0f, 6.0f, 46.0f, 26.0f}, spdLabels[i], sim.simSpeed == spdVals[i])) {
            audio.playClick();
            sim.simSpeed = spdVals[i];
        }
    }
}

void UIPanels::renderLeftRail(SimState& sim, TheaterAssets& assets, AudioEngine& audio) {
    (void)assets;
    struct RailBtn {
        LeftModal modal;
        const char* label;
        Color accent;
    };
    // Campaign is the VERY FIRST button on the left as requested, followed by compressed Fires!
    static constexpr RailBtn btns[] = {
        {LeftModal::Campaign,  "> CAMPAIGN [C]",  {245, 200, 80, 255}},
        {LeftModal::Fires,     "* FIRES [F]",     {255, 115, 75, 255}},
        {LeftModal::AOs,       "# CITIES & AOs",  {115, 215, 255, 255}},
        {LeftModal::Logistics, "= LOGISTICS",     {155, 230, 145, 255}},
        {LeftModal::Ops,       "+ CIVIL / COIN",  {210, 175, 255, 255}},
    };

    float y = 46.0f;
    for (const auto& b : btns) {
        if (drawTacButton({8.0f, y, 124.0f, 32.0f}, b.label, sim.leftModal == b.modal, b.accent)) {
            audio.playClick();
            sim.leftModal = (sim.leftModal == b.modal) ? LeftModal::None : b.modal;
        }
        y += 37.0f;
    }

    // Quick 3D Base Builder Shortcut Button on Left Rail
    if (drawTacButton({8.0f, y + 6.0f, 124.0f, 34.0f},
                      sim.baseBuilderActive ? "[3D BUILD: ON]" : "3D BASE BUILD [B]",
                      sim.baseBuilderActive, {110, 245, 155, 255})) {
        audio.playClick();
        sim.viewMode = ViewMode::Tac3D;
        sim.baseBuilderActive = !sim.baseBuilderActive;
        sim.bottomTab = BottomTab::Bases;
        sim.bottomDockExpanded = true;
        if (sim.selectedBaseIdx >= 0 && sim.selectedBaseIdx < static_cast<int>(sim.bases.size())) {
            sim.camX = sim.bases[sim.selectedBaseIdx].x;
            sim.camY = sim.bases[sim.selectedBaseIdx].y;
            sim.camDist = 11.5f;
        }
    }
}

void UIPanels::renderViewSwitcher(SimState& sim, AudioEngine& audio, int screenW) {
    float x = static_cast<float>(screenW - 385);
    float y = 46.0f;
    if (drawTacButton({x, y, 82.0f, 28.0f}, "2D MAP", sim.viewMode == ViewMode::Map2D)) {
        audio.playClick();
        sim.viewMode = ViewMode::Map2D;
        sim.baseBuilderActive = false;
    }
    if (drawTacButton({x + 86.0f, y, 92.0f, 28.0f}, "3D TAC [B]", sim.viewMode == ViewMode::Tac3D, {115, 235, 255, 255})) {
        audio.playClick();
        sim.viewMode = ViewMode::Tac3D;
    }
    if (drawTacButton({x + 182.0f, y, 72.0f, 28.0f}, "DRONE", sim.viewMode == ViewMode::Drone, {115, 255, 165, 255})) {
        audio.playClick();
        sim.viewMode = ViewMode::Drone;
    }
    if (drawTacButton({x + 258.0f, y, 64.0f, 28.0f}, "LASE", sim.viewMode == ViewMode::Lase, {255, 195, 75, 255})) {
        audio.playClick();
        sim.viewMode = ViewMode::Lase;
    }
    if (drawTacButton({x + 326.0f, y, 52.0f, 28.0f}, sim.satelliteMap ? "SAT" : "MIL", sim.satelliteMap, {235, 200, 110, 255})) {
        audio.playClick();
        sim.satelliteMap = !sim.satelliteMap;
    }
}

void UIPanels::renderLeftModal(SimState& sim, TheaterAssets& assets, AudioEngine& audio, int screenW, int screenH) {
    (void)screenW;
    (void)screenH;
    if (sim.leftModal == LeftModal::None) return;

    Rectangle panel{140.0f, 46.0f, 415.0f, 470.0f};
    DrawRectangleRec(panel, {16, 22, 18, 246});
    DrawRectangleLinesEx(panel, 1.5f, {78, 104, 84, 240});

    // Close button
    if (drawTacButton({panel.x + panel.width - 32.0f, panel.y + 6.0f, 26.0f, 22.0f}, "X", false, {255, 95, 85, 255})) {
        audio.playClick();
        sim.leftModal = LeftModal::None;
        return;
    }

    if (sim.leftModal == LeftModal::Campaign) {
        DrawText("CAMPAIGN PHASE CONTROL (OIF 2003)", static_cast<int>(panel.x + 12), static_cast<int>(panel.y + 10), 13, {245, 205, 85, 255});
        DrawText("Control theater escalation from Kuwait Build-Up to Invasion & COIN.", static_cast<int>(panel.x + 12), static_cast<int>(panel.y + 28), 11, {175, 188, 172, 235});

        float cy = panel.y + 50.0f;
        int curPhaseIdx = static_cast<int>(sim.phase);
        for (size_t i = 0; i < CAMPAIGN_PHASES.size(); ++i) {
            const auto& cp = CAMPAIGN_PHASES[i];
            bool isCurrent = (static_cast<int>(i) == curPhaseIdx);
            bool isCompleted = (static_cast<int>(i) < curPhaseIdx);
            bool isNext = (static_cast<int>(i) == curPhaseIdx + 1);

            Rectangle card{panel.x + 10.0f, cy, panel.width - 20.0f, 76.0f};
            Color cBg = isCurrent ? Color{36, 52, 34, 240} : (isCompleted ? Color{22, 30, 24, 220} : Color{24, 28, 26, 230});
            Color cBdr = isCurrent ? Color{245, 205, 85, 255} : (isCompleted ? Color{85, 165, 105, 200} : Color{60, 72, 64, 200});
            DrawRectangleRec(card, cBg);
            DrawRectangleLinesEx(card, isCurrent ? 1.8f : 1.0f, cBdr);

            char hdr[128];
            std::snprintf(hdr, sizeof(hdr), "%s: %s %s", cp.code, cp.title,
                          isCurrent ? "[ACTIVE]" : (isCompleted ? "[COMPLETED]" : ""));
            DrawText(hdr, static_cast<int>(card.x + 8), static_cast<int>(card.y + 6), 12,
                     isCurrent ? Color{250, 215, 95, 255} : Color{215, 225, 210, 240});
            DrawText(cp.subtitle, static_cast<int>(card.x + 8), static_cast<int>(card.y + 22), 11, {145, 195, 155, 230});
            DrawText(cp.description, static_cast<int>(card.x + 8), static_cast<int>(card.y + 38), 10, {175, 182, 172, 220});

            if (isNext) {
                char btnLbl[64];
                std::snprintf(btnLbl, sizeof(btnLbl), "LAUNCH %s (+$%dM)", cp.code, cp.budgetGrant);
                if (drawTacButton({card.x + card.width - 158.0f, card.y + 6.0f, 150.0f, 24.0f}, btnLbl, true, {255, 200, 75, 255})) {
                    advanceCampaignPhase(sim, assets, audio, cp.phase);
                }
            }
            cy += 82.0f;
        }
    } else if (sim.leftModal == LeftModal::Fires) {
        DrawText("JOINT FIRES & STRIKE SUPPORT (BY MISSION TYPE)", static_cast<int>(panel.x + 12), static_cast<int>(panel.y + 10), 13, {255, 125, 80, 255});
        DrawText("Select mission type, then click any target on the map. Nearest asset fires.", static_cast<int>(panel.x + 12), static_cast<int>(panel.y + 28), 10, {180, 190, 175, 235});

        float fy = panel.y + 48.0f;
        for (const auto& fm : FIRES_MISSION_TYPES) {
            bool active = (sim.activeFireMissionId == fm.id);
            Rectangle row{panel.x + 10.0f, fy, panel.width - 20.0f, 42.0f};
            DrawRectangleRec(row, active ? Color{56, 32, 26, 240} : Color{24, 30, 26, 235});
            DrawRectangleLinesEx(row, active ? 1.8f : 1.0f, active ? fm.color : Color{62, 76, 66, 200});

            DrawText(fm.badge, static_cast<int>(row.x + 8), static_cast<int>(row.y + 7), 10, fm.color);
            DrawText(fm.name, static_cast<int>(row.x + 62), static_cast<int>(row.y + 6), 12, {235, 235, 225, 255});
            DrawText(fm.desc, static_cast<int>(row.x + 8), static_cast<int>(row.y + 24), 10, {168, 178, 165, 225});

            char costLbl[32];
            std::snprintf(costLbl, sizeof(costLbl), active ? "TARGETING" : "CALL ($%dM)", fm.cost);
            if (drawTacButton({row.x + row.width - 96.0f, row.y + 8.0f, 88.0f, 26.0f}, costLbl, active, fm.color)) {
                audio.playClick();
                sim.activeFireMissionId = active ? "" : fm.id;
            }
            fy += 46.0f;
        }
    } else if (sim.leftModal == LeftModal::AOs) {
        DrawText("IRAQ CITIES & SECTARIAN AOs (CLICK TO FOCUS)", static_cast<int>(panel.x + 12), static_cast<int>(panel.y + 10), 13, {115, 215, 255, 255});
        float cy = panel.y + 36.0f;
        size_t maxShow = std::min<size_t>(14, assets.cities.size());
        for (size_t i = 0; i < maxShow; ++i) {
            const auto& c = assets.cities[i];
            char line[128];
            std::snprintf(line, sizeof(line), "%-11s | %s | Ctrl: %3.0f%% | Stab: %3.0f%% | %u blds",
                          c.name.c_str(), c.sect.c_str(), c.control, c.stability, c.bldCount);
            if (drawTacButton({panel.x + 10.0f, cy, panel.width - 20.0f, 28.0f}, line, static_cast<int>(i) == sim.selectedCityIdx)) {
                audio.playClick();
                sim.selectedCityIdx = static_cast<int>(i);
                sim.camX = c.x;
                sim.camY = c.y;
                if (sim.viewMode == ViewMode::Map2D && sim.zoom < 6.0f) sim.zoom = 8.0f;
            }
            cy += 30.0f;
        }
    } else if (sim.leftModal == LeftModal::Logistics) {
        DrawText("THEATER LOGISTICS & FOB RESUPPLY", static_cast<int>(panel.x + 12), static_cast<int>(panel.y + 10), 13, {155, 235, 145, 255});
        DrawText("Maintain MSR Tampa & supply convoys to Forward Operating Bases.", static_cast<int>(panel.x + 12), static_cast<int>(panel.y + 30), 11, {175, 190, 175, 235});
        if (drawTacButton({panel.x + 12.0f, panel.y + 56.0f, panel.width - 24.0f, 34.0f},
                          "DISPATCH EMERGENCY AIRLIFT RESUPPLY ($15M -> ALL FOBs +25% SUPPLY)", false, {130, 235, 155, 255})) {
            if (sim.budget >= 15) {
                sim.budget -= 15;
                for (auto& b : sim.bases) {
                    b.supply = std::min(100.0f, b.supply + 25.0f);
                    b.ammo   = std::min(100.0f, b.ammo + 25.0f);
                    b.fuel   = std::min(100.0f, b.fuel + 25.0f);
                }
                audio.playRadio();
                sim.pushLog("C-17 / CH-47 Theater Resupply completed (+25% Supply/Ammo/Fuel across all FOBs).");
            }
        }
    } else if (sim.leftModal == LeftModal::Ops) {
        DrawText("CIVIL-MILITARY (CIMIC) & COIN OPERATIONS", static_cast<int>(panel.x + 12), static_cast<int>(panel.y + 10), 13, {210, 175, 255, 255});
        const City& sc = assets.cities[std::clamp(sim.selectedCityIdx, 0, static_cast<int>(assets.cities.size()) - 1)];
        std::string hdr = "Target AO: " + sc.name + " (" + sc.sect + ")";
        DrawText(hdr.c_str(), static_cast<int>(panel.x + 12), static_cast<int>(panel.y + 34), 12, {235, 230, 195, 255});

        if (drawTacButton({panel.x + 12.0f, panel.y + 62.0f, panel.width - 24.0f, 34.0f},
                          "CERP INFRASTRUCTURE & POWER GRID RECONSTRUCTION ($12M)", false)) {
            if (sim.budget >= 12) {
                sim.budget -= 12;
                City& mc = assets.cities[std::clamp(sim.selectedCityIdx, 0, static_cast<int>(assets.cities.size()) - 1)];
                mc.stability = std::min(100.0f, mc.stability + 14.0f);
                mc.insurgentActivity = std::max(0.0f, mc.insurgentActivity - 8.0f);
                audio.playClick();
                sim.pushLog("CERP Reconstruction project funded in " + mc.name + " (+14% Stability).");
            }
        }
        if (drawTacButton({panel.x + 12.0f, panel.y + 104.0f, panel.width - 24.0f, 34.0f},
                          "SONS OF IRAQ / TRIBAL AWAKENING OUTREACH ($18M)", false)) {
            if (sim.budget >= 18) {
                sim.budget -= 18;
                City& mc = assets.cities[std::clamp(sim.selectedCityIdx, 0, static_cast<int>(assets.cities.size()) - 1)];
                mc.intel = std::min(100.0f, mc.intel + 22.0f);
                mc.control = std::min(100.0f, mc.control + 15.0f);
                audio.playRadio();
                sim.pushLog("Tribal Awakening partnership established in " + mc.name + " (+22% Intel, +15% Control).");
            }
        }
    }
}

void UIPanels::renderBottomDock(SimState& sim, TheaterAssets& assets, AudioEngine& audio, int screenW, int screenH) {
    float dockH = sim.bottomDockExpanded ? 215.0f : 34.0f;
    float dockY = screenH - dockH;

    DrawRectangle(0, static_cast<int>(dockY), screenW, static_cast<int>(dockH), {14, 19, 16, 246});
    DrawLine(0, static_cast<int>(dockY), screenW, static_cast<int>(dockY), {66, 88, 72, 240});

    // Bottom Dock Tab Buttons
    struct TabInfo { BottomTab tab; const char* label; };
    static constexpr TabInfo tabs[] = {
        {BottomTab::Forces,   "[1] FORCES & OOB"},
        {BottomTab::Bases,    "[2] 3D BASES & GRID BUILDER"},
        {BottomTab::Intel,    "[3] INTEL & TICKER"},
        {BottomTab::TechROE,  "[4] DOCTRINE & ROE"},
        {BottomTab::Settings, "[5] KEYBINDINGS & AUDIO"},
    };

    float tx = 10.0f;
    for (const auto& tb : tabs) {
        if (drawTacButton({tx, dockY + 4.0f, 195.0f, 26.0f}, tb.label, sim.bottomTab == tb.tab && sim.bottomDockExpanded)) {
            audio.playClick();
            if (sim.bottomTab == tb.tab && sim.bottomDockExpanded) {
                sim.bottomDockExpanded = false;
            } else {
                sim.bottomTab = tb.tab;
                sim.bottomDockExpanded = true;
            }
        }
        tx += 200.0f;
    }

    // MGRS & Camera Elevation readout on right of tab bar
    std::string mgrs = toMGRS(sim.camX, sim.camY);
    float elev = assets.elevAtWorld(sim.camX, sim.camY);
    char coordBuf[128];
    std::snprintf(coordBuf, sizeof(coordBuf), "MGRS: %s | ELEV: %.0fm", mgrs.c_str(), elev);
    DrawText(coordBuf, screenW - 295, static_cast<int>(dockY + 11), 11, {155, 215, 165, 240});

    if (!sim.bottomDockExpanded) return;

    float contentY = dockY + 36.0f;

    if (sim.bottomTab == BottomTab::Forces) {
        DrawText("COALITION ORDER OF BATTLE (CLICK TO SELECT, RIGHT-CLICK MAP TO MOVE)", 14, static_cast<int>(contentY), 11, {145, 215, 255, 245});
        float ux = 14.0f;
        float uy = contentY + 18.0f;
        for (const auto& u : sim.units) {
            if (u.faction != UnitFaction::Coalition) continue;
            char uLbl[96];
            std::snprintf(uLbl, sizeof(uLbl), "%s [%.0f%%]", u.name.c_str(), u.strength);
            if (drawTacButton({ux, uy, 290.0f, 26.0f}, uLbl, u.id == sim.selectedUnitId, {115, 205, 255, 255})) {
                audio.playClick();
                sim.selectedUnitId = u.id;
                sim.camX = u.x;
                sim.camY = u.y;
            }
            uy += 30.0f;
            if (uy > screenH - 32.0f) {
                uy = contentY + 18.0f;
                ux += 298.0f;
            }
        }
    } else if (sim.bottomTab == BottomTab::Bases) {
        // 1. FOB Selector Column (Left)
        DrawText("SELECT FOB:", 14, static_cast<int>(contentY), 11, {135, 235, 155, 255});
        float bx = 14.0f;
        float by = contentY + 16.0f;
        for (size_t i = 0; i < sim.bases.size(); ++i) {
            if (drawTacButton({bx, by, 175.0f, 24.0f}, sim.bases[i].name.c_str(), static_cast<int>(i) == sim.selectedBaseIdx)) {
                audio.playClick();
                sim.selectedBaseIdx = static_cast<int>(i);
                sim.camX = sim.bases[i].x;
                sim.camY = sim.bases[i].y;
            }
            by += 26.0f;
            if (by > screenH - 28.0f) {
                by = contentY + 16.0f;
                bx += 180.0f;
            }
        }

        // 2. Structure Palette (Middle)
        float px = 385.0f;
        DrawText("16x16 GRID STRUCTURE CATALOG (SNAPS TO 3D GROUND OR 2D GRID):", static_cast<int>(px), static_cast<int>(contentY), 11, {240, 215, 110, 255});

        // 3D Builder Mode, Rotate [R], Demolish [X] controls
        if (drawTacButton({px + 385.0f, contentY - 2.0f, 118.0f, 20.0f},
                          sim.baseBuilderActive ? "3D BUILD: ACTIVE" : "ENTER 3D BUILD",
                          sim.baseBuilderActive, {110, 245, 150, 255})) {
            audio.playClick();
            sim.viewMode = ViewMode::Tac3D;
            sim.baseBuilderActive = true;
            if (sim.selectedBaseIdx >= 0 && sim.selectedBaseIdx < static_cast<int>(sim.bases.size())) {
                sim.camX = sim.bases[sim.selectedBaseIdx].x;
                sim.camY = sim.bases[sim.selectedBaseIdx].y;
                sim.camDist = 11.5f;
            }
        }
        if (drawTacButton({px + 508.0f, contentY - 2.0f, 82.0f, 20.0f},
                          sim.buildRot ? "ROT: 90 [R]" : "ROT: 0 [R]", sim.buildRot != 0)) {
            audio.playClick();
            sim.buildRot = (sim.buildRot + 1) % 2;
        }
        if (drawTacButton({px + 595.0f, contentY - 2.0f, 95.0f, 20.0f}, "DEMOLISH [X]", sim.demolishMode, {255, 95, 85, 255})) {
            audio.playClick();
            sim.demolishMode = !sim.demolishMode;
        }

        float sx = px;
        float sy = contentY + 20.0f;
        for (const auto& def : BASE_STRUCT_CATALOG) {
            char sLbl[96];
            std::snprintf(sLbl, sizeof(sLbl), "%s ($%dM %dx%d)", def.name, def.cost, def.w, def.d);
            bool isSel = (!sim.demolishMode && sim.buildStructId == def.id);
            if (drawTacButton({sx, sy, 225.0f, 26.0f}, sLbl, isSel, def.color)) {
                audio.playClick();
                sim.buildStructId = def.id;
                sim.demolishMode = false;
                sim.baseBuilderActive = true;
            }
            sy += 29.0f;
            if (sy > screenH - 28.0f) {
                sy = contentY + 20.0f;
                sx += 230.0f;
            }
        }

        // 3. Interactive 16x16 2D Schematic Grid on Right of Dock (synced live with 3D Base Builder!)
        if (sim.selectedBaseIdx >= 0 && sim.selectedBaseIdx < static_cast<int>(sim.bases.size())) {
            Base& b = sim.bases[sim.selectedBaseIdx];
            float gridPx = 152.0f;
            float gx0 = static_cast<float>(screenW) - gridPx - 24.0f;
            float gy0 = contentY + 16.0f;
            float cellPx = gridPx / BASE_GRID_N;

            Rectangle gRect{gx0, gy0, gridPx, gridPx};
            DrawRectangleRec(gRect, {22, 30, 24, 255});
            for (int i = 0; i <= BASE_GRID_N; ++i) {
                DrawLineV({gx0 + i * cellPx, gy0}, {gx0 + i * cellPx, gy0 + gridPx}, {52, 68, 56, 180});
                DrawLineV({gx0, gy0 + i * cellPx}, {gx0 + gridPx, gy0 + i * cellPx}, {52, 68, 56, 180});
            }
            for (const auto& st : b.layout) {
                const BaseStructDef* def = findStructDef(st.kind);
                int w = (st.rot % 2) ? def->d : def->w;
                int d = (st.rot % 2) ? def->w : def->d;
                DrawRectangleRec({gx0 + st.gx * cellPx + 1, gy0 + st.gz * cellPx + 1, w * cellPx - 2, d * cellPx - 2}, def->color);
            }

            // Mouse hover & click directly on 2D schematic grid
            Vector2 mp = GetMousePosition();
            if (CheckCollisionPointRec(mp, gRect)) {
                float u = (mp.x - gx0) / gridPx - 0.5f;
                float v = (mp.y - gy0) / gridPx - 0.5f;
                GridSnapResult snap = bldGridSnap(u, v, sim.buildStructId, sim.buildRot);
                bool ok = b.canPlace(snap.gx, snap.gz, snap.w, snap.d);
                DrawRectangleRec({gx0 + snap.gx * cellPx, gy0 + snap.gz * cellPx, snap.w * cellPx, snap.d * cellPx},
                                 ok ? Color{80, 245, 130, 140} : Color{255, 75, 75, 140});
                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                    if (sim.demolishMode) {
                        int dgx = std::clamp(static_cast<int>((mp.x - gx0) / cellPx), 0, BASE_GRID_N - 1);
                        int dgz = std::clamp(static_cast<int>((mp.y - gy0) / cellPx), 0, BASE_GRID_N - 1);
                        std::erase_if(b.layout, [&](const PlacedStructure& ps) {
                            const BaseStructDef* d = findStructDef(ps.kind);
                            int pw = (ps.rot % 2) ? d->d : d->w;
                            int pd = (ps.rot % 2) ? d->w : d->d;
                            return dgx >= ps.gx && dgx < ps.gx + pw && dgz >= ps.gz && dgz < ps.gz + pd;
                        });
                        b.recalcStats();
                        audio.playPlace();
                    } else if (ok) {
                        const BaseStructDef* def = findStructDef(sim.buildStructId);
                        if (sim.budget >= def->cost) {
                            sim.budget -= def->cost;
                            b.layout.push_back({sim.buildStructId, snap.gx, snap.gz, sim.buildRot});
                            b.recalcStats();
                            audio.playPlace();
                            sim.pushLog(std::string("Constructed ") + def->name + " at " + b.name + ".");
                        }
                    }
                }
            }
        }
    } else if (sim.bottomTab == BottomTab::Intel) {
        DrawText("CENTCOM J2 SITREP & LIVE OPERATIONS LOG:", 14, static_cast<int>(contentY), 11, {245, 210, 95, 255});
        float ly = contentY + 18.0f;
        for (size_t i = 0; i < std::min<size_t>(8, sim.tickerLog.size()); ++i) {
            DrawText(sim.tickerLog[i].c_str(), 14, static_cast<int>(ly), 11,
                     (i == 0) ? Color{140, 245, 165, 255} : Color{185, 196, 182, 230});
            ly += 18.0f;
        }
    } else if (sim.bottomTab == BottomTab::TechROE) {
        DrawText("THEATER DOCTRINE, counter-IED TECH & RULES OF ENGAGEMENT:", 14, static_cast<int>(contentY), 11, {195, 175, 255, 255});
        if (drawTacButton({14.0f, contentY + 22.0f, 320.0f, 32.0f}, "UP-ARMOR HMMWV / MRAP FIELDING PROGRAM ($25M)", false)) {
            if (sim.budget >= 25) {
                sim.budget -= 25;
                sim.coalitionMorale = std::min(100.0f, sim.coalitionMorale + 6.0f);
                audio.playClick();
                sim.pushLog("M1114 Up-Armored HMMWV & CREW Duke Counter-IED jammers fielded (+6% Morale).");
            }
        }
        if (drawTacButton({348.0f, contentY + 22.0f, 320.0f, 32.0f}, "BIOMETRIC BATS/HIIDE CHECKPOINT NETWORK ($20M)", false)) {
            if (sim.budget >= 20) {
                sim.budget -= 20;
                for (auto& c : assets.cities) c.intel = std::min(100.0f, c.intel + 10.0f);
                audio.playClick();
                sim.pushLog("Theater-wide biometric enrollment & HUMINT fusion network deployed (+10% Intel).");
            }
        }
    } else if (sim.bottomTab == BottomTab::Settings) {
        DrawText("KEYBOARD SHORTCUTS (CLICK ANY BINDING TO REBIND) & AUDIO ENGINE:", 14, static_cast<int>(contentY), 11, {135, 235, 165, 255});
        struct BindRow { const char* name; int* keyPtr; int slot; };
        BindRow rows[] = {
            {"Pan Up (+Arrow Up)",    &sim.keys.panUp,        0},
            {"Pan Down (+Arrow Dn)",  &sim.keys.panDown,      1},
            {"Pan Left (+Arrow Lt)",  &sim.keys.panLeft,      2},
            {"Pan Right (+Arrow Rt)", &sim.keys.panRight,     3},
            {"Orbit Left",            &sim.keys.rotLeft,      4},
            {"Orbit Right",           &sim.keys.rotRight,     5},
            {"Campaign Menu",         &sim.keys.campaignMenu, 6},
            {"Fires Menu",            &sim.keys.firesMenu,    7},
            {"3D Tac / Base Build",   &sim.keys.toggle3D,     8},
            {"Rotate Structure",      &sim.keys.rotateStruct, 9},
            {"Lase Designator",       &sim.keys.toggleLase,  10},
            {"Pause / Resume",        &sim.keys.pauseToggle, 11},
        };

        // Check if waiting for key press
        if (sim.keys.rebindingSlot >= 0) {
            int pressed = GetKeyPressed();
            if (pressed > 0 && pressed != KEY_ESCAPE) {
                for (auto& r : rows) {
                    if (r.slot == sim.keys.rebindingSlot) {
                        *r.keyPtr = pressed;
                        break;
                    }
                }
                sim.keys.rebindingSlot = -1;
                audio.playClick();
            }
        }

        float kx = 14.0f;
        float ky = contentY + 20.0f;
        for (const auto& r : rows) {
            bool waiting = (sim.keys.rebindingSlot == r.slot);
            char lbl[64];
            std::snprintf(lbl, sizeof(lbl), "%s: [%s]", r.name, waiting ? "PRESS KEY..." : keyName(*r.keyPtr));
            if (drawTacButton({kx, ky, 225.0f, 26.0f}, lbl, waiting, {255, 215, 80, 255})) {
                audio.playClick();
                sim.keys.rebindingSlot = waiting ? -1 : r.slot;
            }
            ky += 30.0f;
            if (ky > screenH - 28.0f) {
                ky = contentY + 20.0f;
                kx += 235.0f;
            }
        }

        // Audio mute & volume controls
        if (drawTacButton({740.0f, contentY + 20.0f, 165.0f, 28.0f},
                          audio.muted ? "AUDIO: MUTED" : "AUDIO: ACTIVE", !audio.muted)) {
            audio.muted = !audio.muted;
            audio.playClick();
        }
        if (drawTacButton({740.0f, contentY + 54.0f, 165.0f, 28.0f}, "TEST A-10 GAU-8 BRRRT", false, {255, 155, 70, 255})) {
            audio.playCasBurst();
        }
    }
}

void UIPanels::renderAndInteract(SimState& sim, TheaterAssets& assets, AudioEngine& audio, int screenW, int screenH) {
    renderTopBar(sim, assets, audio, screenW);
    renderLeftRail(sim, assets, audio);
    renderViewSwitcher(sim, audio, screenW);
    renderLeftModal(sim, assets, audio, screenW, screenH);
    renderBottomDock(sim, assets, audio, screenW, screenH);
}

} // namespace itc
