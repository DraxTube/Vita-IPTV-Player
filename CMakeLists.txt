cmake_minimum_required(VERSION 3.10)

project(VitaIPTV)

# Opzioni di compilazione per PS Vita
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wl,-q")

# Trova VitaSDK
if(NOT DEFINED ENV{VITASDK})
    message(FATAL_ERROR "Please define VITASDK to point to your SDK path!")
endif()

# Librerie e sorgenti
add_executable(VitaIPTV main.c)

# Link delle librerie (Grafica, Sistema, Audio, Video, Rete)
target_link_libraries(VitaIPTV
    vita2d
    ScePgf_stub
    SceGxm_stub
    SceDisplay_stub
    SceCtrl_stub
    SceSysmodule_stub
    SceNet_stub
    SceAvPlayer_stub
    SceCommonDialog_stub
    m
)

# Crea il file VPK
vita_create_vpk(VitaIPTV "IPTV00001" "My IPTV Player" "eboot.bin")
