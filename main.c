#include <psp2/kernel/process.h>
#include <psp2/ctrl.h>
#include <psp2/sysmodule.h>
#include <vita2d.h>

// Colori
#define WHITE 0xFFFFFFFF
#define GREEN 0xFF00FF00
#define RED   0xFF0000FF

int main() {
    // Carichiamo i moduli necessari (Rete e Video)
    sceSysmoduleLoadModule(SCE_SYSMODULE_NET);
    sceSysmoduleLoadModule(SCE_SYSMODULE_AVPLAYER);

    // Inizializza la grafica 2D
    vita2d_init();
    vita2d_set_clear_color(0xFF000000); // Sfondo nero

    // Prepara i font
    vita2d_pgf *pgf = vita2d_load_default_pgf();

    // Loop principale dell'app
    while (1) {
        // Legge i tasti
        SceCtrlData pad;
        sceCtrlPeekBufferPositive(0, &pad, 1);

        // Se premi START, esci
        if (pad.buttons & SCE_CTRL_START)
            break;

        // Inizio disegno
        vita2d_start_drawing();
        vita2d_clear_screen();

        // Disegna testo a schermo
        vita2d_pgf_draw_text(pgf, 20, 40, GREEN, 1.0f, "PS Vita IPTV Player - Template");
        vita2d_pgf_draw_text(pgf, 20, 80, WHITE, 1.0f, "Premi X per caricare (Simulazione)");
        vita2d_pgf_draw_text(pgf, 20, 100, WHITE, 1.0f, "Premi START per uscire");

        if (pad.buttons & SCE_CTRL_CROSS) {
            vita2d_pgf_draw_text(pgf, 20, 150, RED, 1.0f, "Tentativo connessione stream...");
            // Qui andrebbe inserita la logica di SceAvPlayer
        }

        vita2d_end_drawing();
        vita2d_swap_buffers();
    }

    // Pulisce la memoria prima di uscire
    vita2d_fini();
    vita2d_free_pgf(pgf);
    sceSysmoduleUnloadModule(SCE_SYSMODULE_NET);
    sceSysmoduleUnloadModule(SCE_SYSMODULE_AVPLAYER);
    sceKernelExitProcess(0);
    return 0;
}
