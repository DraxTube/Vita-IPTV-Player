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

// --- LINKER HACKS ---
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
#define CLR_BG     0xFF121212
#define CLR_DIR    0xFFFFAA00
#define CLR_SUB    0xFF00FF88

enum { STATE_BROWSER, STATE_EPISODES, STATE_RESOLUTIONS, STATE_PLAYING };
int app_state = STATE_BROWSER;

typedef struct { char name[256]; int is_dir; } Entry;
Entry entries[200];
int entry_count = 0;
char cur_path[512] = "ux0:";
int sel = 0;

typedef struct { char title[256]; char url[1024]; } ListEntry;
ListEntry episodes[500];
ListEntry resolutions[20];
int ep_count = 0, res_count = 0, ep_sel = 0, res_sel = 0;

SceAvPlayerHandle player = -1;
vita2d_texture *video_tex = NULL;
int audio_port = -1;

void scan_dir(const char *path) {
    DIR *d = opendir(path);
    entry_count = 0; sel = 0;
    if (d) {
        struct dirent *de;
        while ((de = readdir(d)) && entry_count < 200) {
            if (strcmp(de->d_name, ".") == 0) continue;
            strncpy(entries[entry_count].name, de->d_name, 255);
            struct stat st; char full[1024]; snprintf(full, 1024, "%s/%s", path, de->d_name);
            stat(full, &st);
            entries[entry_count].is_dir = S_ISDIR(st.st_mode);
            entry_count++;
        }
        closedir(d);
    }
}

void parse_m3u(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return;
    ep_count = 0; char line[1024], title[256] = "Canale";
    while (fgets(line, sizeof(line), f) && ep_count < 500) {
        if (strncmp(line, "#EXTINF:", 8) == 0) {
            char *c = strrchr(line, ','); if (c) strncpy(title, c + 1, 255);
        } else if (strncmp(line, "http", 4) == 0) {
            strncpy(episodes[ep_count].title, title, 255);
            strncpy(episodes[ep_count].url, line, 1023);
            episodes[ep_count].url[strcspn(episodes[ep_count].url, "\r\n")] = 0;
            ep_count++;
        }
    }
    fclose(f);
}

void start_vid(const char *url) {
    if (player >= 0) { sceAvPlayerStop(player); sceAvPlayerClose(player); player = -1; }
    if (audio_port >= 0) { sceAudioOutReleasePort(audio_port); audio_port = -1; }
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
    
    // Apriamo l'audio in anticipo per "svegliare" il player
    audio_port = sceAudioOutOpenPort(SCE_AUDIO_OUT_PORT_TYPE_MAIN, 1024, 48000, SCE_AUDIO_OUT_MODE_STEREO);
}

