#include <psp2/kernel/processmgr.h>
#include <psp2/ctrl.h>
#include <psp2/sysmodule.h>
#include <psp2/net/net.h>
#include <psp2/appmgr.h>
#include <vita2d.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>

// --- HACK PER IL LINKER (Essenziale) ---
int sceSharedFbClose() { return 0; }
int sceSharedFbOpen() { return 0; }
int sceSharedFbGetInfo() { return 0; }
int sceSharedFbEnd() { return 0; }
int sceSharedFbBegin() { return 0; }
int _sceSharedFbOpen() { return 0; }
// ---------------------------------------

#define WHITE 0xFFFFFFFF
#define GREEN 0xFF00FF00
#define RED   0xFF0000FF
#define GREY  0xFFAAAAAA
#define YELLOW 0xFF00FFFF

// Stati dell'applicazione
enum {
    STATE_BROWSER,
    STATE_CHANNEL_LIST
};

// Strutture Dati
typedef struct {
    char name[256];
    int is_folder;
} FileEntry;

typedef struct {
    char name[256];
    char url[1024];
} Channel;

#define MAX_FILES 500
#define MAX_CHANNELS 200

// Variabili Globali
FileEntry files[MAX_FILES];
int file_count = 0;
char current_path[512] = "ux0:";

Channel channels[MAX_CHANNELS];
int channel_count = 0;

int app_state = STATE_BROWSER;

// --- FUNZIONI DI UTILITÀ ---

// Ordina file: Cartelle prima, poi file
int compare_files(const void *a, const void *b) {
    FileEntry *fa = (FileEntry *)a;
    FileEntry *fb = (FileEntry *)b;
    if (fa->is_folder && !fb->is_folder) return -1;
    if (!fa->is_folder && fb->is_folder) return 1;
    return strcasecmp(fa->name, fb->name);
}

// Scansiona la cartella corrente
void scan_directory(const char *path) {
    DIR *d;
    struct dirent *dir;
    d = opendir(path);
    file_count = 0;

    if (d) {
        while ((dir = readdir(d)) != NULL) {
            if (file_count >= MAX_FILES) break;
            
            // Ignora "." (cartella corrente)
            if (strcmp(dir->d_name, ".") == 0) continue;

            strncpy(files[file_count].name, dir->d_name, 255);
            
            // Controlla se è una cartella
            struct stat st;
            char fullpath[1024];
            snprintf(fullpath, sizeof(fullpath), "%s/%s", path, dir->d_name);
            stat(fullpath, &st);
            
            files[file_count].is_folder = S_ISDIR(st.st_mode);
            file_count++;
        }
        closedir(d);
        
        // Ordina la lista per pulizia
        qsort(files, file_count, sizeof(FileEntry), compare_files);
    }
}

// Parsing M3U
void remove_newline(char *str) {
    int len = strlen(str);
    if (len > 0 && (str[len-1] == '\n' || str[len-1] == '\r')) str[len-1] = '\0';
}

void load_playlist(const char *filepath) {
    FILE *fp = fopen(filepath, "r");
    if (!fp) return;

    channel_count = 0;
    char line[2048];
    char temp_name[256] = "Canale Senza Nome";

    while (fgets(line, sizeof(line), fp)) {
        remove_newline(line);
        if (channel_count >= MAX_CHANNELS) break;

        if (strncmp(line, "#EXTINF", 7) == 0) {
            char *comma = strchr(line, ',');
            if (comma) snprintf(temp_name, sizeof(temp_name), "%s", comma + 1);
        } else if (strncmp(line, "http", 4) == 0 || strncmp(line, "rtmp", 4) == 0) {
            strncpy(channels[channel_count].name, temp_name, 255);
            strncpy(channels[channel_count].url, line, 1023);
            channel_count++;
            snprintf(temp_name, sizeof(temp_name), "Canale Senza Nome");
        }
    }
    fclose(fp);
}

// Verifica se è un file supportato (.m3u o .m3u8)
int is_playlist_file(const char *name) {
    const char *dot = strrchr(name, '.');
    if (!dot) return 0;
    return (strcasecmp(dot, ".m3u") == 0 || strcasecmp(dot, ".m3u8") == 0);
}

