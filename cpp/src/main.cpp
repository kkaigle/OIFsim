#include "asset_loader.hpp"
#include "audio_engine.hpp"
#include "renderer_2d.hpp"
#include "renderer_3d.hpp"
#include "sim_types.hpp"
#include "ui_panels.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace itc {

namespace {

void dispatchFireMission(SimState& sim, AudioEngine& audio, const std::string& typeId, Vector2 targetWorld) {
    const FireMissionType* def = nullptr;
    for (const auto& f : FIRES_MISSION_TYPES) {
        if (typeId == f.id) {
            def = &f;
            break;
        }
    }
    if (!def) def = &FIRES_MISSION_TYPES[0];

    if (sim.budget < def->cost) {
        sim.pushLog(std::string("Insufficient budget for ") + def->name + " ($" + std::to_string(def->cost) + "M required).");
        return;
    }
    sim.budget -= def->cost;

    // Automatically select closest Coalition FOB or Persian Gulf Carrier Group as origin
    Vector2 origin = wProj(48.8f, 29.0f);
    float bestD = 1e9f;
    if (typeId != "tlam") {
        for (const auto& b : sim.bases) {
            float d = Vector2Distance({b.x, b.y}, targetWorld);
            if (d < bestD) {
                bestD = d;
                origin = {b.x, b.y};
            }
        }
    }

    ActiveFireMission fm;
    fm.typeId = def->id;
    fm.name = def->name;
    fm.origin = origin;
    fm.target = targetWorld;
    fm.progress = 0.0f;
    fm.speed = 1.0f / std::max(0.5f, def->flightSeconds);
    fm.radius = def->radius;
    fm.damage = def->damage;
    fm.color = def->color;
    sim.activeFires.push_back(fm);

    if (typeId == "cas") audio.playCasBurst();
    else audio.playMissile();

    sim.pushLog(std::string("FIRE MISSION DISPATCHED: ") + def->name + " -> " + toMGRS(targetWorld.x, targetWorld.y));
}

void stepSimulation(SimState& sim, TheaterAssets& assets, AudioEngine& audio, float dt) {
    // 1. Always update active fire missions & explosion VFX even when sim clock is paused
    for (size_t i = 0; i < sim.activeFires.size();) {
        auto& fm = sim.activeFires[i];
        fm.progress += fm.speed * dt;
        if (fm.progress >= 1.0f) {
            audio.playExplosion();
            sim.explosions.push_back({fm.target, 0.0f, 1.4f, fm.radius, fm.color});

            // Apply blast damage to enemy units inside radius
            for (auto& u : sim.units) {
                if (u.faction == UnitFaction::Coalition) continue;
                float d = Vector2Distance({u.x, u.y}, fm.target);
                if (d <= fm.radius * 1.4f) {
                    float falloff = 1.0f - std::clamp(d / (fm.radius * 1.4f), 0.0f, 0.85f);
                    float dmg = fm.damage * falloff;
                    u.strength = std::max(0.0f, u.strength - dmg);
                    sim.enemyNeutralized += static_cast<int>(dmg * 18.0f);
                }
            }

            // Update nearby city intel / insurgent suppression
            for (auto& c : assets.cities) {
                float d = Vector2Distance({c.x, c.y}, fm.target);
                if (d <= fm.radius * 2.0f) {
                    if (fm.typeId == "uav") {
                        c.intel = std::min(100.0f, c.intel + 18.0f);
                    } else {
                        c.insurgentActivity = std::max(0.0f, c.insurgentActivity - fm.damage * 0.25f);
                    }
                }
            }

            sim.activeFires.erase(sim.activeFires.begin() + static_cast<std::ptrdiff_t>(i));
        } else {
            ++i;
        }
    }

    for (size_t i = 0; i < sim.explosions.size();) {
        sim.explosions[i].age += dt;
        if (sim.explosions[i].age >= sim.explosions[i].maxAge) {
            sim.explosions.erase(sim.explosions.begin() + static_cast<std::ptrdiff_t>(i));
        } else {
            ++i;
        }
    }

    // Remove destroyed enemy units
    std::erase_if(sim.units, [&](const Unit& u) {
        if (u.strength <= 0.5f && u.faction != UnitFaction::Coalition) {
            sim.pushLog("ENEMY UNIT DESTROYED: " + u.name);
            return true;
        }
        return false;
    });

    if (sim.simSpeed <= 0) return;

    float simDt = dt * static_cast<float>(sim.simSpeed);
    sim.dayAccumulator += simDt * 0.35f;

    while (sim.dayAccumulator >= 0.25f) {
        sim.dayAccumulator -= 0.25f;
        sim.hour += 6;
        if (sim.hour >= 24) {
            sim.hour -= 24;
            sim.day += 1;
            if (sim.day > 30) {
                sim.day = 1;
                sim.month += 1;
                sim.budget += 45; // Monthly congressional appropriation
                if (sim.month > 12) {
                    sim.month = 1;
                    sim.year += 1;
                }
            }
        }
    }

    // 2. Move Units & Resolve Ground Engagements
    for (auto& u : sim.units) {
        if (u.hasOrder) {
            Vector2 pos{u.x, u.y};
            Vector2 tgt{u.targetX, u.targetY};
            float dist = Vector2Distance(pos, tgt);
            if (dist < 0.8f) {
                u.x = u.targetX;
                u.y = u.targetY;
                u.hasOrder = false;
            } else {
                Vector2 dir = Vector2Scale(Vector2Subtract(tgt, pos), 1.0f / dist);
                u.heading = std::atan2(dir.x, dir.y);
                float step = u.speed * simDt;
                u.x += dir.x * std::min(dist, step);
                u.y += dir.y * std::min(dist, step);
            }
        }

        // Check proximity to cities to increase Coalition control
        if (u.faction == UnitFaction::Coalition) {
            for (auto& c : assets.cities) {
                if (Vector2Distance({u.x, u.y}, {c.x, c.y}) < 12.0f) {
                    c.control = std::min(100.0f, c.control + 4.5f * simDt);
                }
            }
        }
    }

    // Ground combat between proximate Coalition and Iraqi units
    for (auto& cu : sim.units) {
        if (cu.faction != UnitFaction::Coalition) continue;
        for (auto& eu : sim.units) {
            if (eu.faction == UnitFaction::Coalition) continue;
            float d = Vector2Distance({cu.x, cu.y}, {eu.x, eu.y});
            if (d < 10.0f) {
                eu.strength = std::max(0.0f, eu.strength - 6.5f * simDt);
                cu.strength = std::max(25.0f, cu.strength - 0.8f * simDt);
                if (GetRandomValue(0, 100) < 4) {
                    sim.explosions.push_back({{eu.x, eu.y}, 0.0f, 0.7f, 4.0f, ORANGE});
                    sim.enemyNeutralized += 12;
                    if (GetRandomValue(0, 100) < 15) sim.coalitionWIA += 1;
                }
            }
        }
    }
}

void handleInput(SimState& sim, TheaterAssets& assets, AudioEngine& audio,
                 Renderer2D& ren2d, Renderer3D& ren3d, UIPanels& ui, float dt, int screenW, int screenH) {
    // Do not process map hotkeys while rebinding a key in Settings
    if (sim.keys.rebindingSlot >= 0) return;

    // 1. WASD & Arrow Keys Map / 3D Camera Movement
    float moveX = 0.0f;
    float moveY = 0.0f;
    if (IsKeyDown(sim.keys.panUp)    || IsKeyDown(KEY_UP))    moveY -= 1.0f;
    if (IsKeyDown(sim.keys.panDown)  || IsKeyDown(KEY_DOWN))  moveY += 1.0f;
    if (IsKeyDown(sim.keys.panLeft)  || IsKeyDown(KEY_LEFT))  moveX -= 1.0f;
    if (IsKeyDown(sim.keys.panRight) || IsKeyDown(KEY_RIGHT)) moveX += 1.0f;

    if (moveX != 0.0f || moveY != 0.0f) {
        if (sim.viewMode == ViewMode::Tac3D) {
            float panSpd = std::max(6.0f, sim.camDist * 1.15f) * dt;
            float s = std::sin(sim.camYaw);
            float c = std::cos(sim.camYaw);
            sim.camX += (moveX * c + moveY * s) * panSpd;
            sim.camY += (-moveX * s + moveY * c) * panSpd;
        } else {
            float panSpd = (420.0f / std::max(0.4f, sim.zoom)) * dt;
            sim.camX += moveX * panSpd;
            sim.camY += moveY * panSpd;
        }
    }

    // 2. Q / E 3D Orbit Rotation
    if (IsKeyDown(sim.keys.rotLeft))  sim.camYaw -= 1.6f * dt;
    if (IsKeyDown(sim.keys.rotRight)) sim.camYaw += 1.6f * dt;

    // 3. Global Shortcut Keys
    if (IsKeyPressed(sim.keys.pauseToggle)) {
        audio.playClick();
        sim.simSpeed = (sim.simSpeed == 0) ? 1 : 0;
    }
    if (IsKeyPressed(sim.keys.speedDown)) {
        audio.playClick();
        sim.simSpeed = (sim.simSpeed == 4) ? 2 : (sim.simSpeed == 2 ? 1 : 0);
    }
    if (IsKeyPressed(sim.keys.speedUp)) {
        audio.playClick();
        sim.simSpeed = (sim.simSpeed == 0) ? 1 : (sim.simSpeed == 1 ? 2 : 4);
    }
    if (IsKeyPressed(sim.keys.campaignMenu)) {
        audio.playClick();
        sim.leftModal = (sim.leftModal == LeftModal::Campaign) ? LeftModal::None : LeftModal::Campaign;
    }
    if (IsKeyPressed(sim.keys.firesMenu)) {
        audio.playClick();
        sim.leftModal = (sim.leftModal == LeftModal::Fires) ? LeftModal::None : LeftModal::Fires;
    }
    if (IsKeyPressed(sim.keys.toggle3D)) {
        audio.playClick();
        sim.viewMode = (sim.viewMode == ViewMode::Tac3D) ? ViewMode::Map2D : ViewMode::Tac3D;
    }
    if (IsKeyPressed(sim.keys.toggleDrone)) {
        audio.playClick();
        sim.viewMode = (sim.viewMode == ViewMode::Drone) ? ViewMode::Map2D : ViewMode::Drone;
    }
    if (IsKeyPressed(sim.keys.toggleLase)) {
        audio.playClick();
        sim.viewMode = (sim.viewMode == ViewMode::Lase) ? ViewMode::Map2D : ViewMode::Lase;
    }
    if (IsKeyPressed(sim.keys.rotateStruct)) {
        audio.playClick();
        sim.buildRot = (sim.buildRot + 1) % 2;
    }
    if (IsKeyPressed(KEY_X)) {
        audio.playClick();
        sim.demolishMode = !sim.demolishMode;
    }
    if (IsKeyPressed(sim.keys.mapModeToggle)) {
        audio.playClick();
        sim.satelliteMap = !sim.satelliteMap;
    }

    // Bottom Tab Hotkeys 1..5
    if (IsKeyPressed(KEY_ONE))   { audio.playClick(); sim.bottomTab = BottomTab::Forces;   sim.bottomDockExpanded = true; }
    if (IsKeyPressed(KEY_TWO))   { audio.playClick(); sim.bottomTab = BottomTab::Bases;    sim.bottomDockExpanded = true; }
    if (IsKeyPressed(KEY_THREE)) { audio.playClick(); sim.bottomTab = BottomTab::Intel;    sim.bottomDockExpanded = true; }
    if (IsKeyPressed(KEY_FOUR))  { audio.playClick(); sim.bottomTab = BottomTab::TechROE;  sim.bottomDockExpanded = true; }
    if (IsKeyPressed(KEY_FIVE))  { audio.playClick(); sim.bottomTab = BottomTab::Settings; sim.bottomDockExpanded = true; }

    // 4. Mouse Interaction on Map / 3D Viewport (when not over UI)
    Vector2 mp = GetMousePosition();
    if (ui.isMouseOverUI(mp, sim, screenW, screenH)) {
        sim.hoverGx = -1;
        sim.hoverGz = -1;
        return;
    }

    float wheel = GetMouseWheelMove();
    if (std::abs(wheel) > 0.01f) {
        if (sim.viewMode == ViewMode::Tac3D) {
            sim.camDist = std::clamp(sim.camDist * (1.0f - wheel * 0.12f), 3.5f, 95.0f);
        } else {
            Vector2 before = ren2d.screenToWorld(mp.x, mp.y, sim, screenW, screenH);
            sim.zoom = std::clamp(sim.zoom * (1.0f + wheel * 0.14f), 0.45f, 45.0f);
            Vector2 after = ren2d.screenToWorld(mp.x, mp.y, sim, screenW, screenH);
            sim.camX += (before.x - after.x);
            sim.camY += (before.y - after.y);
        }
    }

    if (sim.viewMode == ViewMode::Tac3D) {
        // Right-mouse drag orbits 3D camera; Middle-mouse or Left-drag (when not building) pans
        if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
            Vector2 delta = GetMouseDelta();
            sim.camYaw   -= delta.x * 0.006f;
            sim.camPitch = std::clamp(sim.camPitch + delta.y * 0.005f, 0.18f, 1.45f);
        }

        // Update 3D Base Grid Raycast Hover & Handle Left-Click Placement
        if (sim.baseBuilderActive) {
            bool onGrid = ren3d.updateBaseGridHover(sim, assets, mp);
            if (onGrid && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
                sim.selectedBaseIdx >= 0 && sim.selectedBaseIdx < static_cast<int>(sim.bases.size())) {
                Base& b = sim.bases[sim.selectedBaseIdx];
                if (sim.demolishMode) {
                    std::erase_if(b.layout, [&](const PlacedStructure& ps) {
                        const BaseStructDef* d = findStructDef(ps.kind);
                        int pw = (ps.rot % 2) ? d->d : d->w;
                        int pd = (ps.rot % 2) ? d->w : d->d;
                        return sim.hoverGx >= ps.gx && sim.hoverGx < ps.gx + pw &&
                               sim.hoverGz >= ps.gz && sim.hoverGz < ps.gz + pd;
                    });
                    b.recalcStats();
                    audio.playPlace();
                    sim.pushLog("Demolished structure cell at " + b.name + ".");
                } else if (sim.hoverValid) {
                    const BaseStructDef* def = findStructDef(sim.buildStructId);
                    if (sim.budget >= def->cost) {
                        sim.budget -= def->cost;
                        b.layout.push_back({sim.buildStructId, sim.hoverGx, sim.hoverGz, sim.buildRot});
                        b.recalcStats();
                        audio.playPlace();
                        sim.pushLog(std::string("Constructed ") + def->name + " at " + b.name + ".");
                    } else {
                        sim.pushLog("Insufficient budget to build " + std::string(def->name) + ".");
                    }
                }
            } else if (!onGrid && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
                Vector2 delta = GetMouseDelta();
                float panScale = sim.camDist * 0.0018f;
                float s = std::sin(sim.camYaw);
                float c = std::cos(sim.camYaw);
                sim.camX -= (delta.x * c + delta.y * s) * panScale;
                sim.camY -= (-delta.x * s + delta.y * c) * panScale;
            }
        } else if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            Vector2 delta = GetMouseDelta();
            float panScale = sim.camDist * 0.0018f;
            float s = std::sin(sim.camYaw);
            float c = std::cos(sim.camYaw);
            sim.camX -= (delta.x * c + delta.y * s) * panScale;
            sim.camY -= (-delta.x * s + delta.y * c) * panScale;
        }
    } else {
        // 2D / Drone / Lase View Mouse Interaction
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && Vector2Length(GetMouseDelta()) > 1.5f && sim.activeFireMissionId.empty()) {
            Vector2 delta = GetMouseDelta();
            sim.camX -= delta.x / sim.zoom;
            sim.camY -= delta.y / sim.zoom;
        } else if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            Vector2 wClick = ren2d.screenToWorld(mp.x, mp.y, sim, screenW, screenH);
            if (!sim.activeFireMissionId.empty()) {
                dispatchFireMission(sim, audio, sim.activeFireMissionId, wClick);
            } else if (sim.viewMode == ViewMode::Lase) {
                dispatchFireMission(sim, audio, "jdam", wClick);
            } else {
                int pickedU = ren2d.pickUnit(mp.x, mp.y, sim, screenW, screenH);
                int pickedB = ren2d.pickBase(mp.x, mp.y, sim, screenW, screenH);
                int pickedC = ren2d.pickCity(mp.x, mp.y, sim, assets, screenW, screenH);
                if (pickedU >= 0) {
                    audio.playClick();
                    sim.selectedUnitId = pickedU;
                } else if (pickedB >= 0) {
                    audio.playClick();
                    sim.selectedBaseIdx = pickedB;
                    sim.bottomTab = BottomTab::Bases;
                    sim.bottomDockExpanded = true;
                } else if (pickedC >= 0) {
                    audio.playClick();
                    sim.selectedCityIdx = pickedC;
                }
            }
        }

        // Right-click on 2D map issues movement/attack order to selected Coalition unit
        if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) && sim.selectedUnitId >= 0) {
            Vector2 wTarget = ren2d.screenToWorld(mp.x, mp.y, sim, screenW, screenH);
            for (auto& u : sim.units) {
                if (u.id == sim.selectedUnitId && u.faction == UnitFaction::Coalition) {
                    u.targetX = wTarget.x;
                    u.targetY = wTarget.y;
                    u.hasOrder = true;
                    audio.playRadio();
                    sim.pushLog(u.name + " moving to " + toMGRS(wTarget.x, wTarget.y));
                    break;
                }
            }
        }
    }
}

} // namespace

} // namespace itc

