#ifndef CONFIGFILE_H
#define CONFIGFILE_H

#include <stdbool.h>

#define CONFIGFILE_DEFAULT "studio-config.txt"

#define MAX_BINDS  3
#define MAX_VOLUME 127

typedef struct {
    unsigned int x, y, w, h;
    bool vsync;
    bool enable_antialias;
    unsigned int antialias_level;
    bool jabo_mode;
    bool reset;
    bool fullscreen;
    bool exiting_fullscreen;
    bool settings_changed;
    bool fps_changed;
} ConfigWindow;

extern ConfigWindow configWindow;
extern unsigned int configFiltering;
extern unsigned int configMasterVolume;
extern unsigned int configMusicVolume;
extern unsigned int configSfxVolume;
extern unsigned int configEnvVolume;
extern unsigned int configAudioMode;
//extern bool         configVoicesEnabled;
extern unsigned int configKeyA[];
extern unsigned int configKeyB[];
extern unsigned int configKeyStart[];
extern unsigned int configKeyL[];
extern unsigned int configKeyR[];
extern unsigned int configKeyZ[];
extern unsigned int configKeyCUp[];
extern unsigned int configKeyCDown[];
extern unsigned int configKeyCLeft[];
extern unsigned int configKeyCRight[];
extern unsigned int configKeyStickUp[];
extern unsigned int configKeyStickDown[];
extern unsigned int configKeyStickLeft[];
extern unsigned int configKeyStickRight[];
extern unsigned int configStickDeadzone;
extern unsigned int configRumbleStrength;
// Saturn
extern unsigned int configKeyFreeze[];
extern unsigned int configKeyPlayAnim[];
extern unsigned int configKeyScreenshot[];
extern unsigned int configKeyStopInpRec[];
#ifdef EXTERNAL_DATA
extern bool         configPrecacheRes;
#endif
extern unsigned int configEditorTheme;
extern bool         configEditorFastApply;
extern bool         configEditorAutoModelCc;
extern bool         configEditorAutoSpark;
extern bool         configEditorNearClipping;
extern bool         configEditorShowTips;
extern unsigned int configFps60;
extern bool         configEditorInterpolateAnims;
extern bool         configEditorExpressionPreviews;
extern unsigned int configFakeStarCount;
extern bool         configUnlockDoors;
extern bool         configEditorAlwaysChroma;
extern bool         configWindowState;
extern bool         configNoWaterBombs;
extern bool         configNoCamShake;
extern bool         configNoButterflies;
extern bool         configSaturnSplash;
extern bool         configNoWater;
extern bool         configCUpLimit;
extern unsigned int configEditorThemeJson;
extern unsigned int configEditorTextures;
extern float        camera_fov;
extern bool         configUnstableFeatures;
extern unsigned int configWorldsimSteps;
extern bool         configEnableShipBug;
#ifdef BETTERCAMERA
extern unsigned int configCameraXSens;
extern unsigned int configCameraYSens;
extern unsigned int configCameraAggr;
extern unsigned int configCameraPan;
extern unsigned int configCameraDegrade;
extern bool         configCameraInvertX;
extern bool         configCameraInvertY;
extern bool         configEnableCamera;
extern bool         configCameraMouse;
extern bool         configCameraAnalog;
#endif
extern bool         configHUD;
extern bool         configSkipIntro;
#ifdef DISCORDRPC
extern bool         configDiscordRPC;
#endif
extern unsigned int configAutosaveDelay;

extern float configCamCtrlMousePanSens;
extern float configCamCtrlMouseRotSens;
extern float configCamCtrlMouseZoomSens;
extern float configCamCtrlKeybPanSens;
extern float configCamCtrlKeybRotSens;
extern float configCamCtrlKeybZoomSens;
extern float configCamCtrlMouseInprecRotSens;
extern float configCamCtrlMouseInprecZoomSens;
extern float configCamCtrlKeybInprecSens;
extern bool configCamCtrlMousePanInvX;
extern bool configCamCtrlMousePanInvY;
extern bool configCamCtrlMouseRotInvX;
extern bool configCamCtrlMouseRotInvY;
extern bool configCamCtrlMouseZoomInv;
extern bool configCamCtrlKeybPanInvX;
extern bool configCamCtrlKeybPanInvY;
extern bool configCamCtrlKeybRotInvX;
extern bool configCamCtrlKeybRotInvY;
extern bool configCamCtrlKeybZoomInv;
extern bool configCamCtrlMouseInprecRotInvX;
extern bool configCamCtrlMouseInprecRotInvY;
extern bool configCamCtrlMouseInprecZoomInv;
extern bool configCamCtrlKeybInprecRotInvX;
extern bool configCamCtrlKeybInprecRotInvY;

void configfile_load(const char *filename);
void configfile_save(const char *filename);
const char *configfile_name(void);

#ifdef __cplusplus
extern "C" {
#endif
    extern bool         configVoicesEnabled;
#ifdef __cplusplus
}
#endif

#endif
