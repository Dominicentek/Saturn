#include "saturn_imgui.h"

#include <filesystem>
#include <ios>
#include <string>
#include <iostream>
#include <algorithm>
#include <map>
#include <fstream>
#include <thread>

#include "behavior_data.h"
#include "game/area.h"
#include "model_ids.h"
#include "object_constants.h"
#include "saturn/filesystem/saturn_embedded_filesystem.h"
#include "saturn/imgui/saturn_imgui_file_browser.h"
#include "saturn/imgui/saturn_imgui_dynos.h"
#include "saturn/imgui/saturn_imgui_cc_editor.h"
#include "saturn/imgui/saturn_imgui_machinima.h"
#include "saturn/imgui/saturn_imgui_settings.h"
#include "saturn/imgui/saturn_imgui_chroma.h"
#include "saturn/libs/imgui/imgui.h"
#include "saturn/libs/imgui/imgui_internal.h"
#include "saturn/libs/imgui/imgui_impl_sdl.h"
#include "saturn/libs/imgui/imgui_impl_opengl3.h"
#include "saturn/libs/imgui/imgui_neo_sequencer.h"
#include "saturn/saturn.h"
#include "saturn/saturn_actors.h"
#include "saturn/saturn_colors.h"
#include "saturn/saturn_textures.h"
#include "saturn/saturn_animation_ids.h"
#include "saturn/discord/saturn_discord.h"
#include "pc/controller/controller_keyboard.h"
#include "data/dynos.cpp.h"
#include "icons/IconsForkAwesome.h"
#include "icons/IconsFontAwesome5.h"
#include "saturn/filesystem/saturn_projectfile.h"
#include "saturn/filesystem/saturn_windowfile.h"
#include "saturn/saturn_json.h"
#include "saturn/saturn_video_renderer.h"
#include "saturn/saturn_version.h"
#include "types.h"
#include "downloader.h"

#include <SDL2/SDL.h>
#include <zip.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#ifdef __MINGW32__
# define FOR_WINDOWS 1
#else
# define FOR_WINDOWS 0
#endif

#if FOR_WINDOWS || defined(OSX_BUILD)
# define GLEW_STATIC
# include <GL/glew.h>
#endif

#define GL_GLEXT_PROTOTYPES 1
#ifdef USE_GLES
# include <SDL2/SDL_opengles2.h>
# define RAPI_NAME "OpenGL ES"
#else
# include <SDL2/SDL_opengl.h>
# define RAPI_NAME "OpenGL"
#endif

#if FOR_WINDOWS
#define PLATFORM "Windows"
#define PLATFORM_ICON ICON_FK_WINDOWS
#elif defined(OSX_BUILD)
#define PLATFORM "Mac OS"
#define PLATFORM_ICON ICON_FK_APPLE
#else
#define PLATFORM "Linux"
#define PLATFORM_ICON ICON_FK_LINUX
#endif

extern "C" {
#include "pc/gfx/gfx_pc.h"
#include "pc/configfile.h"
#include "game/mario.h"
#include "game/game_init.h"
#include "game/camera.h"
#include "game/level_update.h"
#include "engine/level_script.h"
#include "game/object_list_processor.h"
#include "pc/pngutils.h"
#include "game/object_helpers.h"
}

using namespace std;

SDL_Window* window = nullptr;

Array<PackData *> &iDynosPacks = DynOS_Gfx_GetPacks();

// Variables

int currentMenu = 0;
bool showStatusBars = true;

bool windowStats = true;
bool windowCcEditor;
bool windowAnimPlayer;
bool windowSettings;
bool windowChromaKey;

bool chromaRequireReload;

int windowStartHeight;

bool has_copy_camera;
bool copy_relative = true;

bool paste_forever;

int copiedKeyframeIndex = -1;

bool k_context_popout_open = false;
Keyframe k_context_popout_keyframe = Keyframe();
ImVec2 k_context_popout_pos = ImVec2(0, 0);

bool was_camera_frozen = false;

bool splash_finished = false;

std::string editor_theme = "moon";
std::vector<std::pair<std::string, std::string>> theme_list = {};
std::vector<std::string> textures_list = {};

float game_viewport[4] = { 0, 0, -1, -1 };

bool request_mario_tab = false;

int sel_category = 0;
std::vector<std::pair<const char*, std::vector<const BehaviorScript*>>> obj_categories = {
    { "Enemies", {
        bhvGoomba, bhvKoopa, bhvBobomb, bhvKingBobomb, bhvWhompKingBoss, bhvThwomp,
        bhvThwomp2, bhvEnemyLakitu, bhvUnagi, bhvSnufit, bhvBubba, bhvFlyGuy,
        bhvFlamethrower, bhvBoo, bhvMontyMole, bhvMoneybag, bhvMoneybagHidden,
        bhvPokey, bhvPokeyBodyPart, bhvMrIBody, bhvMrI, bhvMrBlizzard,
        bhvScuttlebug, bhvScuttlebugSpawn, bhvChuckya, bhvSmallBully, bhvSmallChillBully,
        bhvBigBully, bhvBigBullyWithMinions, bhvBigChillBully, bhvBowser,
        bhvPiranhaPlant, bhvSpiny, bhvSpindrift, bhvBub, bhvSushiShark, bhvSkeeter,
        bhvHeaveHo, bhvHeaveHoThrowMario, bhvHomingAmp, bhvCirclingAmp
    }},
    { "Coins", {
        bhvYellowCoin, bhvBlueCoinSliding, bhvBlueCoinJumping,
        bhvOneCoin, bhvRedCoin, bhvMovingYellowCoin, bhvMovingBlueCoin,
        bhvSingleCoinGetsSpawned
    }},
    { "Trees", {
        bhvTree
    }},
};

#include "saturn/saturn_timelines.h"

// Bundled Components

