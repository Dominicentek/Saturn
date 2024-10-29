#include "saturn.h"

#include <SDL2/SDL_keyboard.h>
#include <SDL2/SDL_scancode.h>
#include <string>
#include <iostream>
#include <algorithm>
#include <thread>
#include <map>
#include <SDL2/SDL.h>
#include "saturn/saturn_animation_ids.h"

#include "PR/os_cont.h"
#include "data/dynos.cpp.h"
#include "engine/math_util.h"
#include "saturn/imgui/saturn_imgui.h"
#include "saturn/imgui/saturn_imgui_machinima.h"
#include "saturn/imgui/saturn_imgui_chroma.h"
#include "libs/sdl2_scancode_to_dinput.h"
#include "pc/configfile.h"
#include "saturn/filesystem/saturn_projectfile.h"
#include "saturn/imgui/saturn_imgui_dynos.h"
#include "saturn/filesystem/saturn_locationfile.h"
#include "data/dynos.cpp.h"
#include "saturn/filesystem/saturn_animfile.h"
#include "saturn/saturn_animations.h"
#include "saturn/saturn_colors.h"
#include "saturn/saturn_rom_extract.h"
#include "saturn/saturn_timelines.h"
#include "saturn/saturn_actors.h"

extern "C" {
#include "engine/surface_load.h"
#include "game/game_init.h"
#include "engine/graph_node.h"
#include "game/rendering_graph_node.h"
#include "audio/external.h"
#include "engine/surface_collision.h"
#include "game/object_collision.h"
#include "game/object_list_processor.h"
}

bool mario_exists;

bool camera_frozen = true;
float camera_speed = 0.0f;
float camera_focus = 1.f;
float camera_savestate_mult = 1.f;
bool camera_fov_smooth = false;
bool is_camera_moving;

bool camera_view_enabled;
bool camera_view_moving;
bool camera_view_zooming;
bool camera_view_rotating;
int camera_view_move_x;
int camera_view_move_y;

struct MouseState mouse_state;
struct MouseState prev_mouse_state;

bool enable_head_rotations = false;
bool enable_shadows = false;
bool rainbow = false;
bool enable_dust_particles = false;
bool enable_torso_rotation = true;
bool enable_fog = true;
float run_speed = 127.0f;
bool can_fall_asleep = false;
int saturnModelState = 0;
bool linkMarioScale = true;
bool is_spinning;
float spin_mult = 1.0f;

bool using_chainer;
int chainer_index;
enum MarioAnimID selected_animation = MARIO_ANIM_BREAKDANCE;
int current_anim_frame;
int current_anim_id;
int current_anim_length;
bool is_anim_playing = false;
bool is_anim_paused = false;
int paused_anim_frame;
struct AnimationState current_animation = {
    .custom = false,
    .id = MarioAnimID::MARIO_ANIM_RUNNING,
};

float this_face_angle;

bool limit_fps = true;

// discord
bool has_discord_init;

// private
bool is_chroma_keying = false;
bool prev_quicks[3];
int lastCourseNum = -1;
int saturn_launch_timer;

float* active_key_float_value = &camera_fov;
bool* active_key_bool_value;

s32 active_data_type = KEY_FLOAT;
bool keyframe_playing;
bool k_popout_open;
bool k_popout_focused;
int mcam_timer = 0;
int k_current_frame = 0;
int k_previous_frame = 0;
int k_curr_curve_type = 0;

int k_current_anim = -1;

std::map<std::string, std::pair<KeyframeTimeline, std::vector<Keyframe>>> k_frame_keys = {};

int k_last_passed_index = 0;
int k_distance_between;
int k_current_distance;
float k_static_increase_value;
int k_last_placed_frame;
bool k_loop;
bool k_animating_camera;
float k_c_pos0_incr;
float k_c_pos1_incr;
float k_c_pos2_incr;
float k_c_foc0_incr;
float k_c_foc1_incr;
float k_c_foc2_incr;
float k_c_rot0_incr;
float k_c_rot1_incr;
float k_c_rot2_incr;
bool has_set_initial_k_frames;

bool is_cc_editing;

bool autoChroma;
bool autoChromaLevel;
bool autoChromaObjects;

u8 activatedToads = 0;

f32 mario_headrot_yaw = 0;
f32 mario_headrot_pitch = 0;
f32 mario_headrot_speed = 10.0f;

struct Object* saturn_camera_object = nullptr;

bool setting_mario_struct_pos = false;

struct Object (*world_simulation_data)[960] = nullptr;
u16* world_simulation_seeds = nullptr;
int world_simulation_frames = 0;
int world_simulation_prev_frame = 0;
float world_simulation_curr_frame = 0;
u16 world_simulation_seed = 0;

extern struct Object gObjectPool[960];
extern u16 gRandomSeed16;

std::vector<Gfx*> gfxs = {};
bool simulating_world = false;

void saturn_add_alloc_dl(Gfx* gfx) {
    gfxs.push_back(gfx);
}

void saturn_update_object_collision() {
    clear_dynamic_surfaces();
    for (int i = 0; i < 960; i++) {
        if (gObjectPool[i].objList != OBJ_LIST_SURFACE) continue;
        gCurrentObject = &gObjectPool[i];
        load_object_collision_model();
    }
}

void saturn_clear_simulation() {
    if (!world_simulation_data) return;
    memcpy(gObjectPool, world_simulation_data[0], sizeof(*world_simulation_data));
    free(world_simulation_data);
    free(world_simulation_seeds);
    world_simulation_frames = 0;
    world_simulation_curr_frame = 0;
    world_simulation_data = nullptr;
}

void saturn_simulation_step(int frame) {
    simulating_world = true;
    Vec3f prevMarioStructPos;
    float prevMarioStructAngle = gMarioState->fAngle;
    vec3f_copy(prevMarioStructPos, gMarioState->pos);
    saturn_keyframe_apply("k_mariostruct_x", frame);
    saturn_keyframe_apply("k_mariostruct_y", frame);
    saturn_keyframe_apply("k_mariostruct_z", frame);
    saturn_keyframe_apply("k_mariostruct_angle", frame);
    gMarioObject->oPosX = gMarioState->pos[0];
    gMarioObject->oPosY = gMarioState->pos[1];
    gMarioObject->oPosZ = gMarioState->pos[2];
    area_update_objects();
    Gfx* head = gDisplayListHead;
    geo_process_root(gCurrentArea->unk04, NULL, NULL, 0);
    gDisplayListHead = head;
    for (Gfx* gfx : gfxs) {
        free(gfx);
    }
    gfxs.clear();
    vec3f_copy(gMarioState->pos, prevMarioStructPos);
    gMarioState->fAngle = prevMarioStructAngle;
    simulating_world = false;
}