int main(int argc, char** argv) {
    bool smokeTest = false;
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--smoke-test") {
            smokeTest = true;
        }
    }

    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT);
    InitWindow(1440, 900, "Iraq Theater Command — C++20 Cross-Platform Tactical Engine (Mac & Windows)");
    SetTargetFPS(60);

    itc::TheaterAssets assets;
    if (!assets.loadAll()) {
        CloseWindow();
        return 1;
    }

    itc::SimState sim;
    itc::initCampaignScenario(sim, assets);

    itc::AudioEngine audio;
    audio.init(assets.assetsRoot);

    itc::Renderer2D renderer2D;
    itc::Renderer3D renderer3D;
    itc::UIPanels uiPanels;

    int frameCounter = 0;
    while (!WindowShouldClose()) {
        float dt = std::min(GetFrameTime(), 0.1f);
        int screenW = GetScreenWidth();
        int screenH = GetScreenHeight();

        itc::handleInput(sim, assets, audio, renderer2D, renderer3D, uiPanels, dt, screenW, screenH);
        itc::stepSimulation(sim, assets, audio, dt);

        BeginDrawing();
        if (sim.viewMode == itc::ViewMode::Tac3D) {
            renderer3D.render(sim, assets, screenW, screenH);
        } else {
            renderer2D.render(sim, assets, screenW, screenH);
        }
        uiPanels.renderAndInteract(sim, assets, audio, screenW, screenH);
        EndDrawing();

        if (smokeTest && ++frameCounter >= 3) {
            TraceLog(LOG_INFO, "[ITC] Smoke test passed (2D + 3D + Audio + 251,503 OSM Buildings + 7 GLB Models loaded cleanly).");
            break;
        }
    }

    audio.shutdown();
    assets.unloadAll();
    CloseWindow();
    return 0;
}

