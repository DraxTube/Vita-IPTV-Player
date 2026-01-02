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

// --- HACK LINKER ---
int sceSharedFbClose() { return 0; }
int sceSharedFbOpen() { return 0; }
int sceSharedFbGetInfo() { return 0; }
int sceSharedFbEnd() { return 0; }
int sceSharedFbBegin() { return 0; }
int _sceSharedFbOpen() { return 0; }

// Callback memoria AvPlayer
void *av_malloc(void *unused, unsigned int alignment, unsigned int size) { return malloc(size); }
void av_free(void *unused, void *ptr) { free(ptr); }

// Variabili globali
SceAvPlayerHandle movie_player = -1;
int audio_port = -1;
int is_playing = 0;
vita2d_texture *video_texture = NULL;
uint8_t *rgba_buffer = NULL;
SceUID audio_thread_id = -1;
int run_audio = 0;

// Conversione ottimizzata per 1080p
void nv12_to_rgba_fast(uint8_t *y_base, uint8_t *uv_base, int width, int height, uint8_t *rgba) {
    for (int j = 0; j < height; j++) {
        int y_row = j * width;
        int uv_row = (j >> 1) * width;
        for (int i = 0; i < width; i++) {
            int y = y_base[y_row + i];
            int u = uv_base[uv_row + (i & ~1)];
            int v = uv_base[uv_row + (i & ~1) + 1];
            
            int c = y - 16, d = u - 128, e = v - 128;
            int r = (298 * c + 409 * e + 128) >> 8;
            int g = (298 * c - 100 * d - 208 * e + 128) >> 8;
            int b = (298 * c + 516 * d + 128) >> 8;
            
            int pos = (y_row + i) << 2;
            rgba[pos] = (uint8_t)(r < 0 ? 0 : (r > 255 ? 255 : r));
            rgba[pos+1] = (uint8_t)(g < 0 ? 0 : (g > 255 ? 255 : g));
            rgba[pos+2] = (uint8_t)(b < 0 ? 0 : (b > 255 ? 255 : b));
            rgba[pos+3] = 255;
        }
    }
}

// Gestione Audio
int audio_thread(SceSize args, void *argp) {
    SceAvPlayerFrameInfo audio_info;
    while (run_audio) {
        if (is_playing && sceAvPlayerGetAudioData(movie_player, &audio_info)) {
            sceAudioOutOutput(audio_port, audio_info.pData);
        } else {
            sceKernelDelayThread(1000);
        }
    }
    return 0;
}

void start_video(const char *url) {
    SceAvPlayerInitData playerInit;
    memset(&playerInit, 0, sizeof(playerInit));
    playerInit.memoryReplacement.allocate = av_malloc;
    playerInit.memoryReplacement.deallocate = av_free;
    playerInit.memoryReplacement.allocateTexture = av_malloc;
    playerInit.memoryReplacement.deallocateTexture = av_free;
    playerInit.basePriority = 160; // Priorità alta per 1080p

    movie_player = sceAvPlayerInit(&playerInit);
    sceAvPlayerAddSource(movie_player, url);
    
    audio_port = sceAudioOutOpenPort(SCE_AUDIO_OUT_PORT_TYPE_MAIN, 1024, 48000, SCE_AUDIO_OUT_MODE_STEREO);
    run_audio = 1;
    audio_thread_id = sceKernelCreateThread("AudioThread", audio_thread, 0x10000100, 0x10000, 0, 0, NULL);
    sceKernelStartThread(audio_thread_id, 0, NULL);
    
    sceAvPlayerStart(movie_player);
    is_playing = 1;
}

int main(void) {
    sceSysmoduleLoadModule(SCE_SYSMODULE_NET);
    sceSysmoduleLoadModule(SCE_SYSMODULE_AVPLAYER);
    
    // Inizializza Rete (Necessario per m3u8/URL)
    SceNetInitParam netParam;
    netParam.memory = malloc(1024*1024);
    netParam.size = 1024*1024;
    sceNetInit(&netParam);
    sceNetCtlInit();

    vita2d_init();
    vita2d_pgf *pgf = vita2d_load_default_pgf();

    // TEST: Avvia uno stream m3u8 (Sostituisci con un link reale se vuoi)
    // start_video("http://tuo_link_iptv.m3u8");

    while (1) {
        SceCtrlData pad;
        sceCtrlPeekBufferPositive(0, &pad, 1);
        if (pad.buttons & SCE_CTRL_START) break;

        if (is_playing) {
            SceAvPlayerFrameInfo video_info;
            if (sceAvPlayerGetVideoData(movie_player, &video_info)) {
                if (!video_texture) {
                    video_texture = vita2d_create_empty_texture(video_info.details.video.width, video_info.details.video.height);
                    rgba_buffer = malloc(video_info.details.video.width * video_info.details.video.height * 4);
                }
                nv12_to_rgba_fast((uint8_t*)video_info.pData, (uint8_t*)video_info.pData + (video_info.details.video.width * video_info.details.video.height), 
                                  video_info.details.video.width, video_info.details.video.height, rgba_buffer);
                
                void *tex_data = vita2d_texture_get_datap(video_texture);
                memcpy(tex_data, rgba_buffer, video_info.details.video.width * video_info.details.video.height * 4);
            }
        }

        vita2d_start_drawing();
        vita2d_clear_screen();
        if (video_texture) {
            vita2d_draw_texture_scale(video_texture, 0, 0, 960.0f/vita2d_texture_get_width(video_texture), 544.0f/vita2d_texture_get_height(video_texture));
        } else {
            vita2d_pgf_draw_text(pgf, 20, 40, 0xFF00FF00, 1.0f, "Vita IPTV Player - In attesa di stream...");
        }
        vita2d_end_drawing();
        vita2d_swap_buffers();
    }

    sceNetCtlTerm();
    sceNetTerm();
    vita2d_fini();
    return 0;
}
