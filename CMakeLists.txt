cmake_minimum_required(VERSION 3.10)

# Sostituisci 'IPTV' con il Title ID univoco del tuo progetto (9 caratteri)
set(VITA_TITLE_ID "VITAIPTV1")
project(VitaIPTV)

# Includi le librerie del VitaSDK
include("${VITASDK}/share/vita.cmake")

# Aggiungi i file sorgente (assicurati che main.c sia nella root o cambia il percorso)
add_executable(VitaIPTV main.c)

# Link delle librerie necessarie (es. GXM, Net, Audio se le usi)
target_link_libraries(VitaIPTV
    vita2d
    z
    m
    SceGxm_stub
    SceDisplay_stub
    SceCtrl_stub
    SceSysmodule_stub
    SceCommonDialog_stub
    SceNet_stub
    SceNetCtl_stub
)

# 1. Crea il file .self (necessario per il VPK)
vita_create_self(VitaIPTV.self VitaIPTV)

# 2. Crea il file .vpk
# NOTA: Rimosso il riferimento errato a 'IPTV' che causava il crash.
# Se hai icone o cartelle da aggiungere, usa: FILE percorso/file destinazione
vita_create_vpk(VitaIPTV.vpk ${VITA_TITLE_ID} VitaIPTV.self
    NAME "Vita IPTV Player"
    # Se hai una cartella di risorse chiamata 'assets', decommenta la riga sotto:
    # FILE assets sce_sys/icon0.png
)
