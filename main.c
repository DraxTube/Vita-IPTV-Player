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

// --- HACK PER LINKER ---
int sceSharedFbClose() { return 0; }
int sceSharedFbOpen() { return 0; }
int sceSharedFbGetInfo() { return 0; }
int sceSharedFbEnd() { return 0; }
int sceSharedFbBegin() { return 0; }
int _sceSharedFbOpen() { return 0; }

void *av_malloc(void *unused, unsigned int alignment, unsigned int size) { return malloc(size); }
void av_free(void *unused, void *ptr) { free(ptr); }

#define CLR_BG      0xFF121212
#define CLR_ACCENT  0xFF00CCFF
#define CLR_TEXT    0xFFFFFFFF

enum { STATE_BROWSER, STATE_EPISODES, STATE_RESOLUTIONS, STATE_PLAYING };
int app_state = STATE_BROWSER;

// Strutture e variabili globali
typedef struct { char title[256]; char url[1024]; } ListEntry;
ListEntry episodes[500];
int ep_count = 0, ep_sel = 0;

SceAvPlayerHandle player = -1;
vita2d_texture *video_tex = NULL;
int is_playing = 0;

// ... (Funzioni scan_dir e parse_episodes rimosse per brevità, mantieni quelle precedenti) ...

void start_vid(const char *url) {
    if (player >= 0) { sceAvPlayerStop(player); sceAvPlayerClose(player); player = -1; }
    if (video_tex) { vita2d_free_texture(video_tex); video_tex = NULL; }

    SceAvPlayerInitData init; memset(&init, 0, sizeof(init));
    init.memoryReplacement.allocate = av_malloc;
    init.memoryReplacement.deallocate = av_free;
    init.memoryReplacement.allocateTexture = av_malloc;
    init.memoryReplacement.deallocateTexture = av_free;
    init.basePriority = 160;
    init.autoStart = SCE_TRUE;

    player = sceAvPlayerInit(&init);
    sceAvPlayerAddSource(player, url);
    is_playing = 1;
}

int main() {
    sceSysmoduleLoadModule(SCE_SYSMODULE_NET);
    sceSysmoduleLoadModule(SCE_SYSMODULE_AVPLAYER);
    
    SceNetInitParam netParam;
    netParam.memory = malloc(1024*1024); netParam.size = 1024*1024;
    sceNetInit(&netParam); sceNetCtlInit();

    vita2d_init();
    vita2d_pgf *pgf = vita2d_load_default_pgf();
    // scan_dir("ux0:"); // Inizializza browser

    SceCtrlData pad, old_pad;
    while (1) {
        sceCtrlPeekBufferPositive(0, &pad, 1);
        
        // Logica navigazione (Browser -> Episodi -> Risoluzioni)
        // ... (Mantieni la logica CROSS/CIRCLE precedente) ...

        vita2d_start_drawing();
        vita2d_clear_screen(CLR_BG);

        if (app_state == STATE_PLAYING) {
            SceAvPlayerFrameInfo frame;
            if (sceAvPlayerGetVideoData(player, &frame)) {
                if (!video_tex) {
                    video_tex = vita2d_create_empty_texture_format(frame.details.video.width, frame.details.video.height, SCE_GXM_TEXTURE_FORMAT_YVU420P2_CSC1);
                }
                
                void *data = vita2d_texture_get_datap(video_tex);
                
                // --- CORREZIONE QUI ---
                // In alcune versioni di SDK si usa frame.details.video.pitch * height * 1.5 per YUV420
                uint32_t frame_size = frame.details.video.width * frame.details.video.height * 3 / 2;
                memcpy(data, frame.pData, frame_size);
                
                vita2d_draw_texture_scale(video_tex, 0, 0, 960.0f/frame.details.video.width, 544.0f/frame.details.video.height);
            } else {
                // Messaggio di caricamento se il frame non è ancora pronto
                vita2d_pgf_draw_text(pgf, 400, 272, CLR_ACCENT, 1.0f, "Caricamento buffer...");
            }
        } else {
            // Disegno Menu
            vita2d_pgf_draw_text(pgf, 20, 40, CLR_ACCENT, 1.0f, "Vita IPTV - Seleziona un contenuto");
            // ... (Codice per disegnare le liste) ...
        }

        vita2d_end_drawing();
        vita2d_swap_buffers();
        old_pad = pad;
    }
    return 0;
}