void imgui_bundled_tooltip(const char* text) {
    if (!configEditorShowTips) return;

    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(450.0f);
        ImGui::TextUnformatted(text);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

void imgui_bundled_help_marker(const char* desc) {
    if (!configEditorShowTips) {
        ImGui::TextDisabled("");
        return;
    }

    ImGui::TextDisabled(ICON_FA_QUESTION_CIRCLE);
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
        ImGui::TextUnformatted(desc);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

void imgui_bundled_space(float size, const char* title = NULL, const char* help_marker = NULL) {
    ImGui::Dummy(ImVec2(0, size/2));
    ImGui::Separator();
    if (title == NULL) {
        ImGui::Dummy(ImVec2(0, size/2));
    } else {
        ImGui::Dummy(ImVec2(0, size/4));
        ImGui::Text(title);
        if (help_marker != NULL) {
            ImGui::SameLine(); imgui_bundled_help_marker(help_marker);
        }
        ImGui::Dummy(ImVec2(0, size/4));
    }
}

void imgui_bundled_window_reset(const char* windowTitle, int width, int height, int x, int y) {
    if (ImGui::BeginPopupContextItem()) {
        if (ImGui::Selectable(ICON_FK_UNDO " Reset Window Pos")) {
            ImGui::SetWindowSize(ImGui::FindWindowByName(windowTitle), ImVec2(width, height));
            ImGui::SetWindowPos(ImGui::FindWindowByName(windowTitle), ImVec2(x, y));
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

ImGuiWindowFlags imgui_bundled_window_corner(int corner, int width, int height, float alpha) {
    ImGuiIO& io = ImGui::GetIO();
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar;
    if (corner != -1) {
        const float PAD = 10.0f;
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImVec2 work_pos = viewport->WorkPos;
        ImVec2 work_size = viewport->WorkSize;
        ImVec2 window_pos, window_pos_pivot;
        window_pos.x = (corner & 1) ? (work_pos.x + work_size.x - PAD) : (work_pos.x + PAD);
        window_pos.y = (corner & 2) ? (work_pos.y + work_size.y - PAD) : (work_pos.y + PAD);
        window_pos_pivot.x = (corner & 1) ? 1.0f : 0.0f;
        window_pos_pivot.y = (corner & 2) ? 1.0f : 0.0f;
        ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always, window_pos_pivot);
        if (width != 0) ImGui::SetNextWindowSize(ImVec2(width, height));
        window_flags |= ImGuiWindowFlags_NoMove;
        ImGui::SetNextWindowBgAlpha(alpha);
    }

    return window_flags;
}

#define JSON_VEC4(json, entry, dest) if (json.isMember(entry)) dest = ImVec4(json[entry][0].asFloat(), json[entry][1].asFloat(), json[entry][2].asFloat(), json[entry][3].asFloat())
#define JSON_BOOL(json, entry, dest) if (json.isMember(entry)) dest = json[entry].asBool()
#define JSON_FLOAT(json, entry, dest) if (json.isMember(entry)) dest = json[entry].asFloat()
#define JSON_VEC2(json, entry, dest) if (json.isMember(entry)) dest = ImVec2(json[entry][0].asFloat(), json[entry][1].asFloat())
#define JSON_DIR(json, entry, dest) if (json.isMember(entry)) dest = to_dir(json[entry].asString())

ImGuiDir to_dir(std::string dir) {
    if (dir == "up") return ImGuiDir_Up;
    if (dir == "left") return ImGuiDir_Left;
    if (dir == "down") return ImGuiDir_Down;
    if (dir == "right") return ImGuiDir_Right;
    return ImGuiDir_None;
}

void imgui_custom_theme(std::string theme_name) {
    std::filesystem::path path = std::filesystem::path("dynos/themes/" + theme_name + ".json");
    if (!std::filesystem::exists(path)) return;
    std::ifstream file = std::ifstream(path);
    Json::Value json;
    json << file;
    ImGuiStyle* style = &ImGui::GetStyle();
    style->ResetStyle();
    ImVec4* colors = style->Colors;
    if (json.isMember("colors")) {
        Json::Value colorsJson = json["colors"];
        JSON_VEC4(colorsJson, "text", colors[ImGuiCol_Text]);
        JSON_VEC4(colorsJson, "text_disabled", colors[ImGuiCol_TextDisabled]);
        JSON_VEC4(colorsJson, "window_bg", colors[ImGuiCol_WindowBg]);
        JSON_VEC4(colorsJson, "child_bg", colors[ImGuiCol_ChildBg]);
        JSON_VEC4(colorsJson, "popup_bg", colors[ImGuiCol_PopupBg]);
        JSON_VEC4(colorsJson, "border", colors[ImGuiCol_Border]);
        JSON_VEC4(colorsJson, "border_shadow", colors[ImGuiCol_BorderShadow]);
        JSON_VEC4(colorsJson, "frame_bg", colors[ImGuiCol_FrameBg]);
        JSON_VEC4(colorsJson, "frame_bg_hovered", colors[ImGuiCol_FrameBgHovered]);
        JSON_VEC4(colorsJson, "frame_bg_active", colors[ImGuiCol_FrameBgActive]);
        JSON_VEC4(colorsJson, "title_bg", colors[ImGuiCol_TitleBg]);
        JSON_VEC4(colorsJson, "title_bg_active", colors[ImGuiCol_TitleBgActive]);
        JSON_VEC4(colorsJson, "title_bg_collapsed", colors[ImGuiCol_TitleBgCollapsed]);
        JSON_VEC4(colorsJson, "menu_bar_bg", colors[ImGuiCol_MenuBarBg]);
        JSON_VEC4(colorsJson, "scrollbar_bg", colors[ImGuiCol_ScrollbarBg]);
        JSON_VEC4(colorsJson, "scrollbar_grab", colors[ImGuiCol_ScrollbarGrab]);
        JSON_VEC4(colorsJson, "scrollbar_grab_hovered", colors[ImGuiCol_ScrollbarGrabHovered]);
        JSON_VEC4(colorsJson, "scrollbar_grab_active", colors[ImGuiCol_ScrollbarGrabActive]);
        JSON_VEC4(colorsJson, "check_mark", colors[ImGuiCol_CheckMark]);
        JSON_VEC4(colorsJson, "slider_grab", colors[ImGuiCol_SliderGrab]);
        JSON_VEC4(colorsJson, "slider_grab_active", colors[ImGuiCol_SliderGrabActive]);
        JSON_VEC4(colorsJson, "button", colors[ImGuiCol_Button]);
        JSON_VEC4(colorsJson, "button_hovered", colors[ImGuiCol_ButtonHovered]);
        JSON_VEC4(colorsJson, "button_active", colors[ImGuiCol_ButtonActive]);
        JSON_VEC4(colorsJson, "header", colors[ImGuiCol_Header]);
        JSON_VEC4(colorsJson, "header_hovered", colors[ImGuiCol_HeaderHovered]);
        JSON_VEC4(colorsJson, "header_active", colors[ImGuiCol_HeaderActive]);
        JSON_VEC4(colorsJson, "separator", colors[ImGuiCol_Separator]);
        JSON_VEC4(colorsJson, "separator_hovered", colors[ImGuiCol_SeparatorHovered]);
        JSON_VEC4(colorsJson, "separator_active", colors[ImGuiCol_SeparatorActive]);
        JSON_VEC4(colorsJson, "resize_grip", colors[ImGuiCol_ResizeGrip]);
        JSON_VEC4(colorsJson, "resize_grip_hovered", colors[ImGuiCol_ResizeGripHovered]);
        JSON_VEC4(colorsJson, "resize_grip_active", colors[ImGuiCol_ResizeGripActive]);
        JSON_VEC4(colorsJson, "tab", colors[ImGuiCol_Tab]);
        JSON_VEC4(colorsJson, "tab_hovered", colors[ImGuiCol_TabHovered]);
        JSON_VEC4(colorsJson, "tab_active", colors[ImGuiCol_TabActive]);
        JSON_VEC4(colorsJson, "tab_unfocused", colors[ImGuiCol_TabUnfocused]);
        JSON_VEC4(colorsJson, "tab_unfocused_active", colors[ImGuiCol_TabUnfocusedActive]);
        JSON_VEC4(colorsJson, "plot_lines", colors[ImGuiCol_PlotLines]);
        JSON_VEC4(colorsJson, "plot_lines_hovered", colors[ImGuiCol_PlotLinesHovered]);
        JSON_VEC4(colorsJson, "plot_histogram", colors[ImGuiCol_PlotHistogram]);
        JSON_VEC4(colorsJson, "plot_histogram_hovered", colors[ImGuiCol_PlotHistogramHovered]);
        JSON_VEC4(colorsJson, "table_header_bg", colors[ImGuiCol_TableHeaderBg]);
        JSON_VEC4(colorsJson, "table_border_strong", colors[ImGuiCol_TableBorderStrong]);
        JSON_VEC4(colorsJson, "table_border_light", colors[ImGuiCol_TableBorderLight]);
        JSON_VEC4(colorsJson, "table_row_bg", colors[ImGuiCol_TableRowBg]);
        JSON_VEC4(colorsJson, "table_row_bg_alt", colors[ImGuiCol_TableRowBgAlt]);
        JSON_VEC4(colorsJson, "text_selected_bg", colors[ImGuiCol_TextSelectedBg]);
        JSON_VEC4(colorsJson, "drag_drop_target", colors[ImGuiCol_DragDropTarget]);
        JSON_VEC4(colorsJson, "nav_highlight", colors[ImGuiCol_NavHighlight]);
        JSON_VEC4(colorsJson, "nav_windowing_highlight", colors[ImGuiCol_NavWindowingHighlight]);
        JSON_VEC4(colorsJson, "nav_windowing_dim_bg", colors[ImGuiCol_NavWindowingDimBg]);
        JSON_VEC4(colorsJson, "modal_window_dim_bg", colors[ImGuiCol_ModalWindowDimBg]);
    }
    if (json.isMember("style")) {
        Json::Value styleJson = json["style"];
        JSON_BOOL(styleJson, "anti_aliased_lines", style->AntiAliasedLines);
        JSON_BOOL(styleJson, "anti_aliased_lines_use_tex", style->AntiAliasedLinesUseTex);
        JSON_BOOL(styleJson, "anti_aliased_fill", style->AntiAliasedFill);
        JSON_FLOAT(styleJson, "alpha", style->Alpha);
        JSON_FLOAT(styleJson, "disabled_alpha", style->DisabledAlpha);
        JSON_FLOAT(styleJson, "window_rounding", style->WindowRounding);
        JSON_FLOAT(styleJson, "window_border_size", style->WindowBorderSize);
        JSON_FLOAT(styleJson, "child_rounding", style->ChildRounding);
        JSON_FLOAT(styleJson, "child_border_size", style->ChildBorderSize);
        JSON_FLOAT(styleJson, "popup_rounding", style->PopupRounding);
        JSON_FLOAT(styleJson, "popup_border_size", style->PopupBorderSize);
        JSON_FLOAT(styleJson, "frame_rounding", style->FrameRounding);
        JSON_FLOAT(styleJson, "frame_border_size", style->FrameBorderSize);
        JSON_FLOAT(styleJson, "indent_spacing", style->IndentSpacing);
        JSON_FLOAT(styleJson, "columns_min_spacing", style->ColumnsMinSpacing);
        JSON_FLOAT(styleJson, "scrollbar_size", style->ScrollbarSize);
        JSON_FLOAT(styleJson, "scrollbar_rounding", style->ScrollbarRounding);
        JSON_FLOAT(styleJson, "grab_min_size", style->GrabMinSize);
        JSON_FLOAT(styleJson, "grab_rounding", style->GrabRounding);
        JSON_FLOAT(styleJson, "log_slider_deadzone", style->LogSliderDeadzone);
        JSON_FLOAT(styleJson, "tab_rounding", style->TabRounding);
        JSON_FLOAT(styleJson, "tab_border_size", style->TabBorderSize);
        JSON_FLOAT(styleJson, "tab_min_width_for_close_button", style->TabMinWidthForCloseButton);
        JSON_FLOAT(styleJson, "mouse_cursor_scale", style->MouseCursorScale);
        JSON_FLOAT(styleJson, "curve_tessellation_tol", style->CurveTessellationTol);
        JSON_FLOAT(styleJson, "circle_tessellation_max_error", style->CircleTessellationMaxError);
        JSON_VEC2(styleJson, "window_padding", style->WindowPadding);
        JSON_VEC2(styleJson, "window_min_size", style->WindowMinSize);
        JSON_VEC2(styleJson, "window_title_align", style->WindowTitleAlign);
        JSON_VEC2(styleJson, "frame_padding", style->FramePadding);
        JSON_VEC2(styleJson, "item_spacing", style->ItemSpacing);
        JSON_VEC2(styleJson, "item_inner_spacing", style->ItemInnerSpacing);
        JSON_VEC2(styleJson, "cell_padding", style->CellPadding);
        JSON_VEC2(styleJson, "touch_extra_padding", style->TouchExtraPadding);
        JSON_VEC2(styleJson, "button_text_align", style->ButtonTextAlign);
        JSON_VEC2(styleJson, "selectable_text_align", style->SelectableTextAlign);
        JSON_VEC2(styleJson, "display_window_padding", style->DisplayWindowPadding);
        JSON_VEC2(styleJson, "display_safe_area_padding", style->DisplaySafeAreaPadding);
        JSON_DIR(styleJson, "color_button_position", style->ColorButtonPosition);
        JSON_DIR(styleJson, "window_menu_button_position", style->WindowMenuButtonPosition);
    }
}

#undef JSON_VEC4
#undef JSON_BOOL
#undef JSON_FLOAT
#undef JSON_VEC2
#undef JSON_DIR

bool initFont = true;

void imgui_update_theme() {
    ImGuiIO& io = ImGui::GetIO();
    ImGuiStyle* style = &ImGui::GetStyle();

    float SCALE = 1.f;

    if (initFont) {
        initFont = false;

        ImFontConfig defaultConfig;
        defaultConfig.SizePixels = 13.0f * SCALE;
        io.Fonts->AddFontDefault(&defaultConfig);

        ImFontConfig symbolConfig;
        symbolConfig.MergeMode = true;
        symbolConfig.SizePixels = 13.0f * SCALE;
        symbolConfig.GlyphMinAdvanceX = 13.0f * SCALE; // Use if you want to make the icon monospaced
        static const ImWchar icon_ranges[] = { ICON_MIN_FK, ICON_MAX_FK, 0 };
        io.Fonts->AddFontFromFileTTF("fonts/forkawesome-webfont.ttf", symbolConfig.SizePixels, &symbolConfig, icon_ranges);
    }

    // backwards compatibility with older theme settings
    editor_theme = "legacy";
    if (configEditorTheme == 1) editor_theme = "moon";
    else if (configEditorTheme == 2) editor_theme = "halflife";
    else if (configEditorTheme == 3) editor_theme = "moviemaker";
    else if (configEditorTheme == 4) editor_theme = "dear";
    else if (std::filesystem::exists("dynos/themes")) {
        for (const auto& entry : std::filesystem::directory_iterator("dynos/themes")) {
            std::filesystem::path path = entry.path();
            if (path.extension().string() != ".json") continue;
            std::string name = path.filename().string();
            name = name.substr(0, name.length() - 5);
            if (string_hash(name.c_str(), 0, name.length()) == configEditorThemeJson) {
                editor_theme = name;
                break;
            }
        }
    }
    std::cout << "Using theme: " << editor_theme << std::endl;
    configEditorTheme = -1;
    imgui_custom_theme(editor_theme);

    style->ScaleAllSizes(SCALE);
}

int videores[] = { 1920, 1080 };
int capture_buffer = 0;
bool capturing_video = false;
bool orthographic_mode = false;
bool processing_frame = false;
bool transparency_enabled = true, checkbox_transparency_enabled = true;
bool sixty_fps_enabled = true, checkbox_sixty_fps_enabled = true;
std::string capture_destination_file = "";
int stop_capture = 0;
int request_ortho_mode = 0;
struct OrthographicRenderSettings ortho_settings = (struct OrthographicRenderSettings) {
    .scale = 1.f,
    .offset_x = 0.f,
    .offset_y = 0.f,
    .rotation_x = 25.f,
    .rotation_y = 45.f,
};

const float ANTIALIAS_MODIFIER = 1.f;

bool keep_aspect_ratio = false;

void saturn_get_game_bounds(float* out, ImVec2 size) {
    if (keep_aspect_ratio || saturn_imgui_is_capturing_video()) {
        float aspect_ratio = (float)videores[0] / (float)videores[1];
        float container_ar = size.x / size.y;
        if (aspect_ratio < container_ar) {
            out[2] = size.y * aspect_ratio;
            out[3] = size.y;
        }
        else {
            out[2] = size.x;
            out[3] = size.x / aspect_ratio;
        }
        out[0] = (size.x - out[2]) / 2;
        out[1] = (size.y - out[3]) / 2;
        return;
    }
    out[0] = 0;
    out[1] = 0;
    out[2] = size.x;
    out[3] = size.y;
}

bool saturn_imgui_get_viewport(int* width, int* height) {
    int w, h;
    if (width == nullptr) width = &w;
    if (height == nullptr) height = &h;
    if (capturing_video) {
        *width = videores[0];
        *height = videores[1];
        return true;
    }
    if (game_viewport[2] != -1 && game_viewport[3] != -1) {
        float bounds[4];
        saturn_get_game_bounds(bounds, ImVec2(game_viewport[2], game_viewport[3]));
        *width  = bounds[2];
        *height = bounds[3];
        return true;
    }
    SDL_GetWindowSize(window, width, height);
    return false;
}

void* framebuffer;
pthread_t capture_thread;
int renderer_current_frame, renderer_num_frames = 0;
bool ui_only_frame = false;

bool ffmpeg_installed = true;
bool clipboard_enabled = false;

bool is_wayland() {
#ifdef _WIN32
    return false;
#else
    return getenv("WAYLAND_DISPLAY");
#endif
}

void saturn_write_screenshot(std::string dest, void* image) {
    if (dest.empty()) { // clipboard
#ifdef _WIN32
        BITMAPV5HEADER header = {
            .bV5Size = sizeof(BITMAPV5HEADER),
            .bV5Width = videores[0],
            .bV5Height = videores[1],
            .bV5Planes = 1,
            .bV5BitCount = 32,
            .bV5Compression = BI_BITFIELDS,
            .bV5RedMask   = 0x000000FF,
            .bV5GreenMask = 0x0000FF00,
            .bV5BlueMask  = 0x00FF0000,
            .bV5AlphaMask = 0xFF000000,
            .bV5CSType = *(DWORD*)"Win ",
        };
        HGLOBAL global = GlobalAlloc(GMEM_MOVEABLE, sizeof(BITMAPV5HEADER) + videores[0] * videores[1] * 4);
        if (global) {
            BYTE* buffer = (BYTE*)GlobalLock(global);
            if (buffer) {
                CopyMemory(buffer, &header, sizeof(BITMAPV5HEADER));
                CopyMemory(buffer + sizeof(BITMAPV5HEADER), image, videores[0] * videores[1] * 4);
                GlobalUnlock(global);
            }
            if (OpenClipboard(NULL)) {
                EmptyClipboard();
                SetClipboardData(CF_DIBV5, global);
                CloseClipboard();
            }
        }
#else
        int outlen;
        unsigned char* data = pngutils_write_png_to_mem((unsigned char*)image, 0, videores[0], videores[1], 4, &outlen);
        FILE* command;
        if (is_wayland()) command = popen("wl-copy --type image/png", "w");
        else command = popen("xclip -selection clipboard -t image/png -i", "w");
        fwrite(data, outlen, 1, command);
        fclose(command);
        free(data);
#endif
        return;
    }
    pngutils_write_png(dest.c_str(), videores[0], videores[1], 4, image, 0);
}

void* saturn_capture_screenshot(void* image) {
    processing_frame = true;
    if (renderer_num_frames != 0) {
        if (renderer_current_frame >= 0) video_renderer_render((unsigned char*)image);
        renderer_current_frame++;
        k_current_frame = renderer_current_frame / (sixty_fps_enabled ? 2 : 1);
        if (renderer_current_frame < 0) k_current_frame = 0;
        if (k_current_frame != k_previous_frame) for (auto& timeline : k_frame_keys) {
            saturn_keyframe_apply(timeline.first, k_current_frame);
        }
        k_previous_frame = k_current_frame;
        if (stop_capture || renderer_current_frame == renderer_num_frames) {
            video_renderer_finalize();
            capturing_video = false;
            stop_capture = false;
        }
    }
    else {
        saturn_write_screenshot(capture_destination_file, image);
        capturing_video = false;
    }
    free(image);
    processing_frame = false;
    return NULL;
}

void saturn_imgui_capture_next_frame(std::string dest) {
    if (dest == "" && !clipboard_enabled) return;
    capture_destination_file = dest;
    capturing_video = true;
    keyframe_playing = false;
    renderer_num_frames = 0;
    capture_buffer = 2;
}

bool saturn_imgui_is_capturing_video() {
    return capturing_video;
}

bool saturn_imgui_is_orthographic() {
    return orthographic_mode;
}

bool saturn_imgui_is_processing_frame() {
    return processing_frame;
}

void saturn_imgui_ui_only_frame(bool is_ui_only) {
    ui_only_frame = is_ui_only;
}

void saturn_imgui_set_ortho(bool ortho_mode) {
    request_ortho_mode = ortho_mode + 1;
    orthographic_mode = ortho_mode;
}

void saturn_imgui_stop_capture() {
    if (stop_capture > 0) return;
    stop_capture = 1;
}

bool saturn_imgui_is_capturing_transparent_video() {
    return capturing_video && transparency_enabled && ((video_renderer_flags & VIDEO_RENDERER_FLAGS_TRANSPARECY) || renderer_num_frames == 0);
}

void saturn_imgui_set_frame_buffer(void* fb, bool do_capture) {
    framebuffer = fb;
    if (!processing_frame && !ui_only_frame && capturing_video && (do_capture || sixty_fps_enabled) && capture_buffer == 0) {
        uint64_t tex_size = (uint64_t)videores[0] * (uint64_t)videores[1] * 4;
        unsigned char* image = (unsigned char*)malloc(tex_size);
        glBindTexture(GL_TEXTURE_2D, (GLuint)(intptr_t)fb);
        glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, image);
        glBindTexture(GL_TEXTURE_2D, 0);
        pthread_create(&capture_thread, NULL, saturn_capture_screenshot, image);
        pthread_detach(capture_thread);
    }
    if (capture_buffer > 0) capture_buffer--;
}

// Set up ImGui

bool imgui_config_exists = false;
std::map<std::string, bool> visible_windows = {};

void saturn_imgui_create_dockspace_layout(ImGuiID dockspace) {
    if (visible_windows.empty()) {
        visible_windows.insert({ "Machinima", true });
        visible_windows.insert({ "Marios", true });
        visible_windows.insert({ "Settings", true });
        visible_windows.insert({ "Game", true });
        visible_windows.insert({ "Timeline###kf_timeline", true });
        visible_windows.insert({ "Objects", true });
        saturn_load_window_visibility("windows.bin", &visible_windows);
    }
    if (imgui_config_exists) return;
    imgui_config_exists = true;
    ImGuiID left, right, up, down;
    ImGui::DockBuilderRemoveNode(dockspace);
    ImGui::DockBuilderAddNode(dockspace, ImGuiDockNodeFlags_PassthruCentralNode | ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspace, ImGui::GetWindowViewport()->Size);
    ImGui::DockBuilderSplitNode(dockspace, ImGuiDir_Up, 0.7f, &up, &down);
    ImGui::DockBuilderSplitNode(up, ImGuiDir_Left, 0.25f, &left, &right);
    ImGui::DockBuilderDockWindow("Machinima", left);
    ImGui::DockBuilderDockWindow("Marios", left);
    ImGui::DockBuilderDockWindow("Objects", left);
    ImGui::DockBuilderDockWindow("Settings", left);
    ImGui::DockBuilderDockWindow("Game", right);
    ImGui::DockBuilderDockWindow("Timeline###kf_timeline", down);
}

void saturn_imgui_setup_dockspace() {
    ImGuiViewport* viewport = ImGui::GetWindowViewport();
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(viewport->Size);
    ImGuiWindowFlags windowFlags =
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNavFocus |
        ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_MenuBar;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("Dockspace", NULL, windowFlags);
    ImGui::PopStyleVar();
    ImGuiID dockspace = ImGui::DockSpace(ImGui::GetID("Dockspace"), ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);
    ImGui::End();
    saturn_imgui_create_dockspace_layout(dockspace);
}

bool saturn_imgui_window(const char* title, ImGuiWindowFlags extra_flags = ImGuiWindowFlags_None) {
    if (capturing_video) return false;
    if (visible_windows.find(title) == visible_windows.end()) {
        std::cout << "! Window " << title << " not registered" << std::endl;
        return false;
    }
    if (!visible_windows[title]) return false;
    if (ImGui::Begin(title, nullptr, ImGuiWindowFlags_NoCollapse | extra_flags)) return true;
    else {
        ImGui::End();
        return false;
    }
}

void saturn_imgui_init_backend(SDL_Window * sdl_window, SDL_GLContext ctx) {
    window = sdl_window;

    imgui_config_exists = std::filesystem::exists("imgui.ini");

    const char* glsl_version = "#version 120";
    ImGuiContext* imgui = ImGui::CreateContext();
    ImGui::SetCurrentContext(imgui);
    ImGuiIO& io = ImGui::GetIO();
    io.WantSetMousePos = false;
    io.ConfigWindowsMoveFromTitleBarOnly = true;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    imgui_update_theme();

    ImGui_ImplSDL2_InitForOpenGL(window, ctx);
    ImGui_ImplOpenGL3_Init(glsl_version);

    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, configWindowState?"1":"0");
}

void saturn_load_themes() {
    if (!std::filesystem::exists("dynos/themes")) return;
    for (const auto& entry : std::filesystem::directory_iterator("dynos/themes")) {
        std::filesystem::path path = entry.path();
        if (path.extension().string() != ".json") continue;
        std::string id = path.filename().string();
        id = id.substr(0, id.length() - 5);
        std::ifstream stream = std::ifstream(path);
        Json::Value json;
        try {
            json << stream;
        }
        catch (std::runtime_error err) {
            std::cout << "Error parsing theme " << id << std::endl;
            continue;
        }
        if (!json.isMember("name")) continue;
        std::cout << "Found theme " << json["name"].asString() << " (" << id << ")" << std::endl;
        theme_list.push_back({ id, json["name"].asString() });
    }
}

void saturn_load_textures() {
    if (!std::filesystem::exists("dynos/textures")) {
        std::filesystem::create_directory("dynos/textures");
    }
    for (const auto& entry : std::filesystem::directory_iterator("dynos/textures")) {
        if (!entry.is_directory()) continue;
        std::string name = entry.path().filename().string();
        if (string_hash(name.data(), 0, name.length()) == configEditorTextures) current_texture_id = textures_list.size();
        textures_list.push_back(entry.path().filename().string());
    }
}

bool mario_menu_do_open = false;
int game_focus_timer = 0;
int mario_menu_index = -1;

bool is_focused_on_game() {
    return game_focus_timer++ >= 3;
}

void saturn_imgui_open_mario_menu(int index) {
    mario_menu_do_open = true;
    mario_menu_index = index;
}

bool does_command_exist(std::string command) {
    std::string path = std::string(getenv("PATH"));
#ifdef _WIN32
    char delimiter = ';';
    std::string suffix = ".exe";
#else
    char delimiter = ':';
    std::string suffix = "";
#endif
    std::vector<std::string> paths = {};
    std::string word = "";
    for (int i = 0; i < path.length(); i++) {
        if (path[i] == delimiter) {
            paths.push_back(word);
            word = "";
        }
        else word += path[i];
    }
    paths.push_back(word);
    for (std::string p : paths) {
        std::filesystem::path fsPath = std::filesystem::path(p) / (command + suffix);
        if (std::filesystem::exists(fsPath)) return true;
    }
    return false;
}

bool can_write_to_clipboard() {
#ifdef _WIN32
    return true;
#else
    if (is_wayland()) return does_command_exist("wl-copy");
    else return does_command_exist("xclip");
#endif
}

#ifdef _WIN32
std::thread ffmpeg_download_thread;
double ffmpeg_download_progress = 0;
bool ffmpeg_download_failed = false;
bool ffmpeg_download_in_progress = false;
bool ffmpeg_install_in_progress = false;
bool ffmpeg_install_restart = false;

// This can only be called in Windows
void saturn_download_ffmpeg(void(*callback)(char* buf, size_t siz)) {
    ffmpeg_download_in_progress = true;
    ffmpeg_download_thread = std::thread([callback]() {
        Downloader downloader("https://www.gyan.dev/ffmpeg/builds/ffmpeg-release-essentials.zip");
        downloader.progress([](double now, double total) {
            if (total == 0) ffmpeg_download_progress = 0;
            else ffmpeg_download_progress = now / total;
        });
        downloader.download();
        ffmpeg_download_in_progress = false;
        if (downloader.status < 200 || downloader.status > 299) ffmpeg_download_failed = true;
        else callback(downloader.data.data(), downloader.data.size());
    });
    ffmpeg_download_thread.detach();
}

void saturn_zip_extract_file(zip_t* archive, std::string file, std::string dest) {
    std::filesystem::create_directories(std::filesystem::path(dest).parent_path());
    zip_stat_t file_stat;
    zip_int64_t file_index = zip_name_locate(archive, file.c_str(), ZIP_FL_NOCASE);
    zip_file_t* zip_file = zip_fopen_index(archive, file_index, 0);
    zip_stat_index(archive, file_index, 0, &file_stat);
    size_t outsiz = file_stat.size;
    char*  outbuf = (char*)malloc(outsiz);
    zip_fread(zip_file, outbuf, outsiz);
    zip_fclose(zip_file);
    std::ofstream stream = std::ofstream(dest, std::ios::binary);
    stream.write(outbuf, outsiz);
    stream.close();
    free(outbuf);
}

void saturn_install_ffmpeg(char* buf, size_t siz) {
    ffmpeg_install_in_progress = true;
    zip_error_t error;
    zip_source_t* zip_src = zip_source_buffer_create(buf, siz, 0, &error);
    zip_t* zip_archive = zip_open_from_source(zip_src, 0, &error);
    zip_int64_t num_entries = zip_get_num_entries(zip_archive, 0);
    std::string root_name;
    for (zip_int64_t i = 0; i < num_entries; i++) {
        std::string name = zip_get_name(zip_archive, i, 0);
        if (name.find("essentials_build") != std::string::npos) {
            root_name = name;
            break;
        }
    }
    saturn_zip_extract_file(zip_archive, root_name + "bin/ffmpeg.exe",  "C:/ffmpeg/ffmpeg.exe");
    saturn_zip_extract_file(zip_archive, root_name + "bin/ffplay.exe",  "C:/ffmpeg/ffplay.exe");
    saturn_zip_extract_file(zip_archive, root_name + "bin/ffprobe.exe", "C:/ffmpeg/ffprobe.exe");
    zip_close(zip_archive);
    std::string cmd = // jesus fucking christ
        "powershell Start-Process cmd -ArgumentList '/c start /min \"\" "
        "powershell -WindowStyle Hidden -Command "
        "[Environment]::SetEnvironmentVariable(\\\\\\\"PATH\\\\\\\","
        "\\\\\\\"$([Environment]::GetEnvironmentVariable(\\\\\\\"PATH\\\\\\\", \\\\\\\"Machine\\\\\\\"));"
        "C:/ffmpeg\\\\\\\", \\\\\\\"Machine\\\\\\\");"
        "Add-Type -Assembly System.Windows.Forms;"
        "[System.Windows.Forms.MessageBox]::Show(\\\\\\\""
          "Successfully installed FFmpeg. Saturn Studio needs to be restarted to take effect."
        "\\\\\\\", \\\\\\\"Saturn Studio\\\\\\\", \\\\\\\"OK\\\\\\\", \\\\\\\"Information\\\\\\\")"
        "' -Verb runAs";
    printf("%s\n", cmd.c_str());
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    ZeroMemory(&pi, sizeof(pi));
    si.cb = sizeof(si);
    CreateProcess(nullptr, (LPSTR)cmd.c_str(), nullptr, nullptr, false, CREATE_NO_WINDOW | CREATE_NEW_PROCESS_GROUP, nullptr, nullptr, &si, &pi);
    WaitForSingleObject(pi.hProcess, INFINITE);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    Sleep(3); // we sleep a bit
    ffmpeg_install_in_progress = false;
    ffmpeg_install_restart = true;
}
#endif

bool update_available = false;
std::thread check_update_thread;

#ifdef _WIN32
#define UPDATETOOL "updatetool.exe"
#define GET_EXE_PATH GetModuleFileName(NULL, exe_path, 8192)
#else
#define UPDATETOOL "updatetool"
#define GET_EXE_PATH readlink("/proc/self/exe", exe_path, 8192);
#endif

#define GET_UPDATETOOL  \
    char exe_path[8192]; \
    GET_EXE_PATH;         \
    std::filesystem::path path = std::filesystem::path(exe_path).parent_path() / UPDATETOOL; \
    if (!std::filesystem::exists(path)) return;

void saturn_check_update() {
    GET_UPDATETOOL;
    check_update_thread = std::thread([path, exe_path]() {
        std::string cmd = "\"" + path.string() + "\" check \"" + exe_path + "\"";
#ifdef _WIN32
        DWORD exitcode;
        STARTUPINFOA si;
        PROCESS_INFORMATION pi;
        ZeroMemory(&si, sizeof(si));
        ZeroMemory(&pi, sizeof(pi));
        si.cb = sizeof(si);
        CreateProcess(nullptr, (LPSTR)cmd.c_str(), nullptr, nullptr, false, CREATE_NO_WINDOW | CREATE_NEW_PROCESS_GROUP, nullptr, nullptr, &si, &pi);
        WaitForSingleObject(pi.hProcess, INFINITE);
        GetExitCodeProcess(pi.hProcess, &exitcode);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
#else
        int exitcode = system((cmd + " &> /dev/null").c_str());
        exitcode = WEXITSTATUS(exitcode);
#endif
        if (exitcode == 6 /* CAN_UPDATE */) update_available = true;
    });
    check_update_thread.detach();
}

void saturn_do_update() {
    GET_UPDATETOOL;
    std::string cmd = "\"" + path.string() + "\" update \"" + exe_path + "\"";
#ifdef _WIN32
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    ZeroMemory(&pi, sizeof(pi));
    si.cb = sizeof(si);
    CreateProcess(nullptr, (LPSTR)cmd.c_str(), nullptr, nullptr, false, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
#else
    system((cmd + " &> /dev/null &").c_str());
#endif
}

void saturn_start_program_update() {
    atexit(saturn_do_update);
    exit(0);
}

void saturn_imgui_init() {
    saturn_load_themes();
    sdynos_imgui_init();
    smachinima_imgui_init();
    ssettings_imgui_init();
    
    saturn_load_project_list();

    ffmpeg_installed = does_command_exist("ffmpeg");
    clipboard_enabled = can_write_to_clipboard();

    saturn_check_update();
}

void saturn_imgui_handle_events(SDL_Event * event) {
    ImGui_ImplSDL2_ProcessEvent(event);
    switch (event->type){
        case SDL_KEYDOWN:
            if(event->key.keysym.sym == SDLK_F3) {
                saturn_play_keyframe();
            }
            if(event->key.keysym.sym == SDLK_F4) {
                limit_fps = !limit_fps;
                configWindow.fps_changed = true;
            }

            // this is for YOU, baihsense
            // here's your crash key
            //    - bup
            if(event->key.keysym.sym == SDLK_SCROLLLOCK) {
                imgui_update_theme();
            }
            /*if(event->key.keysym.sym == SDLK_F9) {
                DynOS_Gfx_GetPacks().Clear();
                DynOS_Opt_Init();
                //model_details = "" + std::to_string(DynOS_Gfx_GetPacks().Count()) + " model pack";
                //if (DynOS_Gfx_GetPacks().Count() != 1) model_details += "s";
                splash_finished = false;

                if (gCurrLevelNum > 3 || !mario_exists)
                    DynOS_ReturnToMainMenu();
            }*/

            if(event->key.keysym.sym == SDLK_F6) {
                k_popout_open = !k_popout_open;
            }
        
        break;
    }
    smachinima_imgui_controls(event);
}

extern s8 sObjectListUpdateOrder[];
void for_each_obj(std::function<void(struct Object*)> func) {
    for (int i = 0; i < OBJECT_POOL_CAPACITY; i++) {
        if (gObjectPool[i].activeFlags == ACTIVE_FLAG_DEACTIVATED) continue;
        func(gObjectPool + i);
    }
}

void saturn_keyframe_sort(std::vector<Keyframe>* keyframes) {
    for (int i = 0; i < keyframes->size(); i++) {
        for (int j = i + 1; j < keyframes->size(); j++) {
            if ((*keyframes)[i].position < (*keyframes)[j].position) continue;
            Keyframe temp = (*keyframes)[i];
            (*keyframes)[i] = (*keyframes)[j];
            (*keyframes)[j] = temp;
        }
    }
}

int startFrame = 0;
int endFrame = 0;
int endFrameText = 0;

void saturn_copy_keyframe(std::vector<Keyframe>* keyframes, int index) {
    int hoverKeyframeIndex = -1;
    for (int i = 0; i < keyframes->size(); i++) {
        if ((*keyframes)[i].position == k_current_frame) hoverKeyframeIndex = i;
    }
    Keyframe copy = Keyframe();
    copy.position = k_current_frame;
    copy.curve = (*keyframes)[index].curve;
    copy.value = (*keyframes)[index].value;
    copy.timelineID = (*keyframes)[index].timelineID;
    if (hoverKeyframeIndex == -1) keyframes->push_back(copy);
    else (*keyframes)[hoverKeyframeIndex] = copy;
}

void saturn_keyframe_window() {
    const char* windowLabel = "Timeline###kf_timeline";
    if (!saturn_imgui_window(windowLabel, ImGuiWindowFlags_NoScrollWithMouse)) return;
    if (ImGui::BeginPopupContextItem("Keyframe Menu Popup")) {
        k_context_popout_open = false;
        vector<Keyframe>* keyframes = &k_frame_keys[k_context_popout_keyframe.timelineID].second;
        int curve = -1;
        int index = -1;
        for (int i = 0; i < keyframes->size(); i++) {
            if ((*keyframes)[i].position == k_context_popout_keyframe.position) index = i;
        }
        bool isFirst = k_context_popout_keyframe.position == 0;
        if (isFirst) ImGui::BeginDisabled();
        bool doDelete = false;
        bool doCopy = false;
        if (ImGui::MenuItem("Delete")) {
            doDelete = true;
        }
        if (ImGui::MenuItem("Move to Pointer")) {
            doDelete = true;
            doCopy = true;
        }
        if (isFirst) ImGui::EndDisabled();
        if (ImGui::MenuItem("Copy to Pointer")) {
            doCopy = true;
        }
        ImGui::Separator();
        bool forceWait = k_frame_keys[k_context_popout_keyframe.timelineID].first.behavior != KFBEH_DEFAULT;
        if (forceWait) ImGui::BeginDisabled();
        for (int i = 0; i < IM_ARRAYSIZE(curveNames); i++) {
            if (ImGui::MenuItem(curveNames[i].c_str(), NULL, k_context_popout_keyframe.curve == InterpolationCurve(i))) {
                curve = i;
            }
        }
        if (forceWait) ImGui::EndDisabled();
        std::string timeline = k_context_popout_keyframe.timelineID;
        keyframes = &k_frame_keys[timeline].second;
        if (curve != -1) {
            (*keyframes)[index].curve = InterpolationCurve(curve);
            k_context_popout_open = false;
            k_previous_frame = -1;
        }
        if (doCopy) {
            saturn_copy_keyframe(keyframes, index);
            k_context_popout_open = false;
            k_previous_frame = -1;
        }
        if (doDelete) {
            keyframes->erase(keyframes->begin() + index);
            k_context_popout_open = false;
            k_previous_frame = -1;
        }
        saturn_keyframe_sort(keyframes);
        ImGui::EndPopup();
    }

    if (k_current_frame < 0) k_current_frame = 0;

    bool keyframe_prev_playing = keyframe_playing;
    if (keyframe_playing) { if (ImGui::Button(ICON_FK_STOP))          keyframe_playing = false; }
    else                  { if (ImGui::Button(ICON_FK_PLAY))          keyframe_playing = true;  }
                                ImGui::SameLine();
                            if (ImGui::Button(ICON_FK_FAST_BACKWARD)) k_current_frame = startFrame = 0;
                                ImGui::SameLine();
                                ImGui::Checkbox("Loop###k_t_loop", &k_loop);
                                ImGui::SameLine();
                                ImGui::PushItemWidth(48);
                                ImGui::InputInt("Frame", &k_current_frame, 0);
                                ImGui::PopItemWidth();
    if (!keyframe_playing && keyframe_prev_playing) {
        for (auto& entry : k_frame_keys) {
            saturn_keyframe_apply(entry.first, k_current_frame);
        }
    }

    ImVec2 window_size = ImGui::GetWindowSize();
            
    // Scrolling
    int scroll = keyframe_playing
        ? (k_current_frame - startFrame) >= (endFrame - startFrame) / 2 ?
            1 : 0
        : ImGui::IsWindowHovered() ?
            (int)(ImGui::GetIO().MouseWheel * -4) : 0;
    startFrame += scroll;
    if (startFrame < 0) startFrame = 0;
    endFrame = endFrameText = startFrame + window_size.x / 6;

    #define SEQUENCER { \
        ImGui::Dummy(ImVec2(0, 0)); \
        ImGui::SameLine(0.01); /* 0 doesnt work */ \
        if (ImGui::BeginNeoSequencer("Sequencer###k_sequencer", (uint32_t*)&k_current_frame, (uint32_t*)&startFrame, (uint32_t*)&endFrame, ImVec2(window_size.x, 1000), ImGuiNeoSequencerFlags_HideZoom)) { \
            for (auto& entry : k_frame_keys) { \
                if (entry.second.first.marioIndex != mario_index) continue; \
                std::string name = entry.second.first.name; \
                if (ImGui::BeginNeoTimeline(name.c_str(), entry.second.second)) ImGui::EndNeoTimeLine(); \
            } \
            ImGui::EndNeoSequencer(); \
        } \
    }

    int mario_index = -1;

    if (ImGui::BeginTabBar("###sequencers")) {
        if (ImGui::BeginTabItem("Environment")) {
            SEQUENCER;
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Mario", nullptr, ImGuiTabItemFlags_SetSelected * request_mario_tab)) {
            mario_index = mario_menu_index;
            if (!saturn_get_actor(mario_index)) mario_index = mario_menu_index = -1;
            if (mario_index == -1) ImGui::Text("No Mario is selected");
            else SEQUENCER;
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    request_mario_tab = false;

    // Keyframes
    if (!keyframe_playing) {
        for (auto& entry : k_frame_keys) {
            if (k_previous_frame == k_current_frame && !saturn_keyframe_matches(entry.first, k_current_frame)) saturn_place_keyframe(entry.first, k_current_frame);
            else saturn_keyframe_apply(entry.first, k_current_frame);
        }
    }

    if (k_context_popout_open) ImGui::OpenPopup("Keyframe Menu Popup");

    ImGui::End();

    // Auto focus (use controls without clicking window first)
    if (ImGui::IsWindowHovered(ImGuiHoveredFlags_None) && saturn_disable_sm64_input()) {
        ImGui::SetWindowFocus(windowLabel);
    }

    k_popout_focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_None);
    if (k_popout_focused) {
        for (auto& entry : k_frame_keys) {
            if (entry.second.first.name.find(", Main") != string::npos || entry.second.first.name.find(", Shade") != string::npos) {
                // Apply color keyframes
                //apply_cc_from_editor();
            }
        }
    }

    k_previous_frame = k_current_frame;
}

char saturnProjectFilename[257] = "Project";
int current_project_id;
char mario_search_prompt[256];

std::vector<std::string> embedded_models = {};
std::vector<std::string> embedded_anims = {};
std::vector<std::string> embedded_eyes = {};

int num_objects_as_actors = 0;

void saturn_imgui_update() {
    if (!splash_finished) return;

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame(window);
    ImGui::NewFrame();
    
    windowStartHeight = (showStatusBars) ? 48 : 30;

    camera_savestate_mult = 1.f;

   saturn_imgui_setup_dockspace();

    bool is_recording = saturn_actor_is_recording_input();
    if (is_recording) ImGui::BeginDisabled();
    if (saturn_imgui_window("Machinima")) {
        windowCcEditor = false;

        if (ImGui::BeginMenu("Open Project")) {
            saturn_file_browser_filter_extension("spj");
            saturn_file_browser_scan_directory("dynos/projects");
            if (saturn_file_browser_show("project")) {
                std::string str = saturn_file_browser_get_selected().string();
                str = str.substr(0, str.length() - 4); // trim the extension
                if (str.length() <= 256) memcpy(saturnProjectFilename, str.c_str(), str.length() + 1);
                saturnProjectFilename[256] = 0;
            }
            ImGui::PushItemWidth(125);
            ImGui::InputText(".spj###project_file_input", saturnProjectFilename, 256);
            ImGui::PopItemWidth();
            if (ImGui::Button(ICON_FA_FILE " Open###project_file_open")) {
                saturn_load_project((char*)(std::string(saturnProjectFilename) + ".spj").c_str());
            }
            ImGui::SameLine(70);
            bool in_custom_level = gCurrLevelNum == LEVEL_SA && gCurrAreaIndex == 3;
            if (in_custom_level) ImGui::BeginDisabled();
            if (ImGui::Button(ICON_FA_SAVE " Save###project_file_save")) {
                struct Folder   folder;
                struct Folder* pFolder = nullptr;
                if (!embedded_eyes.empty() || !embedded_models.empty() || !embedded_anims.empty()) {
                    folder.type = 1;
                    folder.entries = {};
                    strncpy(folder.name, "dynos", 255);
                    if (!embedded_models.empty()) {
                        struct Folder models_folder;
                        models_folder.type = 1;
                        models_folder.entries = {};
                        strncpy(models_folder.name, "packs", 255);
                        for (std::string model : embedded_models) {
                            models_folder.entries.push_back(saturn_embedded_filesystem_from_local_storage("dynos/packs/" + model));
                        }
                        folder.entries.push_back(*(struct FileEntry*)&models_folder);
                    }
                    if (!embedded_anims.empty()) {
                        struct Folder anims_folder;
                        anims_folder.type = 1;
                        anims_folder.entries = {};
                        strncpy(anims_folder.name, "anims", 255);
                        for (std::string anim : embedded_anims) {
                            std::filesystem::path path = std::filesystem::path("dynos/anims") / anim;
                            struct File animfile;
                            animfile.type = 0;
                            animfile.data_length = std::filesystem::file_size(path);
                            animfile.data = (unsigned char*)malloc(animfile.data_length);
                            strncpy(animfile.name, anim.c_str(), 255);
                            std::ifstream stream = std::ifstream(path);
                            stream.read((char*)animfile.data, animfile.data_length);
                            stream.close();
                            anims_folder.entries.push_back(*(struct FileEntry*)&animfile);
                        }
                        folder.entries.push_back(*(struct FileEntry*)&anims_folder);
                    }
                    if (!embedded_eyes.empty()) {
                        struct Folder eyes_folder;
                        eyes_folder.type = 1;
                        eyes_folder.entries = {};
                        strncpy(eyes_folder.name, "eyes", 255);
                        for (std::string eye : embedded_eyes) {
                            std::filesystem::path path = std::filesystem::path("dynos/eyes") / eye;
                            struct File eyefile;
                            eyefile.type = 0;
                            eyefile.data_length = std::filesystem::file_size(path);
                            eyefile.data = (unsigned char*)malloc(eyefile.data_length);
                            strncpy(eyefile.name, eye.c_str(), 255);
                            std::ifstream stream = std::ifstream(path);
                            stream.read((char*)eyefile.data, eyefile.data_length);
                            stream.close();
                            eyes_folder.entries.push_back(*(struct FileEntry*)&eyefile);
                        }
                        folder.entries.push_back(*(struct FileEntry*)&eyes_folder);
                    }
                    pFolder = &folder;
                }
                saturn_save_project((char*)(std::string(saturnProjectFilename) + ".spj").c_str(), pFolder);
                saturn_embedded_filesystem_free((struct FileEntry*)pFolder);
                saturn_load_project_list();
            }
            if (in_custom_level) ImGui::EndDisabled();
            ImGui::SameLine();
            imgui_bundled_help_marker("NOTE: Project files are currently EXPERIMENTAL and prone to crashing!");
            if (ImGui::TreeNode("Embed Assets")) {
                std::vector<std::string> available_models = {};
                std::vector<std::string> available_anims = {};
                std::vector<std::string> available_eyes = {};
                for (int i = 0; i < saturn_actor_sizeof(); i++) {
                    MarioActor* actor = saturn_get_actor(i);
                    bool has_custom_eyes = true;
                    if (actor->selected_model != -1) {
                        if (!actor->model.UsingVanillaEyes()) has_custom_eyes = false;
                        std::string model = actor->model.FolderName;
                        if (std::find(available_models.begin(), available_models.end(), model) == available_models.end())
                            available_models.push_back(model);
                    }
                    std::string atl = saturn_keyframe_get_mario_timeline_id("k_mario_anim", i);
                    if (actor->animstate.custom) {
                        std::string anim = canim_array[actor->animstate.id];
                        if (std::find(available_anims.begin(), available_anims.end(), anim) == available_anims.end())
                            available_anims.push_back(anim);
                    }
                    if (saturn_timeline_exists(atl.c_str())) {
                        auto keyframes = k_frame_keys[atl].second;
                        for (Keyframe kf : keyframes) {
                            if (kf.value[0] < 1) continue; // isnt custom
                            std::string anim = canim_array[kf.value[1]];
                            if (std::find(available_anims.begin(), available_anims.end(), anim) == available_anims.end())
                                available_anims.push_back(anim);
                        }
                    }
                    if (has_custom_eyes) {
                        std::string eye = actor->model.Expressions[0].Textures[actor->model.Expressions[0].CurrentIndex].DynosPath();
                        if (std::find(available_eyes.begin(), available_eyes.end(), eye) == available_eyes.end())
                            available_eyes.push_back(eye);
                        std::string etl = saturn_keyframe_get_mario_timeline_id("k_mario_expr", i);
                        if (saturn_timeline_exists(etl.c_str())) {
                            auto keyframes = k_frame_keys[etl].second;
                            for (Keyframe kf : keyframes) {
                                eye = actor->model.Expressions[0].Textures[kf.value[0]].DynosPath();
                                if (std::find(available_eyes.begin(), available_eyes.end(), eye) == available_eyes.end())
                                    available_eyes.push_back(eye);
                            }
                        }
                    }
                }
                if (ImGui::Button("Select All")) {
                    embedded_models.clear();
                    embedded_anims.clear();
                    embedded_eyes.clear();
                    for (std::string x : available_models) embedded_models.push_back(x);
                    for (std::string x : available_anims ) embedded_anims .push_back(x);
                    for (std::string x : available_eyes  ) embedded_eyes  .push_back(x);
                }
                ImGui::SameLine();
                if (ImGui::Button("Deselect All")) {
                    embedded_models.clear();
                    embedded_anims.clear();
                    embedded_eyes.clear();
                }
                if (ImGui::TreeNode("Models")) {
                    for (std::string model : available_models) {
                        bool selected = std::find(embedded_models.begin(), embedded_models.end(), model) != embedded_models.end();
                        if (ImGui::Checkbox(model.c_str(), &selected)) {
                            if (selected) embedded_models.push_back(model);
                            else embedded_models.erase(std::find(embedded_models.begin(), embedded_models.end(), model));
                        }
                    }
                    ImGui::TreePop();
                }
                if (ImGui::TreeNode("Animations")) {
                    for (std::string anim : available_anims) {
                        bool selected = std::find(embedded_anims.begin(), embedded_anims.end(), anim) != embedded_anims.end();
                        if (ImGui::Checkbox(anim.c_str(), &selected)) {
                            if (selected) embedded_anims.push_back(anim);
                            else embedded_anims.erase(std::find(embedded_anims.begin(), embedded_anims.end(), anim));
                        }
                    }
                    ImGui::TreePop();
                }
                if (ImGui::TreeNode("Eyes")) {
                    for (std::string eye : available_eyes) {
                        bool selected = std::find(embedded_eyes.begin(), embedded_eyes.end(), eye) != embedded_eyes.end();
                        if (ImGui::Checkbox(eye.c_str(), &selected)) {
                            if (selected) embedded_eyes.push_back(eye);
                            else embedded_eyes.erase(std::find(embedded_eyes.begin(), embedded_eyes.end(), eye));
                        }
                    }
                    ImGui::TreePop();
                }
                ImGui::TreePop();
            }
            if (in_custom_level) ImGui::Text("Saving in a custom\nlevel isn't supported");
            ImGui::EndMenu();
        }
        if (ImGui::MenuItem(ICON_FA_UNDO " Load Autosaved")) {
            saturn_load_project("autosave.spj");
        }

        ImGui::Separator();

        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 0.f, 0.f, 1.f));
        if (ImGui::Selectable(ICON_FK_TRASH " Delete all Marios")) {
            saturn_clear_actors();
        }
        ImGui::PopStyleColor();

        if (ImGui::CollapsingHeader("Camera")) {
            windowCcEditor = false;

            ImGui::Checkbox("Mount Camera", (bool*)&gIsCameraMounted);
            if (!gIsCameraMounted) if (ImGui::Button("Mount and Copy")) {
                gIsCameraMounted = true;
                vec3f_copy(freezecamPos, cameraPos);
                freezecamYaw = cameraYaw;
                freezecamPitch = cameraPitch;
            }

            if (camera_frozen) {
                saturn_keyframe_popout_next_line({ "k_c_camera_pos0", "k_c_camera_pos1", "k_c_camera_pos2", "k_c_camera_yaw", "k_c_camera_pitch", "k_c_camera_roll" });

                if (ImGui::BeginMenu("Options###camera_options")) {
                    camera_savestate_mult = 0.f;
                    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 5.0f);
                    ImGui::BeginChild("###model_metadata", ImVec2(250, 90), true, ImGuiWindowFlags_NoScrollbar);
                    float *pos, *yaw, *pitch;
                    if (gIsCameraMounted) {
                        pos = freezecamPos;
                        yaw = &freezecamYaw;
                        pitch = &freezecamPitch;
                    }
                    else {
                        pos = cameraPos;
                        yaw = &cameraYaw;
                        pitch = &cameraPitch;
                    }
                    float rot[] = { *yaw, *pitch, freezecamRoll };
                    ImGui::PushItemWidth(200);
                    ImGui::DragFloat3("pos", pos, 5.f);
                    ImGui::DragFloat3("rot", rot, 128.f);
                    ImGui::PopItemWidth();
                    *yaw = rot[0];
                    *pitch = rot[1];
                    freezecamRoll = rot[2];
                    if (ImGui::Button(ICON_FK_FILES_O " Copy###copy_camera")) {
                        saturn_copy_camera(copy_relative);
                        if (copy_relative) saturn_paste_camera();
                        has_copy_camera = 1;
                    } ImGui::SameLine();
                    if (!has_copy_camera) ImGui::BeginDisabled();
                    if (ImGui::Button(ICON_FK_CLIPBOARD " Paste###paste_camera")) {
                        if (has_copy_camera) saturn_paste_camera();
                    }
                    /*ImGui::Checkbox("Loop###camera_paste_forever", &paste_forever);
                    if (paste_forever) {
                        saturn_paste_camera();
                    }*/
                    if (!has_copy_camera) ImGui::EndDisabled();

                    ImGui::EndChild();
                    ImGui::PopStyleVar();

                    ImGui::Text(ICON_FK_VIDEO_CAMERA " Speed");
                    ImGui::PushItemWidth(150);
                    ImGui::SliderFloat("Move", &camVelSpeed, 0.0f, 2.0f);
                    if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Right)) { camVelSpeed = 1.f; }
                    ImGui::SliderFloat("Rotate", &camVelRSpeed, 0.0f, 2.0f);
                    if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Right)) { camVelRSpeed = 1.f; }
                    ImGui::PopItemWidth();
                    ImGui::EndMenu();
                }
            }
            ImGui::Separator();
            ImGui::PushItemWidth(100);
            ImGui::SliderFloat("FOV", &camera_fov, 0.0f, 100.0f);
            if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Right)) { camera_fov = 50.f; }
            imgui_bundled_tooltip("Controls the FOV of the in-game camera; Default is 50, or 40 in Project64.");
            saturn_keyframe_popout("k_fov");
            ImGui::SameLine(200); ImGui::TextDisabled("N/M");
            ImGui::PopItemWidth();
            ImGui::Checkbox("Smooth###fov_smooth", &camera_fov_smooth);

            ImGui::Separator();
            ImGui::PushItemWidth(100);
            ImGui::SliderFloat("Follow", &camera_focus, 0.0f, 1.0f);
            if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Right)) { camera_focus = 1.f; }
            saturn_keyframe_popout("k_focus");
            ImGui::PopItemWidth();
        }

        /*if (ImGui::CollapsingHeader("Appearance")) {
            sdynos_imgui_menu();
        }*/

        if (ImGui::CollapsingHeader("Options")) {
            imgui_machinima_quick_options();
        }

        if (ImGui::CollapsingHeader("Autochroma")) {
            if (gCurrLevelNum != LEVEL_SA) {
                if (ImGui::Checkbox("Enabled", &autoChroma)) schroma_imgui_init();
                ImGui::Separator();
            }
            if (!autoChroma && gCurrLevelNum != LEVEL_SA) ImGui::BeginDisabled();
            schroma_imgui_update();
            if (!autoChroma && gCurrLevelNum != LEVEL_SA) ImGui::EndDisabled();
        }
        
        if (ImGui::CollapsingHeader("Rendering")) {
            if (!ffmpeg_installed) {
                ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 5.f);
                if (ImGui::BeginChild("###no_ffmpeg", ImVec2(0, 108), true, ImGuiWindowFlags_NoScrollbar)) {
                    ImGui::Text("FFmpeg isn't installed, so most");
                    ImGui::Text("video formats aren't supported.");
                    ImGui::Separator();
                    
#ifdef _WIN32
                    if (ffmpeg_download_in_progress) {
                        ImGui::Text("Downloading...");
                        ImGui::ProgressBar(ffmpeg_download_progress);
                    }
                    else if (ffmpeg_install_in_progress) ImGui::Text("Installing...");
                    else if (ffmpeg_install_restart) ImGui::Text("Pending Restart");
                    else if (getenv("MSYSTEM")) { // running in MINGW
                        ImGui::Text("To install it, run this command");
                        ImGui::Text("in your MINGW shell:");
                        ImGui::InputText("###cmd_copy_paste", "pacman -S mingw-w64-x86_64-ffmpeg", 34, ImGuiInputTextFlags_ReadOnly);
                    }
                    else {
                        if (ImGui::Button("Install")) saturn_download_ffmpeg(saturn_install_ffmpeg);
                        if (ffmpeg_download_failed) {
                            ImGui::SameLine();
                            ImGui::Text("Downlad failed!");
                        }
                    }
#else
                    ImGui::Text("You can install FFmpeg through");
                    ImGui::Text("your distro's package manager.");
#endif
                    ImGui::EndChild();
                }
                ImGui::PopStyleVar();
            }
            if (ImGui::BeginCombo("###res_preset", "Resolution Preset")) {
                if (ImGui::Selectable("240p 4:3 (N64)")) { videores[0] =  320; videores[1] =  240; }
                if (ImGui::Selectable("360p 4:3"))       { videores[0] =  480; videores[1] =  360; }
                if (ImGui::Selectable("360p 16:9"))      { videores[0] =  640; videores[1] =  360; }
                if (ImGui::Selectable("480p 4:3"))       { videores[0] =  640; videores[1] =  480; }
                if (ImGui::Selectable("480p 16:9"))      { videores[0] =  854; videores[1] =  480; }
                if (ImGui::Selectable("720p 16:9"))      { videores[0] = 1280; videores[1] =  720; }
                if (ImGui::Selectable("1080p 16:9"))     { videores[0] = 1920; videores[1] = 1080; }
                if (ImGui::Selectable("4K 16:9"))        { videores[0] = 3840; videores[1] = 2160; }
                if (ImGui::Selectable("8K 16:9"))        { videores[0] = 7680; videores[1] = 4320; }
                ImGui::EndCombo();
            }
            ImGui::InputInt2("Resolution", videores);
            ImGui::Checkbox("Preview Aspect Ratio", &keep_aspect_ratio);
            ImGui::Checkbox("Transparency", &checkbox_transparency_enabled);
            imgui_bundled_tooltip("Unsupported for MP4s");
            ImGui::Checkbox("60 FPS", &checkbox_sixty_fps_enabled);
            imgui_bundled_tooltip("Unsupported for GIFs");
            int curr_projection = request_ortho_mode == 0 ? orthographic_mode : request_ortho_mode - 1;
            if (ImGui::Combo("Projection", &curr_projection,
                "Perspective\0"
                "Orthographic\0"
            )) orthographic_mode = curr_projection;
            if (orthographic_mode) {
                if (ImGui::BeginTable("###ortho_table", 2)) {
                    #define ORTHO_SETTING(label, variable, speed, kfid, popup) \
                        ImGui::TableNextRow(); \
                        ImGui::TableSetColumnIndex(0); \
                        ImGui::DragFloat(label, &ortho_settings.variable, speed); \
                        if (*popup) ImGui::OpenPopupOnItemClick(popup); \
                        ImGui::TableSetColumnIndex(1); \
                        saturn_keyframe_popout(kfid);
                    if (ImGui::BeginPopup("###ortho_yaw_presets")) {
                        if (ImGui::Selectable("45" )) ortho_settings.rotation_y = 45 ;
                        if (ImGui::Selectable("135")) ortho_settings.rotation_y = 135;
                        if (ImGui::Selectable("225")) ortho_settings.rotation_y = 225;
                        if (ImGui::Selectable("315")) ortho_settings.rotation_y = 315;
                        ImGui::EndPopup();
                    }
                    ORTHO_SETTING("Scale", scale, 0.02f, "k_orthoscale", "");
                    ORTHO_SETTING("Yaw", rotation_y, 1.0f, "k_orthoyaw", "###ortho_yaw_presets");
                    ORTHO_SETTING("Pitch", rotation_x, 1.0f, "k_orthopitch", "");
                    ORTHO_SETTING("Offset X", offset_x, 2.5f * ortho_settings.scale, "k_orthox", "");
                    ORTHO_SETTING("Offset Y", offset_y, 2.5f * ortho_settings.scale, "k_orthoy", "");
                    ImGui::EndTable();
                }
            }
            static bool save_to_clipboard = false;
            std::vector<std::string> video_formats = video_renderer_get_formats(ffmpeg_installed);
            ImGui::Separator();
            if (!clipboard_enabled) {
                if (is_wayland()) ImGui::Text("Install the wl-clipboard package");
                else ImGui::Text("Install the xclip package");
                ImGui::Text("to use the clipboard");
            }
            if (ImGui::Button("Capture Screenshot")) {
                bool do_capture = false;
                std::string dest = "";
                transparency_enabled = checkbox_transparency_enabled;
                if (save_to_clipboard) do_capture = true;
                else {
                    dest = save_file_dialog("Save Screenshot", { "PNG image", "*.png" });
                    do_capture = !dest.empty();
                }
                if (do_capture) saturn_imgui_capture_next_frame(dest);
            }
            ImGui::SameLine();
            ImGui::BeginDisabled(!clipboard_enabled);
            ImGui::Checkbox("Clipboard", &save_to_clipboard);
            ImGui::EndDisabled();
            ImGui::BeginDisabled(k_frame_keys.empty());
            if (ImGui::Button("Render Video")) {
                capture_destination_file = save_file_dialog("Save Video", video_formats);
                if (!capture_destination_file.empty()) {
                    saturn_set_video_destination(capture_destination_file);
                    transparency_enabled = checkbox_transparency_enabled;
                    sixty_fps_enabled = checkbox_sixty_fps_enabled;
                    if (!(video_renderer_flags & VIDEO_RENDERER_FLAGS_60FPS)) sixty_fps_enabled = false;
                    if (!(video_renderer_flags & VIDEO_RENDERER_FLAGS_TRANSPARECY)) transparency_enabled = false;
                    capturing_video = true;
                    keyframe_playing = false;
                    renderer_current_frame = -3;
                    renderer_num_frames = saturn_keyframe_get_length() * (sixty_fps_enabled ? 2 : 1);
                    video_renderer_init(videores[0], videores[1], sixty_fps_enabled);
                }
            }
            ImGui::EndDisabled();
        }
        ImGui::End();
    }

    if (saturn_imgui_window("Marios")) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 0.f, 0.f, 1.f));
        if (ImGui::Selectable(ICON_FK_TRASH " Delete all Marios")) {
            saturn_clear_actors();
        }
        ImGui::PopStyleColor();
        if (ImGui::Selectable(ICON_FK_EYE " Show all Marios")) {
            MarioActor* actor = gMarioActorList;
            while (actor) {
                actor->hidden = false;
                actor = actor->next;
            }
        }
        if (ImGui::Selectable(ICON_FK_EYE_SLASH " Hide all Marios")) {
            MarioActor* actor = gMarioActorList;
            while (actor) {
                actor->hidden = true;
                actor = actor->next;
            }
        }
        if (ImGui::Selectable(ICON_FK_EYEDROPPER " Pick Nearest")) {
            float nearest_dist = 0x10000;
            int nearest_index = -1;
            int i = 0;
            MarioActor* actor = gMarioActorList;
            while (actor) {
                if (!actor->exists) {
                    actor = actor->next;
                    i++;
                    continue;
                }
                float dist = sqrtf(
                    (actor->x - gCamera->pos[0]) * (actor->x - gCamera->pos[0]) +
                    (actor->y - gCamera->pos[1]) * (actor->y - gCamera->pos[1]) +
                    (actor->z - gCamera->pos[2]) * (actor->z - gCamera->pos[2])
                );
                if (nearest_dist > dist) {
                    nearest_dist = dist;
                    nearest_index = i;
                }
                actor = actor->next;
                i++;
            }
            if (nearest_index != -1) saturn_imgui_open_mario_menu(nearest_index);
        }
        if (ImGui::BeginMenu("Mario Struct")) {
            extern bool mstruct_hidden;
            ImGui::PushItemWidth(50);
            ImGui::DragFloat("###mariostruct_x", gMarioState->pos + 0, 1.f, 0.f, 0.f, "X");
            ImGui::SameLine();
            ImGui::DragFloat("###mariostruct_y", gMarioState->pos + 1, 1.f, 0.f, 0.f, "Y");
            ImGui::SameLine();
            ImGui::DragFloat("###mariostruct_z", gMarioState->pos + 2, 1.f, 0.f, 0.f, "Z");
            ImGui::PopItemWidth();
            saturn_keyframe_popout({ "k_mariostruct_x", "k_mariostruct_y", "k_mariostruct_z" });
            ImGui::PushItemWidth(100);
            ImGui::DragFloat("Angle###mariostruct_angle", &gMarioState->fAngle, 128.f);
            ImGui::PopItemWidth();
            saturn_keyframe_popout("k_mariostruct_angle");
            ImGui::Checkbox("Hidden", &mstruct_hidden);
            ImGui::Separator();
            if (ImGui::MenuItem("Set Position and Angle")) {
                setting_mario_struct_pos = true;
            }
            ImGui::EndMenu();
        }
        imgui_bundled_tooltip("This Mario is used for calculations in enemy\nbehaviors and is not visible in renders.");
        ImGui::PushItemWidth(200);
        if (ImGui::BeginCombo("Model", saturn_object_names[current_mario_model].c_str())) {
            for (auto model_entry : saturn_iterable_obj_list) {
                bool selected = model_entry == current_mario_model;
                if (ImGui::Selectable(saturn_object_names[model_entry].c_str(), selected)) current_mario_model = (ModelID)model_entry;
            }
            ImGui::EndCombo();
        }
        ImGui::PopItemWidth();
        ImGui::Separator();
        ImGui::InputTextWithHint("###mariosearch", ICON_FK_SEARCH " Search...", mario_search_prompt, 256);
        MarioActor* actor = gMarioActorList;
        int i = 0;
        while (actor) {
            if (actor->exists && case_insensitive_contains(actor->name, mario_search_prompt)) {
                std::string name = actor->name;
                bool unnamed = name.empty();
                if (unnamed) {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
                    name = "[Unnamed]";
                }
                if (ImGui::Selectable(name.c_str())) {
                    saturn_imgui_open_mario_menu(i);
                }
                if (unnamed) ImGui::PopStyleColor();
            }
            actor = actor->next;
            i++;
        }
        ImGui::End();
    }

    if (saturn_imgui_window("Objects")) {
        if (world_simulation_data) {
            ImGui::Text("A simulation is active");
            ImGui::Separator();
        }
        ImGui::BeginDisabled(world_simulation_data);
        if (ImGui::BeginCombo("###despawn_category_chooser", obj_categories[sel_category].first)) {
            for (int i = 0; i < obj_categories.size(); i++) {
                bool selected = sel_category == i;
                if (ImGui::Selectable(obj_categories[i].first, selected)) {
                    sel_category = i;
                }
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        if (ImGui::Button("Despawn")) {
            for (const BehaviorScript* bhv : obj_categories[sel_category].second) {
                for_each_obj([&](struct Object* obj) {
                    if (obj->behavior != bhv) return;
                    obj->header.gfx.node.flags |= GRAPH_RENDER_INVISIBLE;
                    obj_mark_for_deletion(obj);
                });
            }
        }
        ImGui::Separator();
        bool any_objects = false;
        int iter = 0;
        for_each_obj([&](struct Object* obj) {
            if (obj->header.gfx.node.flags & GRAPH_RENDER_INVISIBLE) return;
            for (int modelID : saturn_iterable_obj_list) {
                if (obj->header.gfx.sharedChild != gLoadedGraphNodes[modelID]) continue;
                if (ImGui::Selectable((saturn_object_names[modelID] + "###model_id_" + std::to_string(iter++)).c_str())) {
                    enum ModelID prev_mario_model = current_mario_model;
                    current_mario_model = (enum ModelID)modelID;
                    MarioActor* actor = saturn_spawn_actor(obj->oPosX, obj->oPosY, obj->oPosZ);
                    current_mario_model = prev_mario_model;
                    actor->angle = obj->oFaceAngleYaw;
                    actor->obj_model = (enum ModelID)modelID;
                    std::string name = saturn_object_names[modelID] + " Object " + std::to_string(++num_objects_as_actors);
                    memcpy(actor->name, name.c_str(), name.length() + 1);
                    struct Object* currobj = gCurrentObject;
                    gCurrentObject = obj;
                    cur_obj_hide();
                    gCurrentObject = currobj;
                }
                if (ImGui::IsItemHovered()) obj->oFlags |= OBJ_FLAG_IS_SELECTED;
                else obj->oFlags &= ~OBJ_FLAG_IS_SELECTED;
                any_objects = true;
                break;
            }
        });
        if (!any_objects) ImGui::Text("There aren't any objects\nyou can create actors from\nin this level.");
        ImGui::EndDisabled();
        ImGui::End();
    }

    if (saturn_imgui_window("Settings")) {
        ssettings_imgui_update();
        ImGui::End();
    }

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    if (saturn_imgui_window("Game")) {
        ImVec2 window_pos = ImGui::GetWindowPos();
        ImVec2 window_size = ImGui::GetWindowSize();
        window_pos.y += 20;
        window_size.y -= 20;
        game_viewport[0] = window_pos.x;
        game_viewport[1] = window_pos.y;
        game_viewport[2] = window_size.x;
        game_viewport[3] = window_size.y;
        if (ImGui::IsWindowHovered()) mouse_state.scrollwheel += ImGui::GetIO().MouseWheel;
        if (is_recording) ImGui::EndDisabled();
        float image_bounds[4];
        saturn_get_game_bounds(image_bounds, window_size);
        ImVec2 cursor_pos = ImGui::GetCursorPos();
        ImGui::SetCursorPos(ImVec2(image_bounds[0] + cursor_pos.x, image_bounds[1] + cursor_pos.y));
        ImGui::Image(
            framebuffer, ImVec2(image_bounds[2], image_bounds[3]),
            ImVec2(0.0f, 0.0f),
            ImVec2(1.0f, 1.0f)
        );
        if (is_recording) ImGui::BeginDisabled();
        ImGui::PopStyleVar();
        if (ImGui::BeginPopup("Mario Menu")) {
            mario_menu_do_open = false;
            MarioActor* actor = saturn_get_actor(mario_menu_index);
            ImGui::InputText("###marioname", actor->name, 256);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 0.f, 0.f, 1.f));
            if (ImGui::MenuItem(ICON_FK_TRASH_O " Remove")) {
                saturn_remove_actor(mario_menu_index);
            }
            ImGui::PopStyleColor();
            if (gIsCameraMounted) ImGui::BeginDisabled(true);
            if (ImGui::MenuItem(ICON_FK_EYE " Look at")) {
                Vec3f mpos;
                float dist;
                s16 pitch, yaw;
                vec3f_set(mpos, actor->x, actor->y + 100, actor->z);
                vec3f_set_dist_and_angle(mpos, cameraPos, 200, 0, actor->angle);
                vec3f_get_dist_and_angle(cameraPos, mpos, &dist, &pitch, &yaw);
                cameraPitch = pitch;
                cameraYaw = yaw;
            }
            if (gIsCameraMounted) ImGui::EndDisabled();
            sdynos_imgui_menu(mario_menu_index);
            ImGui::EndPopup();
        }
        if (mario_menu_do_open) ImGui::OpenPopup("Mario Menu");
        if (!ImGui::IsWindowHovered()) game_focus_timer = 0;
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::End();
    }
    ImGui::PopStyleVar();

    saturn_keyframe_window();

    if (update_available && !capturing_video) {
        ImGui::Begin("Update Available", &update_available, ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
        ImGui::Text("A new Saturn Studio version is released");
        ImGui::Separator();
        if (ImGui::Button("Update")) saturn_start_program_update();
        ImGui::SameLine();
        if (ImGui::Button("Not Now")) update_available = false;
        ImGui::End();
    }

    ImGuiViewportP* viewport = (ImGuiViewportP*)(void*)ImGui::GetMainViewport();
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_MenuBar;
    float height = ImGui::GetFrameHeight();

    ImVec2 menu_bar_size;
    if (showStatusBars) {
        if (ImGui::BeginViewportSideBar("##SecondaryMenuBar", viewport, ImGuiDir_Up, height, window_flags)) {
            if (ImGui::BeginMenuBar()) {
                menu_bar_size = ImGui::GetWindowSize();
                if (is_recording) ImGui::EndDisabled();
                if (ImGui::BeginMenu("x")) {
                    for (auto& window : visible_windows) {
                        if (ImGui::MenuItem(window.first.c_str(), NULL, window.second)) {
                            window.second ^= 1;
                            saturn_save_window_visibility("windows.bin", &visible_windows);
                        }
                    }
                    ImGui::EndMenu();
                }
                ImGui::Text(PLATFORM_ICON " " SATURN_VERSION "     ");
                if (configFps60) ImGui::TextDisabled("%.1f FPS (%.3f ms/frame)", ImGui::GetIO().Framerate, 1000.0f / ImGui::GetIO().Framerate);
                else ImGui::TextDisabled("%.1f FPS (%.3f ms/frame)", ImGui::GetIO().Framerate / 2, 1000.0f / (ImGui::GetIO().Framerate / 2));
                ImGui::Text("     ");
#ifdef GIT_BRANCH
#ifdef GIT_HASH
                ImGui::TextDisabled(ICON_FK_GITHUB " " GIT_BRANCH " " GIT_HASH "     ");
#endif
#endif
                if (is_recording) {
                    if (ImGui::BeginMenu("Input Recording")) {
                        ImGui::PushItemWidth(150);
                        ImGui::SliderFloat(ICON_FK_SKATE " Walkpoint###run_speed", &run_speed, 0.f, 127.f, "%.0f");
                        imgui_bundled_tooltip("Controls the axis range for Mario's running input; Can be used to force walking (36) or tiptoe (25) animations.");
                        ImGui::PopItemWidth();
                        if (ImGui::Button("Stop")) saturn_actor_stop_recording();
                        ImGui::EndMenu();
                    }
                    ImGui::SameLine(ImGui::GetWindowWidth() - 126);
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.00f, 0.00f, 0.00f, 1.00f));
                    ImGui::Text(ICON_FK_CIRCLE);
                    ImGui::PopStyleColor();
                    ImGui::SameLine();
                    ImGui::Text("Recording...");
                }
                else if (gCurrLevelNum == LEVEL_SA && gCurrAreaIndex == 3) {
                    ImGui::SameLine(ImGui::GetWindowWidth() - 280);
                    ImGui::Text("Saving in custom level isn't supported");
                }
                else {
                    ImGui::SameLine(ImGui::GetWindowWidth() - 140);
                    ImGui::Text("Autosaving in %ds", autosaveDelay / 30);
                }
                if (is_recording) ImGui::BeginDisabled();
                ImGui::EndMenuBar();
            }
            ImGui::End();
        }
    }

    if (capturing_video) {
        ImVec2 viewport_size = ImGui::GetMainViewport()->Size;
        ImVec2 window_size = ImVec2(viewport_size.x, viewport_size.y - menu_bar_size.y);
        ImGuiWindowFlags base_flags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize;
        ImGui::SetNextWindowPos(ImVec2(0, menu_bar_size.y), ImGuiCond_Always);
        ImGui::SetNextWindowSize(window_size, ImGuiCond_Always);
        ImGui::Begin("Preview", nullptr, base_flags | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMouseInputs);
        float image_bounds[4];
        saturn_get_game_bounds(image_bounds, window_size);
        ImGui::SetCursorPos(ImVec2(image_bounds[0], image_bounds[1]));
        ImGui::Image(
            framebuffer, ImVec2(image_bounds[2], image_bounds[3]),
            ImVec2(0.0f, 0.0f),
            ImVec2(1.0f, 1.0f)
        );
        ImGui::End();
        if (renderer_num_frames > 0) {
            int curr_frame = renderer_current_frame < 0 ? 0 : renderer_current_frame;
            ImGui::SetNextWindowPos(ImVec2(8, menu_bar_size.y + 8), ImGuiCond_Always);
            ImGui::SetNextWindowSizeConstraints(ImVec2(250, 0), ImVec2(FLT_MAX, FLT_MAX));
            ImGui::Begin("Rendering...", nullptr, base_flags);
            ImGui::ProgressBar(curr_frame / (float)renderer_num_frames);
            if (ImGui::Button("Cancel")) saturn_imgui_stop_capture();
            ImGui::SameLine();
            ImGui::Text("%d/%d", curr_frame, renderer_num_frames);
            ImGui::End();
        }   
    }

    /*if (windowStats) {
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));
        ImGuiWindowFlags stats_flags = imgui_bundled_window_corner(2, 0, 0, 0.64f);
        ImGui::Begin("Stats", &windowStats, stats_flags);

#ifdef DISCORDRPC
        if (has_discord_init && gCurUser.username != "") {
            if (gCurUser.username == NULL || gCurUser.username == "") {
                ImGui::Text(ICON_FK_DISCORD " Loading...");
            } else {
                std::string disc = gCurUser.discriminator;
                if (disc == "0") ImGui::Text(ICON_FK_DISCORD " @%s", gCurUser.username);
                else ImGui::Text(ICON_FK_DISCORD " %s#%s", gCurUser.username, gCurUser.discriminator);
            }
            ImGui::Separator();
        }
#endif
        ImGui::Text(ICON_FK_FOLDER_OPEN " %i model pack(s)", model_list.size());
        ImGui::Text(ICON_FK_FILE_TEXT " %i color code(s)", color_code_list.size());
        ImGui::TextDisabled(ICON_FK_PICTURE_O " %i textures loaded", preloaded_textures_count);

        ImGui::End();
        ImGui::PopStyleColor();
    }
    if (windowCcEditor && support_color_codes && current_model.ColorCodeSupport) {
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));
        ImGui::Begin("Color Code Editor", &windowCcEditor);
        OpenCCEditor();
        ImGui::End();
        ImGui::PopStyleColor();

#ifdef DISCORDRPC
        discord_state = "In-Game // Editing a CC";
#endif
    }
    if (windowAnimPlayer && mario_exists) {
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));
        ImGui::Begin("Animation Mixtape", &windowAnimPlayer);
        imgui_machinima_animation_player();
        ImGui::End();
        ImGui::PopStyleColor();
    }*/

    //ImGui::ShowDemoWindow();
    if (is_recording) ImGui::EndDisabled();

    is_cc_editing = windowCcEditor & support_color_codes & current_model.ColorCodeSupport;

    ImGui::Render();
    GLint last_program;
    glGetIntegerv(GL_CURRENT_PROGRAM, &last_program);
    glUseProgram(0);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    glUseProgram(last_program);

    /*if (was_camera_frozen && !camera_frozen && saturn_timeline_exists("k_c_camera_pos0")) {
        k_frame_keys.erase("k_c_camera_pos0");
        k_frame_keys.erase("k_c_camera_pos1");
        k_frame_keys.erase("k_c_camera_pos2");
        k_frame_keys.erase("k_c_camera_yaw");
        k_frame_keys.erase("k_c_camera_pitch");
        k_frame_keys.erase("k_c_camera_roll");
    }*/
    was_camera_frozen = camera_frozen;

    //if (!is_gameshark_open) apply_cc_from_editor();
}

