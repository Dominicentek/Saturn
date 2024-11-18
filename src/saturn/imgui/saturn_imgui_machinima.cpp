#include "saturn_imgui_machinima.h"

#include <cstdarg>
#include <functional>
#include <string>
#include <iostream>
#include <algorithm>
#include <map>
#include <fstream>

#include "engine/graph_node.h"
#include "game/area.h"
#include "saturn/libs/imgui/imgui.h"
#include "saturn/libs/imgui/imgui_internal.h"
#include "saturn/libs/imgui/imgui_impl_sdl.h"
#include "saturn/libs/imgui/imgui_impl_opengl3.h"
#include "saturn/saturn.h"
#include "saturn/saturn_actors.h"
#include "saturn/saturn_textures.h"
#include "saturn/saturn_animation_ids.h"
#include "saturn/saturn_animations.h"
#include "saturn/saturn_obj_def.h"
#include "saturn/imgui/saturn_imgui_dynos.h"
#include "saturn_imgui.h"
#include "saturn/imgui/saturn_imgui_file_browser.h"
#include "saturn/imgui/saturn_imgui_chroma.h"
#include "saturn/filesystem/saturn_locationfile.h"
#include "saturn/filesystem/saturn_animfile.h"
#include "pc/controller/controller_keyboard.h"
#include <SDL2/SDL.h>

#include "icons/IconsForkAwesome.h"

#include "data/dynos.cpp.h"

extern "C" {
#include "sm64.h"
#include "pc/gfx/gfx_pc.h"
#include "pc/configfile.h"
#include "pc/cheats.h"
#include "game/mario.h"
#include "game/camera.h"
#include "game/level_update.h"
#include "engine/level_script.h"
#include "game/game_init.h"
#include "src/game/envfx_snow.h"
#include "src/game/interaction.h"
#include "include/behavior_data.h"
#include "game/object_helpers.h"
#include "game/object_list_processor.h"
#include "engine/surface_load.h"
}

#include "saturn/saturn_json.h"

using namespace std;

int custom_anim_index = -1;
int current_sanim_index = 7;
std::string current_sanim_name = "RUNNING";
int current_sanim_id = MARIO_ANIM_RUNNING;
std::string anim_preview_name = "RUNNING";
int current_sanim_group_index = 0;
int current_slevel_index = 1;
Vec3f obj_pos;
int obj_rot[3];
int obj_beh_params[4];
int obj_model;
int obj_beh;

float gravity = 1;
int time_freeze_state = 0;

int current_location_index = 0;
char location_name[256];

bool override_level = false;
bool custom_level_loaded = false;
struct GraphNode* override_level_geolayout;
Collision* override_level_collision;

Array<PackData *> &sDynosPacks = DynOS_Gfx_GetPacks();

s16 levelList[] = { 
    LEVEL_SA, LEVEL_CASTLE_GROUNDS, LEVEL_CASTLE, LEVEL_CASTLE_COURTYARD, LEVEL_BOB, 
    LEVEL_WF, LEVEL_PSS, LEVEL_TOTWC, LEVEL_JRB, LEVEL_CCM,
    LEVEL_BITDW, LEVEL_BBH, LEVEL_HMC, LEVEL_COTMC, LEVEL_LLL,
    LEVEL_SSL, LEVEL_VCUTM, LEVEL_DDD, LEVEL_BITFS, 
    LEVEL_SL, LEVEL_WDW, LEVEL_TTM, LEVEL_THI,
    LEVEL_TTC, LEVEL_WMOTR, LEVEL_RR, LEVEL_BITS,
    LEVEL_BOWSER_1, LEVEL_BOWSER_2, LEVEL_BOWSER_3
};

int areaList[] = {
    2, 1, 3, 1, 1,
    1, 1, 1, 2, 2,
    1, 1, 1, 1, 2,
    2, 1, 2, 1,
    2, 2, 2, 3,
    1, 1, 1, 1
};

int levelParam = 0;
std::map<int, std::vector<std::string>> levelParams = {
    { LEVEL_DDD, {
        "No Sub, No Poles",
        "Sub, No Poles",
        "No Sub, Poles",
        "Sub, Poles"
    } },
    { LEVEL_WDW, {
        "Very Low Water Level",
        "Low Water Level",
        "High Water Level",
        "Very High Water Level"
    } },
    { LEVEL_THI, {
        "Not Drained",
        "Drained"
    } },
    { LEVEL_TTC, {
        "Frozen",
        "Slow",
        "Fast",
        "Random"
    } }
};

int current_level_sel = 0;
void warp_to(s16 destLevel, s16 destArea = 0x01, s16 destWarpNode = 0x0A) {
    if (!mario_exists)
        return;

    if (destLevel == gCurrLevelNum && destArea == gCurrAreaIndex) {
        if (current_slevel_index < 4)
            return;
            
        DynOS_Warp_ToLevel(gCurrLevelNum, gCurrAreaIndex, gCurrActNum);
    }

    initiate_warp(destLevel, destArea, destWarpNode, 0);
    fade_into_special_warp(0,0);
}

void anim_play_button() {
    if (current_animation.id == -1) return;
    current_anim_frame = 0;
    is_anim_playing = true;
    if (current_animation.custom) {
        saturn_read_mcomp_animation(nullptr, anim_preview_name);
        saturn_play_animation(MarioAnimID(current_animation.id));
        saturn_play_custom_animation();
    } else {
        saturn_play_animation(MarioAnimID(current_animation.id));
    }
}

void saturn_create_object(int model, const BehaviorScript* behavior, float x, float y, float z, s16 pitch, s16 yaw, s16 roll, int behParams) {
    Object* obj = spawn_object(gMarioState->marioObj, model, behavior);
    obj->oPosX = x;
    obj->oPosY = y;
    obj->oPosZ = z;
    obj->oHomeX = x;
    obj->oHomeY = y;
    obj->oHomeZ = z;
    obj->oFaceAnglePitch = pitch;
    obj->oFaceAngleYaw = yaw;
    obj->oFaceAngleRoll = roll;
    obj->oBehParams = behParams;
}

void smachinima_imgui_controls(SDL_Event * event) {
    switch (event->type){
        case SDL_KEYDOWN:
            if (event->key.keysym.sym == SDLK_m && !saturn_disable_sm64_input()) {
                if (camera_fov <= 98.0f) camera_fov += 2.f;
            } else if (event->key.keysym.sym == SDLK_n && !saturn_disable_sm64_input()) {
                if (camera_fov >= 2.0f) camera_fov -= 2.f;
            }

            /*if (SDL_GetKeyboardState(NULL)[SDL_SCANCODE_LSHIFT]) {
                if (!saturn_disable_sm64_input()) {
                    if (event->key.keysym.sym >= SDLK_0 && event->key.keysym.sym <= SDLK_9) {
                        saturn_load_expression_number(event->key.keysym.sym);
                    }
                };
            }*/

        case SDL_MOUSEMOTION:
            SDL_Delay(2);
            camera_view_move_x = event->motion.xrel;
            camera_view_move_y = event->motion.yrel;
        
        break;
    }
}

void warp_to_level(int level, int area, int act = -1) {
    s32 levelID = levelList[level];
    s32 warpnode = 0x0A;

    if (gCurrLevelNum == levelID && gCurrAreaIndex == area) return;

    is_anim_playing = false;
    is_anim_paused = false;
    time_freeze_state = 0;

    if (level != 0) enable_shadows = true;
    else enable_shadows = false;

    switch (level) {
        case 1:
            warpnode = 0x04;
            break;
        case 2:
            if (area == 1) warpnode = 0x1E;
            else warpnode = 0x10;
            break;
        case 3:
            warpnode = 0x0B;
            break;
    }

    DynOS_Warp_SetParam(levelID, levelParam + 1);
    DynOS_Warp_ToWarpNode(levelID, area, act, warpnode);

    saturn_clear_actors();
    saturn_clear_simulation();
}

