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

// --- HACK PER LINKER ---
int sceSharedFbClose() { return 0; }
int sceSharedFbOpen() { return 0; }
int sceSharedFbGetInfo() { return 0; }
int sceSharedFbEnd() { return 0; }
int sceSharedFbBegin() { return 0; }
int _sceSharedFbOpen() { return 0; }
// -----------------------

#define WHITE 0xFFFFFFFF
#define GREEN 0xFF00FF00
#define RED   0xFF0000FF
#define GREY  0xFFAAAAAA
#define YELLOW 0xFF00FFFF

// Stati App
enum { STATE_BROWSER, STATE_PLAYING };
int app_state = STATE_BROWSER;

// Variabili Browser
typedef struct { char name[256]; int is_folder; } FileEntry;
FileEntry files[500];
int file_count = 0;
char current_path[512] = "ux0:";
char selected_url[1024];

// Variabili Player
SceAvPlayerHandle movie_player;
int audio_port = -1;
int is_playing = 0;
vita2d_texture *video_texture = NULL;
uint8_t *rgba_buffer = NULL;
SceUID audio_thread_id;
int run_audio = 0;

// --- FUNZIONI VIDEO (Conversione Colore) ---
// Converte NV12 (formato nativo video) in RGBA (formato schermo Vita)
// Nota: È una conversione software, pesante per la CPU.
void nv12_to_rgba(uint8_t *y_base, uint8_t *uv_base, int width, int height, uint8_t *rgba) {
    int i, j;
    for (j = 0; j < height; j++) {
        for (i = 0; i < width; i++) {
            int y_pos = j * width + i;
            int uv_pos = (j / 2) * width + (i & ~1);

            int y = y_base[y_pos];
            int u = uv_base[uv_pos];
            int v = uv_base[uv_pos + 1];

            int c = y - 16;
            int d = u - 128;
            int e = v - 128;

            int r = (298 * c + 409 * e + 128) >> 8;
            int g = (298 * c - 100 * d - 208 * e + 128) >> 8;
            int b = (298 * c + 516 * d + 128) >> 8;

            if (r < 0) r = 0; if (r > 255) r = 255;
            if (g < 0) g = 0; if (g > 255) g = 255;
            if (b < 0) b = 0; if (b > 255) b = 255;

            int rgba_pos = (j * width + i) * 4;
            rgba[rgba_pos] = r;
            rgba[rgba_pos + 1] = g;
            rgba[rgba_pos + 2] = b;
            rgba[rgba_pos + 3] = 255; // Alpha
        }
    }
}

// --- THREAD AUDIO ---
// AvPlayer decodifica l'audio, noi dobbiamo inviarlo alle casse manualmente
int audio_thread(SceSize args, void *argp) {
    SceAvPlayerAudioData audio_data;
    memset(&audio_data, 0, sizeof(audio_data));

    while (run_audio) {
        if (is_playing && sceAvPlayerGetAudioData(movie_player, &audio_data) == 0) {
            sceAudioOutOutput(audio_port, audio_data.pData, audio_data.size / 4); // 4 bytes per sample (stereo 16bit)
        } else {
            sceKernelDelayThread(10000); // Riposa se non c'è audio
        }
    }
    return 0;
}

// --- GESTIONE PLAYER ---
void stop_video() {
    run_audio = 0;
    is_playing = 0;
    
    if (audio_thread_id >= 0) {
        sceKernelWaitThreadEnd(audio_thread_id, NULL, NULL);
        sceKernelDeleteThread(audio_thread_id);
    }
    
    if (movie_player) {
        sceAvPlayerStop(movie_player);
        sceAvPlayerClose(movie_player); // Importante: rilascia risorse
        movie_player = NULL;
    }
    if (audio_port >= 0) {
        sceAudioOutReleasePort(audio_port);
        audio_port = -1;
    }
    if (video_texture) {
        vita2d_free_texture(video_texture);
        video_texture = NULL;
    }
    if (rgba_buffer) {
        free(rgba_buffer);
        rgba_buffer = NULL;
    }
}