/*
    ======== SATURN ========
    Advanced Keyframe Engine
    ========================
*/

std::string saturn_keyframe_get_mario_timeline_id(std::string base, int mario) {
    if (mario == -1) return base;
    char data[base.size() + 8];
    sprintf(data, "%s%c%07d", base.c_str(), '_', mario);
    return data;
}

void saturn_keyframe_context_popout(Keyframe keyframe) {
    if (keyframe_playing || k_context_popout_open) return;
    k_context_popout_open = true;
    k_context_popout_pos = ImGui::GetMousePos();
    k_context_popout_keyframe = keyframe;
}

void saturn_keyframe_left_click_modif(Keyframe keyframe) {
    if (keyframe_playing || k_context_popout_open) return;
    const Uint8* kb = SDL_GetKeyboardState(NULL);
    std::vector<Keyframe>* keyframes = &k_frame_keys[keyframe.timelineID].second;
    int index = -1;
    for (int i = 0; i < keyframes->size(); i++) {
        if ((*keyframes)[i].position == keyframe.position) index = i;
    }
    if (kb[SDL_SCANCODE_LSHIFT]) saturn_copy_keyframe(keyframes, index);
    if (kb[SDL_SCANCODE_LCTRL] && keyframe.position != 0) keyframes->erase(keyframes->begin() + index);
    saturn_keyframe_sort(keyframes);
}

