cmake_minimum_required(VERSION 3.10)

if(DEFINED ENV{VITASDK})
    include("$ENV{VITASDK}/share/vita.cmake")
else()
    message(FATAL_ERROR "Please define VITASDK to point to your SDK path!")
endif()

project(VitaIPTV)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wl,-q -O3") # O3 per ottimizzare la velocità video

add_executable(VitaIPTV main.c)

target_link_libraries(VitaIPTV
    vita2d
    freetype
    png
    jpeg
    z
    m
    SceAppMgr_stub
    ScePgf_stub
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
)

set(VITA_APP_TITLE "IPTV Player Native")
vita_create_vpk(VitaIPTV "IPTV00001" "IPTV_Native")
