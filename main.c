#include <psp2/kernel/processmgr.h>
#include <psp2/ctrl.h>
#include <psp2/sysmodule.h>
#include <psp2/net/net.h>
#include <psp2/net/netctl.h>
#include <psp2/avplayer.h>
#include <psp2/audioout.h>
#include <vita2d.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>

// --- HACKS ---
int sceSharedFbClose() { return 0; }
int sceSharedFbOpen() { return 0; }
int sceSharedFbGetInfo() { return 0; }
int sceSharedFbEnd() { return 0; }
int sceSharedFbBegin() { return 0; }
int _sceSharedFbOpen() { return 0; }
void *av_malloc(void *u, unsigned int a, unsigned int s) { return malloc(s); }
void av_free(void *u, void *p) { free(p); }

#define CLR_ACCENT 0xFF00CCFF
#define CLR_TEXT   0xFFFFFFFF
enum { STATE_BROWSER, STATE_EPISODES, STATE_RESOLUTIONS, STATE_PLAYING };
int app_state = STATE_BROWSER;

// Variabili globali
typedef struct { char title[256], url[1024]; } ListEntry;
ListEntry episodes[500]; int ep_count = 0, ep_sel = 0;
SceAvPlayerHandle player = -1;
vita2d_texture *video_tex = NULL;
char debug_msg[128] = "Inizializzazione...";

// --- (scan_dir e parse_episodes rimangono come prima, caricano ux0: e .m3u8) ---
// [Assumi le funzioni scan_dir e parse_episodes qui presenti]

void start_vid(const char *url) {
    if (player >= 0) { sceAvPlayerStop(player); sceAvPlayerClose(player); player = -1; }
    if (video_tex) { vita2d_free_texture(video_tex); video_tex = NULL; }

    SceAvPlayerInitData init; memset(&init, 0, sizeof(init));
    init.memoryReplacement.allocate = av_malloc;
    init.memoryReplacement.deallocate = av_free;
    init.memoryReplacement.allocateTexture = av_malloc;
    init.memoryReplacement.deallocateTexture = av_free;
    init.basePriority = 160;
    init.numVideoTrack = 1;
    init.numAudioTrack = 1;
    init.autoStart = SCE_TRUE;

    player = sceAvPlayerInit(&init);
    sceAvPlayerAddSource(player, url);
    strcpy(debug_msg, "Richiesta URL inviata...");
}

int main() {
    sceSysmoduleLoadModule(SCE_SYSMODULE_NET);
    sceSysmoduleLoadModule(SCE_SYSMODULE_AVPLAYER);
    SceNetInitParam netParam; netParam.memory = malloc(1024*1024); netParam.size = 1024*1024;
    sceNetInit(&netParam); sceNetCtlInit();
    vita2d_init();
    vita2d_pgf *pgf = vita2d_load_default_pgf();
    // scan_dir("ux0:"); // Funzione per caricare i file all'avvio

    SceCtrlData pad, old_pad;
    while (1) {
        sceCtrlPeekBufferPositive(0, &pad, 1);
        // ... (Logica pulsanti STATE_BROWSER, EPISODES, RESOLUTIONS come prima) ...

        vita2d_start_drawing();
        vita2d_clear_screen(0xFF121212);

        if (app_state == STATE_PLAYING) {
            SceAvPlayerFrameInfo frame;
            if (sceAvPlayerGetVideoData(player, &frame)) {
                if (!video_tex) video_tex = vita2d_create_empty_texture_format(frame.details.video.width, frame.details.video.height, SCE_GXM_TEXTURE_FORMAT_YVU420P2_CSC1);
                void *data = vita2d_texture_get_datap(video_tex);
                memcpy(data, frame.pData, (frame.details.video.width * frame.details.video.height * 3 / 2));
                vita2d_draw_texture_scale(video_tex, 0, 0, 960.0f/frame.details.video.width, 544.0f/frame.details.video.height);
            } else {
                // INFO DEBUG MENTRE CARICA
                static int timer = 0; timer++;
                int bar_w = (timer % 100) * 4;
                vita2d_draw_rectangle(280, 270, 400, 20, 0xFF444444);
                vita2d_draw_rectangle(280, 270, bar_w, 20, CLR_ACCENT);
                vita2d_pgf_draw_text(pgf, 320, 250, CLR_ACCENT, 1.0f, "BUFFERING IPTV...");
                vita2d_pgf_draw_text(pgf, 300, 320, CLR_TEXT, 0.7f, debug_msg);
                
                // Monitoraggio stato
                if (sceAvPlayerIsActive(player)) strcpy(debug_msg, "Server trovato, scaricamento segmenti...");
                else if (timer > 500) strcpy(debug_msg, "Errore: Il server non risponde (TLS/SSL?)");
            }
        }
        // ... (Disegno menu STATE_BROWSER ecc) ...

        vita2d_end_drawing();
        vita2d_swap_buffers();
        old_pad = pad;
    }
    return 0;
}
