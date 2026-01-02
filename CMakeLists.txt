cmake_minimum_required(VERSION 3.10)

# Carica il toolchain Vita
if(DEFINED ENV{VITASDK})
    include("$ENV{VITASDK}/share/vita.cmake")
else()
    message(FATAL_ERROR "Please define VITASDK to point to your SDK path!")
endif()

project(VitaIPTV)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wl,-q -O2")

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
    SceAvPlayer_stub
    SceCommonDialog_stub
    SceAudio_stub
)

# FIX DEFINITIVO:
# Usiamo un nome corto "IPTV" senza spazi o caratteri strani
# E lo passiamo come variabile stringa.
set(VITA_APP_TITLE "IPTV Player")
vita_create_vpk(VitaIPTV "IPTV00001" "IPTV")
