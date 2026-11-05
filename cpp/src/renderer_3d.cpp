#include "renderer_3d.hpp"
#include <rlgl.h>
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace itc {

namespace {

inline float terrainY(const TheaterAssets& assets, float wx, float wz) {
    // Convert elevation in meters to subtle 3D vertical relief
    return std::max(0.0f, assets.elevAtWorld(wx, wz) * 0.0025f);
}

} // namespace

Camera3D Renderer3D::buildCamera(const SimState& sim, const TheaterAssets& assets) const {
    float ty = terrainY(assets, sim.camX, sim.camY);
    Vector3 target{sim.camX, ty, sim.camY};
    float pitch = std::clamp(sim.camPitch, 0.18f, 1.45f);
    float dist  = std::clamp(sim.camDist, 3.5f, 95.0f);

    Vector3 offset{
        std::sin(sim.camYaw) * std::cos(pitch) * dist,
        std::sin(pitch) * dist,
        std::cos(sim.camYaw) * std::cos(pitch) * dist
    };

    Camera3D cam{};
    cam.target = target;
    cam.position = Vector3Add(target, offset);
    cam.up = {0.0f, 1.0f, 0.0f};
    cam.fovy = 45.0f;
    cam.projection = CAMERA_PERSPECTIVE;
    return cam;
}

bool Renderer3D::updateBaseGridHover(SimState& sim, const TheaterAssets& assets, Vector2 mousePos) const {
    sim.hoverGx = -1;
    sim.hoverGz = -1;
    sim.hoverValid = false;

    if (sim.selectedBaseIdx < 0 || sim.selectedBaseIdx >= static_cast<int>(sim.bases.size())) {
        return false;
    }

    const Base& b = sim.bases[sim.selectedBaseIdx];
    Camera3D cam = buildCamera(sim, assets);
    Ray ray = GetScreenToWorldRay(mousePos, cam);

    float baseY = terrainY(assets, b.x, b.y) + 0.02f;
    if (std::abs(ray.direction.y) < 1e-5f) return false;
    float t = (baseY - ray.position.y) / ray.direction.y;
    if (t <= 0.0f) return false;

    Vector3 hit = Vector3Add(ray.position, Vector3Scale(ray.direction, t));
    float u = (hit.x - b.x) / BASE_WORLD_SPAN;
    float v = (hit.z - b.y) / BASE_WORLD_SPAN;

    if (u < -0.52f || u > 0.52f || v < -0.52f || v > 0.52f) {
        return false;
    }

    if (sim.demolishMode) {
        int gx = std::clamp(static_cast<int>(std::floor((u + 0.5f) * BASE_GRID_N)), 0, BASE_GRID_N - 1);
        int gz = std::clamp(static_cast<int>(std::floor((v + 0.5f) * BASE_GRID_N)), 0, BASE_GRID_N - 1);
        sim.hoverGx = gx;
        sim.hoverGz = gz;
        sim.hoverW = 1;
        sim.hoverD = 1;
        sim.hoverValid = true;
        return true;
    }

    GridSnapResult snap = bldGridSnap(u, v, sim.buildStructId, sim.buildRot);
    sim.hoverGx = snap.gx;
    sim.hoverGz = snap.gz;
    sim.hoverW = snap.w;
    sim.hoverD = snap.d;
    sim.hoverValid = b.canPlace(snap.gx, snap.gz, snap.w, snap.d);
    return true;
}

void Renderer3D::drawModelScaled(const LoadedModelEntry& entry, Vector3 worldPos, Vector3 targetSize, float yawDeg, Color tint) const {
    if (!entry.valid) return;
    Vector3 scale{
        targetSize.x / entry.size.x,
        targetSize.y / entry.size.y,
        targetSize.z / entry.size.z
    };
    rlPushMatrix();
    rlTranslatef(worldPos.x, worldPos.y, worldPos.z);
    rlRotatef(yawDeg, 0.0f, 1.0f, 0.0f);
    rlScalef(scale.x, scale.y, scale.z);
    rlTranslatef(-entry.center.x, -entry.center.y, -entry.center.z);
    DrawModel(entry.model, {0.0f, 0.0f, 0.0f}, 1.0f, tint);
    rlPopMatrix();
}

void Renderer3D::drawStructure3D(const BaseStructDef& def, size_t catalogIdx, Vector3 pos, float cellW, float cellD,
                                 int rot, Color tintOverride, bool ghostMode, const TheaterAssets& assets) const {
    float h = def.h * 0.35f;
    Color col = (tintOverride.a > 0) ? tintOverride : def.color;

    // 1. If a custom .glb file exists in cpp/assets/models/<id>.glb, render it!
    if (catalogIdx < assets.mdlBaseStructs.size() && assets.mdlBaseStructs[catalogIdx].valid) {
        drawModelScaled(assets.mdlBaseStructs[catalogIdx], pos, {cellW * 0.9f, h, cellD * 0.9f}, rot * 90.0f, col);
        return;
    }

    // 2. Otherwise render detailed composite 3D tactical architecture per structure type
    std::string id = def.id;
    Vector3 center{pos.x, pos.y + h * 0.5f, pos.z};

    if (id == "tower") {
        // 4 legs + elevated armoured cab + roof
        float legW = cellW * 0.12f;
        float dx = cellW * 0.32f;
        float dz = cellD * 0.32f;
        DrawCube({pos.x - dx, pos.y + h * 0.4f, pos.z - dz}, legW, h * 0.8f, legW, col);
        DrawCube({pos.x + dx, pos.y + h * 0.4f, pos.z - dz}, legW, h * 0.8f, legW, col);
        DrawCube({pos.x - dx, pos.y + h * 0.4f, pos.z + dz}, legW, h * 0.8f, legW, col);
        DrawCube({pos.x + dx, pos.y + h * 0.4f, pos.z + dz}, legW, h * 0.8f, legW, col);
        DrawCube({pos.x, pos.y + h * 0.82f, pos.z}, cellW * 0.82f, h * 0.32f, cellD * 0.82f, col);
        DrawCubeWires({pos.x, pos.y + h * 0.82f, pos.z}, cellW * 0.82f, h * 0.32f, cellD * 0.82f, BLACK);
    } else if (id == "heli") {
        // Helipad concrete pad + yellow H marking
        DrawCube({pos.x, pos.y + 0.03f, pos.z}, cellW * 0.94f, 0.06f, cellD * 0.94f, col);
        DrawCubeWires({pos.x, pos.y + 0.03f, pos.z}, cellW * 0.94f, 0.06f, cellD * 0.94f, YELLOW);
        Color hCol = ghostMode ? col : Color{235, 200, 65, 255};
        DrawCube({pos.x - cellW * 0.18f, pos.y + 0.065f, pos.z}, cellW * 0.08f, 0.02f, cellD * 0.5f, hCol);
        DrawCube({pos.x + cellW * 0.18f, pos.y + 0.065f, pos.z}, cellW * 0.08f, 0.02f, cellD * 0.5f, hCol);
        DrawCube({pos.x, pos.y + 0.065f, pos.z}, cellW * 0.36f, 0.02f, cellD * 0.08f, hCol);
    } else if (id == "comms") {
        // SATCOM shelter + tall lattice mast + dish
        DrawCube({pos.x, pos.y + h * 0.18f, pos.z}, cellW * 0.7f, h * 0.36f, cellD * 0.7f, col);
        DrawCube({pos.x, pos.y + h * 0.55f, pos.z}, cellW * 0.14f, h * 0.9f, cellD * 0.14f, {160, 175, 185, col.a});
        DrawSphere({pos.x, pos.y + h * 0.95f, pos.z}, cellW * 0.28f, col);
    } else if (id == "toc") {
        // Tactical Operations Center bunker + sandbag revetment + antenna
        DrawCube(center, cellW * 0.88f, h, cellD * 0.88f, col);
        DrawCube({pos.x, pos.y + h + 0.06f, pos.z}, cellW * 0.65f, 0.12f, cellD * 0.65f, {72, 96, 68, col.a});
        DrawCubeWires(center, cellW * 0.88f, h, cellD * 0.88f, {28, 36, 26, 220});
    } else {
        // Barracks, HESCO, Motorpool, Ammo, Fuel, Med, DFAC, etc.
        DrawCube(center, cellW * 0.90f, h, cellD * 0.90f, col);
        DrawCubeWires(center, cellW * 0.90f, h, cellD * 0.90f, {30, 34, 28, 200});
    }
}

void Renderer3D::drawBaseCompound3D(const Base& b, bool isSelected, const SimState& sim, const TheaterAssets& assets) const {
    float by = terrainY(assets, b.x, b.y) + 0.01f;
    float span = BASE_WORLD_SPAN;
    float cell = span / BASE_GRID_N;
    float x0 = b.x - span * 0.5f;
    float z0 = b.y - span * 0.5f;

    // 1. Gravel/compacted desert pad
    DrawCube({b.x, by - 0.02f, b.y}, span * 1.04f, 0.04f, span * 1.04f, {62, 58, 46, 255});
    DrawCubeWires({b.x, by, b.y}, span * 1.04f, 0.05f, span * 1.04f,
                  isSelected ? Color{115, 240, 145, 255} : Color{85, 165, 105, 200});

    // 2. 16x16 Snap Grid Lines (highlighted when selected or in Base Builder mode)
    Color gridCol = (isSelected && sim.baseBuilderActive) ? Color{120, 235, 155, 145} : Color{95, 115, 95, 75};
    for (int i = 0; i <= BASE_GRID_N; ++i) {
        float gx = x0 + i * cell;
        float gz = z0 + i * cell;
        DrawLine3D({gx, by + 0.01f, z0}, {gx, by + 0.01f, z0 + span}, gridCol);
        DrawLine3D({x0, by + 0.01f, gz}, {x0 + span, by + 0.01f, gz}, gridCol);
    }

    // 3. Placed Base Structures
    for (const auto& st : b.layout) {
        const BaseStructDef* def = findStructDef(st.kind);
        size_t catIdx = static_cast<size_t>(def - BASE_STRUCT_CATALOG.data());
        int w = (st.rot % 2) ? def->d : def->w;
        int d = (st.rot % 2) ? def->w : def->d;
        Vector3 sc{
            x0 + (st.gx + w * 0.5f) * cell,
            by,
            z0 + (st.gz + d * 0.5f) * cell
        };
        drawStructure3D(*def, catIdx, sc, w * cell, d * cell, st.rot, {0, 0, 0, 0}, false, assets);
    }

    // 4. Interactive Ghost Preview when hovering the 16x16 grid in 3D
    if (isSelected && sim.baseBuilderActive && sim.hoverGx >= 0 && sim.hoverGz >= 0) {
        Vector3 hc{
            x0 + (sim.hoverGx + sim.hoverW * 0.5f) * cell,
            by + 0.015f,
            z0 + (sim.hoverGz + sim.hoverD * 0.5f) * cell
        };
        float hw = sim.hoverW * cell;
        float hd = sim.hoverD * cell;

        if (sim.demolishMode) {
            DrawCube(hc, hw * 0.95f, 0.25f, hd * 0.95f, {255, 65, 55, 130});
            DrawCubeWires(hc, hw, 0.35f, hd, {255, 80, 70, 255});
        } else {
            const BaseStructDef* def = findStructDef(sim.buildStructId);
            size_t catIdx = static_cast<size_t>(def - BASE_STRUCT_CATALOG.data());
            Color ghostCol = sim.hoverValid ? Color{75, 245, 125, 155} : Color{255, 65, 65, 165};
            drawStructure3D(*def, catIdx, hc, hw, hd, sim.buildRot, ghostCol, true, assets);
            DrawCube({hc.x, by + 0.02f, hc.z}, hw, 0.03f, hd,
                     sim.hoverValid ? Color{75, 245, 125, 95} : Color{255, 65, 65, 110});
        }
    }
}

void Renderer3D::drawCity3D(const City& c, const SimState& sim, const TheaterAssets& assets) const {
    float distToCam = Vector2Distance({c.x, c.y}, {sim.camX, sim.camY});
    if (distToCam > 55.0f) return;

    // 1. Render OSM Streets in 3D
    uint32_t sEnd = std::min<uint32_t>(c.streetOffset + c.streetCount, static_cast<uint32_t>(assets.streets.size()));
    for (uint32_t i = c.streetOffset; i < sEnd; ++i) {
        const StreetSeg& s = assets.streets[i];
        if (distToCam > 22.0f && s.cls >= 2) continue;
        float mx = (s.x1 + s.x2) * 0.5f;
        float mz = (s.y1 + s.y2) * 0.5f;
        if (std::abs(mx - sim.camX) > 32.0f || std::abs(mz - sim.camY) > 32.0f) continue;

        float y1 = terrainY(assets, s.x1, s.y1) + 0.015f;
        float y2 = terrainY(assets, s.x2, s.y2) + 0.015f;
        Color scol = (s.cls == 0) ? Color{165, 138, 86, 220}
                   : (s.cls == 1) ? Color{118, 108, 84, 185}
                                  : Color{84, 78, 66, 150};
        DrawLine3D({s.x1, y1, s.y1}, {s.x2, y2, s.y2}, scol);
    }

    if (!sim.show3DBuildings) return;

    // 2. Render OSM 3D Buildings (using .glb models for close hero buildings + fast 3D extrusions)
    uint32_t bEnd = std::min<uint32_t>(c.bldOffset + c.bldCount, static_cast<uint32_t>(assets.buildings.size()));
    for (uint32_t i = c.bldOffset; i < bEnd; ++i) {
        const OsmBuilding& b = assets.buildings[i];
        float dx = b.cx - sim.camX;
        float dz = b.cy - sim.camY;
        float d2 = dx * dx + dz * dz;
        if (d2 > 30.0f * 30.0f) continue;
        if (d2 > 14.0f * 14.0f && (i & 1)) continue; // Smooth distance LOD

        float by = terrainY(assets, b.cx, b.cy);
        float bh = std::max(0.08f, (b.levels ? b.levels * 0.055f : (0.09f + b.hNorm * 0.22f)));
        float bw = std::max(0.06f, b.ww);
        float bd = std::max(0.06f, b.wh);

        // Use real .glb 3D models for close prominent buildings
        if (d2 < 8.5f * 8.5f && (i % 7 == 0)) {
            const LoadedModelEntry* mdl = (b.levels >= 4 && assets.mdlBigRes.valid) ? &assets.mdlBigRes
                                        : (b.kind == 4 && assets.mdlRuin.valid)    ? &assets.mdlRuin
                                        : (assets.mdlHouse.valid)                  ? &assets.mdlHouse
                                                                                   : nullptr;
            if (mdl && mdl->valid) {
                drawModelScaled(*mdl, {b.cx, by, b.cy}, {bw, bh, bd}, b.angle * RAD2DEG, {225, 210, 185, 255});
                continue;
            }
        }

        Color bcol = (b.kind == 2) ? Color{168, 146, 118, 255}
                   : (b.levels >= 4) ? Color{156, 142, 122, 255}
                                     : Color{142, 126, 102, 255};
        DrawCube({b.cx, by + bh * 0.5f, b.cy}, bw, bh, bd, bcol);
        if (d2 < 12.0f * 12.0f) {
            DrawCubeWires({b.cx, by + bh * 0.5f, b.cy}, bw, bh, bd, {62, 54, 44, 180});
        }
    }
}

void Renderer3D::drawUnit3D(const Unit& u, bool isSelected, const TheaterAssets& assets) const {
    float uy = terrainY(assets, u.x, u.y);
    bool isCoal = (u.faction == UnitFaction::Coalition);
    Color tint = isCoal ? Color{235, 225, 195, 255} : Color{195, 165, 145, 255};

    // Render 3-vehicle tactical platoon formation in 3D using tank.glb / ifv.glb / hmmwv.glb
    const LoadedModelEntry* mdl = &assets.mdlHmmwv;
    Vector3 vSize{0.42f, 0.24f, 0.75f};
    if (u.category == UnitCategory::Armor && assets.mdlTank.valid) {
        mdl = &assets.mdlTank;
        vSize = {0.55f, 0.30f, 0.95f};
    } else if (u.category == UnitCategory::Mechanized && assets.mdlIfv.valid) {
        mdl = &assets.mdlIfv;
        vSize = {0.48f, 0.28f, 0.85f};
    }

    static constexpr float offsets[3][2] = {{0.0f, 0.0f}, {-0.65f, -0.55f}, {0.65f, -0.55f}};
    for (int k = 0; k < 3; ++k) {
        Vector3 vp{u.x + offsets[k][0], uy, u.y + offsets[k][1]};
        if (mdl && mdl->valid) {
            drawModelScaled(*mdl, vp, vSize, u.heading * RAD2DEG, tint);
        } else {
            DrawCube({vp.x, uy + 0.15f, vp.z}, vSize.x, vSize.y, vSize.z, isCoal ? BLUE : MAROON);
        }
    }

    if (isSelected) {
        DrawCircle3D({u.x, uy + 0.04f, u.y}, 1.35f, {1.0f, 0.0f, 0.0f}, 90.0f, {255, 230, 90, 255});
    }
}

void Renderer3D::render(const SimState& sim, const TheaterAssets& assets, int screenW, int screenH) {
    (void)screenW;
    (void)screenH;
    ClearBackground({22, 27, 30, 255});

    Camera3D cam = buildCamera(sim, assets);
    BeginMode3D(cam);

    // 1. Render 3D Elevation Terrain Mesh around Camera Target
    const int N = 36;
    const float patchSpan = 90.0f;
    const float step = patchSpan / N;
    float startX = std::floor((sim.camX - patchSpan * 0.5f) / step) * step;
    float startZ = std::floor((sim.camY - patchSpan * 0.5f) / step) * step;

    for (int gz = 0; gz < N; ++gz) {
        float z0 = startZ + gz * step;
        float z1 = z0 + step;
        for (int gx = 0; gx < N; ++gx) {
            float x0 = startX + gx * step;
            float x1 = x0 + step;
            float y00 = terrainY(assets, x0, z0);
            float y10 = terrainY(assets, x1, z0);
            float y01 = terrainY(assets, x0, z1);
            float y11 = terrainY(assets, x1, z1);

            float avgElev = assets.elevAtWorld((x0 + x1) * 0.5f, (z0 + z1) * 0.5f);
            Color tCol = (avgElev <= 0.0f) ? Color{20, 42, 58, 255}
                       : (avgElev < 150.0f) ? Color{56, 52, 38, 255}
                       : (avgElev < 600.0f) ? Color{74, 64, 45, 255}
                                            : Color{96, 80, 58, 255};

            DrawTriangle3D({x0, y00, z0}, {x0, y01, z1}, {x1, y10, z0}, tCol);
            DrawTriangle3D({x1, y10, z0}, {x0, y01, z1}, {x1, y11, z1}, tCol);

            if (sim.showGrid) {
                DrawLine3D({x0, y00 + 0.01f, z0}, {x1, y10 + 0.01f, z0}, {95, 110, 90, 45});
                DrawLine3D({x0, y00 + 0.01f, z0}, {x0, y01 + 0.01f, z1}, {95, 110, 90, 45});
            }
        }
    }

    // 2. Render Rivers & MSR Highways in 3D
    for (const auto& rv : assets.rivers) {
        for (size_t i = 1; i < rv.size(); ++i) {
            if (std::abs(rv[i].x - sim.camX) > 48.0f || std::abs(rv[i].y - sim.camY) > 48.0f) continue;
            float y1 = terrainY(assets, rv[i - 1].x, rv[i - 1].y) + 0.02f;
            float y2 = terrainY(assets, rv[i].x, rv[i].y) + 0.02f;
            DrawLine3D({rv[i - 1].x, y1, rv[i - 1].y}, {rv[i].x, y2, rv[i].y}, {38, 105, 155, 240});
        }
    }

    for (const auto& hw : assets.highways) {
        Color hcol = (hw.kind == "msr") ? Color{220, 170, 75, 220} : Color{145, 130, 95, 160};
        for (size_t i = 1; i < hw.pts.size(); ++i) {
            if (std::abs(hw.pts[i].x - sim.camX) > 48.0f || std::abs(hw.pts[i].y - sim.camY) > 48.0f) continue;
            float y1 = terrainY(assets, hw.pts[i - 1].x, hw.pts[i - 1].y) + 0.025f;
            float y2 = terrainY(assets, hw.pts[i].x, hw.pts[i].y) + 0.025f;
            DrawLine3D({hw.pts[i - 1].x, y1, hw.pts[i - 1].y}, {hw.pts[i].x, y2, hw.pts[i].y}, hcol);
        }
    }

    // 3. Render 3D Cities (OSM Streets + 3D Buildings + GLB Apartments/Houses/Ruins)
    for (const auto& c : assets.cities) {
        drawCity3D(c, sim, assets);
    }

    // 4. Render 3D Base Compounds (16x16 Grid + Placed Structures + Live Ghost Preview)
    for (size_t i = 0; i < sim.bases.size(); ++i) {
        if (Vector2Distance({sim.bases[i].x, sim.bases[i].y}, {sim.camX, sim.camY}) < 55.0f) {
            drawBaseCompound3D(sim.bases[i], static_cast<int>(i) == sim.selectedBaseIdx, sim, assets);
        }
    }

    // 5. Render 3D Units (Abrams, Bradleys, HMMWVs)
    if (sim.showUnits) {
        for (const auto& u : sim.units) {
            if (Vector2Distance({u.x, u.y}, {sim.camX, sim.camY}) < 55.0f) {
                drawUnit3D(u, u.id == sim.selectedUnitId, assets);
            }
        }
    }

    // 6. Render 3D Explosions
    for (const auto& ex : sim.explosions) {
        float t = std::clamp(ex.age / ex.maxAge, 0.0f, 1.0f);
        float ey = terrainY(assets, ex.pos.x, ex.pos.y);
        DrawSphere({ex.pos.x, ey + 0.2f, ex.pos.y}, ex.maxRadius * 0.25f * t, Fade(ex.color, 1.0f - t));
    }

    EndMode3D();

    // 7. 3D Screen-space Labels for nearby Cities, FOBs, and Units
    for (const auto& b : sim.bases) {
        if (Vector2Distance({b.x, b.y}, {sim.camX, sim.camY}) > 38.0f) continue;
        Vector2 sp = GetWorldToScreen({b.x, terrainY(assets, b.x, b.y) + 1.2f, b.y}, cam);
        if (sp.x > 0 && sp.x < screenW && sp.y > 40 && sp.y < screenH - 160) {
            DrawText(b.name.c_str(), static_cast<int>(sp.x - 45), static_cast<int>(sp.y), 12, {125, 245, 155, 245});
        }
    }
}

} // namespace itc