int get_saturn_level_id(int level) {
    for (int i = 0; i < IM_ARRAYSIZE(levelList); i++) {
        if (levelList[i] == level) return i;
    }
}

Gfx* geo_switch_override_model(s32 callContext, struct GraphNode *node, UNUSED Mat4 *mtx) {
    struct GraphNodeSwitchCase* switchCase = (struct GraphNodeSwitchCase*)node;
    if (callContext == GEO_CONTEXT_RENDER) {
        switchCase->selectedCase = override_level && override_level_geolayout;
    }
    return NULL;
}

#define C0(pos, width) ((dl->words.w0 >> (pos)) & ((1U << width) - 1))
#define C1(pos, width) ((dl->words.w1 >> (pos)) & ((1U << width) - 1))
void append_collision_data(Gfx* dl, int* cur, int* nvt, std::map<void*, int>* off, std::vector<float>* vtx, std::vector<int>* tri) {
    bool running = true;
    while (running) {
        int opcode = dl->words.w0 >> 24;
        switch (opcode) {
            case G_DL: {
                append_collision_data((Gfx*)dl->words.w1, cur, nvt, off, vtx, tri);
            } break;
            case G_VTX: {
                Vtx* verts = (Vtx*)dl->words.w1;
                if (off->find(verts) == off->end()) {
                    off->insert({ verts, *nvt });
                    int num = C0(12, 8);
                    *nvt += num;
                    for (int i = 0; i < num; i++) {
                        vtx->push_back(verts[i].v.ob[0]);
                        vtx->push_back(verts[i].v.ob[1]);
                        vtx->push_back(verts[i].v.ob[2]);
                    }
                }
                *cur = (*off)[verts];
            } break;
            case G_TRI1: {
                tri->push_back(C0(16, 8) / 2 + *cur);
                tri->push_back(C0( 8, 8) / 2 + *cur);
                tri->push_back(C0( 0, 8) / 2 + *cur);
            } break;
            case G_TRI2: {
                tri->push_back(C0(16, 8) / 2 + *cur);
                tri->push_back(C0( 8, 8) / 2 + *cur);
                tri->push_back(C0( 0, 8) / 2 + *cur);
                tri->push_back(C1(16, 8) / 2 + *cur);
                tri->push_back(C1( 8, 8) / 2 + *cur);
                tri->push_back(C1( 0, 8) / 2 + *cur);
            } break;
            case G_ENDDL: {
                running = false;
            } break;
        }
        dl++;
    }
}

Collision empty_collision[] = {
    COL_INIT(),
    COL_VERTEX_INIT(0),
    COL_TRI_INIT(SURFACE_DEFAULT, 0),
    COL_TRI_STOP(),
    COL_END(),
};

Collision* create_collision_mesh(struct GraphNode* node) {
    if (node == NULL) return NULL;
    if (node->type == GRAPH_NODE_TYPE_DISPLAY_LIST) {
        struct GraphNodeDisplayList* dlnode = (struct GraphNodeDisplayList*)node;
        Gfx* dl = (Gfx*)dlnode->displayList;
        std::vector<float>   vtx = {};
        std::vector<int>     tri = {};
        std::map<void*, int> off = {};
        int                  nvt = 0;
        int                  cur = 0;
        append_collision_data(dl, &cur, &nvt, &off, &vtx, &tri);
        Collision* coll = (Collision*)malloc(sizeof(s16) * (6 + vtx.size() + tri.size()));
        int ptr = 0;
        coll[ptr++] = TERRAIN_LOAD_VERTICES;
        coll[ptr++] = vtx.size() / 3;
        for (int i = 0; i < vtx.size(); i++) {
            coll[ptr++] = vtx[i];
        }
        coll[ptr++] = SURFACE_DEFAULT;
        coll[ptr++] = tri.size() / 3;
        for (int i = 0; i < tri.size(); i++) {
            coll[ptr++] = tri[i];
        }
        coll[ptr++] = TERRAIN_LOAD_CONTINUE;
        coll[ptr++] = TERRAIN_LOAD_END;
        return coll;
    }
    return create_collision_mesh(node->children);
}

void smachinima_imgui_init() {
    Cheats.EnableCheats = true;
    Cheats.GodMode = true;
    Cheats.ExitAnywhere = true;
    current_anim_dir_path = "dynos/anims/";
}

bool enabled_acts[6];
int current_warp_area = 1;

int frames_to_simulate = 300;

extern struct Object gObjectPool[960];
extern u16 gRandomSeed16;

char animname[256];
char animauthor[256];
bool animlooping = false;
int animformat = 0;