void start_video(const char *url) {
    stop_video(); // Pulisce precedenti

    // 1. Inizializza AvPlayer
    SceAvPlayerInitData playerInit;
    memset(&playerInit, 0, sizeof(playerInit));
    playerInit.memoryReplacement.allocate = malloc;
    playerInit.memoryReplacement.deallocate = free;
    playerInit.memoryReplacement.allocateTexture = malloc;
    playerInit.memoryReplacement.deallocateTexture = free;
    playerInit.basePriority = 100;

    movie_player = sceAvPlayerInit(&playerInit);
    sceAvPlayerAddSource(movie_player, url);
    
    // 2. Setup Audio Port (48kHz Stereo è standard)
    audio_port = sceAudioOutOpenPort(SCE_AUDIO_OUT_PORT_TYPE_MAIN, 1024, 48000, SCE_AUDIO_OUT_MODE_STEREO);
    
    // 3. Avvia Thread Audio
    run_audio = 1;
    audio_thread_id = sceKernelCreateThread("AudioThread", audio_thread, 0x10000100, 0x10000, 0, 0, NULL);
    sceKernelStartThread(audio_thread_id, 0, NULL);

    sceAvPlayerStart(movie_player);
    is_playing = 1;
}

// Aggiorna texture video
void update_video_frame() {
    if (!is_playing || !movie_player) return;

    SceAvPlayerVideoData video_data;
    if (sceAvPlayerGetVideoData(movie_player, &video_data) == 0) {
        // Se è il primo frame, crea la texture e il buffer
        if (!video_texture) {
            video_texture = vita2d_create_empty_texture(video_data.width, video_data.height);
            rgba_buffer = malloc(video_data.width * video_data.height * 4);
        }

        // Converti NV12 -> RGBA
        // pData = Luma (Y), pData + width*height = Chroma (UV)
        nv12_to_rgba((uint8_t*)video_data.pData, 
                     (uint8_t*)video_data.pData + (video_data.width * video_data.height), 
                     video_data.width, video_data.height, rgba_buffer);

        // Aggiorna texture GPU
        void *tex_data = vita2d_texture_get_datap(video_texture);
        memcpy(tex_data, rgba_buffer, video_data.width * video_data.height * 4);
    }
}

// --- BROWSER E PARSER (Codice precedente condensato) ---
int compare_files(const void *a, const void *b) {
    return ((FileEntry*)a)->is_folder > ((FileEntry*)b)->is_folder ? -1 : 1;
}

void scan_directory(const char *path) {
    DIR *d = opendir(path);
    file_count = 0;
    if (d) {
        struct dirent *dir;
        while ((dir = readdir(d)) != NULL) {
            if (file_count >= 500) break;
            if (strcmp(dir->d_name, ".") == 0) continue;
            strncpy(files[file_count].name, dir->d_name, 255);
            struct stat st; char full[1024];
            snprintf(full, sizeof(full), "%s/%s", path, dir->d_name);
            stat(full, &st);
            files[file_count].is_folder = S_ISDIR(st.st_mode);
            file_count++;
        }
        closedir(d);
        qsort(files, file_count, sizeof(FileEntry), compare_files);
    }
}

// Parsing M3U semplificato: cerca solo il primo URL per testare
void parse_and_play_m3u(const char *filepath) {
    FILE *fp = fopen(filepath, "r");
    if (!fp) return;
    char line[2048];
    while (fgets(line, sizeof(line), fp)) {
        int len = strlen(line);
        if (len > 0 && line[len-1] == '\n') line[len-1] = '\0';
        if (strncmp(line, "http", 4) == 0) {
            strncpy(selected_url, line, 1023); // Prende il primo link
            fclose(fp);
            app_state = STATE_PLAYING;
            start_video(selected_url);
            return;
        }
    }
    fclose(fp);
}

