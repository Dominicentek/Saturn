#include "saturn_imgui_settings.h"

#include <string>
#include <iostream>
#include <fstream>
#include <filesystem>

#include "saturn/libs/imgui/imgui.h"
#include "saturn/libs/imgui/imgui_internal.h"
#include "saturn/libs/imgui/imgui_impl_sdl.h"
#include "saturn/libs/imgui/imgui_impl_opengl3.h"
#include "saturn/libs/imgui/imgui-knobs.h"
#include "saturn/saturn.h"
#include "saturn/saturn_actors.h"
#include "saturn_imgui.h"
#include <SDL2/SDL.h>
#include "saturn/saturn_json.h"

extern "C" {
#include "pc/gfx/gfx_pc.h"
#include "pc/gfx/gfx_opengl.h"
#include "pc/configfile.h"
#include "game/mario.h"
#include "game/camera.h"
#include "game/level_update.h"
#include "pc/cheats.h"
#include "pc/controller/controller_api.h"
#include "pc/controller/controller_sdl.h"
#include "pc/controller/controller_keyboard.h"
#include "audio/load.h"
#include "engine/math_util.h"
#include "pc/platform.h"
#include "buffers/buffers.h"
#include "course_table.h"
#include "game/save_file.h"
}

#include "icons/IconsForkAwesome.h"

using namespace std;

// Variables

int windowWidth, windowHeight;
bool waitingForKeyPress;
int fpsChoice;

const char* translate_bind_to_name(int bind) {
    static char name[11] = { 0 };
    sprintf(name, "%04X", bind);

    if (bind == VK_INVALID) { return ""; }

    // gamepad
    if (bind >= VK_BASE_SDL_GAMEPAD) {
        int gamepad_button = (bind - VK_BASE_SDL_GAMEPAD);
        switch (gamepad_button) {
            case 0: return "[A]";
            case 1: return "[B]";
            case 2: return "[X]";
            case 3: return "[Y]";
            case 4: return "[Back]";
            case 5: return "[Guide]";
            case 6: return "[Start]";
            case 7: return "[L Stick]";
            case 8: return "[R Stick]";
            case 9: return "[L Bump]";
            case 10: return "[R Bump]";
            case 11: return "[DPAD Up]";
            case 12: return "[DPAD Down]";
            case 13: return "[DPAD Left]";
            case 14: return "[DPAD Right]";
            case 0x1A: return "[L Trig]";
            case 0x1B: return "[R Trig]";
            default: return name;
        }
    }

    // keyboard
    if (bind >= 512) { return name; }

    SDL_Scancode sc = bind_to_sdl_scancode[bind];
    if (sc == 0) { return name; }

    const char* sname = SDL_GetScancodeName(sc);
    if (strlen(sname) <= 9) { return sname; }

    char* space = (char*)strchr(sname, ' ');
    if (space == NULL) { return sname; }

    snprintf(name, 10, "%c%s", sname[0], (space + 1));
    
    return name;
}

int waitingIndex = -1;

void SaturnKeyBind(const char* title, unsigned int configKey[], const char* id, int buttonIndex) {
    for (int i = 0; i < MAX_BINDS; i++) {
        unsigned int key = configKey[i];

        // A different ID for each button
        string buttonName = (string)translate_bind_to_name(key) + (string)"###btn" + to_string(buttonIndex + i);

        // Only run this code for the button we're currently rebinding
        if(buttonIndex + i == waitingIndex) {
            buttonName = (string)"...###btn" + to_string(buttonIndex + i);

            // Configure key
            u32 rawKey = controller_get_raw_key();
            if (rawKey != VK_INVALID && rawKey != 0x1101 && rawKey != -1) {
                configKey[i] = rawKey;

                // Escape is our "clear"
                if (rawKey == 1) configKey[i] = 0xffff;

                controller_reconfigure();
                configfile_save(configfile_name());

                // Clear data
                rawKey = -1;
                waitingIndex = -1;
                key = -1;
            }

            // Set a tooltip so the user knows they are rebinding
            ImGui::SetTooltip("Press any key or gamepad button. ESC to clear.");
        }

        // buttonIndex is a unique ordered number for each button
        if (ImGui::Button(buttonName.c_str(), ImVec2(75, 20))) {
            waitingIndex = buttonIndex + i;
        }

        ImGui::SameLine();
    }

    ImGui::Text(title);
}

