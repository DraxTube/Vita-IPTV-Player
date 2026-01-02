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

// Hack Linker
int sceSharedFbClose() { return 0; }
int sceSharedFbOpen() { return 0; }
int sceSharedFbGetInfo() { return 0; }
int sceSharedFbEnd() { return 0; }
int sceSharedFbBegin() { return 0; }
int _sceSharedFbOpen() { return 0; }

// Callback Memoria
void *av_malloc(void *unused, unsigned int alignment, unsigned int size) { return malloc(size); }
void av_free(void *unused, void *ptr) { free(ptr); }

// Temi e Colori
#define CLR_BG      0xFF121212
#define CLR_ACCENT  0xFF00CCFF
#define CLR_TEXT    0xFFFFFFFF
#define CLR_DIR     0xFF00FFCC

// Stati e Browser
enum { STATE_BROWSER, STATE_PLAYING };
int app_state = STATE_BROWSER;

typedef struct { char name[256]; int is_dir; } Entry;
Entry entries[200];
int entry_count = 0;
char cur_path[512] = "ux0:";
int sel = 0;

// Player
SceAvPlayerHandle player = -1;
int audio_port = -1;
vita2d_texture *vid_tex = NULL;
uint8_t *rgba_buf = NULL;
int is_playing = 0;

void scan_dir(const char *path) {
    DIR *d = opendir(path);
    entry_count = 0;
    if (d) {
        struct dirent *de;
        while ((de = readdir(d)) && entry_count < 200) {
            if (strcmp(de->d_name, ".") == 0) continue;
            strncpy(entries[entry_count].name, de->d_name, 255);
            entries[entry_count].is_dir = (de->d_type == DT_DIR);
            entry_count++;
        }
        closedir(d);
    }
}

void stop_vid() {
    is_playing = 0;
    if (player >= 0) { sceAvPlayerStop(player); sceAvPlayerClose(player); player = -1; }
    if (audio_port >= 0) { sceAudioOutReleasePort(audio_port); audio_port = -1; }
    if (vid_tex) { vita2d_free_texture(vid_tex); vid_tex = NULL; }
    if (rgba_buf) { free(rgba_buf); rgba_buf = NULL; }
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

    player = sceAvPlayerInit(&init);
    sceAvPlayerAddSource(player, url);
    audio_port = sceAudioOutOpenPort(SCE_AUDIO_OUT_PORT_TYPE_MAIN, 1024, 48000, SCE_AUDIO_OUT_MODE_STEREO);
    sceAvPlayerStart(player);
    is_playing = 1;
}

int main() {
    sceSysmoduleLoadModule(SCE_SYSMODULE_NET);
    sceSysmoduleLoadModule(SCE_SYSMODULE_AVPLAYER);
    
    vita2d_init();
    vita2d_pgf *pgf = vita2d_load_default_pgf();
    scan_dir(cur_path);

    SceCtrlData pad, old_pad;
    while (1) {
        sceCtrlPeekBufferPositive(0, &pad, 1);
        
        if (app_state == STATE_BROWSER) {
            if ((pad.buttons & SCE_CTRL_DOWN) && !(old_pad.buttons & SCE_CTRL_DOWN)) sel = (sel + 1) % entry_count;
            if ((pad.buttons & SCE_CTRL_UP) && !(old_pad.buttons & SCE_CTRL_UP)) sel = (sel - 1 + entry_count) % entry_count;
            if ((pad.buttons & SCE_CTRL_CROSS) && !(old_pad.buttons & SCE_CTRL_CROSS)) {
                if (entries[sel].is_dir) {
                    strcat(cur_path, "/"); strcat(cur_path, entries[sel].name);
                    scan_dir(cur_path); sel = 0;
                } else {
                    char target[1024]; snprintf(target, 1024, "%s/%s", cur_path, entries[sel].name);
                    start_vid(target); app_state = STATE_PLAYING;
                }
            }
        } else {
            if (pad.buttons & SCE_CTRL_CIRCLE) { stop_vid(); app_state = STATE_BROWSER; }
        }

        vita2d_start_drawing();
        vita2d_clear_screen(CLR_BG);

        if (app_state == STATE_BROWSER) {
            vita2d_draw_rectangle(0, 0, 960, 60, CLR_ACCENT);
            vita2d_pgf_draw_text(pgf, 20, 40, CLR_BG, 1.2f, "VITA IPTV NATIVE BROWSER");
            for (int i = 0; i < entry_count && i < 15; i++) {
                uint32_t c = (i == sel) ? CLR_ACCENT : (entries[i].is_dir ? CLR_DIR : CLR_TEXT);
                vita2d_pgf_draw_text(pgf, 30, 100 + (i * 30), c, 1.0f, entries[i].name);
            }
        } else if (is_playing) {
            // Logica di rendering video frame...
            vita2d_pgf_draw_text(pgf, 20, 520, CLR_ACCENT, 1.0f, "Riproduzione in corso... Premi O per uscire");
        }

        vita2d_end_drawing();
        vita2d_swap_buffers();
        old_pad = pad;
    }
    return 0;
}
