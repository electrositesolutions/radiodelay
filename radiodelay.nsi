;NSIS Modern User Interface version 1.67
;Welcome/Finish Page Example Script
SetCompressor /solid lzma
Unicode true
;--------------------------------
;Include Modern UI

  !include "MUI.nsh"

;--------------------------------
;Configuration

  ;General
  Name "Virtualdelay"
  OutFile "VirtualDelay-Windows-x64-Setup.exe"

  ;Folder selection page
  InstallDir "$PROGRAMFILES64\Virtualdelay"
  
  ;Get install folder from registry if available
;  InstallDirRegKey HKCU "Software\Virtualdelay" ""

;--------------------------------
;Interface Settings

  !define MUI_ABORTWARNING

;--------------------------------
;Pages

  !insertmacro MUI_PAGE_WELCOME
  !insertmacro MUI_PAGE_LICENSE "COPYING"
 ; !insertmacro MUI_PAGE_COMPONENTS
  !insertmacro MUI_PAGE_DIRECTORY
  !insertmacro MUI_PAGE_INSTFILES
  !define MUI_FINISHPAGE_SHOWREADME "$INSTDIR\README.md"
  !define MUI_FINISHPAGE_RUN "$INSTDIR\Virtualdelay.exe"
  !insertmacro MUI_PAGE_FINISH
  
  !insertmacro MUI_UNPAGE_WELCOME
  !insertmacro MUI_UNPAGE_CONFIRM
  !insertmacro MUI_UNPAGE_INSTFILES
  !insertmacro MUI_UNPAGE_FINISH
  
;--------------------------------
;Languages
 
  !insertmacro MUI_LANGUAGE "English"

;--------------------------------
;Installer Sections

Function .onInit
  FindWindow $0 "" "Virtualdelay"
  StrCmp $0 0 continueinstall
  SendMessage $0 16 0 0
continueinstall:
FunctionEnd

Section "Virtualdelay.exe" SecVirtualdelay
  SetOutPath "$INSTDIR"
  
  ;ADD YOUR OWN STUFF HERE!
 
  
 ; add files / whatever that need to be installed here.
  File "Virtualdelay.exe"
  File "README.md"

  WriteRegStr HKEY_LOCAL_MACHINE "SOFTWARE\KeanSystems\Virtualodelay" "" "$INSTDIR"
  WriteRegStr HKEY_LOCAL_MACHINE "Software\Microsoft\Windows\CurrentVersion\Uninstall\Virtualdelay" "DisplayName" "Virtualdelay (remove only)"
  WriteRegStr HKEY_LOCAL_MACHINE "Software\Microsoft\Windows\CurrentVersion\Uninstall\Virtualdelay" "UninstallString" '"$INSTDIR\uninst.exe"'
  ; write out uninstaller
  WriteUninstaller "$INSTDIR\uninst.exe"

SectionEnd

;--------------------------------
;Descriptions

 
;--------------------------------
;Uninstaller Section


; optional section
Section "Start Menu Shortcuts"
  CreateDirectory "$SMPROGRAMS\Virtualdelay"
  CreateShortCut "$SMPROGRAMS\Virtualdelay\Virtualdelay.lnk" "$INSTDIR\Virtualdelay.exe" "" "$INSTDIR\Virtualdelay.exe" 0
  CreateShortCut "$SMPROGRAMS\Virtualdelay\README.md.lnk" "$INSTDIR\README.md" "" "$INSTDIR\README.md" 0
  CreateShortCut "$SMPROGRAMS\Virtualdelay\Uninstall.lnk" "$INSTDIR\uninst.exe" "" "$INSTDIR\uninst.exe" 0
  CreateShortCut "$DESKTOP\Virtualdelay.lnk" "$INSTDIR\Virtualdelay.exe" "" "$INSTDIR\Virtualdelay.exe"
SectionEnd

Section "Uninstall"

  ;ADD YOUR OWN STUFF HERE!


; add delete commands to delete whatever files/registry keys/etc you installed here.
Delete "$INSTDIR\uninst.exe"
Delete "$INSTDIR\Virtualdelay.exe"
Delete "$INSTDIR\README.md"

DeleteRegKey HKEY_LOCAL_MACHINE "SOFTWARE\KeanSystems\Virtualdelay"
DeleteRegKey HKEY_LOCAL_MACHINE "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\Virtualdelay"
; remove shortcuts, if any.
Delete "$DESKTOP\Virtualdelay.lnk"
Delete "$SMPROGRAMS\Virtualdelay\*.*"
; remove directories used.
RMDir "$SMPROGRAMS\Virtualdelay"
RMDir "$INSTDIR"

SectionEnd  