void saturn_simulation_update() {
    int prev = world_simulation_prev_frame;
    int curr = world_simulation_curr_frame;
    if (prev == curr) return;

    struct GraphNodeObject_sub animdata;
    memcpy(&animdata, &gMarioObject->header.gfx.unk38, sizeof(animdata));

    int sample_frame_prev = prev / configWorldsimSteps;
    int sample_frame_curr = curr / configWorldsimSteps;
    int start_from = prev;
    if (sample_frame_prev != sample_frame_curr || prev > curr) {
        start_from = sample_frame_curr * configWorldsimSteps;
        gRandomSeed16 = world_simulation_seeds[curr];
        memcpy(gObjectPool, world_simulation_data[sample_frame_curr], sizeof(*world_simulation_data));
    }

    for (int i = start_from; i < curr; i++) {
        saturn_simulation_step(i);
    }

    memcpy(&gMarioObject->header.gfx.unk38, &animdata, sizeof(animdata));
    gAreaUpdateCounter = world_simulation_curr_frame;

    world_simulation_prev_frame = curr;
}

void saturn_simulate(int frames) {
    saturn_clear_simulation();
    if (frames <= 0) return;
    simulating_world = true;
    world_simulation_frames = frames;
    world_simulation_data = (struct Object(*)[960])malloc(sizeof(*world_simulation_data) * ceilf(frames / (float)configWorldsimSteps));
    world_simulation_seeds = (u16*)malloc(sizeof(u16) * ceilf(frames / (float)configWorldsimSteps));
    memcpy(world_simulation_data[0], gObjectPool, sizeof(*world_simulation_data));
    world_simulation_seed = gRandomSeed16;
    for (int i = 1; i < frames; i++) {
        saturn_simulation_step(i);
        if (i % configWorldsimSteps != 0) continue;
        memcpy(world_simulation_data[i / configWorldsimSteps], gObjectPool, sizeof(*world_simulation_data));
        world_simulation_seeds[i / configWorldsimSteps] = gRandomSeed16;
    }
}

extern "C" {
#include "game/camera.h"
#include "game/area.h"
#include "game/level_update.h"
#include "engine/level_script.h"
#include "game/game_init.h"
#include "data/dynos.h"
#include "pc/configfile.h"
#include "game/mario.h"
#include <mario_animation_ids.h>
#include <sm64.h>
#include "pc/controller/controller_keyboard.h"
#include "pc/cheats.h"
#include "game/save_file.h"
}

using namespace std;

#define FREEZE_CAMERA	    0x0800
#define CYCLE_EYE_STATE     0x0100
#define LOAD_ANIMATION      0x0200
#define TOGGLE_MENU         0x0400

unsigned int chromaKeyColorR = 0;
unsigned int chromaKeyColorG = 255;
unsigned int chromaKeyColorB = 0;

int autosaveDelay = -1;

u16 gChromaKeyColor = 0x07C1;
u16 gChromaKeyBackground = 0;

u8 godmode_temp_off = false;

bool extract_thread_began = false;
bool extraction_finished = false;
float extraction_progress = -1;
int extract_return_code = 0;

int marios_spawned = 0;

extern void saturn_run_chainer();

float key_increase_val(std::vector<float> vecfloat) {
    float next_val = vecfloat.at(k_last_passed_index + 1);
    float this_val = vecfloat.at(k_last_passed_index);

    return (next_val - this_val) / k_distance_between;
}

bool timeline_has_id(std::string id) {
    if (k_frame_keys.size() > 0) {
        for (auto& entry : k_frame_keys) {
            for (Keyframe keyframe : entry.second.second) {
                if (keyframe.timelineID == id)
                    return true;
            }
        }
    }

    return false;
}

// SATURN Machinima Functions

float inpreccam_distfrommario = 500.f;
s16 inpreccam_yaw = 0;
s16 inpreccam_pitch = 0;
Vec3f inpreccam_pos;
Vec3f inpreccam_focus;
bool inprec_keep_angle = false;

