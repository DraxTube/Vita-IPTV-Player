#include <psp2/kernel/processmgr.h>
#include <psp2/ctrl.h>
#include <psp2/power.h>
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
#include <malloc.h>

// --- FIX LINKER ---
int sceSharedFbClose() { return 0; }
int sceSharedFbOpen() { return 0; }
int sceSharedFbGetInfo() { return 0; }
int sceSharedFbEnd() { return 0; }
int sceSharedFbBegin() { return 0; }
int _sceSharedFbOpen() { return 0; }

// --- FIX MEMORIA (CRITICO PER 1080p) ---
// Usa memalign per garantire blocchi contigui per il decoder HW
void *av_malloc(void *u, unsigned int a, unsigned int s) { 
    return memalign(a, s); 
}
void av_free(void *u, void *p) { 
    free(p); 
}

#define CLR_ACCENT 0xFF00CCFF
#define CLR_TEXT   0xFFFFFFFF
#define CLR_BG     0xFF121212
#define CLR_DIR    0xFFFFAA00
#define CLR_SUB    0xFF00FF88
#define CLR_ERR    0xFF0000FF

enum { STATE_BROWSER, STATE_EPISODES, STATE_RESOLUTIONS, STATE_PLAYING };
int app_state = STATE_BROWSER;

// Variabili Browser
typedef struct { char name[256]; int is_dir; } Entry;
Entry entries[200];
int entry_count = 0, sel = 0;
char cur_path[512] = "ux0:";

// Variabili Playlist
typedef struct { char title[256]; char url[1024]; } ListEntry;
ListEntry episodes[500];
ListEntry resolutions[2]; 
int ep_count = 0, ep_sel = 0, res_sel = 0;

SceAvPlayerHandle player = -1;
vita2d_texture *video_tex = NULL;
int audio_port = -1;

