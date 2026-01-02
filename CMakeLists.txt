cmake_minimum_required(VERSION 3.10)

set(VITA_TITLE_ID "IPTV00001")
project(VitaIPTV)

# Includi le utility del VitaSDK
if(DEFINED ENV{VITASDK})
    include("$ENV{VITASDK}/share/vita.cmake")
else()
    message(FATAL_ERROR "VITASDK non trovato!")
endif()

# Sorgenti
add_executable(VitaIPTV main.c)

# Librerie Stub
target_link_libraries(VitaIPTV
    vita2d
    SceAppMgr_stub
    SceGxm_stub
    SceDisplay_stub
    SceCtrl_stub
    SceSysmodule_stub
    SceCommonDialog_stub
    ScePgf_stub
    SceNet_stub
    SceNetCtl_stub
    SceAvPlayer_stub
    SceAudio_stub
    SceLibKernel_stub
    z
    m
)

# Creazione pacchetto VPK
vita_create_self(VitaIPTV.self VitaIPTV)
vita_create_vpk(VitaIPTV.vpk ${VITA_TITLE_ID} VitaIPTV.self
    NAME "Vita IPTV Player"
)