void saturn_update() {

    // Keybinds

    if (mario_exists) {
        if (!saturn_disable_sm64_input()) {
            if (gPlayer1Controller->buttonPressed & D_JPAD) {
                saturn_actor_stop_recording();
            }
            if (gPlayer1Controller->buttonPressed & R_JPAD) {
                saturn_imgui_capture_next_frame("");
            }
        }
    }

    bool mouse_l, mouse_r;
    prev_mouse_state = mouse_state;
    ImGuiIO& io = ImGui::GetIO();
    mouse_state.x = io.MousePos.x;
    mouse_state.y = io.MousePos.y;
    mouse_state.held = 0;
    for (int i = 0; i < 5; i++) {
        mouse_state.held |= io.MouseDown[i] << i;
    }
    mouse_state.pressed = mouse_state.held & ~prev_mouse_state.held;
    mouse_state.released = ~mouse_state.held & prev_mouse_state.held;
    mouse_state.x_diff = mouse_state.x - prev_mouse_state.x;
    mouse_state.y_diff = mouse_state.y - prev_mouse_state.y;

    // Machinima

    machinimaMode = (camera_frozen) ? 1 : 0;
    machinimaKeyframing = (keyframe_playing && active_data_type == KEY_CAMERA);

    bool is_focused = is_focused_on_game();
    if (mouse_state.pressed & (MOUSEBTN_MASK_L | MOUSEBTN_MASK_R)) {
        mouse_state.x_orig = mouse_state.x;
        mouse_state.y_orig = mouse_state.y;
        mouse_state.focused_on_game = is_focused;
        if (mouse_state.focused_on_game) mouse_state.update_camera = true;
    }
    if (mouse_state.held & (MOUSEBTN_MASK_L | MOUSEBTN_MASK_R))
        mouse_state.dist_travelled = sqrt(
            (mouse_state.x - mouse_state.x_orig) * (mouse_state.x - mouse_state.x_orig) +
            (mouse_state.y - mouse_state.y_orig) * (mouse_state.y - mouse_state.y_orig)
        );
    else mouse_state.update_camera = false;

    if (!saturn_disable_sm64_input()) {
        cameraRollLeft  = SDL_GetKeyboardState(NULL)[SDL_SCANCODE_V];
        cameraRollRight = SDL_GetKeyboardState(NULL)[SDL_SCANCODE_B];
    }

    if (!keyframe_playing && !camera_frozen) {
        gLakituState.focHSpeed = camera_focus * camera_savestate_mult * 0.8f;
        gLakituState.focVSpeed = camera_focus * camera_savestate_mult * 0.3f;
        gLakituState.posHSpeed = camera_focus * camera_savestate_mult * 0.3f;
        gLakituState.posVSpeed = camera_focus * camera_savestate_mult * 0.3f;
    }

    camera_default_fov = camera_fov + 5.0f;

    //SDL_GetMouseState(&camera_view_move_x, &camera_view_move_y);

    if (gCurrLevelNum == LEVEL_SA || autoChroma) {
        if (!is_chroma_keying) is_chroma_keying = true;
    }

    //if (gCurrLevelNum == LEVEL_SA && !is_chroma_keying) {
        //is_chroma_keying = true;
        // Called once when entering Chroma Key Stage
        //prev_quicks[0] = enable_shadows;
        //prev_quicks[1] = enable_dust_particles;
        //prev_quicks[2] = configHUD;
        //enable_shadows = false;
        //enable_dust_particles = false;
        //configHUD = false;
    //}
    if (gCurrLevelNum != LEVEL_SA && !autoChroma) {
        if (!is_chroma_keying) is_chroma_keying = false;
        // Called once when exiting Chroma Key Stage
        //enable_shadows = prev_quicks[0];
        //enable_dust_particles = prev_quicks[1];
        //configHUD = prev_quicks[2];
    }

    if (splash_finished) saturn_launch_timer++;
    if (gCurrLevelNum == LEVEL_SA && saturn_launch_timer <= 2 && splash_finished) {
        gMarioState->faceAngle[1] = 0;
        if (gCamera) { // i hate the sm64 camera system aaaaaaaaaaaaaaaaaa
            float dist = 0;
            s16 yaw, pitch;
            vec3f_set(gCamera->pos, 0.f, 192.f, 264.f);
            vec3f_set(gCamera->focus, 0.f, 181.f, 28.f);
            vec3f_copy(freezecamPos, gCamera->pos);
            vec3f_copy(cameraPos, freezecamPos);
            vec3f_get_dist_and_angle(gCamera->pos, gCamera->focus, &dist, &pitch, &yaw);
            freezecamYaw = cameraYaw = (float)yaw;
            freezecamPitch = cameraPitch = (float)pitch;
            vec3f_copy(gLakituState.pos, gCamera->pos);
            vec3f_copy(gLakituState.focus, gCamera->focus);
            vec3f_copy(gLakituState.goalPos, gCamera->pos);
            vec3f_copy(gLakituState.goalFocus, gCamera->focus);
            gCamera->yaw = calculate_yaw(gCamera->focus, gCamera->pos);
            gLakituState.yaw = gCamera->yaw;
        }
    }

    // Keyframes

    if (!k_popout_open) k_popout_focused = false;
    if (keyframe_playing) {
        if (timeline_has_id("k_c_camera_pos0")) {
            // Prevents smoothing for sharper, more consistent panning
            gLakituState.focHSpeed = 15.f * camera_focus * 0.8f;
            gLakituState.focVSpeed = 15.f * camera_focus * 0.3f;
            gLakituState.posHSpeed = 15.f * camera_focus * 0.3f;
            gLakituState.posVSpeed = 15.f * camera_focus * 0.3f;
        }
        
        bool end = true;
        for (const auto& entry : k_frame_keys) {
            if (!saturn_keyframe_apply(entry.first, k_current_frame)) end = false;
        }
        if (end) {
            if (saturn_imgui_is_capturing_video()) saturn_imgui_stop_capture();
            else if (k_loop) k_current_frame = 0;
        }

        if (timeline_has_id("k_angle"))
            gMarioState->faceAngle[1] = (s16)(this_face_angle * 182.04f);

        k_current_frame++;
    }

    // Camera

    // im sorry for this mess
    f32 dist;
    float   kmove_x = 0,   kmove_y = 0;
    float krotate_x = 0, krotate_y = 0;
    float kzoom = 0;
    float   mmove_x = 0,   mmove_y = 0;
    float mrotate_x = 0, mrotate_y = 0;
    float mzoom = 0;
    bool inprec = saturn_actor_is_recording_input();
    const Uint8* kb = SDL_GetKeyboardState(NULL);

#define inv(var) ((var) * -2 + 1)

    if (saturn_imgui_is_capturing_video()); // do nothing
    else if (!saturn_imgui_is_orthographic()) {
        float mzoom_modif = 200;
        if (inprec) {
            mzoom_modif = 50;
            mzoom_modif *= configCamCtrlMouseInprecZoomSens * inv(configCamCtrlMouseInprecZoomInv);
        }
        else mzoom_modif *= configCamCtrlMouseZoomSens * inv(configCamCtrlMouseZoomInv);
        if (kb[SDL_SCANCODE_LSHIFT]) mzoom_modif /= 4;
        if (kb[SDL_SCANCODE_LCTRL]) mzoom_modif *= 4;
        mzoom = mouse_state.scrollwheel * mzoom_modif;

        if (mouse_state.update_camera) { // mouse
            bool pan = mouse_state.held & MOUSEBTN_MASK_L;
            bool rotate = mouse_state.held & MOUSEBTN_MASK_R;
            if (inprec) { pan |= rotate; rotate = false; }
            float *x = &mmove_x, *y = &mmove_y;
            if (rotate) { x = &mrotate_x; y = &mrotate_y; }
            *x = mouse_state.x_diff;
            *y = mouse_state.y_diff;
            if (inprec) {
                *x *= 64 * configCamCtrlMouseRotSens * inv(configCamCtrlMouseInprecRotInvX);
                *y *= 64 * configCamCtrlMouseRotSens * inv(configCamCtrlMouseInprecRotInvY);
                
            }
            else if (rotate) {
                *x *= 24 * configCamCtrlMouseRotSens * inv(configCamCtrlMouseRotInvX);
                *y *= 24 * configCamCtrlMouseRotSens * inv(configCamCtrlMouseRotInvY);
            }
            else {
                *x *= 1.5f * configCamCtrlMousePanSens * inv(configCamCtrlMousePanInvX);
                *y *= 1.5f * configCamCtrlMousePanSens * inv(configCamCtrlMousePanInvY);
            }
        }

        if (!inprec && !saturn_disable_sm64_input()) { // keyboard
            bool rotate = kb[SDL_SCANCODE_O];
            bool pan = !rotate;
            bool up = kb[SDL_SCANCODE_P] || rotate;

            float *x = &kmove_x, *y = &kmove_y;
            if (rotate) { x = &krotate_x; y = &krotate_y; }

            if (up) {
                if (kb[SDL_SCANCODE_W]) (*y)++;
                if (kb[SDL_SCANCODE_S]) (*y)--;
            }
            else {
                if (kb[SDL_SCANCODE_W] || kb[SDL_SCANCODE_S]) kzoom = 0;
                if (kb[SDL_SCANCODE_W]) kzoom++;
                if (kb[SDL_SCANCODE_S]) kzoom--;
            }
            if (kb[SDL_SCANCODE_A]) (*x)++;
            if (kb[SDL_SCANCODE_D]) (*x)--;

            float modif = 60;
            if (kb[SDL_SCANCODE_LSHIFT]) modif /= 4;
            if (kb[SDL_SCANCODE_LCTRL]) modif *= 4;
            if (rotate) modif *= 8;
            *x *= modif * (rotate ? configCamCtrlKeybRotSens : configCamCtrlKeybPanSens) * inv(rotate ? configCamCtrlKeybRotInvX : configCamCtrlKeybPanInvX);
            *y *= modif * (rotate ? configCamCtrlKeybRotSens : configCamCtrlKeybPanSens) * inv(rotate ? configCamCtrlKeybRotInvY : configCamCtrlKeybPanInvY);
            kzoom *= modif * configCamCtrlKeybZoomSens * inv(configCamCtrlKeybZoomInv);
        }

        else { // input record cbuttons
            float speed = 1200 * configCamCtrlKeybInprecSens;
            if (gPlayer1Controller->buttonDown & U_CBUTTONS) kmove_y -= speed * inv(configCamCtrlKeybInprecRotInvY);
            if (gPlayer1Controller->buttonDown & D_CBUTTONS) kmove_y += speed * inv(configCamCtrlKeybInprecRotInvY);
            if (gPlayer1Controller->buttonDown & L_CBUTTONS) kmove_x -= speed * inv(configCamCtrlKeybInprecRotInvX);
            if (gPlayer1Controller->buttonDown & R_CBUTTONS) kmove_x += speed * inv(configCamCtrlKeybInprecRotInvX);
        }
    }
    else {
        ortho_settings.scale -= mouse_state.scrollwheel * ortho_settings.scale * 0.1;
        if (mouse_state.update_camera) {
            if (mouse_state.held & MOUSEBTN_MASK_L) {
                ortho_settings.offset_x += mouse_state.x_diff * 2.5 * ortho_settings.scale;
                ortho_settings.offset_y += mouse_state.y_diff * 2.5 * ortho_settings.scale;
            }
            if (mouse_state.held & MOUSEBTN_MASK_R) {
                ortho_settings.rotation_y += mouse_state.x_diff * 0.2;
                ortho_settings.rotation_x += mouse_state.y_diff * 0.2;
            }
        }
    }
#undef inv
    mouse_state.scrollwheel = 0;

    float move_x = kmove_x + mmove_x;
    float move_y = kmove_y + mmove_y;
    float rotate_x = krotate_x + mrotate_x;
    float rotate_y = krotate_y + mrotate_y;
    float zoom = kzoom + mzoom;

    if (!inprec || !inprec_keep_angle) {
        if (inprec) {
            inpreccam_yaw   += move_x * camVelRSpeed;
            inpreccam_pitch -= move_y * camVelRSpeed;
            inpreccam_distfrommario -= zoom;
            if (inpreccam_distfrommario < 50) inpreccam_distfrommario = 50;
            MarioActor* actor = saturn_get_actor(recording_mario_actor);
            if (actor != nullptr) {
                InputRecordingFrame last = actor->input_recording[actor->input_recording.size() - 1];
                vec3f_set(inpreccam_focus, last.x, last.y + 80, last.z);
                vec3f_set_dist_and_angle(inpreccam_focus, inpreccam_pos, inpreccam_distfrommario, inpreccam_pitch, inpreccam_yaw);
            }
        }
        else {
            Vec3f *camPos;
            float *camYaw, *camPitch;
            if (gIsCameraMounted) {
                camPos = &freezecamPos;
                camYaw = &freezecamYaw;
                camPitch = &freezecamPitch;
            }
            else {
                camPos = &cameraPos;
                camYaw = &cameraYaw;
                camPitch = &cameraPitch;
            }
            Vec3f offset;
            vec3f_set(offset, 0, 0, 0);
            offset[0] += sins(*camYaw + atan2s(0, 127)) * move_x * camVelSpeed;
            offset[2] += coss(*camYaw + atan2s(0, 127)) * move_x * camVelSpeed;
            offset[1] += coss(*camPitch) * move_y * camVelSpeed;
            offset[0] += sins(*camPitch) * coss(*camYaw + atan2s(0, 127)) * move_y * camVelSpeed;
            offset[2] -= sins(*camPitch) * sins(*camYaw + atan2s(0, 127)) * move_y * camVelSpeed;
            *camYaw   += rotate_x * camVelRSpeed;
            *camPitch += rotate_y * camVelRSpeed;
            vec3f_add(*camPos, offset);
            vec3f_set_dist_and_angle(*camPos, *camPos, zoom * camVelSpeed, *camPitch, *camYaw);
        }
    }

    if (cameraRollLeft) freezecamRoll += camVelRSpeed * 512;
    if (cameraRollRight) freezecamRoll -= camVelRSpeed * 512;

    if (gCamera) {
        saturn_camera_object->header.gfx.node.flags |= GRAPH_RENDER_INVISIBLE;
        if (saturn_imgui_is_orthographic()) {
            float pitch = ortho_settings.rotation_x, yaw  = ortho_settings.rotation_y;
            float offX  = ortho_settings.offset_x  , offY = ortho_settings.offset_y  ;
            float scale = ortho_settings.scale;
            pitch = pitch / 360 * 65536;
            yaw   = yaw   / 360 * 65536;
            vec3f_set(gCamera->pos, 0, 0, 0);
            gCamera->pos[0] += sins(yaw + atan2s(0, 127)) * offX * camVelSpeed;
            gCamera->pos[2] += coss(yaw + atan2s(0, 127)) * offX * camVelSpeed;
            gCamera->pos[1] += coss(pitch) * offY * camVelSpeed;
            gCamera->pos[0] += sins(pitch) * coss(yaw + atan2s(0, 127)) * offY * camVelSpeed;
            gCamera->pos[2] -= sins(pitch) * sins(yaw + atan2s(0, 127)) * offY * camVelSpeed;
            vec3f_set_dist_and_angle(gCamera->pos, gCamera->focus, 100, -pitch, yaw);
        }
        else if (inprec && !inprec_keep_angle) {
            vec3f_copy(gCamera->pos, inpreccam_pos);
            vec3f_copy(gCamera->focus, inpreccam_focus);
        }
        else {
            if (saturn_imgui_is_capturing_video() || gIsCameraMounted) {
                vec3f_copy(gCamera->pos, freezecamPos);
                vec3f_set_dist_and_angle(gCamera->pos, gCamera->focus, 100, freezecamPitch, freezecamYaw);
            }
            else {
                vec3f_copy(gCamera->pos, cameraPos);
                vec3f_set_dist_and_angle(gCamera->pos, gCamera->focus, 100, cameraPitch, cameraYaw);
            }
        }
        vec3f_copy(gLakituState.pos, gCamera->pos);
        vec3f_copy(gLakituState.focus, gCamera->focus);
        vec3f_copy(gLakituState.goalPos, gCamera->pos);
        vec3f_copy(gLakituState.goalFocus, gCamera->focus);
        gCamera->yaw = calculate_yaw(gCamera->focus, gCamera->pos);
        gLakituState.yaw = gCamera->yaw;
        gLakituState.roll = 0;

        if (!saturn_imgui_is_orthographic()) {
            if (saturn_imgui_is_capturing_video()) gLakituState.roll = freezecamRoll;
            else if (gIsCameraMounted) {
                vec3f_copy(cameraPos, freezecamPos);
                cameraYaw = freezecamYaw;
                cameraPitch = freezecamPitch;
                gLakituState.roll = freezecamRoll;
            }
            else {
                saturn_camera_object->header.gfx.node.flags &= ~GRAPH_RENDER_INVISIBLE;
                vec3f_copy(saturn_camera_object->header.gfx.pos, freezecamPos);
                vec3s_set(saturn_camera_object->header.gfx.angle, freezecamPitch, freezecamYaw + 0x8000, freezecamRoll);
            }
        }
    }

    // Animations

    /*if (mario_exists) {
        if (is_anim_paused) {
            gMarioState->marioObj->header.gfx.unk38.animFrame = current_anim_frame;
            gMarioState->marioObj->header.gfx.unk38.animFrameAccelAssist = current_anim_frame;
        } else if (is_anim_playing) {
            if (current_animation.hang) {
                if (is_anim_past_frame(gMarioState, (int)gMarioState->marioObj->header.gfx.unk38.curAnim->unk08 - 1)) {
                    is_anim_paused = !is_anim_paused;
                }
            }

            if (is_anim_past_frame(gMarioState, (int)gMarioState->marioObj->header.gfx.unk38.curAnim->unk08) || is_anim_at_end(gMarioState)) {
                if (current_animation.loop && !using_chainer) {
                    gMarioState->marioObj->header.gfx.unk38.animFrame = 0;
                    gMarioState->marioObj->header.gfx.unk38.animFrameAccelAssist = 0;
                } else {
                    if (using_chainer) {
                        chainer_index++;
                    } else {
                        if (gMarioState->action == ACT_DEBUG_FREE_MOVE)
                            set_mario_animation(gMarioState, MARIO_ANIM_A_POSE);
                        is_anim_playing = false;
                        is_anim_paused = false;
                    }
                }
            }

            if (selected_animation != gMarioState->marioObj->header.gfx.unk38.animID) {
                is_anim_playing = false;
                is_anim_paused = false;
            }

            current_anim_id = (int)gMarioState->marioObj->header.gfx.unk38.animID;
            if (gMarioState->action == ACT_IDLE || gMarioState->action == ACT_FIRST_PERSON || gMarioState->action == ACT_DEBUG_FREE_MOVE) {
                current_anim_frame = (int)gMarioState->marioObj->header.gfx.unk38.animFrame;
                current_anim_length = (int)gMarioState->marioObj->header.gfx.unk38.curAnim->unk08 - 1;
            }

            if (current_animation.speed != 1.0f)
                gMarioState->marioObj->header.gfx.unk38.animAccel = current_animation.speed * 65535;

            if (using_chainer && is_anim_playing) saturn_run_chainer();
        }
    }*/

    bool should_do_mouse_action = mouse_state.dist_travelled <= 3 && mouse_state.released && mouse_state.focused_on_game && !saturn_actor_is_recording_input() && !saturn_imgui_is_orthographic();
    if (saturn_imgui_is_capturing_video()) should_do_mouse_action = false;

    if (setting_mario_struct_pos) {
        Vec3f dir, hit;
        s16 yaw, pitch;
        float dist;
        float x = (mouse_state.x - game_viewport[0]) / game_viewport[2];
        float y = (mouse_state.y - game_viewport[1]) / game_viewport[3];
        struct Surface* surface = nullptr;
        vec3f_get_dist_and_angle(gCamera->pos, gCamera->focus, &dist, &pitch, &yaw);
        get_raycast_dir(dir, yaw, pitch, camera_fov, gfx_current_dimensions.aspect_ratio, x, y);
        vec3f_mul(dir, 8000);
        find_surface_on_ray(gCamera->pos, dir, &surface, hit);
        vec3f_get_dist_and_angle(hit, gCamera->pos, &dist, &pitch, &yaw);
        vec3f_copy(gMarioState->pos, hit);
        gMarioState->fAngle = yaw;
        if (should_do_mouse_action && (mouse_state.released & MOUSEBTN_MASK_L)) {
            setting_mario_struct_pos = false;
        }
    }
    else if (should_do_mouse_action) {
        Vec3f dir, hit;
        s16 yaw, pitch;
        float dist;
        float x = (mouse_state.x - game_viewport[0]) / game_viewport[2];
        float y = (mouse_state.y - game_viewport[1]) / game_viewport[3];
        struct Surface* surface = nullptr;
        vec3f_get_dist_and_angle(gCamera->pos, gCamera->focus, &dist, &pitch, &yaw);
        get_raycast_dir(dir, yaw, pitch, camera_fov, gfx_current_dimensions.aspect_ratio, x, y);
        vec3f_mul(dir, 8000);
        if (mouse_state.released & MOUSEBTN_MASK_L) {
            find_surface_on_ray(gCamera->pos, dir, &surface, hit);
            vec3f_get_dist_and_angle(hit, gCamera->pos, &dist, &pitch, &yaw);
            MarioActor* actor = saturn_spawn_actor(hit[0], hit[1], hit[2]);
            actor->angle = yaw;
            std::string name = "Unnamed " + saturn_object_names[current_mario_model] + " " + std::to_string(++marios_spawned);
            memcpy(actor->name, name.c_str(), name.length() + 1);
        }
        if (mouse_state.released & MOUSEBTN_MASK_R) {
            struct Object* obj = get_mario_actor_from_ray(gCamera->pos, dir);
            if (obj) {
                if (obj->oMarioActorIndex >= 0 && obj->oMarioActorIndex < saturn_actor_sizeof()) {
                    saturn_imgui_open_mario_menu(obj->oMarioActorIndex);
                }
            }
        }
    }

    // Misc

    mario_exists = (gMarioState->action != ACT_UNINITIALIZED & sCurrPlayMode != 2 & mario_loaded);

    if (!mario_exists) {
        is_anim_playing = false;
        is_anim_paused = false;
    }

    switch(saturnModelState) {
        case 0:     scrollModelState = 0;       break;
        case 1:     scrollModelState = 0x200;   break;  // Metal Cap
        case 2:     scrollModelState = 0x180;   break;  // Vanish Cap
        case 3:     scrollModelState = 0x380;   break;  // Both
        default:    scrollModelState = 0;       break;
    }

    if (linkMarioScale) {
        marioScaleSizeY = marioScaleSizeX;
        marioScaleSizeZ = marioScaleSizeX;
    }

    if (is_spinning && mario_exists) {
        gMarioState->faceAngle[1] += (s16)(spin_mult * 15 * 182.04f);
    }

    if (current_project != "") saturn_load_project((char*)current_project.c_str());

    if (world_simulation_data) {
        saturn_simulation_update();
    }

    saturn_update_object_collision();

    // Autosave

    if (gCurrLevelNum != LEVEL_SA || gCurrAreaIndex != 3) {
        if (autosaveDelay <= 0) autosaveDelay = 30 * configAutosaveDelay;
        autosaveDelay--;
        if (autosaveDelay == 0) saturn_save_project("autosave.spj", nullptr);
    }
}