void imgui_machinima_quick_options() {
    if (ImGui::MenuItem(ICON_FK_CLOCK_O " Limit FPS",      "F4", limit_fps)) {
        limit_fps = !limit_fps;
        configWindow.fps_changed = true;
    }

    if (mario_exists) {
        ImGui::Separator();

        if (ImGui::BeginCombo("###warp_to_level", saturn_get_stage_name(levelList[current_slevel_index]), ImGuiComboFlags_None)) {
            for (int n = 0; n < IM_ARRAYSIZE(levelList); n++) {
                const bool is_selected = (current_slevel_index == n);

                if (ImGui::Selectable(saturn_get_stage_name(levelList[n]), is_selected)) {
                    current_slevel_index = n;
                    current_warp_area = 1;
                }

                // Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
                if (is_selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        if (ImGui::BeginCombo("###level_area", ("Area " + std::to_string(current_warp_area)).c_str())) {
            for (int i = 1; i <= areaList[current_slevel_index]; i++) {
                bool is_selected = (current_warp_area == i);
                if (ImGui::Selectable(("Area " + std::to_string(i)).c_str())) {
                    current_warp_area = i;
                }
                if (is_selected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        if (levelParams.find(levelList[current_slevel_index]) != levelParams.end()) {
            if (ImGui::BeginCombo("###level_param", levelParams[levelList[current_slevel_index]][levelParam].c_str())) {
                for (int i = 0; i < levelParams[levelList[current_slevel_index]].size(); i++) {
                    bool is_selected = levelParam == i;
                    if (ImGui::Selectable(levelParams[levelList[current_slevel_index]][i].c_str())) levelParam = i;
                    if (is_selected) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
        }

        if (ImGui::Button("Warp to Level")) {
            autoChroma = false;
            override_level = false;

            warp_to_level(current_slevel_index, current_warp_area, 1);
            // Erase existing timelines
            k_frame_keys.clear();
        }

        for (int i = 0; i < 6; i++) {
            ImGui::SameLine();
            if (ImGui::Button((std::string(enabled_acts[i] ? ICON_FK_STAR : ICON_FK_STAR_O) + "###act_select_" + std::to_string(i)).c_str())) {
                enabled_acts[i] = !enabled_acts[i];
            }
            imgui_bundled_tooltip((std::string(enabled_acts[i] ? "Disable" : "Enable") + " act " + std::to_string(i + 1) + " objects").c_str());
        }

        if (gCurrLevelNum == LEVEL_SA) {
            static bool custom_level_collision = true;
            ImGui::Separator();
            ImGui::Checkbox("Collision", &custom_level_collision);
            ImGui::SameLine();
            imgui_bundled_help_marker("Having collisions on and loading models\nthat have more than 32767 triangles will\ncrash the game.\n\nChanges will only apply once you\nreload the model.");
            if (ImGui::Checkbox("Load Level Model", &override_level)) {
                gCurrentArea->terrainData = 
                    override_level && override_level_collision ?
                        (custom_level_collision ? override_level_collision : empty_collision) :
                        gAreas[gCurrAreaIndex].terrainDataOrig;
                load_area_terrain(gCurrAreaIndex, gCurrentArea->terrainData, gCurrentArea->surfaceRooms, NULL);
            }
            if (override_level) {
                Array<PackData*>& sDynosPacks = DynOS_Gfx_GetPacks();
                ImGui::BeginChild("##level_model_select", ImVec2(0, 120), ImGuiChildFlags_Border);
                for (Model& model : model_list) {
                    if (model.Type != "level") continue;
                    GfxData* gfx = DynOS_Gfx_LoadFromBinary(sDynosPacks[model.DynOSId]->mPath, "mario_geo");
                    GraphNode* geo = (GraphNode*)DynOS_Geo_GetGraphNode((*(gfx->mGeoLayouts.end() - 1))->mData, true);
                    bool selected = geo == override_level_geolayout;
                    if (ImGui::Selectable(model.Name.c_str(), selected)) {
                        if (override_level_collision) {
                            free(override_level_collision);
                            override_level_collision = NULL;
                        }
                        override_level_geolayout = geo;
                        override_level_collision = custom_level_collision ? create_collision_mesh(geo) : empty_collision;
                        gCurrentArea->terrainData = override_level_collision;
                        load_area_terrain(gCurrAreaIndex, gCurrentArea->terrainData, gCurrentArea->surfaceRooms, NULL);
                    }
                    if (ImGui::IsItemHovered()) {
                        ImGui::BeginTooltip();
                        ImGui::TextDisabled("%s - by", model.Version.c_str());
                        ImGui::SameLine();
                        ImGui::Text("%s", model.Author.c_str());
                        ImGui::Text("%s", model.Description.c_str());
                        ImGui::EndTooltip();
                    }
                }
                ImGui::EndChild();
            }
        }

        /*auto locations = saturn_get_locations();
        bool do_save = false;
        std::vector<std::string> forRemoval = {};
        if (ImGui::BeginMenu("Locations")) {
            for (auto& entry : *locations) {
                if (ImGui::Button((std::string(ICON_FK_PLAY "###warp_to_location_") + entry.first).c_str())) {
                    gMarioState->pos[0] = entry.second.second[0];
                    gMarioState->pos[1] = entry.second.second[1];
                    gMarioState->pos[2] = entry.second.second[2];
                    gMarioState->faceAngle[1] = entry.second.first;
                }
                imgui_bundled_tooltip("Warp");
                ImGui::SameLine();
                if (ImGui::Button((std::string(ICON_FK_TRASH "###delete_location_") + entry.first).c_str())) {
                    forRemoval.push_back(entry.first);
                    do_save = true;
                }
                imgui_bundled_tooltip("Remove");
                ImGui::SameLine();
                ImGui::Text(entry.first.c_str());
            }
            if (ImGui::Button(ICON_FK_PLUS "###add_location")) {
                saturn_add_location(location_name);
                do_save = true;
            }
            imgui_bundled_tooltip("Add");
            ImGui::SameLine();
            ImGui::InputText("###location_input", location_name, 256);
            ImGui::EndMenu();
        }
        for (std::string key : forRemoval) {
            locations->erase(key);
        }
        if (do_save) saturn_save_locations();*/
    }
    ImGui::Separator();
    ImGui::Checkbox("HUD", &configHUD);
    imgui_bundled_tooltip("Controls the in-game HUD visibility.");
    saturn_keyframe_popout("k_hud");
    ImGui::Checkbox("Shadows", &enable_shadows);
    imgui_bundled_tooltip("Displays the shadows of various objects.");
    saturn_keyframe_popout("k_shadows");
    ImGui::Checkbox("Invulnerability", (bool*)&enable_immunity);
    imgui_bundled_tooltip("If enabled, Mario will be invulnerable to most enemies and hazards.");
    ImGui::Checkbox("Fog", &enable_fog);
    imgui_bundled_tooltip("Toggles the fog, useful for near-clipping shots");
    ImGui::Checkbox("Rainbow Level", &rainbow);
    imgui_bundled_tooltip("Makes the level rainbow.");
    saturn_keyframe_popout("k_r_dls");
    int previous_time_freeze_state = time_freeze_state;
    ImGui::PushItemWidth(150);
    ImGui::Combo("Time Freeze", &time_freeze_state, "Unfrozen\0Mario-exclusive\0Everything\0");
    if (previous_time_freeze_state != time_freeze_state) {
        if (previous_time_freeze_state == 1) disable_time_stop();
        if (previous_time_freeze_state == 2) disable_time_stop_including_mario();
        if (time_freeze_state == 1) enable_time_stop();
        if (time_freeze_state == 2) enable_time_stop_including_mario();
    }
    imgui_bundled_tooltip("Pauses all in-game movement, excluding the camera.");
    ImGui::Checkbox("Object Interactions", (bool*)&enable_dialogue);
    imgui_bundled_tooltip("Toggles interactions with some objects; This includes opening/closing doors, triggering dialogue when interacting with an NPC or readable sign, etc.");
    if (mario_exists) {
        if (gMarioState->action == ACT_IDLE) {
            if (ImGui::Button("Sleep")) {
                set_mario_action(gMarioState, ACT_START_SLEEPING, 0);
            }
        }
        ImGui::Separator();
        const char* mEnvSettings[] = { "Default", "None", "Snow", "Blizzard" };
        ImGui::PushItemWidth(100);
        ImGui::Combo("Environment###env_dropdown", (int*)&gLevelEnv, mEnvSettings, IM_ARRAYSIZE(mEnvSettings));
        ImGui::SliderFloat("Gravity", &gravity, 0.f, 3.f);
        saturn_keyframe_popout("k_gravity");
        ImGui::PopItemWidth();
        
        if (ImGui::BeginMenu("Spawn Object")) {
            ImGui::Text("Position");
            ImGui::SameLine();
            ImGui::InputFloat3("###obj_set_pos", (float*)&obj_pos);
            ImGui::Text("Rotation");
            ImGui::SameLine();
            ImGui::InputInt3("###obj_set_rot", (int*)&obj_rot);
            if (ImGui::Button("Copy Mario")) {
                vec3f_copy(obj_pos, gMarioState->pos);
                obj_rot[0] = gMarioState->faceAngle[0];
                obj_rot[1] = gMarioState->faceAngle[1];
                obj_rot[2] = gMarioState->faceAngle[2];
            }
            ImGui::Separator();
            ImGui::Text("Model");
            if (ImGui::BeginCombo("###obj_model", obj_models[obj_model].first.c_str())) {
                for (int i = 0; i < IM_ARRAYSIZE(obj_models); i++) {
                    bool selected = obj_model == i;
                    if (ImGui::Selectable(obj_models[i].first.c_str())) obj_model = i;
                    if (selected) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            ImGui::Text("Behavior");
            if (ImGui::BeginCombo("###obj_beh", obj_behaviors[obj_beh].first.c_str())) {
                for (int i = 0; i < IM_ARRAYSIZE(obj_behaviors); i++) {
                    bool selected = obj_beh == i;
                    if (ImGui::Selectable(obj_behaviors[i].first.c_str())) obj_beh = i;
                    if (selected) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            ImGui::Text("Behavior Parameters");
            ImGui::InputInt4("###obj_beh_params", obj_beh_params);
            ImGui::Separator();
            if (ImGui::Button("Spawn Object")) {
                saturn_create_object(
                    obj_models[obj_model].second, obj_behaviors[obj_beh].second,
                    obj_pos[0], obj_pos[1], obj_pos[2],
                    obj_rot[0], obj_rot[1], obj_rot[2],
                    ((obj_beh_params[0] & 0xFF) << 24) | ((obj_beh_params[1] & 0xFF) << 16) | ((obj_beh_params[2] & 0xFF) << 8) | (obj_beh_params[3] & 0xFF)
                );
            }
            ImGui::EndMenu();
        }
    }

    if (ImGui::BeginMenu("Shading")) {
        ImGui::SliderFloat("X###wdir_x", &world_light_dir1, -2.f, 2.f);
        saturn_keyframe_popout("k_shade_x");
        ImGui::SliderFloat("Y###wdir_y", &world_light_dir2, -2.f, 2.f);
        saturn_keyframe_popout("k_shade_y");
        ImGui::SliderFloat("Z###wdir_z", &world_light_dir3, -2.f, 2.f);
        saturn_keyframe_popout("k_shade_z");
        ImGui::SliderFloat("Tex###wdir_tex", &world_light_dir4, 1.f, 4.f);
        saturn_keyframe_popout("k_shade_t");

        ImGui::ColorEdit4("Col###wlight_col", gLightingColor, ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_InputRGB | ImGuiColorEditFlags_Uint8 | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_NoOptions);
        
        if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Right))
            ImGui::OpenPopup("###texColColorPresets");

        if (ImGui::BeginPopup("###texColColorPresets")) {
            if (ImGui::Selectable(ICON_FK_UNDO " Reset")) {
                gLightingColor[0] = 1.f;
                gLightingColor[1] = 1.f;
                gLightingColor[2] = 1.f;
            }
            if (ImGui::Selectable("Randomize")) {
                gLightingColor[0] = (rand() % 255) / 255.0f;
                gLightingColor[1] = (rand() % 255) / 255.0f;
                gLightingColor[2] = (rand() % 255) / 255.0f;
            }
            ImGui::EndPopup();
        }

        ImGui::SameLine(); ImGui::Text("Col");
        saturn_keyframe_popout("k_light_col");

        if (world_light_dir1 != 0.f || world_light_dir2 != 0.f || world_light_dir3 != 0.f || world_light_dir4 != 1.f) {
            if (ImGui::Button("Reset###reset_wshading")) {
                world_light_dir1 = 0.f;
                world_light_dir2 = 0.f;
                world_light_dir3 = 0.f;
                world_light_dir4 = 1.f;
                gLightingColor[0] = 1.f;
                gLightingColor[1] = 1.f;
                gLightingColor[2] = 1.f;
            }
        }

        ImGui::EndMenu();
    }

    ImGui::InputInt("###simulation_frames", &frames_to_simulate, 1, 10);
    ImGui::SameLine();
    if (ImGui::Button("Simulate")) {
        gRandomSeed16 = world_simulation_seed;
        saturn_simulate(frames_to_simulate);
        world_simulation_curr_frame = 0;
        if (saturn_timeline_exists("k_worldsim_frame")) k_frame_keys.erase("k_worldsim_frame");
    }
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        const char* units[] = { "B", "kB", "MB", "GB", "TB", "PB", "EB", "ZB", "YB" };
        int unitIndex = 0;
        size_t bytes = sizeof(gObjectPool) * (frames_to_simulate / configWorldsimSteps);
        int fraction = 0;
        while (unitIndex < sizeof(units) / sizeof(*units) && bytes >= 1024) {
            unitIndex++;
            fraction = bytes % 1024;
            bytes /= 1024;
        }
        if (fraction == 0) ImGui::Text("Memory Usage: %ld %s", bytes, units[unitIndex]);
        else ImGui::Text("Memory Usage: %.2f %s", fraction / 1024.f + bytes, units[unitIndex]);
        ImGui::EndTooltip();
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(!world_simulation_data);
    if (ImGui::Button(ICON_FK_TRASH)) {
        saturn_clear_simulation();
        if (saturn_timeline_exists("k_worldsim_frame")) k_frame_keys.erase("k_worldsim_frame");
    }
    int frame = world_simulation_curr_frame;
    if (ImGui::SliderInt("Simulation Frame", &frame, 0, world_simulation_frames - 1, "%d", ImGuiSliderFlags_AlwaysClamp)) {
        world_simulation_curr_frame = frame;
    }
    saturn_keyframe_popout("k_worldsim_frame");
    ImGui::EndDisabled();
}

static char animSearchTerm[128];
uint64_t enabled_categories = UINT64_MAX;

bool case_insensitive_contains(std::string base, std::string substr) {
    std::string lower_b = base;
    std::string lower_s = substr;
    std::transform(base.begin(), base.end(), lower_b.begin(),
        [](unsigned char c){ return std::tolower(c); });
    std::transform(substr.begin(), substr.end(), lower_s.begin(),
        [](unsigned char c){ return std::tolower(c); });
    return lower_b.find(lower_s) != std::string::npos;
}

std::vector<int> get_sorted_anim_list(MarioActor* actor) {
    std::vector<int> anim_list = {};
    std::vector<int> fav_anim_list = {};
    std::vector<int> avail_anims = saturn_animation_categories[actor->obj_model]["_"];
    auto categories = saturn_animation_categories[actor->obj_model];
    int iter = 0;
    for (auto entry : categories) {
        if (!(enabled_categories & (1 << iter++))) continue;
        for (int i = 0; i < entry.second.size(); i++) {
            avail_anims.push_back(entry.second[i]);
        }
    }
    for (int anim : avail_anims) {
        if (!case_insensitive_contains(saturn_animation_names[anim], animSearchTerm)) continue;
        bool contains = std::find(favorite_anims.begin(), favorite_anims.end(), anim) != favorite_anims.end();
        if (contains) fav_anim_list.push_back(anim);
        else anim_list.push_back(anim);
    }
    std::reverse(fav_anim_list.begin(), fav_anim_list.end());
    for (int fav : fav_anim_list) {
        anim_list.insert(anim_list.begin(), fav);
    }
    return anim_list;
}

void get_animation_rotations(MarioActor* actor, float* dst, int frame) {
    for (auto timeline : k_frame_keys) {
        saturn_keyframe_apply(timeline.first, frame);
    }
    for (int i = 0; i < 60; i++) {
        dst[i * 3 + 0] = actor->bones[i][0];
        dst[i * 3 + 1] = actor->bones[i][1];
        dst[i * 3 + 2] = actor->bones[i][2];
    }
}

struct BinaryStream {
    int length;
    unsigned char* data;
};

struct BinaryStream* make_stream_from_string(std::string str) {
    struct BinaryStream* stream = (struct BinaryStream*)malloc(sizeof(struct BinaryStream));
    stream->length = str.length();
    stream->data = (unsigned char*)malloc(stream->length);
    memcpy(stream->data, str.data(), str.length());
    return stream;
}

std::string format_string(const char* fmt, ...) {
    char dst[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(dst, 1024, fmt, args);
    va_end(args);
    return dst;
}

struct BinaryStream* create_anim_json(int frames, u16* indices, u16* values, int num_indices, int num_values) {
    std::string json = "{\n";
    json += format_string("    \"name\": \"%s\",\n", Json::escaped_str(animname).c_str());
    json += format_string("    \"author\": \"%s\",\n", Json::escaped_str(animauthor).c_str());
    json += format_string("    \"looping\": \"%s\",\n", animlooping ? "true" : "false");
    json += format_string("    \"length\": %d,\n", frames);
    json += format_string("    \"nodes\": 0,\n");
    json += format_string("    \"indices\": [");
    for (int i = 0; i < num_indices; i++) {
        if (i % 6 == 0) json += "\n        ";
        json += format_string("\"0x%02X\",\"0x%02X\",", (indices[i] >> 8) & 0xFF, indices[i] & 0xFF);
    }
    json += "\n    ],\n";
    json += "    \"values\": [";
    for (int i = 0; i < num_values; i++) {
        if (i % 6 == 0) json += "\n        ";
        json += format_string("\"0x%02X\",\"0x%02X\",", (values[i] >> 8) & 0xFF, values[i] & 0xFF);
    }
    json += "\n    ]\n}\n";
    return make_stream_from_string(json);
}

struct BinaryStream* create_anim_c(int frames, u16* indices, u16* values, int num_indices, int num_values) {
    std::string c = format_string("static const struct Animation %s[] = {\n", animname);
    c += format_string("    %d,\n", animlooping ? 0 : 1);
    c += format_string("    0,\n");
    c += format_string("    0,\n");
    c += format_string("    0,\n");
    c += format_string("    0x%02X,\n", frames);
    c += format_string("    ANIMINDEX_NUMPARTS(anim_indices),\n");
    c += format_string("    anim_values,\n");
    c += format_string("    anim_indices,\n");
    c += format_string("    0,\n");
    c += format_string("};\n\nstatic const u16 %s_indices[] = {", animname);
    for (int i = 0; i < num_indices; i++) {
        if (i % 6 == 0) c += "\n    ";
        c += format_string("0x%04X, ", indices[i]);
    }
    c += format_string("\n};\n\nstatic const s16 %s_values[] = {", animname);
    for (int i = 0; i < num_values; i++) {
        if (i % 6 == 0) c += "\n    ";
        c += format_string("0x%04X, ", values[i]);
    }
    c += format_string("\n};\n");
    return make_stream_from_string(c);
}

struct BinaryStream* create_anim_panim(int frames, u16* indices, u16* values, int num_indices, int num_values) {
    struct BinaryStream* stream = (struct BinaryStream*)malloc(sizeof(struct BinaryStream*));
    stream->length = 80 + (num_values + num_indices) * 2;
    stream->data = (unsigned char*)malloc(stream->length);
    memset(stream->data, 0, stream->length);
    memcpy(stream->data + 0x00, animname, 48);
    memcpy(stream->data + 0x30, animauthor, 48);
    stream->data[0x60] = animlooping;
    stream->data[0x61] =  frames       & 0xFF;
    stream->data[0x62] = (frames >> 8) & 0xFF;
    int ptr = 0x63;
    memcpy(stream->data + ptr, "values", 6);
    ptr += 6;
    for (int i = 0; i < num_values; i++) {
        stream->data[ptr++] = (values[i] >> 8) & 0xFF;
        stream->data[ptr++] =  values[i]       & 0xFF;
    }
    memcpy(stream->data + ptr, "indices", 7);
    ptr += 7;
    for (int i = 0; i < num_indices; i++) {
        stream->data[ptr++] = (indices[i] >> 8) & 0xFF;
        stream->data[ptr++] =  indices[i]       & 0xFF;
    }
    return stream;
}

#define ANIM_EXT(ext) { "." #ext, "*." #ext, #ext " files", create_anim_##ext }
struct AnimationFormat {
    const char* combo_item;
    const char* filter;
    const char* filter_name;
    std::function<struct BinaryStream*(int, u16*, u16*, int, int)> encode;
};

std::vector<struct AnimationFormat> anim_formats = {
    ANIM_EXT(json),
    ANIM_EXT(c),
    ANIM_EXT(panim)
};

struct Animation sampling_animation;
float sampling_frame = 0;
bool sampling_anim_loaded = false;
std::vector<s16> sampling_values = {};
std::vector<s16> sampling_indices = {};

void imgui_machinima_animation_player(MarioActor* actor, bool sampling) {
    if (!sampling) actor->custom_bone = false;
    bool should_update_sample = false;
    if (ImGui::BeginTabBar("###anim_tab_bar")) {
        if (ImGui::BeginTabItem("SM64")) {
            ImGui::PushItemWidth(316);
            if (ImGui::BeginCombo("##anim_filters", "Filters...")) {
                auto filters = saturn_animation_categories[actor->obj_model];
                int iter = 0;
                for (auto entry : filters) {
                    if (entry.first == "_") continue;
                    bool checked = enabled_categories & (1 << iter);
                    if (ImGui::Checkbox(entry.first.c_str(), &checked)) {
                        if (checked) enabled_categories |= (1 << iter);
                        else enabled_categories &= ~(1 << iter);
                    }
                    iter++;
                }
                ImGui::EndCombo();
            }
            ImGui::InputTextWithHint("###anim_search", ICON_FK_SEARCH " Search...", animSearchTerm, 128);
            if (ImGui::BeginChild("###anim_box_child", ImVec2(316, 100), true)) {
                std::vector<int> anim_order = get_sorted_anim_list(actor);
                for (int i : anim_order) {
                    const bool is_selected = i == actor->animstate.id && !actor->animstate.custom;
                    auto position = std::find(favorite_anims.begin(), favorite_anims.end(), i);
                    bool contains = position != favorite_anims.end();
                    if (ImGui::SmallButton((std::string(contains ? ICON_FK_STAR : ICON_FK_STAR_O) + "###anim_fav_" + std::to_string(i)).c_str())) {
                        if (contains) favorite_anims.erase(position);
                        else favorite_anims.push_back(i);
                        saturn_save_favorite_anims();
                    }
                    ImGui::SameLine();
                    if (ImGui::Selectable(saturn_animation_names[i].c_str(), is_selected)) {
                        if (sampling) {
                            auto anim = saturn_animation_data[i];
                            sampling_anim_loaded = true;
                            sampling_animation = anim.second(anim.first);
                            sampling_frame = 0;
                            should_update_sample = true;
                        }
                        else {
                            actor->animstate.id = i;
                            actor->animstate.custom = false;
                            actor->animstate.frame = 0;
                        }
                    }
                }
                ImGui::EndChild();
            }
            ImGui::PopItemWidth();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("MComp")) {
            ImGui::PushItemWidth(316);
            saturn_file_browser_filter_extensions({ "json", "panim" });
            saturn_file_browser_scan_directory("dynos/anims");
            saturn_file_browser_height(120);
            if (saturn_file_browser_show("animations")) {
                std::string path = saturn_file_browser_get_selected().string();
                if (std::find(canim_array.begin(), canim_array.end(), path) == canim_array.end()) canim_array.push_back(path);
                if (sampling) {
                    sampling_anim_loaded = true;
                    std::filesystem::path fpath = current_anim_dir_path / std::filesystem::path(path);
                    std::ifstream stream = std::ifstream(fpath);
                    int length;
                    std::vector<s16> values, indices;
                    if (fpath.extension() == ".panim") {
                        int filelen = std::filesystem::file_size(fpath);
                        unsigned char* data = (unsigned char*)malloc(filelen);
                        stream.read((char*)data, filelen);
                        length = (int)data[0x41] + (int)data[0x42] * 0x100;
                        int ptr = 0x43;
                        std::vector<s16>* curr_array;
                        while (ptr < filelen) {
                            if (strcmp((char*)data + ptr, "values")  == 0) {
                                curr_array = &values;
                                ptr += 6;
                                continue;
                            }
                            if (strcmp((char*)data + ptr, "indices") == 0) {
                                curr_array = &indices;
                                ptr += 7;
                                continue;
                            }
                            curr_array->push_back((int)data[ptr] * 256 + (int)data[ptr + 1]);
                            ptr += 2;
                        }
                        free(data);
                        stream.close();
                    }
                    else {
                        Json::Value value;
                        value << stream;
                        auto [ _length, _values, _indices ] = read_bone_data(value);
                        length = _length;
                        values = _values;
                        indices = _indices;
                    }
                    sampling_values = values;
                    sampling_indices = indices;
                    sampling_animation.flags = 4;
                    sampling_animation.unk02 = 0;
                    sampling_animation.unk04 = 0;
                    sampling_animation.unk06 = 0;
                    sampling_animation.unk08 = (s16)length;
                    sampling_animation.unk0A = sampling_indices.size() / 6 - 1;
                    sampling_animation.values = sampling_values.data();
                    sampling_animation.index = (const u16*)sampling_indices.data();
                    sampling_animation.length = (s16)length;
                    should_update_sample = true;
                }
                else {
                    actor->animstate.id = std::find(canim_array.begin(), canim_array.end(), path) - canim_array.begin();
                    actor->animstate.custom = true;
                    actor->animstate.frame = 0;
                    saturn_read_mcomp_animation(actor, path.c_str());
                }
            }
            ImGui::PopItemWidth();
            ImGui::EndTabItem();
        }
        if (!sampling) if (ImGui::BeginTabItem("Custom")) {
            actor->custom_bone = true;
            int currbone = 0;
            if (ImGui::TreeNode("Export")) {
                ImGui::InputText("Name", animname, 256);
                ImGui::InputText("Author", animauthor, 256);
                ImGui::Checkbox("Looping", &animlooping);
                if (ImGui::Button("Export")) {
                    int frames = 1;
                    for (int i = 0; i <= 20; i++) {
                        std::string timelineID = saturn_keyframe_get_mario_timeline_id("k_mariobone_" + (i == 0 ? "t" : std::to_string(i)), saturn_actor_indexof(actor));
                        if (saturn_timeline_exists(timelineID.c_str())) {
                            for (auto kf : k_frame_keys[timelineID].second) {
                                if (frames < kf.position) frames = kf.position + 1;
                            }
                        }
                    }
                    for (int i = 0; i < 60; i++) {
                        std::string timelineID = saturn_keyframe_get_mario_timeline_id("k_objbone_" + (i == 0 ? "t" : std::to_string(i - 1)), saturn_actor_indexof(actor));
                        if (saturn_timeline_exists(timelineID.c_str())) {
                            for (auto kf : k_frame_keys[timelineID].second) {
                                if (frames < kf.position) frames = kf.position + 1;
                            }
                        }
                    }
                    int num_indices = 6 * actor->num_bones;
                    int num_values = 3 * actor->num_bones * frames;
                    u16* indices = (u16*)malloc(sizeof(u16) * num_indices);
                    u16* values = (u16*)malloc(sizeof(u16) * num_values);
                    float rotations[3 * 60];
                    for (int i = 0; i < actor->num_bones; i++) {
                        indices[i * 6 + 0] = indices[i * 6 + 2] = indices[i * 6 + 4] = frames;
                        indices[i * 6 + 1] = (i * 3 + 0) * frames;
                        indices[i * 6 + 3] = (i * 3 + 1) * frames;
                        indices[i * 6 + 5] = (i * 3 + 2) * frames;
                    }
                    for (int i = 0; i < frames; i++) {
                        get_animation_rotations(actor, rotations, i);
                        for (int j = 0; j < actor->num_bones; j++) {
                            float multiplier = j == 0 ? 1 : (65536 / 360.f);
                            values[(j * 3 + 0) * frames + i] = rotations[j * 3 + 0] * multiplier;
                            values[(j * 3 + 1) * frames + i] = rotations[j * 3 + 1] * multiplier;
                            values[(j * 3 + 2) * frames + i] = rotations[j * 3 + 2] * multiplier;
                        }
                    }
                    for (auto timeline : k_frame_keys) {
                        saturn_keyframe_apply(timeline.first, k_current_frame);
                    }
                    struct BinaryStream* data = anim_formats[animformat].encode(frames, indices, values, num_indices, num_values);
                    std::string filepath = save_file_dialog("Save Animation", { anim_formats[animformat].filter_name, anim_formats[animformat].filter, "All Files", "*" });
                    if (filepath != "") {
                        std::ofstream stream = std::ofstream(filepath, std::ios::binary);
                        stream.write((char*)data->data, data->length);
                        stream.close();
                    }
                    free(indices);
                    free(values);
                    free(data->data);
                    free(data);
                }
                ImGui::SameLine();
                ImGui::Text("as");
                ImGui::SameLine();
                ImGui::PushItemWidth(80);
                if (ImGui::BeginCombo("###animformat", anim_formats[animformat].combo_item)) {
                    for (int i = 0; i < anim_formats.size(); i++) {
                        bool selected = i == animformat;
                        if (ImGui::Selectable(anim_formats[i].combo_item, selected)) animformat = i;
                    }
                    ImGui::EndCombo();
                }
                ImGui::PopItemWidth();
                ImGui::TreePop();
            }
            ImGui::Separator();
            if (ImGui::BeginMenu("Sample")) {
                imgui_machinima_animation_player(actor, true);
                ImGui::EndMenu();
            }
            if (ImGui::Button("Randomize")) {
                for (int i = 0; i < 60; i++) {
                    actor->bones[i][0] = (rand() % 65536) / 65536.f * 360;
                    actor->bones[i][1] = (rand() % 65536) / 65536.f * 360;
                    actor->bones[i][2] = (rand() % 65536) / 65536.f * 360;
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Next Frame")) k_current_frame++;

#define BONE_ENTRY(name) {                                      \
                ImGui::TableSetColumnIndex(0);                   \
                ImGui::PushItemWidth(200);                        \
                ImGui::DragFloat3(name, actor->bones[currbone++]); \
                ImGui::PopItemWidth();                              \
                ImGui::TableSetColumnIndex(1);                       \
                saturn_keyframe_popout(KF_BONE_ID);                   \
                ImGui::TableNextRow();                                 \
            }
            if (ImGui::BeginTable("Bone Editor", 2)) {
                ImGui::TableNextRow();
                if (actor->obj_model == MODEL_MARIO) {
#define KF_BONE_ID "k_mariobone_" + (currbone == 1 ? "t" : std::to_string(currbone - 1))
                    BONE_ENTRY("Translation"    );
                    BONE_ENTRY("Root"           );
                    BONE_ENTRY("Body"           );
                    BONE_ENTRY("Torso"          );
                    BONE_ENTRY("Head"           );
                    BONE_ENTRY("Left Arm"       );
                    BONE_ENTRY("Upper Left Arm" );
                    BONE_ENTRY("Lower Left Arm" );
                    BONE_ENTRY("Left Hand"      );
                    BONE_ENTRY("Right Arm"      );
                    BONE_ENTRY("Upper Right Arm");
                    BONE_ENTRY("Lower Right Arm");
                    BONE_ENTRY("Right Hand"     );
                    BONE_ENTRY("Left Leg"       );
                    BONE_ENTRY("Upper Left Leg" );
                    BONE_ENTRY("Lower Left Leg" );
                    BONE_ENTRY("Left Foot"      );
                    BONE_ENTRY("Right Leg"      );
                    BONE_ENTRY("Upper Right Leg");
                    BONE_ENTRY("Lower Right Leg");
                    BONE_ENTRY("Right Foot"     );
#undef KF_BONE_ID
                }
                else {
                    for (int i = 0; i < actor->num_bones; i++) {
#define KF_BONE_ID "k_objbone_" + (currbone == 1 ? "t" : std::to_string(currbone - 2))
                        BONE_ENTRY(i == 0 ? "Translation" : i == 1 ? "Root" : ("Bone " + std::to_string(i - 1)).c_str());
#undef KF_BONE_ID
                    }
                }
                ImGui::EndTable();
            }
#undef BONE_ENTRY
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    if (actor->custom_bone && !sampling) return;
    ImGui::Separator();
    if (sampling) {
        if (ImGui::SliderFloat("Frame", &sampling_frame, 0, sampling_animation.unk08 - 1, "%.0f")) should_update_sample = true;
        if (!sampling_anim_loaded || !should_update_sample) return;
        saturn_sample_animation(actor, &sampling_animation, sampling_frame);
        return;
    }
    ImGui::SliderFloat("Frame", &actor->animstate.frame, 0, actor->animstate.length - 1, "%.0f");
    saturn_keyframe_popout("k_mario_anim_frame");
    saturn_keyframe_popout_next_line("k_mario_anim");
    if (saturn_timeline_exists(saturn_keyframe_get_mario_timeline_id("k_mario_anim_frame", saturn_actor_indexof(actor)).c_str()))
        saturn_keyframe_helper("k_mario_anim_frame", &actor->animstate.frame, actor->animstate.length - 1);
    ImGui::PushItemWidth(100);
    ImGui::DragInt("Y Translation", &actor->animstate.yTransform, 1.0f, -32768, 32767, "%d\n", ImGuiSliderFlags_AlwaysClamp);
    ImGui::PopItemWidth();
    ImGui::SameLine();
    imgui_bundled_help_marker("Recommended to keep at default value (189) for Mario's animations; 0 for everything else");
}

/*void imgui_machinima_animation_player() {
    selected_animation = (MarioAnimID)current_sanim_id;
    
    if (is_anim_playing || keyframe_playing)
        ImGui::BeginDisabled();

    const char* anim_groups[] = { "Movement (50)", "Actions (25)", "Automatic (27)", "Damage/Deaths (22)",
        "Cutscenes (23)", "Water (16)", "Climbing (20)", "Object (24)", ICON_FK_FILE_O " CUSTOM...", "All (209)",
        (std::string("Favorites (") + std::to_string(favorite_anims.size()) + ")").c_str() };
    int animArraySize = (canim_array.size() > 0) ? IM_ARRAYSIZE(anim_groups) : IM_ARRAYSIZE(anim_groups) - 1;

    ImGui::PushItemWidth(290);
    if (ImGui::BeginCombo("###anim_group", anim_groups[current_sanim_group_index], ImGuiComboFlags_HeightLarge)) {
        for (int n = 0; n < animArraySize; n++) {
            const bool is_selected = (current_sanim_group_index == n);
            if (ImGui::Selectable(anim_groups[n], is_selected)) {
                current_sanim_group_index = n;
                if (current_sanim_group_index == 8) {
                    current_animation.custom = true;
                    current_sanim_id = MARIO_ANIM_A_POSE;

                    current_sanim_name = canim_array[custom_anim_index];
                    anim_preview_name = current_sanim_name;
                    anim_preview_name = anim_preview_name.substr(0, anim_preview_name.size() - 5);
                    saturn_read_mcomp_animation(anim_preview_name);
                    current_animation.loop = current_canim_looping;
                } else if (current_sanim_group_index == 9) {
                    current_anim_map = sanim_maps[9];
                    current_animation.custom = false;

                    current_anim_map.clear();
                    for (int i = 0; i < 8; i++) {
                        for (auto& anim : sanim_maps[i]) {
                            current_anim_map.insert(anim);
                        }
                    }
                    current_sanim_index = current_anim_map.begin()->first.first;
                    current_sanim_name = current_anim_map.begin()->first.second;
                    current_sanim_id = current_anim_map.begin()->second;
                    anim_preview_name = current_sanim_name;
                } else if (current_sanim_group_index == 10) {
                    current_animation.custom = false;
                    current_sanim_id = favorite_anims.size() == 0 ? MARIO_ANIM_A_POSE : favorite_anims[0];
                    current_sanim_index = 0;
                    current_sanim_name = saturn_animations_list[current_sanim_id];
                    anim_preview_name = current_sanim_name;
                } else {
                    current_anim_map = sanim_maps[current_sanim_group_index];
                    current_animation.custom = false;
                    current_sanim_index = current_anim_map.begin()->first.first;
                    current_sanim_name = current_anim_map.begin()->first.second;
                    current_sanim_id = current_anim_map.begin()->second;
                    anim_preview_name = current_sanim_name;
                }
            }

            if (n == 8) {
                ImGui::SameLine();
                imgui_bundled_help_marker("These are custom METAL Composer+ JSON animations.\nPlace in dynos/anims.");
            }

            // Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
            if (is_selected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    if ((current_sanim_group_index == 8 && canim_array.size() >= 20) || current_sanim_group_index == 9) {
        ImGui::InputTextWithHint("###anim_search_text", ICON_FK_SEARCH " Search animations...", animSearchTerm, IM_ARRAYSIZE(animSearchTerm), ImGuiInputTextFlags_AutoSelectAll);
    } else {
        // If our anim list is reloaded, and we now have less than 20 anims, this can cause filter issues if not reset to nothing
        if (animSearchTerm != "") strcpy(animSearchTerm, "");
    }
    string animSearchLower = animSearchTerm;
    std::transform(animSearchLower.begin(), animSearchLower.end(), animSearchLower.begin(),
        [](unsigned char c){ return std::tolower(c); });

    ImGui::PopItemWidth();

    ImGui::BeginChild("###anim_box_child", ImVec2(290, 100), true);
    if (current_sanim_group_index == 8) {
        for (int i = 0; i < canim_array.size(); i++) {
            current_sanim_name = canim_array[i];
            if (canim_array[i].find("/") != string::npos)
                current_sanim_name = ICON_FK_FOLDER " " + canim_array[i].substr(0, canim_array[i].size() - 1);

            if (canim_array[i] == "../")
                current_sanim_name = ICON_FK_FOLDER " ../";

            const bool is_selected = (custom_anim_index == i);

            // If we're searching, only include anims with the search keyword in the name
            // Also convert to lowercase
            if (animSearchLower != "") {
                string nameLower = current_sanim_name;
                std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(),
                    [](unsigned char c1){ return std::tolower(c1); });

                if (nameLower.find(animSearchLower) == string::npos) {
                    continue;
                }
            }

            if (ImGui::Selectable(current_sanim_name.c_str(), is_selected)) {
                custom_anim_index = i;
                current_sanim_name = canim_array[i];
                anim_preview_name = current_sanim_name;
                current_animation.loop = current_canim_looping;
                // Remove .json extension
                if (canim_array[i].find(".json") != string::npos) {
                    anim_preview_name = anim_preview_name.substr(0, anim_preview_name.size() - 5);
                    saturn_read_mcomp_animation(anim_preview_name);
                    current_animation.loop = current_canim_looping;
                } else if (canim_array[i].find("/") != string::npos) {
                    saturn_load_anim_folder(anim_preview_name, &custom_anim_index);
                    current_sanim_name = canim_array[custom_anim_index];
                    anim_preview_name = current_sanim_name;
                    anim_preview_name = anim_preview_name.substr(0, anim_preview_name.size() - 5);
                    saturn_read_mcomp_animation(anim_preview_name);
                    current_animation.loop = current_canim_looping;
                }
                // Stop anim
                is_anim_playing = false;
                is_anim_paused = false;
                using_chainer = false;
                chainer_index = 0;
                current_animation.custom = true;
                current_animation.id = i;
            }

            if (ImGui::BeginPopupContextItem()) {
                ImGui::Text(canim_array[i].c_str());
                imgui_bundled_tooltip((current_anim_dir_path + canim_array[i]).c_str());
                ImGui::Separator();
                ImGui::TextDisabled("%i MComp+ animation(s)", canim_array.size());
                if (ImGui::Button("Refresh###refresh_canim")) {
                    saturn_load_anim_folder(current_anim_dir_path, &custom_anim_index);
                    current_sanim_name = canim_array[custom_anim_index];
                    anim_preview_name = current_sanim_name;
                    anim_preview_name = anim_preview_name.substr(0, anim_preview_name.size() - 5);
                    saturn_read_mcomp_animation(anim_preview_name);
                    current_animation.loop = current_canim_looping;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }

            if (is_selected)
                ImGui::SetItemDefaultFocus();
        }
    } else if (current_sanim_group_index == 9) {
        for (int i = 0; i < 209; i++) {
            const bool is_selected = (current_sanim_index == i);
            current_sanim_name = saturn_animations_list[i];

            // If we're searching, only include anims with the search keyword in the name
            // Also convert to lowercase
            if (animSearchLower != "") {
                string nameLower = current_sanim_name;
                std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(),
                    [](unsigned char c1){ return std::tolower(c1); });

                if (nameLower.find(animSearchLower) == string::npos) {
                    continue;
                }
            }

            auto position = std::find(favorite_anims.begin(), favorite_anims.end(), i);
            bool contains = position != favorite_anims.end();
            if (ImGui::SmallButton((std::string(contains ? ICON_FK_STAR : ICON_FK_STAR_O) + "###" + std::to_string(i)).c_str())) {
                if (contains) favorite_anims.erase(position);
                else favorite_anims.push_back(i);
                saturn_save_favorite_anims();
            }
            ImGui::SameLine();
            if (ImGui::Selectable(current_sanim_name.c_str(), is_selected)) {
                current_sanim_index = i;
                current_sanim_name = saturn_animations_list[i];
                current_sanim_id = i;
                current_animation.custom = false;
                current_animation.id = i;
            }

            if (is_selected)
                ImGui::SetItemDefaultFocus();
        }
    } else if (current_sanim_group_index == 10) {
        int idx = 0;
        for (int anim : favorite_anims) {
            current_sanim_name = saturn_animations_list[anim];
            const bool is_selected = (current_sanim_id == anim);
            auto position = std::find(favorite_anims.begin(), favorite_anims.end(), anim);
            if (ImGui::SmallButton((std::string(ICON_FK_STAR) + "###" + std::to_string(anim)).c_str())) {
                favorite_anims.erase(position);
                saturn_save_favorite_anims();
            }
            ImGui::SameLine();
            if (ImGui::Selectable(current_sanim_name.c_str(), is_selected)) {
                current_sanim_index = idx;
                current_sanim_name = saturn_animations_list[anim];
                current_sanim_id = anim;
                current_animation.custom = false;
                current_animation.id = anim;
            }
            if (is_selected) ImGui::SetItemDefaultFocus();
            idx++;
        }
    } else {
        for (auto &[a,b]:current_anim_map) {
            current_sanim_index = a.first;
            current_sanim_name = a.second;

            const bool is_selected = (current_sanim_id == b);
            auto position = std::find(favorite_anims.begin(), favorite_anims.end(), b);
            bool contains = position != favorite_anims.end();
            if (ImGui::SmallButton((std::string(contains ? ICON_FK_STAR : ICON_FK_STAR_O) + "###" + std::to_string(b)).c_str())) {
                if (contains) favorite_anims.erase(position);
                else favorite_anims.push_back(b);
                saturn_save_favorite_anims();
            }
            ImGui::SameLine();
            if (ImGui::Selectable(current_sanim_name.c_str(), is_selected)) {
                current_sanim_index = a.first;
                current_sanim_name = a.second;
                current_sanim_id = b;
                anim_preview_name = current_sanim_name;
                current_animation.custom = false;
                current_animation.id = b;
            }

            if (is_selected)
                ImGui::SetItemDefaultFocus();
        }
    }
    ImGui::EndChild();

    if (is_anim_playing && !keyframe_playing)
        ImGui::EndDisabled();

    ImGui::PushItemWidth(290);
    ImGui::Separator();

    // Metadata
    if (current_animation.custom && custom_anim_index != -1) {
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 5.0f);
        ImGui::BeginChild("###anim_metadata", ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 3), true, ImGuiWindowFlags_NoScrollbar);
        ImGui::Text(current_canim_name.c_str());
        ImGui::TextDisabled(("@ " + current_canim_author).c_str());
        ImGui::EndChild();
        ImGui::PopStyleVar();
    }

    ImGui::PopItemWidth();

    // Player
    if (is_anim_playing) {
        if (current_animation.custom) {
            ImGui::Text("Now Playing: %s", current_canim_name.c_str());
            string anim_credit1 = ("By " + current_canim_author);
            ImGui::SameLine(); imgui_bundled_help_marker(anim_credit1.c_str());
            if (using_chainer) ImGui::Text("Chainer: Enabled");
        } else {
            ImGui::Text("Now Playing: %s", anim_preview_name.c_str());
        }

        if (ImGui::Button("Stop")) {
            is_anim_playing = false;
            is_anim_paused = false;
            using_chainer = false;
            chainer_index = 0;
        }
        ImGui::SameLine(); ImGui::Checkbox("Loop", &current_animation.loop);
        ImGui::SameLine();
        if (ImGui::Checkbox("Hang", &current_animation.hang) && !current_animation.hang) {
            is_anim_playing = false;
            is_anim_paused = false;
            using_chainer = false;
            chainer_index = 0;
        }
        
        ImGui::PushItemWidth(150);
        ImGui::SliderInt("Frame###animation_frames", &current_anim_frame, 0, current_anim_length - 1);
        ImGui::PopItemWidth();
        ImGui::Checkbox("Paused###animation_paused", &is_anim_paused);
    } else {
        ImGui::Text("");
        if (ImGui::Button("Play")) {
            anim_play_button();
        }
        ImGui::SameLine(); ImGui::Checkbox("Loop", &current_animation.loop);
        ImGui::SameLine(); ImGui::Checkbox("Hang", &current_animation.hang);

        ImGui::PushItemWidth(150);
        ImGui::SliderFloat("Speed###anim_speed", &current_animation.speed, 0.1f, 2.0f);
        ImGui::PopItemWidth();
        if (current_animation.speed != 1.0f) {
            if (ImGui::Button("Reset###reset_anim_speed"))
                current_animation.speed = 1.0f;
        }
    }
    if (keyframe_playing) ImGui::EndDisabled();
    saturn_keyframe_popout_next_line("k_mario_anim");
}*/
