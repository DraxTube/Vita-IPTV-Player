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
#define CLR_DIR     0xFFFFAA00
#define CLR_EPISODE 0xFF00FF88

enum { STATE_BROWSER, STATE_PLAYLIST, STATE_PLAYING };
int app_state = STATE_BROWSER;

typedef struct { char name[256]; int is_dir; } Entry;
Entry entries[200];
int entry_count = 0;

typedef struct { char title[256]; char url[1024]; } PlaylistEntry;
PlaylistEntry playlist[500];
int playlist_count = 0;

char cur_path[512] = "ux0:";
int sel = 0;
int play_sel = 0;

// Player vars
SceAvPlayerHandle player = -1;
int audio_port = -1;
int is_playing = 0;

// Funzione per caricare e leggere il contenuto di un file M3U
void parse_m3u(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return;

    playlist_count = 0;
    char line[1024];
    char current_title[256] = "Titolo Sconosciuto";

    while (fgets(line, sizeof(line), f) && playlist_count < 500) {
        if (strncmp(line, "#EXTINF:", 8) == 0) {
            char *title_part = strrchr(line, ',');
            if (title_part) strncpy(current_title, title_part + 1, 255);
        } else if (strncmp(line, "http", 4) == 0 || strncmp(line, "ux0:", 4) == 0) {
            strncpy(playlist[playlist_count].title, current_title, 255);
            strncpy(playlist[playlist_count].url, line, 1023);
            // Pulisci i caratteri di fine riga
            playlist[playlist_count].url[strcspn(playlist[playlist_count].url, "\r\n")] = 0;
            playlist_count++;
        }
    }
    fclose(f);
}

void scan_dir(const char *path) {
    DIR *d = opendir(path);
    entry_count = 0;
    sel = 0;
    if (d) {
        struct dirent *de;
        while ((de = readdir(d)) && entry_count < 200) {
            if (strcmp(de->d_name, ".") == 0) continue;
            strncpy(entries[entry_count].name, de->d_name, 255);
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

void start_vid(const char *url) {
    if (player >= 0) { sceAvPlayerStop(player); sceAvPlayerClose(player); player = -1; }
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
                    scan_dir(cur_path);
                } else {
                    char target[1024]; snprintf(target, 1024, "%s/%s", cur_path, entries[sel].name);
                    if (strstr(target, ".m3u")) {
                        parse_m3u(target);
                        app_state = STATE_PLAYLIST;
                        play_sel = 0;
                    } else {
                        start_vid(target);
                        app_state = STATE_PLAYING;
                    }
                }
            }
        } 
        else if (app_state == STATE_PLAYLIST) {
            if ((pad.buttons & SCE_CTRL_DOWN) && !(old_pad.buttons & SCE_CTRL_DOWN)) play_sel = (play_sel + 1) % playlist_count;
            if ((pad.buttons & SCE_CTRL_UP) && !(old_pad.buttons & SCE_CTRL_UP)) play_sel = (play_sel - 1 + playlist_count) % playlist_count;
            if ((pad.buttons & SCE_CTRL_CROSS) && !(old_pad.buttons & SCE_CTRL_CROSS)) {
                start_vid(playlist[play_sel].url);
                app_state = STATE_PLAYING;
            }
            if ((pad.buttons & SCE_CTRL_CIRCLE) && !(old_pad.buttons & SCE_CTRL_CIRCLE)) app_state = STATE_BROWSER;
        }
        else {
            if (pad.buttons & SCE_CTRL_CIRCLE) { is_playing = 0; app_state = STATE_PLAYLIST; }
        }

        vita2d_start_drawing();
        vita2d_clear_screen(CLR_BG);

        if (app_state == STATE_BROWSER) {
            vita2d_draw_rectangle(0, 0, 960, 60, CLR_ACCENT);
            vita2d_pgf_draw_text(pgf, 20, 40, CLR_BG, 1.2f, "Scegli File o Playlist");
            for (int i = 0; i < entry_count && i < 15; i++) {
                uint32_t c = (i == sel) ? CLR_ACCENT : (entries[i].is_dir ? CLR_DIR : CLR_TEXT);
                vita2d_pgf_draw_text(pgf, 30, 100 + (i * 30), c, 1.0f, entries[i].name);
            }
        } 
        else if (app_state == STATE_PLAYLIST) {
            vita2d_draw_rectangle(0, 0, 960, 60, CLR_EPISODE);
            vita2d_pgf_draw_text(pgf, 20, 40, CLR_BG, 1.2f, "Scegli Episodio/Canale");
            for (int i = 0; i < playlist_count && i < 15; i++) {
                uint32_t c = (i == play_sel) ? CLR_EPISODE : CLR_TEXT;
                vita2d_pgf_draw_text(pgf, 30, 100 + (i * 30), c, 1.0f, playlist[i].title);
            }
        }
        else if (is_playing) {
            vita2d_pgf_draw_text(pgf, 20, 520, CLR_ACCENT, 1.0f, "In riproduzione... Premi O per la lista");
        }

        vita2d_end_drawing();
        vita2d_swap_buffers();
        old_pad = pad;
    }
    return 0;
}