void* saturn_keyframe_get_timeline_ptr(KeyframeTimeline& timeline) {
    if (timeline.marioIndex == -1) return timeline.dest;
    return (char*)saturn_get_actor(timeline.marioIndex) + (size_t)timeline.dest;
    // cast to char since its 1 byte long
}

float saturn_keyframe_setup_interpolation(std::string id, int frame, int* keyframe, bool* last) {
    KeyframeTimeline timeline = k_frame_keys[id].first;
    std::vector<Keyframe> keyframes = k_frame_keys[id].second;

    // Get the keyframe to interpolate from
    for (int i = 0; i < keyframes.size(); i++) {
        if (frame < keyframes[i].position) break;
        *keyframe = i;
    }

    // Stop/loop if reached the end
    *last = *keyframe + 1 == keyframes.size();
    if (*last) *keyframe -= 1; // Assign values from final keyframe

    // Interpolate, formulas from easings.net
    float x = (frame - keyframes[*keyframe].position) / (float)(keyframes[*keyframe + 1].position - keyframes[*keyframe].position);
    if (*last) x = 1;
    else if (keyframes[*keyframe].curve == InterpolationCurve::SLOW) x = x * x;
    else if (keyframes[*keyframe].curve == InterpolationCurve::FAST) x = 1 - (1 - x) * (1 - x);
    else if (keyframes[*keyframe].curve == InterpolationCurve::SMOOTH) x = x < 0.5 ? 2 * x * x : 1 - pow(-2 * x + 2, 2) / 2;
    else if (keyframes[*keyframe].curve == InterpolationCurve::WAIT) x = floor(x);

    return x;
}

