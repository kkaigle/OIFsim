; =============================================================================
; Iraq Theater Command — Windows NSIS Installer Script
; Builds IraqTheaterCommand-Windows-Setup.exe
; =============================================================================
!include "MUI2.nsh"

Name "Iraq Theater Command"
OutFile "..\dist\IraqTheaterCommand-Windows-Setup.exe"
InstallDir "$PROGRAMFILES64\Iraq Theater Command"
InstallDirRegKey HKLM "Software\IraqTheaterCommand" "Install_Dir"
RequestExecutionLevel admin
Unicode True
SetCompressor /SOLID lzma

!define MUI_ABORTWARNING
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!define MUI_FINISHPAGE_RUN "$INSTDIR\IraqTheaterCommand.exe"
!define MUI_FINISHPAGE_RUN_TEXT "Launch Iraq Theater Command"
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"

Section "Iraq Theater Command (Core Engine & Assets)" SecCore
    SectionIn RO
    SetOutPath "$INSTDIR"

    ; Main C++20 Executable
    File "..\dist\win_stage\IraqTheaterCommand.exe"

    ; Native Map, Elevation, 251k+ OSM Buildings, Streets & 3D .glb Models
    SetOutPath "$INSTDIR\assets"
    File /r "..\assets\*.*"

    SetOutPath "$INSTDIR"
    WriteRegStr HKLM "Software\IraqTheaterCommand" "Install_Dir" "$INSTDIR"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\IraqTheaterCommand" "DisplayName" "Iraq Theater Command"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\IraqTheaterCommand" "UninstallString" '"$INSTDIR\Uninstall.exe"'
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\IraqTheaterCommand" "DisplayIcon" '"$INSTDIR\IraqTheaterCommand.exe"'
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\IraqTheaterCommand" "Publisher" "OIFsim"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\IraqTheaterCommand" "DisplayVersion" "0.7.0"
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\IraqTheaterCommand" "NoModify" 1
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\IraqTheaterCommand" "NoRepair" 1
    WriteUninstaller "$INSTDIR\Uninstall.exe"

    ; Start Menu & Desktop Shortcuts
    CreateDirectory "$SMPROGRAMS\Iraq Theater Command"
    CreateShortcut "$SMPROGRAMS\Iraq Theater Command\Iraq Theater Command.lnk" "$INSTDIR\IraqTheaterCommand.exe" "" "$INSTDIR\IraqTheaterCommand.exe" 0
    CreateShortcut "$SMPROGRAMS\Iraq Theater Command\Uninstall Iraq Theater Command.lnk" "$INSTDIR\Uninstall.exe"
    CreateShortcut "$DESKTOP\Iraq Theater Command.lnk" "$INSTDIR\IraqTheaterCommand.exe" "" "$INSTDIR\IraqTheaterCommand.exe" 0
SectionEnd

Section "Uninstall"
    Delete "$DESKTOP\Iraq Theater Command.lnk"
    Delete "$SMPROGRAMS\Iraq Theater Command\*.*"
    RMDir "$SMPROGRAMS\Iraq Theater Command"

    RMDir /r "$INSTDIR\assets"
    Delete "$INSTDIR\IraqTheaterCommand.exe"
    Delete "$INSTDIR\Uninstall.exe"
    RMDir "$INSTDIR"

    DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\IraqTheaterCommand"
    DeleteRegKey HKLM "Software\IraqTheaterCommand"
SectionEnd

