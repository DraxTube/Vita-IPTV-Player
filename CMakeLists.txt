cmake_minimum_required(VERSION 3.10)

set(VITA_TITLE_ID "VITAIPTV1")
project(VitaIPTV)

# Includi le librerie del VitaSDK
include("${VITASDK}/share/vita.cmake")

# Sorgenti
add_executable(VitaIPTV main.c)

# LINKER FIX: Utilizzo dei nomi corretti delle librerie stub del VitaSDK
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
    SceLibFont_stub
    SceNet_stub
    SceNetCtl_stub
    SceAvPlayer_stub
    z
    m
)

# Creazione file .self
vita_create_self(VitaIPTV.self VitaIPTV)

# Creazione VPK (Rimosso ogni riferimento errato a target 'IPTV')
vita_create_vpk(VitaIPTV.vpk ${VITA_TITLE_ID} VitaIPTV.self
    NAME "Vita IPTV Player"
)