// applies the values from keyframes to its destination, returns true if its the last frame, false if otherwise
bool saturn_keyframe_apply(std::string id, int frame) {
    if (!saturn_timeline_exists(id.c_str())) return true;

    KeyframeTimeline timeline = k_frame_keys[id].first;
    std::vector<Keyframe> keyframes = k_frame_keys[id].second;

    void* ptr = saturn_keyframe_get_timeline_ptr(timeline);
    std::vector<float> values;
    bool last = true;
    if (keyframes.size() == 1) values = keyframes[0].value;
    else {
        int keyframe = 0;
        last = false;
        float x = saturn_keyframe_setup_interpolation(id, frame, &keyframe, &last);
        for (int i = 0; i < keyframes[keyframe].value.size(); i++) {
            values.push_back((keyframes[keyframe + 1].value[i] - keyframes[keyframe].value[i]) * x + keyframes[keyframe].value[i]);
        }    
    }

    if (timeline.type == KFTYPE_BOOL) *(bool*)ptr = values[0] >= 1;
    if (timeline.type == KFTYPE_FLOAT || timeline.type == KFTYPE_COLORF) {
        float* vals = (float*)ptr;
        for (int i = 0; i < timeline.numValues; i++) {
            *vals = values[i];
            vals++;
        }
    }
    if (timeline.type == KFTYPE_COLOR) {
        int* colors = (int*)ptr;
        for (int i = 0; i < 6; i++) {
            *colors = values[i];
            colors++;
        }
    }
    if (timeline.type == KFTYPE_ANIM) {
        AnimationState* dest = (AnimationState*)ptr;
        dest->custom = values[0] >= 1;
        dest->id = values[1];
        if (dest->custom) saturn_read_mcomp_animation(saturn_get_actor(timeline.marioIndex), canim_array[dest->id]);
    }
    if (timeline.type == KFTYPE_EXPRESSION) {
        Model* dest = (Model*)ptr;
        for (int i = 0; i < values.size(); i++) {
            dest->Expressions[i].CurrentIndex = values[i];
        }
    }
    if (timeline.type == KFTYPE_SWITCH) {
        *((int*)ptr) = values[0];
    }

    return last;
}

