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

// --- HACK PER LINKER ---
int sceSharedFbClose() { return 0; }
int sceSharedFbOpen() { return 0; }
int sceSharedFbGetInfo() { return 0; }
int sceSharedFbEnd() { return 0; }
int sceSharedFbBegin() { return 0; }
int _sceSharedFbOpen() { return 0; }

// --- CALLBACK MEMORIA PER AVPLAYER ---
void *av_malloc(void *unused, unsigned int alignment, unsigned int size) { return malloc(size); }
void av_free(void *unused, void *ptr) { free(ptr); }

int main(void) {
    // Inizializzazione moduli
    sceSysmoduleLoadModule(SCE_SYSMODULE_NET);
    sceSysmoduleLoadModule(SCE_SYSMODULE_AVPLAYER);
    
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

        vita2d_pgf_draw_text(pgf, 20, 40, 0xFF00FF00, 1.0f, "Vita IPTV Player Native");
        vita2d_pgf_draw_text(pgf, 20, 80, 0xFFFFFFFF, 1.0f, "Compilazione completata con successo!");
        vita2d_pgf_draw_text(pgf, 20, 120, 0xFFFFFFFF, 1.0f, "Premi START per uscire");

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
