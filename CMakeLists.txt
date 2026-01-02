cmake_minimum_required(VERSION 3.10)

set(VITA_TITLE_ID "VITAIPTV1")
project(VitaIPTV)

# Includi le utility del VitaSDK
include("${VITASDK}/share/vita.cmake")

# Sorgenti
add_executable(VitaIPTV main.c)

# LINKER FIX: Aggiunte le librerie di sistema mancanti
target_link_libraries(VitaIPTV
    vita2d
    SceLibKernel_stub
    SceAppMgr_stub
    SceAppUtil_stub
    SceGxm_stub
    SceDisplay_stub
    SceCtrl_stub
    SceSysmodule_stub
    SceCommonDialog_stub
    ScePgf_stub
    SceFont_stub
    SceNet_stub
    SceNetCtl_stub
    SceAvPlayer_stub
    z
    m
)

# Creazione file eseguibile per Vita
vita_create_self(VitaIPTV.self VitaIPTV)

# Creazione pacchetto VPK finale
vita_create_vpk(VitaIPTV.vpk ${VITA_TITLE_ID} VitaIPTV.self
    NAME "Vita IPTV Player"
)