// returns true if the value is the same as if the keyframe was applied
bool saturn_keyframe_matches(std::string id, int frame) {
    KeyframeTimeline& timeline = k_frame_keys[id].first;
    std::vector<Keyframe> keyframes = k_frame_keys[id].second;

    std::vector<float> expectedValues;
    if (keyframes.size() == 1) expectedValues = keyframes[0].value;
    else {
        int keyframe = 0;
        bool last = false;
        float x = saturn_keyframe_setup_interpolation(id, frame, &keyframe, &last);
        for (int i = 0; i < keyframes[keyframe].value.size(); i++) {
            expectedValues.push_back((keyframes[keyframe + 1].value[i] - keyframes[keyframe].value[i]) * x + keyframes[keyframe].value[i]);
        }
    }

    void* ptr = saturn_keyframe_get_timeline_ptr(timeline);
    if (timeline.type == KFTYPE_BOOL) {
        if (*(bool*)ptr != 0 != expectedValues[0] >= 1) return false;
        return true;
    }
    if (timeline.type == KFTYPE_FLOAT || timeline.type == KFTYPE_COLORF) {
        for (int i = 0; i < timeline.numValues; i++) {
            float value = ((float*)ptr)[i];
            float distance = abs(value - expectedValues[i]);
            if (distance > pow(10, timeline.precision)) return false;
        }
    }
    if (timeline.type == KFTYPE_COLOR) {
        for (int i = 0; i < 6; i++) {
            int value = ((int*)ptr)[i];
            if (value != (int)expectedValues[i]) return false;
        }
    }
    if (timeline.type == KFTYPE_ANIM) {
        AnimationState* anim_state = (AnimationState*)ptr;
        if (anim_state->custom != (expectedValues[0] >= 1)) return false;
        if (anim_state->id != ((int)expectedValues[1])) return false;
    }
    if (timeline.type == KFTYPE_EXPRESSION) {
        Model* model = (Model*)ptr;
        for (int i = 0; i < model->Expressions.size(); i++) {
            if (model->Expressions[i].CurrentIndex != (int)expectedValues[i]) return false;
        }
    }
    if (timeline.type == KFTYPE_SWITCH) {
        return *((int*)ptr) == (int)expectedValues[0];
    }

    return true;
}

void saturn_create_keyframe(std::string id, InterpolationCurve curve) {
    Keyframe keyframe = Keyframe();
    keyframe.position = k_current_frame;
    keyframe.curve = curve;
    keyframe.timelineID = id;
    KeyframeTimeline timeline = k_frame_keys[id].first;
    void* ptr = saturn_keyframe_get_timeline_ptr(timeline);
    if (timeline.type == KFTYPE_BOOL) keyframe.value.push_back(*(bool*)ptr);
    if (timeline.type == KFTYPE_FLOAT || timeline.type == KFTYPE_COLORF) {
        float* values = (float*)ptr;
        for (int i = 0; i < timeline.numValues; i++) {
            keyframe.value.push_back(*values);
            values++;
        }
    }
    if (timeline.type == KFTYPE_ANIM) {
        AnimationState* anim_state = (AnimationState*)ptr;
        keyframe.value.push_back(anim_state->custom);
        keyframe.value.push_back(anim_state->id);
    }
    if (timeline.type == KFTYPE_EXPRESSION) {
        Model* model = (Model*)ptr;
        for (int i = 0; i < model->Expressions.size(); i++) {
            keyframe.value.push_back(model->Expressions[i].CurrentIndex);
        }
    }
    if (timeline.type == KFTYPE_COLOR) {
        int* values = (int*)ptr;
        for (int i = 0; i < 6; i++) {
            keyframe.value.push_back(*values);
            values++;
        }
    }
    if (timeline.type == KFTYPE_SWITCH) {
        keyframe.value.push_back(*(int*)ptr);
    }
    keyframe.curve = curve;
    k_frame_keys[id].second.push_back(keyframe);
    saturn_keyframe_sort(&k_frame_keys[id].second);
}