void saturn_keyframe_show_kf_content(Keyframe keyframe) {
    ImGui::BeginTooltip();
    KeyframeTimeline timeline = k_frame_keys[keyframe.timelineID].first;
    if (timeline.type == KFTYPE_BOOL) {
        bool checked = keyframe.value[0] >= 1;
        ImGui::Checkbox("###kf_content_checkbox", &checked);
    }
    if (timeline.type == KFTYPE_FLOAT) {
        char buf[64];
        std::string fmt = timeline.precision >= 0 ? "%d" : ("%." + std::to_string(-timeline.precision) + "f");
        snprintf(buf, 64, fmt.c_str(), keyframe.value[0]);
        buf[63] = 0;
        ImGui::Text(buf);
    }
    if (timeline.type == KFTYPE_ANIM) {
        std::string anim_name;
        bool anim_custom = keyframe.value[0] >= 1;
        int anim_id = keyframe.value[1];
        if (anim_custom) anim_name = canim_array[anim_id];
        else anim_name = saturn_animation_names[anim_id];
        ImGui::Text(anim_name.c_str());
    }
    if (timeline.type == KFTYPE_EXPRESSION) {
        Model* model = (Model*)saturn_keyframe_get_timeline_ptr(timeline);
        for (int i = 0; i < keyframe.value.size(); i++) {
            if (model->Expressions[i].Textures.size() == 2 && (
                model->Expressions[i].Textures[0].FileName.find("default") != std::string::npos ||
                model->Expressions[i].Textures[1].FileName.find("default") != std::string::npos
            )) {
                int   select_index = (model->Expressions[i].Textures[0].FileName.find("default") != std::string::npos) ? 0 : 1;
                int deselect_index = (model->Expressions[i].Textures[0].FileName.find("default") != std::string::npos) ? 1 : 0;
                bool is_selected = (model->Expressions[i].CurrentIndex == select_index);
                ImGui::Checkbox((model->Expressions[i].Name + "###kf_content_expr").c_str(), &is_selected);
            }
            else {
                ImGui::Text((model->Expressions[i].Textures[model->Expressions[i].CurrentIndex].FileName + " - " + model->Expressions[i].Name).c_str());
            }
        }
    }
    if (timeline.type == KFTYPE_COLOR) {
        ImVec4 colorm;
        colorm.x = keyframe.value[0] / 255.f;
        colorm.y = keyframe.value[2] / 255.f;
        colorm.z = keyframe.value[4] / 255.f;
        colorm.w = 1;
        ImVec4 colors;
        colors.x = keyframe.value[1] / 255.f;
        colors.y = keyframe.value[3] / 255.f;
        colors.z = keyframe.value[5] / 255.f;
        colors.w = 1;
        ImGui::ColorEdit4("###kfprev_maincol",  (float*)&colorm, ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_InputRGB | ImGuiColorEditFlags_Uint8 | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_NoOptions);
        ImGui::ColorEdit4("###kfprev_shadecol", (float*)&colors, ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_InputRGB | ImGuiColorEditFlags_Uint8 | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_NoOptions);
    }
    if (timeline.type == KFTYPE_COLORF) {
        ImVec4 color;
        color.x = keyframe.value[0];
        color.y = keyframe.value[1];
        color.z = keyframe.value[2];
        color.w = 1;
        ImGui::ColorEdit4("###kfprev_color", (float*)&color, ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_InputRGB | ImGuiColorEditFlags_Uint8 | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_NoOptions);
    }
    if (timeline.type == KFTYPE_SWITCH) {
        std::string id = keyframe.timelineID;
        if (timeline.marioIndex != -1) id = id.substr(0, id.length() - 8);
        if (id == "k_anim_state") {
            MarioActor* actor = saturn_get_actor(timeline.marioIndex);
            ImGui::Text(saturn_obj_switches[actor->obj_model][(int)keyframe.value[0]].c_str());
        }
        else if (kf_switch_names.find(id) == kf_switch_names.end()) ImGui::Text("u forgor");
        else ImGui::Text(kf_switch_names[id][(int)keyframe.value[0]].c_str());
    }
    ImVec2 window_pos = ImGui::GetMousePos();
    ImVec2 window_size = ImGui::GetWindowSize();
    window_pos.x -= window_size.x - 4;
    window_pos.y += 4;
    ImGui::SetWindowPos(window_pos);
    ImGui::EndTooltip();
}

