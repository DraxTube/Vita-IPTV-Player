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

void *av_malloc(void *unused, unsigned int alignment, unsigned int size) { return malloc(size); }
void av_free(void *unused, void *ptr) { free(ptr); }

// Temi
#define CLR_BG      0xFF121212
#define CLR_ACCENT  0xFF00CCFF
#define CLR_TEXT    0xFFFFFFFF
#define CLR_DIR     0xFFFFAA00 // Arancione per le cartelle

enum { STATE_BROWSER, STATE_PLAYING };
int app_state = STATE_BROWSER;

typedef struct { char name[256]; int is_dir; } Entry;
Entry entries[200];
int entry_count = 0;
char cur_path[512] = "ux0:";
int sel = 0;

// Player vars
SceAvPlayerHandle player = -1;
int audio_port = -1;
vita2d_texture *vid_tex = NULL;
uint8_t *rgba_buf = NULL;
int is_playing = 0;

// Funzione corretta per scansionare cartelle su Vita
void scan_dir(const char *path) {
    DIR *d = opendir(path);
    entry_count = 0;
    sel = 0;
    if (d) {
        struct dirent *de;
        while ((de = readdir(d)) && entry_count < 200) {
            if (strcmp(de->d_name, ".") == 0) continue;
            
            strncpy(entries[entry_count].name, de->d_name, 255);
            
            // Fix d_type: usiamo stat()
            struct stat st;
            char full_path[1024];
            snprintf(full_path, sizeof(full_path), "%s/%s", path, de->d_name);
            stat(full_path, &st);
            entries[entry_count].is_dir = S_ISDIR(st.st_mode);
            
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
                    if(strcmp(entries[sel].name, "..") == 0) {
                        char *last = strrchr(cur_path, '/');
                        if (last) *last = '\0';
                    } else {
                        strcat(cur_path, "/"); strcat(cur_path, entries[sel].name);
                    }
                    scan_dir(cur_path);
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
            // Header
            vita2d_draw_rectangle(0, 0, 960, 60, CLR_ACCENT);
            vita2d_pgf_draw_text(pgf, 20, 40, CLR_BG, 1.2f, "VITA IPTV NATIVE");
            
            // Path info
            vita2d_pgf_draw_text(pgf, 20, 85, 0xFF888888, 0.8f, cur_path);
            
            // File List
            for (int i = 0; i < entry_count; i++) {
                if (i < 15) { // Limite visivo
                    uint32_t c = (i == sel) ? CLR_ACCENT : (entries[i].is_dir ? CLR_DIR : CLR_TEXT);
                    char display_name[300];
                    snprintf(display_name, 300, "%s %s", entries[i].is_dir ? "[DIR]" : "     ", entries[i].name);
                    vita2d_pgf_draw_text(pgf, 30, 130 + (i * 28), c, 1.0f, display_name);
                }
            }
        } else if (is_playing) {
            // Messaggio UI se il video sta caricando o in background
            vita2d_pgf_draw_text(pgf, 20, 520, CLR_ACCENT, 1.0f, "Riproduzione... Premi O per tornare al browser");
        }

        vita2d_end_drawing();
        vita2d_swap_buffers();
        old_pad = pad;
    }
    
    vita2d_fini();
    return 0;
}