void saturn_place_keyframe(std::string id, int frame) {
    KeyframeTimeline timeline = k_frame_keys[id].first;
    std::vector<Keyframe>* keyframes = &k_frame_keys[id].second;
    int keyframeIndex = 0;
    for (int i = 0; i < keyframes->size(); i++) {
        if (frame >= (*keyframes)[i].position) keyframeIndex = i;
    }
    bool create_new = keyframes->size() == 0;
    InterpolationCurve curve = InterpolationCurve::WAIT;
    if (!create_new) {
        create_new = (*keyframes)[keyframeIndex].position != frame;
        curve = (*keyframes)[keyframeIndex].curve;
    }
    if (create_new) saturn_create_keyframe(id, curve);
    else {
        Keyframe* keyframe = &(*keyframes)[keyframeIndex];
        void* ptr = saturn_keyframe_get_timeline_ptr(timeline);
        if (timeline.type == KFTYPE_BOOL) keyframe->value[0] = *(bool*)ptr;
        if (timeline.type == KFTYPE_FLOAT || timeline.type == KFTYPE_COLORF) {
            float* values = (float*)ptr;
            for (int i = 0; i < timeline.numValues; i++) {
                keyframe->value[i] = *values;
                values++;
            }
        }
        if (timeline.type == KFTYPE_ANIM) {
            AnimationState* anim_state = (AnimationState*)ptr;
            keyframe->value[0] = anim_state->custom;
            keyframe->value[1] = anim_state->id;
        }
        if (timeline.type == KFTYPE_EXPRESSION) {
            Model* model = (Model*)ptr;
            for (int i = 0; i < model->Expressions.size(); i++) {
                keyframe->value[i] = model->Expressions[i].CurrentIndex;
            }
        }
        if (timeline.type == KFTYPE_COLOR) {
            int* values = (int*)ptr;
            for (int i = 0; i < 6; i++) {
                keyframe->value[i] = *values;
                values++;
            }
        }
        if (timeline.type == KFTYPE_SWITCH) {
            keyframe->value[0] = *(int*)ptr;
        }
        if (timeline.behavior != KFBEH_DEFAULT) keyframe->curve = InterpolationCurve::WAIT;
    }
}

int saturn_keyframe_get_length() {
    int max_len = 0;
    for (auto& timeline : k_frame_keys) {
        for (auto& kf : timeline.second.second) {
            if (kf.position > max_len) max_len = kf.position;
        }
    }
    return max_len;
}

// Play Animation

void saturn_play_animation(MarioAnimID anim) {
    force_set_mario_animation(gMarioState, anim);
    //set_mario_anim_with_accel(gMarioState, anim, anim_speed * 65535);
    is_anim_playing = true;
}

void saturn_play_keyframe() {
    if (k_frame_keys.size() <= 0) return;

    if (!keyframe_playing) {
        k_last_passed_index = 0;
        k_distance_between = 0;
        k_current_frame = 0;
        mcam_timer = 0;
        keyframe_playing = true;
    } else {
        if (k_current_frame > 0)
            keyframe_playing = false;
    }
}

// Copy

void saturn_copy_object(Vec3f from, Vec3f to) {
    vec3f_copy(from, to);
    vec3s_set(gMarioState->marioObj->header.gfx.angle, 0, gMarioState->faceAngle[1], 0);
}

Vec3f stored_mario_pos;
Vec3s stored_mario_angle;

void saturn_copy_mario() {
    vec3f_copy(stored_mario_pos, gMarioState->pos);
    vec3s_copy(stored_mario_angle, gMarioState->faceAngle);
}

void saturn_paste_mario() {
    vec3f_copy(gMarioState->pos, stored_mario_pos);
    vec3f_copy(gMarioState->marioObj->header.gfx.pos, stored_mario_pos);
    vec3s_copy(gMarioState->faceAngle, stored_mario_angle);
    vec3s_set(gMarioState->marioObj->header.gfx.angle, 0, stored_mario_angle[1], 0);
}

Vec3f pos_relative;
Vec3s foc_relative;
bool was_relative;

void saturn_copy_camera(bool relative) {
    vec3f_copy(stored_camera_pos, cameraPos);
    vec3f_set(stored_camera_rot, cameraYaw, cameraPitch, freezecamRoll);
}

void saturn_paste_camera() {
    Vec3f *pos;
    float *yaw, *pitch;
    if (gIsCameraMounted) {
        pos = &freezecamPos;
        yaw = &freezecamYaw;
        pitch = &freezecamPitch;
    }
    else {
        pos = &cameraPos;
        yaw = &cameraYaw;
        pitch = &cameraPitch;
    }
    vec3f_copy(*pos, stored_camera_pos);
    *yaw = stored_camera_rot[0];
    *pitch = stored_camera_rot[1];
    freezecamRoll = stored_camera_rot[2];
}

// Debug

void saturn_print(const char* text) {
    if (SDL_GetKeyboardState(NULL)[SDL_SCANCODE_RSHIFT])
        printf(text);
}

// Other

SDL_Scancode saturn_key_to_scancode(unsigned int configKey[]) {
    for (int i = 0; i < MAX_BINDS; i++) {
        unsigned int key = configKey[i];

        if (key >= 0 && key < 0xEF) {
            for (int i = 0; i < SDL_NUM_SCANCODES; i++) {
                if(scanCodeToKeyNum[i] == key) return (SDL_Scancode)i;
            }
        }
        return SDL_SCANCODE_UNKNOWN;
    }
}

const char* saturn_get_stage_name(int courseNum) {
    switch(courseNum) {
        case LEVEL_SA: return "Chroma Key Stage"; break;
        case LEVEL_CASTLE: return "Peach's Castle"; break;
        case LEVEL_CASTLE_GROUNDS: return "Castle Grounds"; break;
        case LEVEL_CASTLE_COURTYARD: return "Castle Courtyard"; break;
        case LEVEL_BOB: return "Bob-omb Battlefield"; break;
        case LEVEL_CCM: return "Cool, Cool Mountain"; break;
        case LEVEL_WF: return "Whomp's Fortress"; break;
        case LEVEL_JRB: return "Jolly Roger Bay"; break;
        case LEVEL_PSS: return "Princess's Secret Slide"; break;
        case LEVEL_TOTWC: return "Tower of the Wing Cap"; break;
        case LEVEL_BITDW: return "Bowser in the Dark World"; break;
        case LEVEL_BBH: return "Big Boo's Haunt"; break;
        case LEVEL_HMC: return "Hazy Maze Cave"; break;
        case LEVEL_COTMC: return "Cavern of the Metal Cap"; break;
        case LEVEL_LLL: return "Lethal Lava Land"; break;
        case LEVEL_SSL: return "Shifting Sand Land"; break;
        case LEVEL_VCUTM: return "Vanish Cap Under the Moat"; break;
        case LEVEL_DDD: return "Dire, Dire Docks"; break;
        case LEVEL_BITFS: return "Bowser in the Fire Sea"; break;
        case LEVEL_SL: return "Snowman's Land"; break;
        case LEVEL_WDW: return "Wet-Dry World"; break;
        case LEVEL_TTM: return "Tall, Tall Mountain"; break;
        case LEVEL_THI: return "Tiny, Huge Island"; break;
        case LEVEL_TTC: return "Tick Tock Clock"; break;
        case LEVEL_WMOTR: return "Wing Mario Over the Rainbow"; break;
        case LEVEL_RR: return "Rainbow Ride"; break;
        case LEVEL_BITS: return "Bowser in the Sky"; break;
        case LEVEL_BOWSER_1: return "Bowser 1"; break;
        case LEVEL_BOWSER_2: return "Bowser 2"; break;
        case LEVEL_BOWSER_3: return "Bowser 3"; break;

        default: return "Unknown"; break;
    }
}

std::thread extract_thread;
std::thread load_thread;

bool load_thread_began = false;
bool loading_finished = false;

