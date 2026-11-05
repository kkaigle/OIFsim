#include "renderer_2d.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace itc {

Vector2 Renderer2D::worldToScreen(float wx, float wy, const SimState& sim, int screenW, int screenH) const {
    return {
        screenW * 0.5f + (wx - sim.camX) * sim.zoom,
        screenH * 0.5f + (wy - sim.camY) * sim.zoom
    };
}

Vector2 Renderer2D::screenToWorld(float sx, float sy, const SimState& sim, int screenW, int screenH) const {
    return {
        sim.camX + (sx - screenW * 0.5f) / sim.zoom,
        sim.camY + (sy - screenH * 0.5f) / sim.zoom
    };
}

void Renderer2D::drawCityStreetsAndBuildings(const City& c, const SimState& sim, const TheaterAssets& assets,
                                             float minWx, float maxWx, float minWy, float maxWy,
                                             int screenW, int screenH) {
    // Quick bounding radius check around city center
    if (c.x + 25.0f < minWx || c.x - 25.0f > maxWx || c.y + 25.0f < minWy || c.y - 25.0f > maxWy) {
        return;
    }

    // 1. Draw OSM Streets
    uint32_t sEnd = std::min<uint32_t>(c.streetOffset + c.streetCount, static_cast<uint32_t>(assets.streets.size()));
    for (uint32_t i = c.streetOffset; i < sEnd; ++i) {
        const StreetSeg& s = assets.streets[i];
        if (sim.zoom < 6.0f && s.cls >= 2) continue; // LOD: major/secondary roads at medium zoom
        if (std::max(s.x1, s.x2) < minWx || std::min(s.x1, s.x2) > maxWx ||
            std::max(s.y1, s.y2) < minWy || std::min(s.y1, s.y2) > maxWy) {
            continue;
        }
        Vector2 p1 = worldToScreen(s.x1, s.y1, sim, screenW, screenH);
        Vector2 p2 = worldToScreen(s.x2, s.y2, sim, screenW, screenH);
        Color col = (s.cls == 0) ? Color{178, 148, 92, 190}
                  : (s.cls == 1) ? Color{136, 122, 92, 165}
                                 : Color{98, 92, 76, 130};
        float thick = (s.cls == 0) ? std::clamp(sim.zoom * 0.12f, 1.2f, 4.0f)
                    : (s.cls == 1) ? std::clamp(sim.zoom * 0.08f, 1.0f, 2.5f)
                                   : 1.0f;
        DrawLineEx(p1, p2, thick, col);
    }

    // 2. Draw OSM Building Footprints when zoomed in enough
    if (sim.zoom < 4.5f) return;

    uint32_t bEnd = std::min<uint32_t>(c.bldOffset + c.bldCount, static_cast<uint32_t>(assets.buildings.size()));
    for (uint32_t i = c.bldOffset; i < bEnd; ++i) {
        const OsmBuilding& b = assets.buildings[i];
        if (b.cx + b.ww < minWx || b.cx - b.ww > maxWx || b.cy + b.wh < minWy || b.cy - b.wh > maxWy) {
            continue;
        }
        float pw = std::max(1.8f, b.ww * sim.zoom);
        float ph = std::max(1.8f, b.wh * sim.zoom);
        if (pw < 2.2f && ph < 2.2f && (i & 1)) continue; // Sub-pixel LOD

        Vector2 sp = worldToScreen(b.cx, b.cy, sim, screenW, screenH);
        Color fill = (b.kind == 2) ? Color{142, 118, 92, 215}   // Commercial / Govt
                   : (b.kind == 3) ? Color{118, 128, 112, 220}  // Mosque / Landmark
                   : (b.levels >= 3) ? Color{134, 122, 102, 210}
                                     : Color{116, 104, 86, 195};

        if (sim.viewMode == ViewMode::Drone) {
            fill = {175, 190, 175, 210}; // Thermal hot signature in FLIR mode
        }

        Rectangle rec{sp.x, sp.y, pw, ph};
        DrawRectanglePro(rec, {pw * 0.5f, ph * 0.5f}, b.angle * RAD2DEG, fill);
    }
}

