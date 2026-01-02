cmake_minimum_required(VERSION 3.10)

set(VITA_TITLE_ID "IPTV00001")
project(VitaIPTV)

include("${VITASDK}/share/vita.cmake")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wl,-q -O3")

add_executable(VitaIPTV main.c)

target_link_libraries(VitaIPTV
    vita2d
    SceAppMgr_stub
    SceGxm_stub
    SceDisplay_stub
    SceCtrl_stub
    SceSysmodule_stub
    SceNet_stub
    SceNetCtl_stub
    SceAvPlayer_stub
    SceAudio_stub
    SceVideodec_stub
    SceCommonDialog_stub
    ScePgf_stub
    SceLibKernel_stub
    SceAppUtil_stub
    SceSsl_stub    # Necessario per la crittografia
    SceHttp_stub   # <--- CORRETTO (Gestisce sia HTTP che HTTPS)
    z
    m
)

vita_create_self(VitaIPTV.self VitaIPTV)
vita_create_vpk(VitaIPTV.vpk ${VITA_TITLE_ID} VitaIPTV.self
    NAME "Vita IPTV HD Player"
)