void scan_dir(const char *path) {
    DIR *d = opendir(path);
    entry_count = 0; sel = 0;
    if (d) {
        struct dirent *de;
        while ((de = readdir(d)) && entry_count < 200) {
            if (de->d_name[0] == '.' && de->d_name[1] != '.') continue;
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
        // Rimuovi newline
        char *n = strpbrk(line, "\r\n"); if (n) *n = 0;
        
        if (strncmp(line, "#EXTINF:", 8) == 0) {
            char *c = strrchr(line, ','); 
            if (c) strncpy(title, c + 1, 255);
        } else if (strncmp(line, "http", 4) == 0) {
            strncpy(episodes[ep_count].title, title, 255);
            strncpy(episodes[ep_count].url, line, 1023);
            ep_count++;
        }
    }
    fclose(f);
}

void start_vid(const char *url) {
    // Pulizia precedente
    if (player >= 0) { sceAvPlayerStop(player); sceAvPlayerClose(player); player = -1; }
    if (audio_port >= 0) { sceAudioOutReleasePort(audio_port); audio_port = -1; }
    if (video_tex) { vita2d_free_texture(video_tex); video_tex = NULL; }

    SceAvPlayerInitData init; 
    memset(&init, 0, sizeof(init));
    
    // Gestione Memoria Avanzata
    init.memoryReplacement.allocate = av_malloc;
    init.memoryReplacement.deallocate = av_free;
    init.memoryReplacement.allocateTexture = av_malloc;
    init.memoryReplacement.deallocateTexture = av_free;
    
    // Configurazione Player per 1080p
    init.basePriority = 100; // Priorità thread
    init.numOutputVideoFrameBuffers = 2; // 2 frame buffer bastano, 3 consumano troppa RAM col 1080p
    init.autoStart = SCE_TRUE;
    init.debugLevel = 0; 
    
    player = sceAvPlayerInit(&init);
    
    // --- USER AGENT TRUCCO ---
    // Finge di essere un PC Windows con VLC/Browser per evitare blocchi IPTV
    sceAvPlayerSetUserAgent(player, "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/58.0.3029.110 Safari/537.36");

    sceAvPlayerAddSource(player, url);
    
    // Audio Port: 48kHz Stereo
    audio_port = sceAudioOutOpenPort(SCE_AUDIO_OUT_PORT_TYPE_MAIN, 1024, 48000, SCE_AUDIO_OUT_MODE_STEREO);
}

int main() {
    // --- CARICAMENTO MODULI ---
    sceSysmoduleLoadModule(SCE_SYSMODULE_NET);
    sceSysmoduleLoadModule(SCE_SYSMODULE_SSL);  // HTTPS
    sceSysmoduleLoadModule(SCE_SYSMODULE_HTTP); // Client HTTP
    sceSysmoduleLoadModule(SCE_SYSMODULE_AVPLAYER);
    
    // --- INIT RETE POTENZIATO ---
    // 8MB di heap per la rete per gestire il buffering del 1080p
    SceNetInitParam netParam; 
    netParam.memory = malloc(8*1024*1024); 
    netParam.size = 8*1024*1024;
    sceNetInit(&netParam); 
    sceNetCtlInit();
    
    vita2d_init();
    vita2d_set_clear_color(CLR_BG);
    vita2d_pgf *pgf = vita2d_load_default_pgf();
    
    scan_dir(cur_path);

    strncpy(resolutions[0].title, "Standard (Usa come default)", 255);
    strncpy(resolutions[1].title, "Alternativo", 255);

    SceCtrlData pad, old_pad;
    memset(&old_pad, 0, sizeof(old_pad));

    while (1) {
        sceCtrlPeekBufferPositive(0, &pad, 1);
        
        // Evita che lo schermo si spenga durante la visione
        sceKernelPowerTick(SCE_KERNEL_POWER_TICK_DEFAULT);

        if (app_state == STATE_BROWSER) {
            if ((pad.buttons & SCE_CTRL_DOWN) && !(old_pad.buttons & SCE_CTRL_DOWN)) sel = (sel + 1) % entry_count;
            if ((pad.buttons & SCE_CTRL_UP) && !(old_pad.buttons & SCE_CTRL_UP)) sel = (sel - 1 + entry_count) % entry_count;
            if ((pad.buttons & SCE_CTRL_CROSS) && !(old_pad.buttons & SCE_CTRL_CROSS)) {
                if (entry_count > 0) {
                    if (entries[sel].is_dir) {
                        char new_path[1024]; snprintf(new_path, 1024, "%s/%s", cur_path, entries[sel].name);
                        strcpy(cur_path, new_path); 
                        scan_dir(cur_path);
                    } else {
                        char full[1024]; snprintf(full, 1024, "%s/%s", cur_path, entries[sel].name);
                        parse_m3u(full); 
                        if (ep_count > 0) {
                            app_state = STATE_EPISODES; 
                            ep_sel = 0;
                        }
                    }
                }
            }
            if ((pad.buttons & SCE_CTRL_TRIANGLE) && !(old_pad.buttons & SCE_CTRL_TRIANGLE)) {
                // Torna indietro directory (semplificato)
                strcpy(cur_path, "ux0:"); scan_dir(cur_path);
            }
        } else if (app_state == STATE_EPISODES) {
            if ((pad.buttons & SCE_CTRL_DOWN) && !(old_pad.buttons & SCE_CTRL_DOWN)) ep_sel = (ep_sel + 1) % ep_count;
            if ((pad.buttons & SCE_CTRL_UP) && !(old_pad.buttons & SCE_CTRL_UP)) ep_sel = (ep_sel - 1 + ep_count) % ep_count;
            if ((pad.buttons & SCE_CTRL_CROSS) && !(old_pad.buttons & SCE_CTRL_CROSS)) {
                start_vid(episodes[ep_sel].url); 
                app_state = STATE_PLAYING;
            }
            if (pad.buttons & SCE_CTRL_CIRCLE) app_state = STATE_BROWSER;
        } else if (app_state == STATE_PLAYING) {
            if (pad.buttons & SCE_CTRL_CIRCLE) { 
                if (player >= 0) { sceAvPlayerStop(player); sceAvPlayerClose(player); player = -1; }
                if (video_tex) { vita2d_free_texture(video_tex); video_tex = NULL; }
                app_state = STATE_EPISODES; 
            }
        }

        vita2d_start_drawing();
        vita2d_clear_screen();

        if (app_state == STATE_PLAYING) {
            SceAvPlayerFrameInfo a_info, v_info;
            
            // Audio Output
            if (player >= 0 && sceAvPlayerGetAudioData(player, &a_info)) {
                 sceAudioOutOutput(audio_port, a_info.pData);
            }
            
            // Video Output
            if (player >= 0 && sceAvPlayerGetVideoData(player, &v_info)) {
                // Crea texture solo se cambia risoluzione o è NULL
                if (!video_tex || 
                    vita2d_texture_get_width(video_tex) != v_info.details.video.width ||
                    vita2d_texture_get_height(video_tex) != v_info.details.video.height) {
                    
                    if(video_tex) vita2d_free_texture(video_tex);
                    video_tex = vita2d_create_empty_texture_format(
                        v_info.details.video.width, 
                        v_info.details.video.height, 
                        SCE_GXM_TEXTURE_FORMAT_YVU420P2_CSC1
                    );
                }

                if (video_tex) {
                    void *data = vita2d_texture_get_datap(video_tex);
                    // Copia YUV data
                    memcpy(data, v_info.pData, (v_info.details.video.width * v_info.details.video.height * 1.5));
                    
                    // Scala a tutto schermo (960x544)
                    float scale_x = 960.0f / v_info.details.video.width;
                    float scale_y = 544.0f / v_info.details.video.height;
                    vita2d_draw_texture_scale(video_tex, 0, 0, scale_x, scale_y);
                }
            } else {
                // UI di Caricamento
                static int loader = 0; loader = (loader + 8) % 960;
                vita2d_draw_rectangle(0, 540, 960, 4, 0xFF444444);
                vita2d_draw_rectangle(loader, 540, 100, 4, CLR_ACCENT);
                
                vita2d_pgf_draw_text(pgf, 20, 520, CLR_ACCENT, 1.2f, "BUFFERING HD...");
                if (player >= 0 && !sceAvPlayerIsActive(player)) {
                     vita2d_pgf_draw_text(pgf, 20, 520, CLR_ERR, 1.2f, "ERRORE STREAM O TIMEOUT");
                }
            }
        } else {
            // INTERFACCIA UTENTE
            vita2d_pgf_draw_text(pgf, 10, 25, CLR_ACCENT, 1.0f, "PS VITA IPTV PLAYER - HD EDITION");
            vita2d_draw_line(0, 30, 960, 30, CLR_ACCENT);

            if (app_state == STATE_BROWSER) {
                for (int i = 0; i < 14 && i < entry_count; i++) {
                    int idx = (sel / 14) * 14 + i;
                    if (idx >= entry_count) break;
                    
                    uint32_t c = (idx == sel) ? CLR_ACCENT : (entries[idx].is_dir ? CLR_DIR : CLR_TEXT);
                    char display[300];
                    snprintf(display, 300, "%s %s", entries[idx].is_dir ? "[DIR]" : "", entries[idx].name);
                    vita2d_pgf_draw_text(pgf, 20, 60 + (i * 32), c, 1.0f, display);
                }
            } else if (app_state == STATE_EPISODES) {
                for (int i = 0; i < 14 && i < ep_count; i++) {
                    int idx = (ep_sel / 14) * 14 + i;
                    if (idx >= ep_count) break;

                    uint32_t c = (idx == ep_sel) ? CLR_SUB : CLR_TEXT;
                    vita2d_pgf_draw_text(pgf, 20, 60 + (i * 32), c, 1.0f, episodes[idx].title);
                }
            }
        }

        vita2d_end_drawing();
        vita2d_swap_buffers();
        old_pad = pad;
    }
    return 0;
}
