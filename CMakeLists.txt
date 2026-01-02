cmake_minimum_required(VERSION 3.10)

set(VITA_TITLE_ID "VITAIPTV1")
project(VitaIPTV)

# Includi le utility del VitaSDK
include("${VITASDK}/share/vita.cmake")

# Sorgenti
add_executable(VitaIPTV main.c)

# LINKER FIX: Rimossa SceFont_stub che causava l'errore.
# ScePgf_stub è solitamente sufficiente per il testo in vita2d.
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
    SceAppUtil_stub
    SceLibKernel_stub
    z
    m
)

# Creazione file .self
vita_create_self(VitaIPTV.self VitaIPTV)

# Creazione VPK finale
# Assicurati che non ci siano parametri extra come "IPTV" qui.
vita_create_vpk(VitaIPTV.vpk ${VITA_TITLE_ID} VitaIPTV.self
    NAME "Vita IPTV Player"
)