// --- MAIN ---
int main() {
    sceSysmoduleLoadModule(SCE_SYSMODULE_NET);
    
    vita2d_init();
    vita2d_set_clear_color(0xFF101010); // Grigio scuro elegante
    vita2d_pgf *pgf = vita2d_load_default_pgf();

    // Carica la root iniziale
    scan_directory(current_path);

    int selection = 0;
    int scroll = 0;
    SceCtrlData pad, old_pad;
    memset(&old_pad, 0, sizeof(old_pad));

    while (1) {
        sceCtrlPeekBufferPositive(0, &pad, 1);

        // --- CONTROLLI COMUNI ---
        if ((pad.buttons & SCE_CTRL_UP) && !(old_pad.buttons & SCE_CTRL_UP)) {
            if (selection > 0) selection--;
        }
        if ((pad.buttons & SCE_CTRL_DOWN) && !(old_pad.buttons & SCE_CTRL_DOWN)) {
            int limit = (app_state == STATE_BROWSER) ? file_count : channel_count;
            if (selection < limit - 1) selection++;
        }
        
        // Scroll automatico
        if (selection < scroll) scroll = selection;
        if (selection > scroll + 10) scroll = selection - 10;

        // --- LOGICA BROWSER ---
        if (app_state == STATE_BROWSER) {
            // Entra in cartella o apre file (X)
            if ((pad.buttons & SCE_CTRL_CROSS) && !(old_pad.buttons & SCE_CTRL_CROSS)) {
                if (file_count > 0) {
                    if (files[selection].is_folder) {
                        // Se è ".." torna indietro, altrimenti entra
                        if (strcmp(files[selection].name, "..") == 0) {
                            // Logica per tornare indietro (semplificata: reset a ux0 se complessa)
                             // Per semplicità in questo esempio supportiamo solo navigazione in avanti
                             // o implementiamo reset path con Triangolo
                        } else {
                            strcat(current_path, "/");
                            strcat(current_path, files[selection].name);
                            scan_directory(current_path);
                            selection = 0; scroll = 0;
                        }
                    } else if (is_playlist_file(files[selection].name)) {
                        // È un file M3U! Caricalo.
                        char full_file_path[1024];
                        snprintf(full_file_path, sizeof(full_file_path), "%s/%s", current_path, files[selection].name);
                        load_playlist(full_file_path);
                        
                        if (channel_count > 0) {
                            app_state = STATE_CHANNEL_LIST;
                            selection = 0; scroll = 0;
                        }
                    }
                }
            }
            
            // Torna indietro di cartella (TRIANGOLO)
            if ((pad.buttons & SCE_CTRL_TRIANGLE) && !(old_pad.buttons & SCE_CTRL_TRIANGLE)) {
                // Rimuove l'ultimo pezzo del path
                char *last_slash = strrchr(current_path, '/');
                if (last_slash && last_slash != current_path) {
                    *last_slash = '\0';
                } else {
                    // Se siamo già alla root, proviamo a switchare a uma0 se esiste, o restiamo lì
                    strcpy(current_path, "ux0:");
                }
                scan_directory(current_path);
                selection = 0; scroll = 0;
            }
        }
        
        // --- LOGICA LISTA CANALI ---
        else if (app_state == STATE_CHANNEL_LIST) {
            // Torna al browser (CERCHIO)
            if ((pad.buttons & SCE_CTRL_CIRCLE) && !(old_pad.buttons & SCE_CTRL_CIRCLE)) {
                app_state = STATE_BROWSER;
                scan_directory(current_path); // Ricarica file
                selection = 0; scroll = 0;
            }
            
            // Avvia Canale (X)
            if ((pad.buttons & SCE_CTRL_CROSS) && !(old_pad.buttons & SCE_CTRL_CROSS)) {
                if (channel_count > 0) {
                    sceAppMgrLaunchAppByUri(0x20000, channels[selection].url);
                }
            }
        }

        old_pad = pad;

        // --- DISEGNO ---
        vita2d_start_drawing();
        vita2d_clear_screen();

        if (app_state == STATE_BROWSER) {
            vita2d_pgf_draw_text(pgf, 20, 30, GREEN, 1.0f, "FILE BROWSER - Scegli lista .m3u");
            vita2d_pgf_draw_text(pgf, 20, 50, GREY, 0.7f, current_path);
            vita2d_pgf_draw_text(pgf, 500, 30, WHITE, 0.7f, "[TRIANGOLO] Indietro");

            int y = 80;
            for (int i = scroll; i < file_count && i < scroll + 12; i++) {
                uint32_t color = (i == selection) ? GREEN : WHITE;
                if (files[i].is_folder) color = (i == selection) ? YELLOW : 0xFF00AAFF; // Cartelle Gialle/Blu
                
                vita2d_pgf_draw_text(pgf, 30, y, color, 1.0f, files[i].name);
                y += 25;
            }
        } 
        else if (app_state == STATE_CHANNEL_LIST) {
            vita2d_pgf_draw_text(pgf, 20, 30, YELLOW, 1.0f, "LISTA CANALI");
            vita2d_pgf_draw_text(pgf, 500, 30, WHITE, 0.7f, "[CERCHIO] Torna ai file");

            int y = 80;
            for (int i = scroll; i < channel_count && i < scroll + 12; i++) {
                uint32_t color = (i == selection) ? GREEN : WHITE;
                vita2d_pgf_draw_text(pgf, 30, y, color, 1.0f, channels[i].name);
                
                if (i == selection) {
                   vita2d_pgf_draw_text(pgf, 40, y + 20, GREY, 0.7f, channels[i].url);
                   y += 20; 
                }
                y += 25;
            }
        }

        vita2d_end_drawing();
        vita2d_swap_buffers();
    }

    vita2d_fini();
    vita2d_free_pgf(pgf);
    sceSysmoduleUnloadModule(SCE_SYSMODULE_NET);
    sceKernelExitProcess(0);
    return 0;
}
