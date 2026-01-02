cmake_minimum_required(VERSION 3.10)

project(VitaIPTV)

# Verifica la presenza dell'SDK
if(NOT DEFINED ENV{VITASDK})
    message(FATAL_ERROR "Please define VITASDK to point to your SDK path!")
endif()

# Flag di compilazione essenziali
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wl,-q -O2")

# Definiamo l'eseguibile
add_executable(VitaIPTV main.c)

# Qui sta la correzione: Aggiunte le dipendenze mancanti (freetype, png, jpeg, z)
# L'ordine è importante!
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

# Creazione del VPK (Sintassi corretta e semplificata)
vita_create_vpk(VitaIPTV "IPTV00001" "My IPTV Player")
