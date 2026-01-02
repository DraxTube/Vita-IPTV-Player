cmake_minimum_required(VERSION 3.10)

# Include essenziale per le funzioni Vita
if(DEFINED ENV{VITASDK})
    include("$ENV{VITASDK}/share/vita.cmake")
else()
    message(FATAL_ERROR "Please define VITASDK to point to your SDK path!")
endif()

project(VitaIPTV)

# Opzioni compilatore
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wl,-q -O2")

add_executable(VitaIPTV main.c)

# Elenco librerie (AGGIORNATO con le dipendenze mancanti)
target_link_libraries(VitaIPTV
    vita2d
    freetype
    png
    jpeg
    z
    m
    SceSharedFb_stub      # <--- NUOVO (Risolve sceSharedFb...)
    SceAppMgr_stub        # <--- NUOVO (Risolve sceAppMgr...)
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

# Crea il VPK
vita_create_vpk(VitaIPTV "IPTV00001" "My IPTV Player")