void Renderer2D::drawBaseCompound2D(const Base& b, bool selected, const SimState& sim, int screenW, int screenH) {
    Vector2 sp = worldToScreen(b.x, b.y, sim, screenW, screenH);
    float compoundWorldSize = 1.6f; // 1.6 world units (~1.5 km perimeter)
    float pxSize = std::max(12.0f, compoundWorldSize * sim.zoom);

    Rectangle perim{sp.x - pxSize * 0.5f, sp.y - pxSize * 0.5f, pxSize, pxSize};
    DrawRectangleRec(perim, {36, 48, 38, 200});
    DrawRectangleLinesEx(perim, selected ? 2.2f : 1.4f,
                         selected ? Color{120, 240, 150, 255} : Color{90, 175, 115, 220});

    // If zoomed in, draw the 16x16 grid structures inside the compound
    if (pxSize >= 32.0f) {
        float cellPx = pxSize / BASE_GRID_N;
        for (const auto& st : b.layout) {
            const BaseStructDef* def = findStructDef(st.kind);
            int w = (st.rot % 2) ? def->d : def->w;
            int d = (st.rot % 2) ? def->w : def->d;
            Rectangle sr{
                perim.x + st.gx * cellPx + 0.5f,
                perim.y + st.gz * cellPx + 0.5f,
                w * cellPx - 1.0f,
                d * cellPx - 1.0f
            };
            DrawRectangleRec(sr, def->color);
        }
    }

    // FOB Flag / Icon & Label
    DrawCircleV(sp, 3.5f, {110, 235, 145, 255});
    if (sim.showLabels && sim.zoom >= 1.2f) {
        DrawText(b.name.c_str(), static_cast<int>(sp.x + pxSize * 0.5f + 5), static_cast<int>(sp.y - 6), 10, {155, 235, 175, 240});
    }
}

void Renderer2D::drawUnitCounter(const Unit& u, bool selected, const SimState& sim, int screenW, int screenH) {
    Vector2 sp = worldToScreen(u.x, u.y, sim, screenW, screenH);
    if (sp.x < -50 || sp.x > screenW + 50 || sp.y < -50 || sp.y > screenH + 50) return;

    // Draw movement order vector if active
    if (u.hasOrder) {
        Vector2 tp = worldToScreen(u.targetX, u.targetY, sim, screenW, screenH);
        DrawLineEx(sp, tp, 1.6f, {100, 210, 255, 180});
        DrawCircleLines(static_cast<int>(tp.x), static_cast<int>(tp.y), 5.0f, {100, 210, 255, 220});
    }

    bool isCoalition = (u.faction == UnitFaction::Coalition);
    Color boxFill = isCoalition ? Color{28, 62, 96, 235}
                  : (u.faction == UnitFaction::IraqConventional) ? Color{98, 32, 32, 235}
                                                                 : Color{110, 58, 24, 235};
    Color boxBorder = selected ? Color{255, 230, 95, 255}
                    : isCoalition ? Color{110, 195, 255, 245}
                                  : Color{245, 105, 95, 245};

    float bw = 24.0f;
    float bh = 16.0f;
    Rectangle r{sp.x - bw * 0.5f, sp.y - bh * 0.5f, bw, bh};

    if (selected) {
        DrawRectangleLinesEx({r.x - 3, r.y - 3, r.width + 6, r.height + 6}, 1.5f, {255, 225, 90, 220});
    }

    DrawRectangleRec(r, boxFill);
    DrawRectangleLinesEx(r, 1.5f, boxBorder);

    // NATO APP-6 Symbol inside box
    if (u.category == UnitCategory::Armor) {
        // Armor oval
        DrawEllipseLines(static_cast<int>(sp.x), static_cast<int>(sp.y), 7, 4, boxBorder);
    } else if (u.category == UnitCategory::Mechanized) {
        // Mechanized: X + oval
        DrawLineV({r.x, r.y}, {r.x + r.width, r.y + r.height}, boxBorder);
        DrawLineV({r.x + r.width, r.y}, {r.x, r.y + r.height}, boxBorder);
        DrawEllipseLines(static_cast<int>(sp.x), static_cast<int>(sp.y), 6, 3, boxBorder);
    } else if (u.category == UnitCategory::Aviation) {
        // Rotary wing bowtie
        DrawTriangle({sp.x - 6, sp.y - 4}, {sp.x - 6, sp.y + 4}, sp, boxBorder);
        DrawTriangle(sp, {sp.x + 6, sp.y + 4}, {sp.x + 6, sp.y - 4}, boxBorder);
    } else {
        // Infantry X
        DrawLineV({r.x, r.y}, {r.x + r.width, r.y + r.height}, boxBorder);
        DrawLineV({r.x + r.width, r.y}, {r.x, r.y + r.height}, boxBorder);
    }

    // Strength bar below counter
    float strPct = std::clamp(u.strength / u.maxStrength, 0.0f, 1.0f);
    DrawRectangle(static_cast<int>(r.x), static_cast<int>(r.y + bh + 2), static_cast<int>(bw), 3, {20, 24, 22, 220});
    Color sCol = (strPct > 0.65f) ? Color{85, 220, 115, 255} : (strPct > 0.35f ? Color{240, 190, 65, 255} : Color{235, 75, 65, 255});
    DrawRectangle(static_cast<int>(r.x), static_cast<int>(r.y + bh + 2), static_cast<int>(bw * strPct), 3, sCol);

    if (sim.showLabels && (sim.zoom >= 1.5f || selected)) {
        DrawText(u.branch.c_str(), static_cast<int>(r.x), static_cast<int>(r.y - 11), 10, boxBorder);
    }
}

