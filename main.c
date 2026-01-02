#include <psp2/kernel/processmgr.h>
#include <psp2/ctrl.h>
#include <psp2/sysmodule.h>
#include <psp2/net/net.h>
#include <vita2d.h>
#include <stdlib.h>
#include <string.h>

// --- HACK PER IL LINKER ---
// Creiamo funzioni vuote per ingannare la libreria vita2d
// che cerca funzioni non presenti nel toolchain standard.
int sceSharedFbClose() { return 0; }
int sceSharedFbOpen() { return 0; }
int sceSharedFbGetInfo() { return 0; }
int sceSharedFbEnd() { return 0; }
int sceSharedFbBegin() { return 0; }
int _sceSharedFbOpen() { return 0; }
// --------------------------

// Colori
#define WHITE 0xFFFFFFFF
#define GREEN 0xFF00FF00
#define RED   0xFF0000FF

int main() {
    // Carichiamo moduli
    sceSysmoduleLoadModule(SCE_SYSMODULE_NET);
    sceSysmoduleLoadModule(SCE_SYSMODULE_AVPLAYER);

    // Inizializza la grafica
    vita2d_init();
    vita2d_set_clear_color(0xFF000000); 

    vita2d_pgf *pgf = vita2d_load_default_pgf();

    while (1) {
        SceCtrlData pad;
        sceCtrlPeekBufferPositive(0, &pad, 1);

        if (pad.buttons & SCE_CTRL_START)
            break;

        vita2d_start_drawing();
        vita2d_clear_screen();

        vita2d_pgf_draw_text(pgf, 20, 40, GREEN, 1.0f, "PS Vita IPTV Player - SUCCESS!");
        vita2d_pgf_draw_text(pgf, 20, 80, WHITE, 1.0f, "Se leggi questo, il VPK funziona.");
        vita2d_pgf_draw_text(pgf, 20, 100, WHITE, 1.0f, "Premi START per uscire");

        vita2d_end_drawing();
        vita2d_swap_buffers();
    }

    vita2d_fini();
    vita2d_free_pgf(pgf);
    sceSysmoduleUnloadModule(SCE_SYSMODULE_NET);
    sceSysmoduleUnloadModule(SCE_SYSMODULE_AVPLAYER);
    
    sceKernelExitProcess(0);
    return 0;
}