void saturn_keyframe_popout(std::string id) {
    ImGui::SameLine();
    saturn_keyframe_popout_next_line(id);
}

void saturn_keyframe_popout_next_line(std::string id) {
    saturn_keyframe_popout_next_line(std::vector<std::string>{ id });
}

void saturn_keyframe_popout(std::vector<std::string> ids) {
    ImGui::SameLine();
    saturn_keyframe_popout_next_line(ids);
}

void saturn_keyframe_popout_next_line(std::vector<std::string> ids) {
    if (ids.size() == 0) return;
    if (timelineDataTable.find(ids[0]) == timelineDataTable.end()) return;
    auto timeline_metadata = timelineDataTable[ids[0]];
    bool contains = saturn_timeline_exists(saturn_keyframe_get_mario_timeline_id(ids[0], get<6>(timeline_metadata) ? mario_menu_index : -1).c_str());
    std::string button_label = std::string(contains ? ICON_FK_TRASH : ICON_FK_LINK) + "###kb_" + ids[0];
    bool event_button = false;
    if (ImGui::Button(button_label.c_str())) {
        k_popout_open = true;
        for (std::string id : ids) {
            if (contains) {
                timeline_metadata = timelineDataTable[id];
                int index = get<6>(timeline_metadata) ? mario_menu_index : -1;
                k_frame_keys.erase(saturn_keyframe_get_mario_timeline_id(id, index));
            }
            else {
                auto [ptr, type, behavior, name, precision, num_values, is_mario] = timelineDataTable[id];
                KeyframeTimeline timeline = KeyframeTimeline();
                timeline.dest = ptr;
                timeline.type = type;
                timeline.behavior = behavior;
                timeline.name = name;
                timeline.precision = precision;
                timeline.numValues = num_values;
                timeline.marioIndex = is_mario ? mario_menu_index : -1;
                k_current_frame = 0;
                startFrame = 0;
                std::string timeline_id = saturn_keyframe_get_mario_timeline_id(id, is_mario ? mario_menu_index : -1);
                k_frame_keys.insert({ timeline_id, { timeline, {} } });
                saturn_create_keyframe(timeline_id, behavior == KFBEH_FORCE_WAIT ? InterpolationCurve::WAIT : InterpolationCurve::LINEAR);
            }
        }
    }
    for (std::string id : ids) {
        timeline_metadata = timelineDataTable[id];
        std::string timeline_id = saturn_keyframe_get_mario_timeline_id(id, get<6>(timeline_metadata) ? mario_menu_index : -1);
    }
    imgui_bundled_tooltip(contains ? "Remove" : "Animate");
}