void Renderer2D::drawDroneOverlay(const SimState& sim, const TheaterAssets& assets, int screenW, int screenH) {
    // Green/White-hot FLIR scanline & telemetry overlay
    DrawRectangle(0, 0, screenW, screenH, {12, 38, 22, 75});
    for (int y = 40; y < screenH - 160; y += 4) {
        DrawLine(0, y, screenW, y, {0, 0, 0, 38});
    }
    int cx = screenW / 2;
    int cy = screenH / 2;
    Color hud = {130, 255, 165, 220};
    DrawLine(cx - 60, cy, cx - 12, cy, hud);
    DrawLine(cx + 12, cy, cx + 60, cy, hud);
    DrawLine(cx, cy - 60, cx, cy - 12, hud);
    DrawLine(cx, cy + 12, cx, cy + 60, hud);
    DrawRectangleLines(cx - 110, cy - 85, 220, 170, hud);

    std::string mgrs = toMGRS(sim.camX, sim.camY);
    float elev = assets.elevAtWorld(sim.camX, sim.camY);
    char buf[128];
    std::snprintf(buf, sizeof(buf), "MQ-1B PREDATOR WESCAM MX-15 | IR WHOT | ALT 18,500 FT | ELEV %.0fM", elev);
    DrawText(buf, cx - 210, 56, 12, hud);
    DrawText(("TGT MGRS: " + mgrs).c_str(), cx - 95, cy + 95, 12, hud);
}

void Renderer2D::drawLaseOverlay(const SimState& sim, const TheaterAssets& assets, int screenW, int screenH) {
    DrawRectangle(0, 0, screenW, screenH, {28, 18, 8, 65});
    int cx = screenW / 2;
    int cy = screenH / 2;
    Color lcol = {255, 195, 70, 235};
    DrawCircleLines(cx, cy, 95.0f, lcol);
    DrawCircleLines(cx, cy, 42.0f, lcol);
    DrawLine(cx - 120, cy, cx - 15, cy, lcol);
    DrawLine(cx + 15, cy, cx + 120, cy, lcol);
    DrawLine(cx, cy - 120, cx, cy - 15, lcol);
    DrawLine(cx, cy + 15, cx, cy + 120, lcol);

    // Pulsing laser designator dot at center
    float pulse = 3.0f + std::sin(static_cast<float>(GetTime()) * 12.0f) * 2.0f;
    DrawCircle(cx, cy, pulse, {255, 70, 50, 255});

    std::string mgrs = toMGRS(sim.camX, sim.camY);
    float elev = assets.elevAtWorld(sim.camX, sim.camY);
    char buf[160];
    std::snprintf(buf, sizeof(buf), "AN/PEQ-1 SOFLAM LASER DESIGNATOR | PRF CODE 1688 | ELEV %.0fM | %s", elev, mgrs.c_str());
    DrawText(buf, cx - 250, 56, 12, lcol);
    DrawText("CLICK ANY TARGET ON MAP TO DISPATCH LASER-GUIDED ORDANCE", cx - 190, cy + 108, 12, {255, 120, 90, 240});
}