int main() {
    sceSysmoduleLoadModule(SCE_SYSMODULE_NET);
    sceSysmoduleLoadModule(SCE_SYSMODULE_AVPLAYER);
    SceNetInitParam netParam; netParam.memory = malloc(1024*1024); netParam.size = 1024*1024;
    sceNetInit(&netParam); sceNetCtlInit();
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
                    strcat(cur_path, "/"); strcat(cur_path, entries[sel].name); scan_dir(cur_path);
                } else {
                    char full[1024]; snprintf(full, 1024, "%s/%s", cur_path, entries[sel].name);
                    parse_m3u(full); app_state = STATE_EPISODES; ep_sel = 0;
                }
            }
        } else if (app_state == STATE_EPISODES) {
            if ((pad.buttons & SCE_CTRL_DOWN) && !(old_pad.buttons & SCE_CTRL_DOWN)) ep_sel = (ep_sel + 1) % ep_count;
            if ((pad.buttons & SCE_CTRL_UP) && !(old_pad.buttons & SCE_CTRL_UP)) ep_sel = (ep_sel - 1 + ep_count) % ep_count;
            if ((pad.buttons & SCE_CTRL_CROSS) && !(old_pad.buttons & SCE_CTRL_CROSS)) {
                res_count = 2;
                strncpy(resolutions[0].title, "1080p Full HD", 255);
                strncpy(resolutions[1].title, "720p HD", 255);
                app_state = STATE_RESOLUTIONS; res_sel = 0;
            }
            if (pad.buttons & SCE_CTRL_CIRCLE) app_state = STATE_BROWSER;
        } else if (app_state == STATE_RESOLUTIONS) {
            if ((pad.buttons & SCE_CTRL_CROSS) && !(old_pad.buttons & SCE_CTRL_CROSS)) {
                start_vid(episodes[ep_sel].url); app_state = STATE_PLAYING;
            }
            if (pad.buttons & SCE_CTRL_CIRCLE) app_state = STATE_EPISODES;
        } else if (app_state == STATE_PLAYING) {
            if (pad.buttons & SCE_CTRL_CIRCLE) { 
                if (player >= 0) { sceAvPlayerStop(player); sceAvPlayerClose(player); player = -1; }
                app_state = STATE_BROWSER; 
            }
        }

        vita2d_start_drawing();
        vita2d_clear_screen(CLR_BG);

        if (app_state == STATE_PLAYING) {
            // GESTIONE AUDIO (Cruciale per non bloccare il video)
            if (sceAvPlayerIsActive(player)) {
                SceAvPlayerAudioOutRaw raw;
                if (sceAvPlayerGetAudioData(player, &raw)) {
                    sceAudioOutOutput(audio_port, raw.pData);
                }
            }

            SceAvPlayerFrameInfo frame;
            if (sceAvPlayerGetVideoData(player, &frame)) {
                if (!video_tex) video_tex = vita2d_create_empty_texture_format(frame.details.video.width, frame.details.video.height, SCE_GXM_TEXTURE_FORMAT_YVU420P2_CSC1);
                void *data = vita2d_texture_get_datap(video_tex);
                memcpy(data, frame.pData, (frame.details.video.width * frame.details.video.height * 1.5));
                vita2d_draw_texture_scale(video_tex, 0, 0, 960.0f/frame.details.video.width, 544.0f/frame.details.video.height);
            } else {
                static int loader = 0; loader = (loader + 5) % 400;
                vita2d_draw_rectangle(280, 272, 400, 20, 0xFF444444);
                vita2d_draw_rectangle(280, 272, loader, 20, CLR_ACCENT);
                vita2d_pgf_draw_text(pgf, 320, 250, CLR_ACCENT, 1.2f, "RETE ATTIVA...");
                vita2d_pgf_draw_text(pgf, 290, 320, CLR_TEXT, 0.8f, "Sincronizzazione stream HLS...");
            }
        } else {
            // DISEGNO MENU (Ripristinato e fisso)
            if (app_state == STATE_BROWSER) {
                vita2d_pgf_draw_text(pgf, 20, 40, CLR_ACCENT, 1.0f, "1. BROWSER FILE (ux0:)");
                for (int i = 0; i < entry_count && i < 15; i++) {
                    uint32_t c = (i == sel) ? CLR_ACCENT : (entries[i].is_dir ? CLR_DIR : CLR_TEXT);
                    vita2d_pgf_draw_text(pgf, 30, 100 + (i * 30), c, 1.0f, entries[i].name);
                }
            } else if (app_state == STATE_EPISODES) {
                vita2d_pgf_draw_text(pgf, 20, 40, CLR_SUB, 1.0f, "2. LISTA CANALI / EPISODI");
                for (int i = 0; i < ep_count && i < 15; i++) {
                    uint32_t c = (i == ep_sel) ? CLR_SUB : CLR_TEXT;
                    vita2d_pgf_draw_text(pgf, 30, 100 + (i * 30), c, 1.0f, episodes[i].title);
                }
            } else if (app_state == STATE_RESOLUTIONS) {
                vita2d_pgf_draw_text(pgf, 20, 40, CLR_ACCENT, 1.0f, "3. SELEZIONA RISOLUZIONE");
                for (int i = 0; i < res_count; i++) {
                    uint32_t c = (i == res_sel) ? CLR_ACCENT : CLR_TEXT;
                    vita2d_pgf_draw_text(pgf, 30, 100 + (i * 40), c, 1.0f, resolutions[i].title);
                }
            }
        }

        vita2d_end_drawing();
        vita2d_swap_buffers();
        old_pad = pad;
    }
    return 0;
}