std::map<std::string, float*> keyframe_helper_values = {};

void saturn_keyframe_helper(std::string id, float* value, float max) {
    if (keyframe_helper_values.find(id) == keyframe_helper_values.end()) {
        float* data = (float*)malloc(sizeof(float) * 4);
        data[0] = max;
        data[1] = max - *value;
        data[2] = 1;
        data[3] = max;
        keyframe_helper_values.insert({ id, data });
    }
    float* data = keyframe_helper_values[id];
    if (data[3] != max) {
        free(data);
        keyframe_helper_values[id] = data = (float*)malloc(sizeof(float) * 4);
        data[0] = max;
        data[1] = max - *value;
        data[2] = 1;
        data[3] = max;
    }
    ImGui::SeparatorText("Keyframe Helper");
    if (ImGui::SliderFloat("End Frame", data + 0, 0, max         ) |
        ImGui::SliderFloat("Duration" , data + 1, 0, max - *value) )
        data[2] = (data[0] - *value) / data[1];
    if (ImGui::SliderFloat("Speed", data + 2, 0, 2)) {
        data[1] = (data[0] - *value) / data[2];
    }
    if (ImGui::Button("Place Keyframe")) {
        k_current_frame += data[1];
        k_previous_frame = k_current_frame;
        for (auto timeline : k_frame_keys) {
            saturn_keyframe_apply(timeline.first, k_current_frame);
        }
        *value = data[0];
    }
}

