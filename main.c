#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/ctrl.h>
#include <psp2/sysmodule.h>
#include <psp2/net/net.h>
#include <psp2/avplayer.h>
#include <psp2/audioout.h>
#include <vita2d.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>

// --- HACK LINKER ---
int sceSharedFbClose() { return 0; }
int sceSharedFbOpen() { return 0; }
int sceSharedFbGetInfo() { return 0; }
int sceSharedFbEnd() { return 0; }
int sceSharedFbBegin() { return 0; }
int _sceSharedFbOpen() { return 0; }

// --- CALLBACK MEMORIA ---
void *av_malloc(void *unused, unsigned int alignment, unsigned int size) { return malloc(size); }
void av_free(void *unused, void *ptr) { free(ptr); }

// --- CONFIGURAZIONE ---
#define CLR_BG      0xFF121212
#define CLR_ACCENT  0xFF00CCFF
#define CLR_TEXT    0xFFFFFFFF

enum { STATE_BROWSER, STATE_PLAYLIST, STATE_PLAYING };
int app_state = STATE_BROWSER;

typedef struct { char title[256]; char url[1024]; } PlaylistEntry;
PlaylistEntry playlist[500];
int playlist_count = 0;
int play_sel = 0;

SceAvPlayerHandle player = -1;
int audio_port = -1;
int is_playing = 0;
vita2d_texture *vid_tex = NULL;

// --- PARSER AVANZATO PER MASTER PLAYLIST ---
void parse_m3u_master(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return;
    playlist_count = 0;
    char line[1024];
    char current_info[256] = "Stream Video";

    while (fgets(line, sizeof(line), f) && playlist_count < 500) {
        if (strncmp(line, "#EXT-X-STREAM-INF:", 18) == 0) {
            char *res = strstr(line, "RESOLUTION=");
            if (res) snprintf(current_info, 255, "Risoluzione: %s", res + 11);
        } else if (strncmp(line, "http", 4) == 0) {
            strncpy(playlist[playlist_count].title, current_info, 255);
            strncpy(playlist[playlist_count].url, line, 1023);
            playlist[playlist_count].url[strcspn(playlist[playlist_count].url, "\r\n")] = 0;
            playlist_count++;
        }
    }
    fclose(f);
}

void stop_vid() {
    is_playing = 0;
    if (player >= 0) { sceAvPlayerStop(player); sceAvPlayerClose(player); player = -1; }
    if (audio_port >= 0) { sceAudioOutReleasePort(audio_port); audio_port = -1; }
    if (vid_tex) { vita2d_free_texture(vid_tex); vid_tex = NULL; }
}

void start_vid(const char *url) {
    stop_vid();
    SceAvPlayerInitData init;
    memset(&init, 0, sizeof(init));
    init.memoryReplacement.allocate = av_malloc;
    init.memoryReplacement.deallocate = av_free;
    init.memoryReplacement.allocateTexture = av_malloc;
    init.memoryReplacement.deallocateTexture = av_free;
    init.basePriority = 160;
    init.autoStart = SCE_TRUE;

    player = sceAvPlayerInit(&init);
    sceAvPlayerAddSource(player, url);
    audio_port = sceAudioOutOpenPort(SCE_AUDIO_OUT_PORT_TYPE_MAIN, 1024, 48000, SCE_AUDIO_OUT_MODE_STEREO);
    is_playing = 1;
}

int main() {
    sceSysmoduleLoadModule(SCE_SYSMODULE_NET);
    sceSysmoduleLoadModule(SCE_SYSMODULE_AVPLAYER);
    
    // Inizializzazione Rete per HTTPS
    SceNetInitParam netParam;
    netParam.memory = malloc(1024*1024);
    netParam.size = 1024*1024;
    sceNetInit(&netParam);
    sceNetCtlInit();

    vita2d_init();
    vita2d_pgf *pgf = vita2d_load_default_pgf();

    // Simulazione apertura link master che hai fornito
    parse_m3u_master("ux0:data/master.m3u8"); // Assicurati di averlo scaricato lì o cambia logica
    app_state = STATE_PLAYLIST;

    SceCtrlData pad, old_pad;
    while (1) {
        sceCtrlPeekBufferPositive(0, &pad, 1);
        
        if (app_state == STATE_PLAYLIST) {
            if ((pad.buttons & SCE_CTRL_DOWN) && !(old_pad.buttons & SCE_CTRL_DOWN)) play_sel = (play_sel + 1) % playlist_count;
            if ((pad.buttons & SCE_CTRL_UP) && !(old_pad.buttons & SCE_CTRL_UP)) play_sel = (play_sel - 1 + playlist_count) % playlist_count;
            if ((pad.buttons & SCE_CTRL_CROSS) && !(old_pad.buttons & SCE_CTRL_CROSS)) {
                start_vid(playlist[play_sel].url);
                app_state = STATE_PLAYING;
            }
        } else if (app_state == STATE_PLAYING) {
            if (pad.buttons & SCE_CTRL_CIRCLE) { stop_vid(); app_state = STATE_PLAYLIST; }
        }

        vita2d_start_drawing();
        vita2d_clear_screen(CLR_BG);

        if (app_state == STATE_PLAYLIST) {
            vita2d_pgf_draw_text(pgf, 20, 40, CLR_ACCENT, 1.2f, "Seleziona Risoluzione:");
            for (int i = 0; i < playlist_count; i++) {
                uint32_t c = (i == play_sel) ? CLR_ACCENT : CLR_TEXT;
                vita2d_pgf_draw_text(pgf, 30, 100 + (i * 35), c, 1.0f, playlist[i].title);
            }
        } else if (is_playing) {
            // Qui l'AVPlayer usa il piano video hardware (sotto la grafica vita2d)
            vita2d_pgf_draw_text(pgf, 20, 520, CLR_ACCENT, 1.0f, "Streaming in corso... Premi O per fermare");
        }

        vita2d_end_drawing();
        vita2d_swap_buffers();
        old_pad = pad;
    }

    return 0;
}