void Renderer2D::render(const SimState& sim, const TheaterAssets& assets, int screenW, int screenH) {
    ClearBackground({18, 22, 20, 255});

    // 1. Draw Base Relief or Satellite Map Texture
    const Texture2D& bgTex = (sim.satelliteMap && assets.satLoaded) ? assets.satTexture : assets.milTexture;
    if (bgTex.id != 0) {
        Vector2 tl = wProj(assets.geo.lo0, assets.geo.la1);
        Vector2 br = wProj(assets.geo.lo1, assets.geo.la0);
        Vector2 stl = worldToScreen(tl.x, tl.y, sim, screenW, screenH);
        Vector2 sbr = worldToScreen(br.x, br.y, sim, screenW, screenH);
        Rectangle src{0.0f, 0.0f, static_cast<float>(bgTex.width), static_cast<float>(bgTex.height)};
        Rectangle dst{stl.x, stl.y, sbr.x - stl.x, sbr.y - stl.y};
        DrawTexturePro(bgTex, src, dst, {0.0f, 0.0f}, 0.0f, WHITE);
    }

    // Compute visible world bounds for culling
    Vector2 wTL = screenToWorld(0.0f, 0.0f, sim, screenW, screenH);
    Vector2 wBR = screenToWorld(static_cast<float>(screenW), static_cast<float>(screenH), sim, screenW, screenH);

    // 2. Tactical Lat/Lon Grid
    if (sim.showGrid) {
        Color gridCol = {145, 165, 140, 32};
        for (int lon = 38; lon <= 50; ++lon) {
            Vector2 p1 = worldToScreen(wProj(static_cast<float>(lon), 38.0f).x, wProj(static_cast<float>(lon), 38.0f).y, sim, screenW, screenH);
            Vector2 p2 = worldToScreen(wProj(static_cast<float>(lon), 28.0f).x, wProj(static_cast<float>(lon), 28.0f).y, sim, screenW, screenH);
            DrawLineV(p1, p2, gridCol);
        }
        for (int lat = 29; lat <= 38; ++lat) {
            Vector2 p1 = worldToScreen(wProj(38.0f, static_cast<float>(lat)).x, wProj(38.0f, static_cast<float>(lat)).y, sim, screenW, screenH);
            Vector2 p2 = worldToScreen(wProj(50.0f, static_cast<float>(lat)).x, wProj(50.0f, static_cast<float>(lat)).y, sim, screenW, screenH);
            DrawLineV(p1, p2, gridCol);
        }
    }

    // 3. Rivers (Tigris, Euphrates, Diyala, Shatt al-Arab)
    for (const auto& rv : assets.rivers) {
        for (size_t i = 1; i < rv.size(); ++i) {
            Vector2 p1 = worldToScreen(rv[i - 1].x, rv[i - 1].y, sim, screenW, screenH);
            Vector2 p2 = worldToScreen(rv[i].x, rv[i].y, sim, screenW, screenH);
            DrawLineEx(p1, p2, std::clamp(sim.zoom * 0.45f, 1.5f, 5.0f), {34, 82, 118, 210});
        }
    }

    // 4. Lakes
    for (const auto& lk : assets.lakes) {
        for (size_t i = 1; i < lk.poly.size(); ++i) {
            Vector2 p1 = worldToScreen(lk.poly[i - 1].x, lk.poly[i - 1].y, sim, screenW, screenH);
            Vector2 p2 = worldToScreen(lk.poly[i].x, lk.poly[i].y, sim, screenW, screenH);
            DrawLineEx(p1, p2, 2.0f, {38, 92, 132, 210});
        }
    }

    // 5. MSRs & Highways
    for (const auto& hw : assets.highways) {
        bool isMsr = (hw.kind == "msr");
        Color hCol = isMsr ? Color{215, 165, 72, 175} : Color{155, 135, 95, 120};
        float thick = isMsr ? std::clamp(sim.zoom * 0.25f, 1.4f, 3.5f) : 1.1f;
        for (size_t i = 1; i < hw.pts.size(); ++i) {
            Vector2 p1 = worldToScreen(hw.pts[i - 1].x, hw.pts[i - 1].y, sim, screenW, screenH);
            Vector2 p2 = worldToScreen(hw.pts[i].x, hw.pts[i].y, sim, screenW, screenH);
            DrawLineEx(p1, p2, thick, hCol);
        }
    }

    // 6. Kuwait & Iraq National Borders
    auto drawPolyBorder = [&](const std::vector<Vector2>& pts, Color col, float thick) {
        for (size_t i = 0; i < pts.size(); ++i) {
            const Vector2& a = pts[i];
            const Vector2& b = pts[(i + 1) % pts.size()];
            DrawLineEx(worldToScreen(a.x, a.y, sim, screenW, screenH),
                       worldToScreen(b.x, b.y, sim, screenW, screenH), thick, col);
        }
    };
    drawPolyBorder(assets.kuwaitBorder, {125, 195, 145, 160}, 1.5f);
    drawPolyBorder(assets.iraqBorder, {215, 185, 110, 210}, 2.2f);

    // 7. OSM City Streets & 251,503 Building Footprints (when zoomed in)
    if (sim.zoom >= 2.6f) {
        for (const auto& c : assets.cities) {
            drawCityStreetsAndBuildings(c, sim, assets, wTL.x, wBR.x, wTL.y, wBR.y, screenW, screenH);
        }
    }

    // 8. Cities / Control Rings / Labels
    for (size_t i = 0; i < assets.cities.size(); ++i) {
        const City& c = assets.cities[i];
        Vector2 sp = worldToScreen(c.x, c.y, sim, screenW, screenH);
        if (sp.x < -80 || sp.x > screenW + 80 || sp.y < -80 || sp.y > screenH + 80) continue;

        bool isSel = (static_cast<int>(i) == sim.selectedCityIdx);
        float rad = std::clamp(4.0f + std::sqrt(c.pop) * 4.5f, 4.5f, 14.0f);

        Color ringCol = (c.control >= 60.0f) ? Color{85, 220, 130, 230}
                      : (c.control >= 25.0f) ? Color{235, 195, 75, 230}
                                             : Color{235, 85, 75, 230};
        if (isSel) {
            DrawCircleLines(static_cast<int>(sp.x), static_cast<int>(sp.y), rad + 6.0f, {255, 230, 95, 255});
        }
        DrawCircleV(sp, rad, {22, 28, 24, 210});
        DrawCircleLines(static_cast<int>(sp.x), static_cast<int>(sp.y), rad, ringCol);
        DrawCircleV(sp, 2.5f, ringCol);

        if (sim.showLabels) {
            DrawText(c.name.c_str(), static_cast<int>(sp.x + rad + 5), static_cast<int>(sp.y - 6), 12,
                     isSel ? Color{255, 235, 120, 255} : Color{225, 220, 195, 240});
        }
    }

    // 9. Coalition FOB Compounds
    for (size_t i = 0; i < sim.bases.size(); ++i) {
        drawBaseCompound2D(sim.bases[i], static_cast<int>(i) == sim.selectedBaseIdx, sim, screenW, screenH);
    }

    // 10. Units
    if (sim.showUnits) {
        for (const auto& u : sim.units) {
            drawUnitCounter(u, u.id == sim.selectedUnitId, sim, screenW, screenH);
        }
    }

    // 11. Active Fire Missions in Flight & Explosions
    for (const auto& fm : sim.activeFires) {
        Vector2 orig = worldToScreen(fm.origin.x, fm.origin.y, sim, screenW, screenH);
        Vector2 tgt  = worldToScreen(fm.target.x, fm.target.y, sim, screenW, screenH);
        Vector2 cur  = Vector2Lerp(orig, tgt, fm.progress);
        DrawLineEx(orig, cur, 1.6f, Fade(fm.color, 0.55f));
        DrawCircleV(cur, 4.0f, fm.color);
        DrawCircleLines(static_cast<int>(tgt.x), static_cast<int>(tgt.y), fm.radius * sim.zoom, Fade(fm.color, 0.7f));
    }

    for (const auto& ex : sim.explosions) {
        float t = std::clamp(ex.age / ex.maxAge, 0.0f, 1.0f);
        Vector2 sp = worldToScreen(ex.pos.x, ex.pos.y, sim, screenW, screenH);
        float r = (ex.maxRadius * sim.zoom) * (0.25f + 0.75f * t);
        DrawCircleV(sp, r * (1.0f - t * 0.5f), Fade(ex.color, (1.0f - t) * 0.65f));
        DrawCircleLines(static_cast<int>(sp.x), static_cast<int>(sp.y), r, Fade(YELLOW, 1.0f - t));
    }

    // 12. Mode-specific HUD Overlays (Drone FLIR / SOFLAM Laser Designator / Fire Targeting Reticle)
    if (sim.viewMode == ViewMode::Drone) {
        drawDroneOverlay(sim, assets, screenW, screenH);
    } else if (sim.viewMode == ViewMode::Lase) {
        drawLaseOverlay(sim, assets, screenW, screenH);
    }

    if (!sim.activeFireMissionId.empty()) {
        Vector2 mp = GetMousePosition();
        DrawCircleLines(static_cast<int>(mp.x), static_cast<int>(mp.y), 18.0f, {255, 85, 65, 245});
        DrawLine(static_cast<int>(mp.x - 24), static_cast<int>(mp.y), static_cast<int>(mp.x + 24), static_cast<int>(mp.y), {255, 85, 65, 245});
        DrawLine(static_cast<int>(mp.x), static_cast<int>(mp.y - 24), static_cast<int>(mp.x), static_cast<int>(mp.y + 24), {255, 85, 65, 245});
        DrawText(("FIRE MISSION READY [" + sim.activeFireMissionId + "] - CLICK TARGET").c_str(),
                 static_cast<int>(mp.x + 22), static_cast<int>(mp.y - 8), 12, {255, 110, 85, 255});
    }
}