bool saturn_disable_sm64_input() {
    return ImGui::GetIO().WantTextInput;
}

std::map<std::string, std::string> texture_forwards = {};

void saturn_get_textures_folder(char* out) {
    std::string path;
    if (current_texture_id == -1) path = FS_TEXTUREDIR "/";
    else path = "../dynos/textures/" + textures_list[current_texture_id] + "/";
    memcpy(out, path.data(), path.length() + 1);
}

void saturn_fallback_texture(char* tex, const char* path) {
    fs_file_t* f = fs_open(tex);
    if (f) {
        fs_close(f);
        return;
    }
    std::string fallback = std::string(FS_TEXTUREDIR "/") + path;
    memcpy(tex, fallback.data(), fallback.length() + 1);
}

const char* saturn_texture_forward(const char* input) {
    if (texture_forwards.find(input) == texture_forwards.end()) return input;
    return texture_forwards[input].c_str();
}

template <typename T>
void saturn_keyframe_popout(const T &edit_value, s32 data_type, string value_name, string id) {
    /*string buttonLabel = ICON_FK_LINK "###kb_" + id;
    string windowLabel = "Timeline###kw_" + id;

#ifdef DISCORDRPC
    discord_state = "In-Game // Keyframing";
#endif

    ImGui::SameLine();
    if (ImGui::Button(buttonLabel.c_str())) {
        if (active_key_float_value != edit_value) {
            keyframe_playing = false;
            k_last_placed_frame = 0;
            k_frame_keys = {0};
            k_v_float_keys = {0.f};

            k_c_pos1_keys = {0.f};
            k_c_pos2_keys = {0.f};
            k_c_foc0_keys = {0.f};
            k_c_foc1_keys = {0.f};
            k_c_foc2_keys = {0.f};
            k_c_rot0_keys = {0.f};
            k_c_rot1_keys = {0.f};
        }

        k_popout_open = true;
        active_key_float_value = edit_value;
        active_data_type = data_type;
    }

    if (k_popout_open && active_key_float_value == edit_value) {
        if (windowSettings) windowSettings = false;

        ImGuiWindowFlags timeline_flags = imgui_bundled_window_corner(1, 0, 0, 0.64f);
        ImGui::Begin(windowLabel.c_str(), &k_popout_open, timeline_flags);
        if (!keyframe_playing) {
            if (ImGui::Button(ICON_FK_PLAY " Play###k_t_play")) {
                saturn_play_keyframe();
            }
            ImGui::SameLine();
            ImGui::Checkbox("Loop###k_t_loop", &k_loop);
            ImGui::SameLine(180);
            if (ImGui::Button(ICON_FK_TRASH_O " Clear All###k_t_clear")) {
                k_last_placed_frame = 0;
                k_frame_keys = {0};
                k_v_float_keys = {0.f};
                k_v_int_keys = {0};
                
                k_c_pos1_keys = {0.f};
                k_c_pos2_keys = {0.f};
                k_c_foc0_keys = {0.f};
                k_c_foc1_keys = {0.f};
                k_c_foc2_keys = {0.f};
                k_c_rot0_keys = {0.f};
                k_c_rot1_keys = {0.f};
            }
        } else {
            if (ImGui::Button(ICON_FK_STOP " Stop###k_t_stop")) {
                keyframe_playing = false;
            }
            ImGui::SameLine();
            ImGui::Checkbox("Loop###k_t_loop", &k_loop);
        }

        ImGui::Separator();

        ImGui::PushItemWidth(35);
        if (ImGui::InputInt("Frames", &endFrameText, 0, 0, ImGuiInputTextFlags_EnterReturnsTrue)) {
            if (endFrameText >= 60) {
                endFrame = (uint32_t)endFrameText;
            } else {
                endFrame = 60;
                endFrameText = 60;
            }
        }
        ImGui::PopItemWidth();
                
        // Popout
        if (ImGui::BeginNeoSequencer("Sequencer###k_sequencer", (uint32_t*)&k_current_frame, &startFrame, &endFrame, ImVec2(endFrame * 6, 0), ImGuiNeoSequencerFlags_HideZoom)) {
            if (ImGui::BeginNeoTimeline(value_name.c_str(), k_frame_keys)) { ImGui::EndNeoTimeLine(); }
            if (data_type == KEY_CAMERA) if(ImGui::BeginNeoTimeline("Rotation", k_frame_keys)) { ImGui::EndNeoTimeLine(); }
            ImGui::EndNeoSequencer();
        }

        // UI Controls
        if (!keyframe_playing) {
            if (k_current_frame != 0) {
                if (std::find(k_frame_keys.begin(), k_frame_keys.end(), k_current_frame) != k_frame_keys.end()) {
                    // We are hovering over a keyframe
                    auto it = std::find(k_frame_keys.begin(), k_frame_keys.end(), k_current_frame);
                    if (it != k_frame_keys.end()) {
                        int key_index = it - k_frame_keys.begin();
                        if (ImGui::Button(ICON_FK_MINUS_SQUARE " Delete Keyframe###k_d_frame")) {
                            // Delete Keyframe
                            k_frame_keys.erase(k_frame_keys.begin() + key_index);
                            if (data_type == KEY_FLOAT || data_type == KEY_CAMERA) k_v_float_keys.erase(k_v_float_keys.begin() + key_index);
                            if (data_type == KEY_INT) k_v_int_keys.erase(k_v_int_keys.begin() + key_index);

                            if (data_type == KEY_CAMERA) {
                                k_c_pos1_keys.erase(k_c_pos1_keys.begin() + key_index);
                                k_c_pos2_keys.erase(k_c_pos2_keys.begin() + key_index);
                                k_c_foc0_keys.erase(k_c_foc0_keys.begin() + key_index);
                                k_c_foc1_keys.erase(k_c_foc1_keys.begin() + key_index);
                                k_c_foc2_keys.erase(k_c_foc2_keys.begin() + key_index);
                                k_c_rot0_keys.erase(k_c_rot0_keys.begin() + key_index);
                                k_c_rot1_keys.erase(k_c_rot1_keys.begin() + key_index);
                            }

                            k_last_placed_frame = k_frame_keys[k_frame_keys.size() - 1];
                        } ImGui::SameLine(); ImGui::Text("at %i", (int)k_current_frame);
                    }
                } else {
                    // No keyframe here
                    if (ImGui::Button(ICON_FK_PLUS_SQUARE " Create Keyframe###k_c_frame")) {
                        if (k_last_placed_frame > k_current_frame) {
                            
                        } else {
                            k_frame_keys.push_back(k_current_frame);
                            if (data_type == KEY_FLOAT || data_type == KEY_CAMERA) k_v_float_keys.push_back(*edit_value);
                            if (data_type == KEY_INT) k_v_int_keys.push_back(*edit_value);

                            if (data_type == KEY_CAMERA) {
                                f32 dist;
                                s16 pitch, yaw;
                                vec3f_get_dist_and_angle(gCamera->pos, gCamera->focus, &dist, &pitch, &yaw);
                                k_c_pos1_keys.push_back(gCamera->pos[1]);
                                k_c_pos2_keys.push_back(gCamera->pos[2]);
                                k_c_foc0_keys.push_back(gCamera->focus[0]);
                                k_c_foc1_keys.push_back(gCamera->focus[1]);
                                k_c_foc2_keys.push_back(gCamera->focus[2]);
                                k_c_rot0_keys.push_back(yaw);
                                k_c_rot1_keys.push_back(pitch);
                                std::cout << pitch << std::endl;
                            }
                            k_last_placed_frame = k_current_frame;
                        }
                    } ImGui::SameLine(); ImGui::Text("at %i", (int)k_current_frame);
                }
            } else {
                // On frame 0
                if (data_type == KEY_CAMERA)
                    ImGui::Text("(Setting initial value)");
            }
        }
        ImGui::End();

        // Auto focus (use controls without clicking window first)
        if (ImGui::IsWindowHovered(ImGuiHoveredFlags_None) && accept_text_input == false) {
            ImGui::SetWindowFocus(windowLabel.c_str());
        }
    }*/
}