bool saturn_begin_extract_rom_thread() {
    if (extract_thread_began) return extraction_finished;
    extract_thread_began = true;
    extraction_finished = false;
    extract_thread = std::thread([]() {
        extract_return_code = saturn_extract_rom(EXTRACT_TYPE_ALL);
        extraction_finished = true;
    });
    extract_thread.detach();
    return false;
}

extern void split_skyboxes();

bool saturn_do_load() {
    if (load_thread_began) return loading_finished;
    load_thread_began = true;
    loading_finished = false;
    load_thread = std::thread([]() {
        if (!(save_file_get_flags() & SAVE_FLAG_TALKED_TO_ALL_TOADS)) DynOS_Gfx_GetPacks().Clear();
        DynOS_Opt_Init();
        //model_details = "" + std::to_string(DynOS_Gfx_GetPacks().Count()) + " model pack";
        //if (DynOS_Gfx_GetPacks().Count() != 1) model_details += "s";
        saturn_imgui_init();
        saturn_load_locations();
        saturn_load_favorite_anims();
        saturn_fill_data_table();
        split_skyboxes();
        saturn_launch_timer = 0;
        loading_finished = true;
    });
    load_thread.detach();
    return false;
}

void saturn_on_splash_finish() {
    splash_finished = true;
}

bool saturn_timeline_exists(const char* name) {
    return k_frame_keys.find(name) != k_frame_keys.end();
}

// vvv    i would put this in a separate file but for SOME REASON the linker defines these but still throws "function undefined" smh

#define NUM_STARS 2048
#define STAR_WIDTH 16000
#define STAR_HEIGHT 9000
#define STAR_SPAWN_DISTANCE 1
#define STAR_SPEED 2

SDL_Texture* saturn_splash_screen_banner = nullptr;
int saturn_splash_screen_banner_width = 0;
int saturn_splash_screen_banner_height = 0;
unsigned char saturn_splash_screen_banner_data[] = {
#include "splashdata/overlay.h"
};

SDL_Texture* saturn_splash_screen_bg = nullptr;
unsigned char saturn_splash_screen_bg_data[] = {
#include "splashdata/background.h"
};

SDL_Texture* saturn_splash_screen_rom_prompt = nullptr;
unsigned char saturn_splash_screen_rom_prompt_data[] = {
#include "splashdata/rom_prompt.h"
};

int load_delay = 2;

struct Star {
    float x, y, z;
};

Star stars[NUM_STARS];

SDL_Texture* saturn_load_splash_img(SDL_Renderer* renderer, unsigned char* data, int len, int* w, int* h) {
    int width, height;
    unsigned char* image_data = stbi_load_from_memory(data, len, &width, &height, nullptr, STBI_rgb_alpha);
    SDL_Surface* surface = SDL_CreateRGBSurfaceFrom(image_data, width, height, 32, width * 4, 0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000);
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);
    stbi_image_free(image_data);
    if (w) *w = width;
    if (h) *h = height;
    return texture;
}

#define splashtex(id, width, height) if (saturn_splash_screen_##id == nullptr) saturn_splash_screen_##id = saturn_load_splash_img(renderer, saturn_splash_screen_##id##_data, sizeof(saturn_splash_screen_##id##_data), width, height);
void saturn_splash_screen_init(SDL_Renderer* renderer) {
    splashtex(banner, &saturn_splash_screen_banner_width, &saturn_splash_screen_banner_height);
    splashtex(bg, NULL, NULL);
    splashtex(rom_prompt, NULL, NULL);
    memset(stars, 0, sizeof(Star) * NUM_STARS);
    for (int i = 0; i < NUM_STARS; i++) {
        stars[i] = (Star){ .x = rand() % STAR_WIDTH - STAR_WIDTH / 2, .y = rand() % STAR_HEIGHT - STAR_HEIGHT / 2, .z = (i + 1) * STAR_SPAWN_DISTANCE };
    }
}

void saturn_splash_screen_update_stars() {
    for (int i = 0; i < NUM_STARS; i++) {
        stars[i].z -= STAR_SPEED;
        if (stars[i].z <= 0) stars[i] = (Star){ .x = rand() % STAR_WIDTH - STAR_WIDTH / 2, .y = rand() % STAR_HEIGHT - STAR_HEIGHT / 2, .z = NUM_STARS * STAR_SPAWN_DISTANCE };
    }
}

bool saturn_splash_screen_update(SDL_Renderer* renderer) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) exit(0);
        if (event.type == SDL_DROPFILE) {
            rom_path = event.drop.file;
            prompting_for_rom = false;
        }
    }
    SDL_Rect rect = (SDL_Rect){
        .x = 0,
        .y = 0,
        .w = 640,
        .h = 360
    };
    SDL_RenderCopy(renderer, saturn_splash_screen_bg, &rect, &rect);
    saturn_splash_screen_update_stars();
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    for (int i = 0; i < NUM_STARS; i++) {
        float x = stars[i].x / (stars[i].z / 16) + 640 / 2.f;
        float y = stars[i].y / (stars[i].z / 16) + 360 / 2.f;
        SDL_Rect rect = (SDL_Rect){ .x = x, .y = y, .w = 1, .h = 1 };
        SDL_RenderFillRect(renderer, &rect);
    }
    SDL_Rect src = (SDL_Rect){
        .x = 0,
        .y = 0,
        .w = saturn_splash_screen_banner_width,
        .h = saturn_splash_screen_banner_height
    };
    SDL_Rect dst = (SDL_Rect){
        .x = 640 / 2 - saturn_splash_screen_banner_width / 2,
        .y = 360 / 2 - saturn_splash_screen_banner_height / 2,
        .w = saturn_splash_screen_banner_width,
        .h = saturn_splash_screen_banner_height
    };
    SDL_RenderCopy(renderer, saturn_splash_screen_banner, &src, &dst);
    if (prompting_for_rom) SDL_RenderCopy(renderer, saturn_splash_screen_rom_prompt, &rect, &rect);
    if (!saturn_begin_extract_rom_thread()) {
        if (extraction_progress >= 0) {
            SDL_Rect progress = (SDL_Rect){ .x = 16, .y = 360 - 32, .w = (640 - 32) * extraction_progress, .h = 16 };
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            SDL_RenderFillRect(renderer, &progress);
        }
        return false;
    }
    if (load_delay-- > 0) return false;
    if (!saturn_do_load()) return false;
    return true;
}

int saturn_splash_screen_open() {
    // make the x11 compositor on linux not kill itself
    SDL_SetHint(SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR, "0");
    SDL_Window* window = SDL_CreateWindow("Saturn Studio", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 640, 360, 0);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetWindowBordered(window, SDL_FALSE);
    saturn_splash_screen_init(renderer);
    bool extracting_assets = true;
    while (true) {
        clock_t before = clock();
        if (saturn_splash_screen_update(renderer)) break;
        SDL_RenderPresent(renderer);
        clock_t after = clock();
        if (after - before < CLOCKS_PER_SEC / 60) std::this_thread::sleep_for(std::chrono::microseconds((int)(1000000 / 60.0f - (float)(after - before) / CLOCKS_PER_SEC * 1000000)));
    }
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return extract_return_code;
}
