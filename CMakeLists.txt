cmake_minimum_required(VERSION 3.10)

set(VITA_TITLE_ID "VITAIPTV1")
project(VitaIPTV)

# Includi le utility del VitaSDK [cite: 1]
include("${VITASDK}/share/vita.cmake")

# Sorgenti [cite: 1]
add_executable(VitaIPTV main.c)

# LINKER FIX: Utilizzo dei nomi corretti e universali per il toolchain
target_link_libraries(VitaIPTV
    vita2d
    SceAppMgr_stub
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
    SceAppUtil_stub
    SceLibKernel_stub
    z
    m
)

# Creazione file .self [cite: 1]
vita_create_self(VitaIPTV.self VitaIPTV)

# Creazione VPK finale [cite: 2, 3]
vita_create_vpk(VitaIPTV.vpk ${VITA_TITLE_ID} VitaIPTV.self
    NAME "Vita IPTV Player"
)