int Renderer2D::pickCity(float sx, float sy, const SimState& sim, const TheaterAssets& assets, int screenW, int screenH) const {
    int best = -1;
    float bestDist = 18.0f;
    for (size_t i = 0; i < assets.cities.size(); ++i) {
        Vector2 sp = worldToScreen(assets.cities[i].x, assets.cities[i].y, sim, screenW, screenH);
        float d = Vector2Distance({sx, sy}, sp);
        if (d < bestDist) {
            bestDist = d;
            best = static_cast<int>(i);
        }
    }
    return best;
}

int Renderer2D::pickUnit(float sx, float sy, const SimState& sim, int screenW, int screenH) const {
    int bestId = -1;
    float bestDist = 16.0f;
    for (const auto& u : sim.units) {
        Vector2 sp = worldToScreen(u.x, u.y, sim, screenW, screenH);
        float d = Vector2Distance({sx, sy}, sp);
        if (d < bestDist) {
            bestDist = d;
            bestId = u.id;
        }
    }
    return bestId;
}

int Renderer2D::pickBase(float sx, float sy, const SimState& sim, int screenW, int screenH) const {
    int bestIdx = -1;
    float bestDist = 18.0f;
    for (size_t i = 0; i < sim.bases.size(); ++i) {
        Vector2 sp = worldToScreen(sim.bases[i].x, sim.bases[i].y, sim, screenW, screenH);
        float d = Vector2Distance({sx, sy}, sp);
        if (d < bestDist) {
            bestDist = d;
            bestIdx = static_cast<int>(i);
        }
    }
    return bestIdx;
}

} // namespace itc