// --- MAIN ---
int main() {
    sceSysmoduleLoadModule(SCE_SYSMODULE_NET);
    sceSysmoduleLoadModule(SCE_SYSMODULE_AVPLAYER);
    
    vita2d_init();
    vita2d_set_clear_color(0xFF000000);
    vita2d_pgf *pgf = vita2d_load_default_pgf();

    scan_directory(current_path);

    int selection = 0;
    int scroll = 0;
    SceCtrlData pad, old_pad;
    memset(&old_pad, 0, sizeof(old_pad));

    while (1) {
        sceCtrlPeekBufferPositive(0, &pad, 1);

        if (app_state == STATE_BROWSER) {
            // Controlli Browser
            if ((pad.buttons & SCE_CTRL_UP) && !(old_pad.buttons & SCE_CTRL_UP)) if (selection > 0) selection--;
            if ((pad.buttons & SCE_CTRL_DOWN) && !(old_pad.buttons & SCE_CTRL_DOWN)) if (selection < file_count - 1) selection++;
            
            if ((pad.buttons & SCE_CTRL_CROSS) && !(old_pad.buttons & SCE_CTRL_CROSS)) {
                if (files[selection].is_folder) {
                     if (strcmp(files[selection].name, "..") != 0) {
                        strcat(current_path, "/"); strcat(current_path, files[selection].name);
                        scan_directory(current_path); selection = 0; scroll = 0;
                     }
                } else {
                    // Se è M3U, estrae link e avvia player
                    char full[1024]; snprintf(full, sizeof(full), "%s/%s", current_path, files[selection].name);
                    parse_and_play_m3u(full);
                }
            }
        } 
        else if (app_state == STATE_PLAYING) {
            // Logica Player
            update_video_frame(); // CATTURA FRAME VIDEO

            if ((pad.buttons & SCE_CTRL_CIRCLE) && !(old_pad.buttons & SCE_CTRL_CIRCLE)) {
                stop_video();
                app_state = STATE_BROWSER;
            }
        }

        // Scroll
        if (selection < scroll) scroll = selection;
        if (selection > scroll + 10) scroll = selection - 10;
        old_pad = pad;

        vita2d_start_drawing();
        vita2d_clear_screen();

        if (app_state == STATE_BROWSER) {
            vita2d_pgf_draw_text(pgf, 20, 30, GREEN, 1.0f, "IPTV NATIVE - Browser");
            vita2d_pgf_draw_text(pgf, 20, 50, GREY, 0.7f, current_path);
            int y = 80;
            for (int i = scroll; i < file_count && i < scroll + 12; i++) {
                uint32_t color = (i == selection) ? GREEN : WHITE;
                if (files[i].is_folder) color = (i == selection) ? YELLOW : 0xFF00AAFF;
                vita2d_pgf_draw_text(pgf, 30, y, color, 1.0f, files[i].name);
                y += 25;
            }
        } 
        else if (app_state == STATE_PLAYING) {
            if (video_texture) {
                // Disegna il video a tutto schermo (scala se necessario)
                float scale_x = 960.0f / vita2d_texture_get_width(video_texture);
                float scale_y = 544.0f / vita2d_texture_get_height(video_texture);
                vita2d_draw_texture_scale(video_texture, 0, 0, scale_x, scale_y);
            } else {
                vita2d_pgf_draw_text(pgf, 400, 272, YELLOW, 1.0f, "Buffering...");
            }
            
            // UI Overlay (scompare dopo un po' se volessimo, qui fissa)
            vita2d_pgf_draw_text(pgf, 20, 500, RED, 1.0f, "[CERCHIO] Stop / Indietro");
        }

        vita2d_end_drawing();
        vita2d_swap_buffers();
    }

    stop_video();
    vita2d_fini();
    vita2d_free_pgf(pgf);
    sceSysmoduleUnloadModule(SCE_SYSMODULE_NET);
    sceSysmoduleUnloadModule(SCE_SYSMODULE_AVPLAYER);
    sceKernelExitProcess(0);
    return 0;
}