static char starCount[128];

void ssettings_imgui_init() {
    windowWidth = configWindow.w;
    windowHeight = configWindow.h;

    if (configAudioMode == 0) gSoundMode = 0;
    if (configAudioMode == 1) gSoundMode = 3;
}

int current_theme_id = 0;
int current_texture_id = -1;

extern void split_skyboxes();
extern std::map<std::string, std::string> texture_forwards;

void ssettings_imgui_update() {
    if (saturn_actor_is_recording_input()) ImGui::EndDisabled();
    std::vector<const char*> theme_names = {};
    for (int i = 0; i < theme_list.size(); i++) {
        auto& entry = theme_list[i];
        if (entry.first == editor_theme) current_theme_id = i;
        theme_names.push_back(entry.second.c_str());
    }
    ImGui::PushItemWidth(150);
    if (ImGui::Combo(ICON_FK_PAINT_BRUSH " Theme", &current_theme_id, theme_names.data(), theme_names.size())) {
        configEditorThemeJson = string_hash(theme_list[current_theme_id].first.c_str(), 0, theme_list[current_theme_id].first.length());
        imgui_update_theme();
    }
    ImGui::SameLine(); imgui_bundled_help_marker("Changes the UI theme.");
    if (ImGui::BeginCombo(ICON_FK_PICTURE_O " Textures", current_texture_id == -1 ? "Vanilla" : textures_list[current_texture_id].c_str())) {
        if (ImGui::Selectable("Vanilla")) {
            configEditorTextures = 0;
            current_texture_id = -1;
            texture_forwards.clear();
            gfx_precache_textures();
        }
        for (int i = 0; i < textures_list.size(); i++) {
            if (ImGui::Selectable(textures_list[i].c_str())) {
                configEditorTextures = string_hash(textures_list[i].c_str(), 0, textures_list[i].length());
                current_texture_id = i;
                texture_forwards.clear();
                std::filesystem::path json_path = std::filesystem::path("dynos/textures/") / textures_list[i] / "texture_forwards.json";
                if (std::filesystem::exists(json_path)) {
                    std::ifstream file = std::ifstream(json_path);
                    Json::Value json;
                    json << file;
                    for (auto& entry : json.object()) {
                        texture_forwards.insert({ entry.first, entry.second.asString() });
                    }
                }
                gfx_precache_textures();
                split_skyboxes();
            }
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine(); imgui_bundled_help_marker("Changes the in-game textures.");
    if (ImGui::Button("Clear Texture Cache")) {
        gfx_precache_textures();
        split_skyboxes();
    }
    ImGui::PopItemWidth();
#ifdef DISCORDRPC
    ImGui::Checkbox(ICON_FK_DISCORD " Discord Activity Status", &configDiscordRPC);
    imgui_bundled_tooltip("Enables/disables Discord Rich Presence. Requires restart.");
#endif
    std::filesystem::path no_updates_file = std::filesystem::path(sys_user_path()) / "no_updates";
    bool pauseUpdates = std::filesystem::exists(no_updates_file);
    if (ImGui::Checkbox("Pause Updates", &pauseUpdates)) {
        if (pauseUpdates) {
            std::ofstream stream = std::ofstream(no_updates_file);
            stream.close();
        }
        else std::filesystem::remove(no_updates_file);
    }

    ImGui::SetNextItemOpen(true, ImGuiCond_FirstUseEver);
    if (ImGui::CollapsingHeader("Graphics")) {
        if (ImGui::Checkbox("Fullscreen", &configWindow.fullscreen))
            configWindow.settings_changed = true;
        if (!configWindow.fullscreen) {
            ImGui::Text("Resolution");
            static int screen_res[2] = { windowWidth, windowHeight };
            ImGui::InputInt2("###screen_resolution", screen_res); ImGui::SameLine();
            if (ImGui::Button("Set###screen_apply")) {
                windowWidth = screen_res[0];
                windowHeight = screen_res[1];
                if (windowWidth < 640 || windowHeight < 480) {
                    windowWidth = screen_res[0] = 640;
                    windowHeight = screen_res[1] = 480;
                }
                SDL_SetWindowSize(window, windowWidth, windowHeight);
                SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
                configWindow.w = windowWidth;
                configWindow.h = windowHeight;
            }
        }

        ImGui::Dummy(ImVec2(0, 5));

        if (ImGui::Checkbox("VSync", &configWindow.vsync))
            configWindow.settings_changed = true;
        imgui_bundled_tooltip("Enable/disable this if your game speed is too fast or slow.");

        const char* fps_limits[] = { "30", "60" };
        ImGui::Text(ICON_FK_CLOCK_O " FPS");
        ImGui::PushItemWidth(150);
        if (ImGui::Combo("###fps_limits", (int*)&configFps60, fps_limits, IM_ARRAYSIZE(fps_limits))) {
            configWindow.settings_changed = true;
            is_anim_playing = false;
            is_anim_paused = false;
        }
        ImGui::PopItemWidth();

        ImGui::Checkbox("Disable near-clipping", &configEditorNearClipping);
        imgui_bundled_tooltip("Enable when some close to the camera starts clipping through. Disable if the level fog goes nuts.");

        ImGui::Checkbox("Anti-aliasing", &configWindow.enable_antialias);
        imgui_bundled_tooltip("Enable smooth edges via OpenGL.");

        ImGui::Dummy(ImVec2(0, 5));

        ImGui::Checkbox("Stretched widescreen", &configWindow.jabo_mode);
        imgui_bundled_tooltip("Forces the game into a 4:3 aspect ratio, regardless of window resolution; For classic enthusiasts.");

        ImGui::Checkbox("Show wireframes", &wireframeMode);
        imgui_bundled_tooltip("Displays wireframes instead of filled polys; For debugging purposes.");

        ImGui::Text(ICON_FK_PICTURE_O " Texture Filtering");
        const char* texture_filters[] = { "Nearest", "Linear", "Three-point" };
        ImGui::PushItemWidth(150);
        ImGui::Combo("###texture_filters", (int*)&configFiltering, texture_filters, IM_ARRAYSIZE(texture_filters));
        ImGui::PopItemWidth();

        if (configFps60) ImGui::Dummy(ImVec2(0, 5));
    }
    if (ImGui::CollapsingHeader("Audio")) {
        ImGuiKnobs::KnobInt("Master", (int*)&configMasterVolume, 0, MAX_VOLUME, 0.f, "%i dB", ImGuiKnobVariant_Tick, 0.f, ImGuiKnobFlags_DragHorizontal); ImGui::SameLine();
        ImGuiKnobs::KnobInt("SFX", (int*)&configSfxVolume, 0, MAX_VOLUME, 0.f, "%i dB", ImGuiKnobVariant_Tick, 0.f, ImGuiKnobFlags_DragHorizontal); ImGui::SameLine();
        ImGuiKnobs::KnobInt("Music", (int*)&configMusicVolume, 0, MAX_VOLUME, 0.f, "%i dB", ImGuiKnobVariant_Tick, 0.f, ImGuiKnobFlags_DragHorizontal); ImGui::SameLine();
        ImGuiKnobs::KnobInt("Fanfares", (int*)&configEnvVolume, 0, MAX_VOLUME, 0.f, "%i dB", ImGuiKnobVariant_Tick, 0.f, ImGuiKnobFlags_DragHorizontal);

        ImGui::Checkbox("Enable voices", &configVoicesEnabled);
        imgui_bundled_tooltip("If disabled, Mario's voice clips will be silenced; Preferable for custom character models.");
        
        const char* audio_modes[] = { "Stereo", "Mono" };
        ImGui::Text(ICON_FK_VOLUME_CONTROL_PHONE " Audio Mode");
        ImGui::PushItemWidth(150);
        if (ImGui::Combo("###audio_mode", (int*)&configAudioMode, audio_modes, IM_ARRAYSIZE(audio_modes))) {
            if (configAudioMode == 0) gSoundMode = 0;
            if (configAudioMode == 1) gSoundMode = 3;
        }
        ImGui::PopItemWidth();
    }
    if (ImGui::CollapsingHeader("Controls")) {
        if (ImGui::Checkbox("Unfocused gamepad input", &configWindowState))
            SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, configWindowState?"1":"0");

        imgui_bundled_tooltip("Allows controller inputs to be processed while the window is not focused.");
        if (ImGui::BeginTabBar("###controls_tabbar", ImGuiTabBarFlags_None)) {
            if (ImGui::BeginTabItem("Game")) {
                ImGui::Text("Movement");
                SaturnKeyBind("Up", configKeyStickUp, "bUp", 3*1);
                SaturnKeyBind("Down", configKeyStickDown, "bDown", 3*2);
                SaturnKeyBind("Left", configKeyStickLeft, "bLeft", 3*3);
                SaturnKeyBind("Right", configKeyStickRight, "bRight", 3*4);
                SaturnKeyBind("A", configKeyA, "bA", 3*5);
                SaturnKeyBind("B", configKeyB, "bB", 3*6);
                SaturnKeyBind("Z", configKeyZ, "bZ", 3*7);
                ImGui::Text("Camera");
                SaturnKeyBind("C Up", configKeyCUp, "bCUp", 3*8);
                SaturnKeyBind("C Down", configKeyCDown, "bCDown", 3*9);
                SaturnKeyBind("C Left", configKeyCLeft, "bCLeft", 3*10);
                SaturnKeyBind("C Right", configKeyCRight, "bCRight", 3*11);
                SaturnKeyBind("R", configKeyR, "bR", 3*13);
                ImGui::Text("Other");
                SaturnKeyBind("L", configKeyL, "bL", 3*12);
                SaturnKeyBind("Start", configKeyStart, "bStart", 0);
                ImGui::SliderInt("Rumble###rumble_strength", (int*)&configRumbleStrength, 0, 50);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Editor")) {
                SaturnKeyBind("Stop Input Record", configKeyStopInpRec, "bStopInpRec", 3*26);
                SaturnKeyBind("Screenshot", configKeyScreenshot, "bScreenshot", 3*27);
                ImGui::SameLine();
                imgui_bundled_help_marker("The screenshot goes to the clipboard.");
                ImGui::Text("Camera");
                if (ImGui::TreeNode("Mouse")) {
                    ImGui::Checkbox("X###1", &configCamCtrlMousePanInvX);
                    ImGui::SameLine();
                    ImGui::Checkbox("Y###2", &configCamCtrlMousePanInvY);
                    ImGui::SameLine();
                    ImGui::Text(" - Invert Pan");
                    ImGui::Checkbox("X###3", &configCamCtrlMouseRotInvX);
                    ImGui::SameLine();
                    ImGui::Checkbox("Y###4", &configCamCtrlMouseRotInvY);
                    ImGui::SameLine();
                    ImGui::Text(" - Invert Rotation");
                    ImGui::Checkbox("Invert Zoom###5", &configCamCtrlMouseZoomInv);
                    ImGui::PushItemWidth(200);
                    ImGui::SliderFloat("Pan Sensitivity###6", &configCamCtrlMousePanSens, 0, 2);
                    ImGui::SliderFloat("Rotate Sensitivity###7", &configCamCtrlMouseRotSens, 0, 2);
                    ImGui::SliderFloat("Zoom Sensitivity###8", &configCamCtrlMouseZoomSens, 0, 2);
                    ImGui::PopItemWidth();
                    ImGui::TreePop();
                }
                if (ImGui::TreeNode("Keyboard")) {
                    ImGui::Checkbox("X###9", &configCamCtrlKeybPanInvX);
                    ImGui::SameLine();
                    ImGui::Checkbox("Y###10", &configCamCtrlKeybPanInvY);
                    ImGui::SameLine();
                    ImGui::Text(" - Invert Pan");
                    ImGui::Checkbox("X###11", &configCamCtrlKeybRotInvX);
                    ImGui::SameLine();
                    ImGui::Checkbox("Y###12", &configCamCtrlKeybRotInvY);
                    ImGui::SameLine();
                    ImGui::Text(" - Invert Rotation");
                    ImGui::Checkbox("Invert Zoom###13", &configCamCtrlKeybZoomInv);
                    ImGui::PushItemWidth(200);
                    ImGui::SliderFloat("Pan Sensitivity###14", &configCamCtrlKeybPanSens, 0, 2);
                    ImGui::SliderFloat("Rotate Sensitivity###15", &configCamCtrlKeybRotSens, 0, 2);
                    ImGui::SliderFloat("Zoom Sensitivity###16", &configCamCtrlKeybZoomSens, 0, 2);
                    ImGui::PopItemWidth();
                    ImGui::TreePop();
                }
                if (ImGui::TreeNode("Input Recording")) {
                    if (ImGui::TreeNode("Mouse")) {
                        ImGui::Checkbox("X###17", &configCamCtrlMouseInprecRotInvX);
                        ImGui::SameLine();
                        ImGui::Checkbox("Y###18", &configCamCtrlMouseInprecRotInvY);
                        ImGui::SameLine();
                        ImGui::Text(" - Invert Rotation");
                        ImGui::Checkbox("Invert Zoom###19", &configCamCtrlMouseInprecZoomInv);
                        ImGui::PushItemWidth(200);
                        ImGui::SliderFloat("Rotate Sensitivity###20", &configCamCtrlMouseInprecRotSens, 0, 2);
                        ImGui::SliderFloat("Zoom Sensitivity###21", &configCamCtrlMouseInprecZoomSens, 0, 2);
                        ImGui::PopItemWidth();
                        ImGui::TreePop();
                    }
                    if (ImGui::TreeNode("Keyboard")) {
                        ImGui::Checkbox("X###22", &configCamCtrlKeybInprecRotInvX);
                        ImGui::SameLine();
                        ImGui::Checkbox("Y###23", &configCamCtrlKeybInprecRotInvY);
                        ImGui::SameLine();
                        ImGui::Text(" - Invert Rotation");
                        ImGui::PushItemWidth(200);
                        ImGui::SliderFloat("Rotate Sensitivity###24", &configCamCtrlKeybInprecSens, 0, 2);
                        ImGui::PopItemWidth();
                        ImGui::TreePop();
                    }
                    ImGui::TreePop();
                }
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
    }
    if (ImGui::CollapsingHeader("Gameplay")) {
        ImGui::Checkbox("Infinite Health", &Cheats.GodMode);
        ImGui::Checkbox("Moon Jump", &Cheats.MoonJump);
        imgui_bundled_tooltip("Just like '07! Hold L to in the air to moon jump.");

        //ImGui::PushItemWidth(150);
        //if (ImGui::InputInt("Stars###star_count", (int*)&configFakeStarCount)) {
        //    gMarioState->numStars = configFakeStarCount;
        //} ImGui::SameLine(); imgui_bundled_help_marker("The game-logic star counter; Controls unlockable doors, HUD, cutscenes, etc.");
        //ImGui::PopItemWidth();
        ImGui::Checkbox("Unlock Doors", &configUnlockDoors);
        imgui_bundled_tooltip("Unlocks all areas in the castle, regardless of save file.");
        ImGui::Checkbox("Disable Water Bombs", &configNoWaterBombs);
        imgui_bundled_tooltip("Removes Water Bombs from Bob-Omb Battlefield.");
        ImGui::Checkbox("Disable Camera Shake", &configNoCamShake);
        imgui_bundled_tooltip("Removes camera shakes.");
        ImGui::Checkbox("Butterflies Begone (TM)", &configNoButterflies);
        imgui_bundled_tooltip("Hides butterflies.");
        ImGui::Checkbox("Disable Water Controls", &configNoWater);
        imgui_bundled_tooltip("Makes it so you have non-water controls in water.");
        ImGui::Checkbox("Disable Neck Breaks", &configCUpLimit);
        imgui_bundled_tooltip("Limits the C-Up head rotations, just like in vanilla.\nMario's neck will be happy.");
        ImGui::Checkbox("#justice4jrbship", &configEnableShipBug);
        imgui_bundled_tooltip("Re-introduce the funny bug");
    }
    if (ImGui::CollapsingHeader("Editor###editor_settings")) {
        ImGui::Checkbox("Show tooltips", &configEditorShowTips);
        imgui_bundled_tooltip("Displays tooltips and help markers for advanced features; Helpful for new users.");
        ImGui::Checkbox("Show expression previews", &configEditorExpressionPreviews);
        imgui_bundled_tooltip("Displays small image previews for expression and eye textures; May cause lag on low-end machines.");
        //ImGui::Checkbox("Auto-apply CC color editor", &configEditorFastApply);
        //imgui_bundled_tooltip("If enabled, color codes will automatically apply in the CC editor; May cause lag on low-end machines.");
        ImGui::Checkbox("Splash screen", &configSaturnSplash);
        imgui_bundled_tooltip("Shows a Saturn splash screen on startup.");
        ImGui::SliderInt("###autosave_delay_slider", (int*)&configAutosaveDelay, 10, 600, "Autosave Delay: %ds");
        imgui_bundled_tooltip("Delay between each project autosave.");
        ImGui::Checkbox("Unstable Features", &configUnstableFeatures);
        imgui_bundled_tooltip("Enables features that are not stable enough for an official release.\nThese features are marked with a (!)");
        //ImGui::Checkbox("Auto-apply model default CC", &configEditorAutoModelCc);
        //imgui_bundled_tooltip("If enabled, a model-unique color code (if present) will automatically be assigned when selecting a model.");
        //ImGui::Checkbox("Always show chroma options", &configEditorAlwaysChroma);
        //imgui_bundled_tooltip("Allows the usage of CHROMA KEY features outside of the paired stage; Useful only for models and custom-compiled levels.");
        ImGui::SliderInt("Worldsim Steps", (int*)&configWorldsimSteps, 1, 50);
        imgui_bundled_tooltip("Only make nth worldsim frame as sampling frame.\nHigher value reduces memory usage at the cost of performance.");
    }
    if (ImGui::CollapsingHeader("Save File")) {
        ImGui::SeparatorText("Stars");
        if (ImGui::BeginTable("###savefile_table", 9)) {
            ImGui::TableSetupColumn("###textcol", ImGuiTableColumnFlags_WidthStretch);
            for (int i = 0; i < 8; i++) {
                ImGui::TableSetupColumn("###textcol", ImGuiTableColumnFlags_WidthFixed, ImGui::GetFrameHeight());
            }
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("Level");
            for (int i = 0; i < 8; i++) {
                ImGui::TableNextColumn();
                if (i == 6) {
                    ImGui::Text("O");
                    imgui_bundled_tooltip("100 Coin Star");
                }
                else if (i == 7) {
                    ImGui::Text("C");
                    imgui_bundled_tooltip("Cannon Unlocked");
                }
                else {
                    ImGui::Text("%d", i + 1);
                }
            }
            const char* course_names[] = {
                "BOB", "WF", "JRB", "CCM", "BBH",
                "HMC", "LLL", "SSL", "DDD", "SL",
                "WDW", "TTM", "THI", "TTC", "RR",
                "BITDW", "BITFS", "BITS", "PSS",
                "COTMC", "TOTWC", "VCUTM", "WMOTR",
                "SA", "Cake"
            };
            for (int i = 0; i < COURSE_COUNT; i++) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("%s", course_names[i]);
                u32 flags = gSaveBuffer.files[0][0].courseStars[i];
                for (int j = 0; j < 8; j++) {
                    ImGui::TableNextColumn();
                    bool checked = flags & (1 << j);
                    if (ImGui::Checkbox(("###" + std::to_string(i) + "," + std::to_string(j)).c_str(), &checked)) {
                        u32 prev_flags = flags;
                        if (checked) flags |= (1 << j);
                        else flags &= ~(1 << j);
                        gSaveBuffer.files[0][0].courseStars[i] = flags;
                        gSaveFileModified = true;
                        save_file_do_save(0);
                    }
                }
            }
            ImGui::EndTable();
        }
        ImGui::SeparatorText("Flags");
        const char* flag_names[] = {
            "Save File Exists",
            "Wing Cap Unlocked",
            "Metal Cap Unlocked",
            "Vanish Cap Unlocked",
            "Has Key 1",
            "Has Key 2",
            "Basement Unlocked",
            "Upstairs Unlocked",
            "DDD Moved Back",
            "Moat Drained",
            "Unlocked PSS Door",
            "Unlocked WF Door",
            "Unlocked CCM Door",
            "Unlocked JRB Door",
            "Unlocked BITDW Door",
            "Unlocked BITFS Door",
            "Cap on Ground",
            "Cap on Klepto",
            "Cap on Ukiki",
            "Cap on Mr Blizzard",
            "Unlocked 50 Star Door",
        };
        for (int i = 0; i < IM_ARRAYSIZE(flag_names); i++) {
            bool checked = gSaveBuffer.files[0][0].flags & (1 << i);
            if (ImGui::Checkbox(flag_names[i], &checked)) {
                if (checked) gSaveBuffer.files[0][0].flags |= (1 << i);
                else gSaveBuffer.files[0][0].flags &= ~(1 << i);
                gSaveFileModified = true;
                save_file_do_save(0);
            }
        }
    }
    if (saturn_actor_is_recording_input()) ImGui::BeginDisabled();
}