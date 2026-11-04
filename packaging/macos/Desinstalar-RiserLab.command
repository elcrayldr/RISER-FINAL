#!/bin/bash
# Desinstala RiserLab (VST3 y AU). Pide la contraseña de administrador con el diálogo normal de macOS.
VST3="/Library/Audio/Plug-Ins/VST3/RiserLab.vst3"
AU="/Library/Audio/Plug-Ins/Components/RiserLab.component"
BK="/Library/Application Support/RiserLab"

/usr/bin/osascript <<OSA
do shell script "rm -rf '$VST3' '$AU' '$BK'; pkgutil --forget com.crayldr.riserlab.pkg >/dev/null 2>&1; killall -9 AudioComponentRegistrar >/dev/null 2>&1; exit 0" with administrator privileges
display dialog "RiserLab se ha desinstalado. Reinicia tu DAW." buttons {"OK"} default button "OK"
OSA
