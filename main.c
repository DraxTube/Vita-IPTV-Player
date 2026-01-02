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

// --- CALLBACK MEMORIA PER AVPLAYER ---
void *av_malloc(void *unused, unsigned int alignment, unsigned int size) { return malloc(size); }
void av_free(void *unused, void *ptr) { free(ptr); }

#define WHITE 0xFFFFFFFF
#define GREEN 0xFF00FF00
#define RED   0xFF0000FF
#define GREY  0xFFAAAAAA
#define YELLOW 0xFF00FFFF

enum { STATE_BROWSER, STATE_PLAYING };
int app_state = STATE_BROWSER;

typedef struct { char name[256]; int is_folder; } FileEntry;
FileEntry files[500];
int file_count = 0;
char current_path[512] = "ux0:";
char selected_url[1024];

SceAvPlayerHandle movie_player = -1;
int audio_port = -1;
int is_playing = 0;
vita2d_texture *video_texture = NULL;
uint8_t *rgba_buffer = NULL;
SceUID audio_thread_id = -1;
int run_audio = 0;

void nv12_to_rgba(uint8_t *y_base, uint8_t *uv_base, int width, int height, uint8_t *rgba) {
    for (int j = 0; j < height; j++) {
        for (int i = 0; i < width; i++) {
            int y = y_base[j * width + i];
            int u = uv_base[(j / 2) * width + (i & ~1)];
            int v = uv_base[(j / 2) * width + (i & ~1) + 1];
            int c = y - 16, d = u - 128, e = v - 128;
            int r = (298 * c + 409 * e + 128) >> 8;
            int g = (298 * c - 100 * d - 208 * e + 128) >> 8;
            int b = (298 * c + 516 * d + 128) >> 8;
            int pos = (j * width + i) * 4;
            rgba[pos] = (r<0)?0:(r>255)?255:r;
            rgba[pos+1] = (g<0)?0:(g>255)?255:g;
            rgba[pos+2] = (b<0)?0:(b>255)?255:b;
            rgba[pos+3] = 255;
        }
    }
}

int audio_thread(SceSize args, void *argp) {
    SceAvPlayerFrameInfo audio_info;
    while (run_audio) {
        if (is_playing && sceAvPlayerGetAudioData(movie_player, &audio_info)) {
            sceAudioOutOutput(audio_port, audio_info.pData);
        } else {
            sceKernelDelayThread(10000);
        }
    }
    return 0;
}

void stop_video() {
    run_audio = 0; is_playing = 0;
    if (audio_thread_id >= 0) { sceKernelWaitThreadEnd(audio_thread_id, NULL, NULL); sceKernelDeleteThread(audio_thread_id); audio_thread_id = -1; }
    if (movie_player >= 0) { sceAvPlayerStop(movie_player); sceAvPlayerClose(movie_player); movie_player = -1; }
    if (audio_port >= 0) { sceAudioOutReleasePort(audio_port); audio_port = -1; }
    if (video_texture) { vita2d_free_texture(video_texture); video_texture = NULL; }
    if (rgba_buffer) { free(rgba_buffer); rgba_buffer = NULL; }
}

void start_video(const char *url) {
    stop_video();
    SceAvPlayerInitData playerInit;
    memset(&playerInit, 0, sizeof(playerInit));
    playerInit.memoryReplacement.allocate = av_malloc;
    playerInit.memoryReplacement.deallocate = av_free;
    playerInit.memoryReplacement.allocateTexture = av_malloc;
    playerInit.memoryReplacement.deallocateTexture = av_free;
    playerInit.basePriority = 100;

    movie_player = sceAvPlayerInit(&playerInit);
    sceAvPlayerAddSource(movie_player, url);
    audio_port = sceAudioOutOpenPort(SCE_AUDIO_OUT_PORT_TYPE_MAIN, 1024, 48000, SCE_AUDIO_OUT_MODE_STEREO);
    run_audio = 1;
    audio_thread_id = sceKernelCreateThread("AudioThread", audio_thread, 0x10000100, 0x10000, 0, 0, NULL);
    sceKernelStartThread(audio_thread_id, 0, NULL);
    sceAvPlayerStart(movie_player);
    is_playing = 1;
}

void update_video_frame() {
    if (!is_playing || movie_player < 0) return;
    SceAvPlayerFrameInfo video_info;
    if (sceAvPlayerGetVideoData(movie_player, &video_info)) {
        if (!video_texture) {
            video_texture = vita2d_create_empty_texture(video_info.details.video.width, video_info.details.video.height);
            rgba_buffer = malloc(video_info.details.video.width * video_info.details.video.height * 4);
        }
        nv12_to_rgba((uint8_t*)video_info.pData, (uint8_t*)video_info.pData + (video_info.details.video.width * video_info.details.video.height), 
                     video_info.details.video.width, video_info.details.video.height, rgba_buffer);
        void *tex_data = vita2d_texture_get_datap(video_texture);
        memcpy(tex_data, rgba_buffer, video_info.details.video.width * video_info.details.video.height * 4);
    }
}

// ... (Resto delle funzioni scan_directory e main rimangono simili) ...
// Assicurati solo che nel main chiami start_video(selected_url) correttamente
