#include <psp2/kernel/processmgr.h>
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

#define CLR_BG      0xFF121212
#define CLR_ACCENT  0xFF00CCFF
#define CLR_TEXT    0xFFFFFFFF
#define CLR_DIR     0xFFFFAA00
#define CLR_SUB     0xFF00FF88

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

// Variabili Video Cruciali
SceAvPlayerHandle player = -1;
vita2d_texture *video_tex = NULL;
int is_playing = 0;

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

void parse_episodes(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return;
    ep_count = 0;
    char line[1024];
    char current_title[256] = "Episodio";
    while (fgets(line, sizeof(line), f) && ep_count < 500) {
        if (strncmp(line, "#EXTINF:", 8) == 0) {
            char *t = strrchr(line, ','); if (t) strncpy(current_title, t + 1, 255);
        } else if (strncmp(line, "http", 4) == 0) {
            strncpy(episodes[ep_count].title, current_title, 255);
            strncpy(episodes[ep_count].url, line, 1023);
            episodes[ep_count].url[strcspn(episodes[ep_count].url, "\r\n")] = 0;
            ep_count++;
        }
    }
    fclose(f);
}

void start_vid(const char *url) {
    if (player >= 0) { sceAvPlayerStop(player); sceAvPlayerClose(player); player = -1; }
    
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
    
    // Inizializza Rete (Fondamentale per i link esterni)
    SceNetInitParam netParam;
    netParam.memory = malloc(1024*1024); netParam.size = 1024*1024;
    sceNetInit(&netParam); sceNetCtlInit();

    vita2d_init();
    vita2d_pgf *pgf = vita2d_load_default_pgf();
    scan_dir(cur_path);

    SceCtrlData pad, old_pad;
    while (1) {
        sceCtrlPeekBufferPositive(0, &pad, 1);
        
        // Logica stati (Browser -> Episodi -> Risoluzioni) come prima
        if (app_state == STATE_BROWSER) {
            if ((pad.buttons & SCE_CTRL_DOWN) && !(old_pad.buttons & SCE_CTRL_DOWN)) sel = (sel + 1) % entry_count;
            if ((pad.buttons & SCE_CTRL_UP) && !(old_pad.buttons & SCE_CTRL_UP)) sel = (sel - 1 + entry_count) % entry_count;
            if ((pad.buttons & SCE_CTRL_CROSS) && !(old_pad.buttons & SCE_CTRL_CROSS)) {
                if (entries[sel].is_dir) {
                    strcat(cur_path, "/"); strcat(cur_path, entries[sel].name);
                    scan_dir(cur_path);
                } else {
                    char full[1024]; snprintf(full, 1024, "%s/%s", cur_path, entries[sel].name);
                    parse_episodes(full); app_state = STATE_EPISODES;
                }
            }
        } 
        else if (app_state == STATE_EPISODES) {
            if ((pad.buttons & SCE_CTRL_CROSS) && !(old_pad.buttons & SCE_CTRL_CROSS)) {
                // Simuliamo le risoluzioni per il tuo link
                res_count = 2;
                strncpy(resolutions[0].title, "1080p Full HD", 255);
                strncpy(resolutions[1].title, "720p HD", 255);
                app_state = STATE_RESOLUTIONS;
            }
            if (pad.buttons & SCE_CTRL_CIRCLE) app_state = STATE_BROWSER;
        }
        else if (app_state == STATE_RESOLUTIONS) {
            if ((pad.buttons & SCE_CTRL_CROSS) && !(old_pad.buttons & SCE_CTRL_CROSS)) {
                start_vid(episodes[ep_sel].url); 
                app_state = STATE_PLAYING;
            }
            if (pad.buttons & SCE_CTRL_CIRCLE) app_state = STATE_EPISODES;
        }
        else if (app_state == STATE_PLAYING) {
            if (pad.buttons & SCE_CTRL_CIRCLE) { is_playing = 0; app_state = STATE_BROWSER; }
        }

        vita2d_start_drawing();
        vita2d_clear_screen(CLR_BG);

        if (app_state != STATE_PLAYING) {
            // UI dei Menu (Browser/Episodi/Risoluzioni)
            vita2d_pgf_draw_text(pgf, 20, 40, CLR_ACCENT, 1.0f, "Vita IPTV Native");
            // ... (Disegno liste come prima)
        } else {
            // RENDERING VIDEO ATTIVO
            SceAvPlayerFrameInfo frame;
            if (sceAvPlayerGetVideoData(player, &frame)) {
                // Se non abbiamo la texture o la risoluzione è cambiata, la creiamo
                if (!video_tex) {
                    video_tex = vita2d_create_empty_texture_format(frame.details.video.width, frame.details.video.height, SCE_GXM_TEXTURE_FORMAT_YVU420P2_AU_BC1);
                }
                
                // Aggiorniamo i dati della texture con il frame decodificato
                void *data = vita2d_texture_get_datap(video_tex);
                memcpy(data, frame.pData, frame.frameSize);
                
                // Disegniamo il video a tutto schermo
                vita2d_draw_texture_scale(video_tex, 0, 0, 960.0f/frame.details.video.width, 544.0f/frame.details.video.height);
            }
            vita2d_pgf_draw_text(pgf, 10, 530, 0x88FFFFFF, 0.6f, "Premi O per uscire dal video");
        }

        vita2d_end_drawing();
        vita2d_swap_buffers();
        old_pad = pad;
    }
    return 0;
}
