cmake_minimum_required(VERSION 3.10)

# --- QUESTA È LA PARTE CHE MANCAVA ---
# Carica le funzioni speciali (come vita_create_vpk)
if(DEFINED ENV{VITASDK})
    include("$ENV{VITASDK}/share/vita.cmake")
else()
    message(FATAL_ERROR "Please define VITASDK to point to your SDK path!")
endif()
# -------------------------------------

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

# Ora questo comando verrà riconosciuto
vita_create_vpk(VitaIPTV "IPTV00001" "My IPTV Player")
