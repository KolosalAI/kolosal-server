; Kolosal Server NSIS Installer Script
; This script creates a Windows installer for Kolosal Server
; Build with: makensis script.nsi

;--------------------------------
; Includes

!include "MUI2.nsh"
!include "x64.nsh"
!include "FileFunc.nsh"
!include "LogicLib.nsh"
!include "StrFunc.nsh"

; Initialize string functions
${StrStr}
${StrRep}

;--------------------------------
; Conditional Compilation

; Check if static files exist and define flag accordingly
!if /FileExists "..\build\Release\static\kolosal-product\index.html"
  !define INCLUDE_STATIC_FILES
!endif

;--------------------------------
; Define Variables

; PROGRAMDATA for Windows (usually C:\ProgramData)
Var PROGRAMDATA

;--------------------------------
; General Configuration

; Application name and version
!define PRODUCT_NAME "Kolosal Server"
!define PRODUCT_VERSION "1.0.0"
!define PRODUCT_PUBLISHER "Kolosal AI"
!define PRODUCT_WEB_SITE "https://github.com/KolosalAI/kolosal-server"
!define PRODUCT_DIR_REGKEY "Software\Microsoft\Windows\CurrentVersion\App Paths\kolosal-server.exe"
!define PRODUCT_UNINST_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}"
!define PRODUCT_UNINST_ROOT_KEY "HKLM"

; Installer name
Name "${PRODUCT_NAME} ${PRODUCT_VERSION}"
OutFile "kolosal-server-installer-${PRODUCT_VERSION}.exe"

; Default installation directory
InstallDir "$PROGRAMFILES64\Kolosal Server"
InstallDirRegKey HKLM "${PRODUCT_DIR_REGKEY}" ""

; Request admin privileges
RequestExecutionLevel admin

; Compression
SetCompressor /SOLID lzma
SetCompressorDictSize 32

;--------------------------------
; Interface Settings

!define MUI_ABORTWARNING
!define MUI_ICON "..\assets\icon.ico"
!define MUI_UNICON "..\assets\icon.ico"
; Note: NSIS only supports BMP format for welcome/finish images
; Uncomment and convert logo.png to logo.bmp if you want to use custom images
; !define MUI_HEADERIMAGE
; !define MUI_HEADERIMAGE_BITMAP "..\assets\logo.bmp"
; !define MUI_WELCOMEFINISHPAGE_BITMAP "..\assets\logo.bmp"

;--------------------------------
; Pages

; Installer pages
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "..\LICENSE"
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_DIRECTORY

; Custom page for configuration
Page custom ConfigPage ConfigPageLeave

!insertmacro MUI_PAGE_INSTFILES

; Finish page with options
!define MUI_FINISHPAGE_RUN
!define MUI_FINISHPAGE_RUN_TEXT "Start Kolosal Server"
!define MUI_FINISHPAGE_RUN_FUNCTION "LaunchApplication"
!define MUI_FINISHPAGE_LINK "Visit the Kolosal Server website"
!define MUI_FINISHPAGE_LINK_LOCATION "${PRODUCT_WEB_SITE}"
!insertmacro MUI_PAGE_FINISH

; Uninstaller pages
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_UNPAGE_FINISH

;--------------------------------
; Languages

!insertmacro MUI_LANGUAGE "English"

;--------------------------------
; Version Information

VIProductVersion "${PRODUCT_VERSION}.0"
VIAddVersionKey "ProductName" "${PRODUCT_NAME}"
VIAddVersionKey "CompanyName" "${PRODUCT_PUBLISHER}"
VIAddVersionKey "LegalCopyright" "Copyright © 2025 ${PRODUCT_PUBLISHER}"
VIAddVersionKey "FileDescription" "${PRODUCT_NAME} Installer"
VIAddVersionKey "FileVersion" "${PRODUCT_VERSION}"
VIAddVersionKey "ProductVersion" "${PRODUCT_VERSION}"

;--------------------------------
; Variables

Var StartMenuFolder
Var CreateDesktopIcon
Var AddToPath
Var AutoStart

;--------------------------------
; Custom Configuration Page

Function ConfigPage
  !insertmacro MUI_HEADER_TEXT "Configuration Options" "Choose additional installation options"
  
  nsDialogs::Create 1018
  Pop $0
  
  ${If} $0 == error
    Abort
  ${EndIf}
  
  ; Desktop shortcut checkbox
  ${NSD_CreateCheckBox} 0 0 100% 12u "Create Desktop Shortcut"
  Pop $CreateDesktopIcon
  ${NSD_Check} $CreateDesktopIcon
  
  ; Add to PATH checkbox
  ${NSD_CreateCheckBox} 0 20u 100% 12u "Add to System PATH"
  Pop $AddToPath
  ${NSD_Check} $AddToPath
  
  ; Auto-start checkbox
  ${NSD_CreateCheckBox} 0 40u 100% 12u "Start Kolosal Server automatically at login"
  Pop $AutoStart
  
  nsDialogs::Show
FunctionEnd

Function ConfigPageLeave
  ; Nothing to validate here
FunctionEnd

;--------------------------------
; Installer Sections

Section "!Core Files" SecCore
  SectionIn RO  ; Read-only, always installed
  
  ; Main executable goes to root directory
  SetOutPath "$INSTDIR"
  File "..\build\Release\kolosal-server.exe"
  
  ; All DLLs go to bin/ subdirectory (matching ZIP structure)
  CreateDirectory "$INSTDIR\bin"
  SetOutPath "$INSTDIR\bin"
  File "..\build\Release\*.dll"
  
  ; Create openblas.dll and liblapack.dll from libopenblas.dll if it exists
  ${If} ${FileExists} "$INSTDIR\bin\libopenblas.dll"
    CopyFiles /SILENT "$INSTDIR\bin\libopenblas.dll" "$INSTDIR\bin\openblas.dll"
    CopyFiles /SILENT "$INSTDIR\bin\libopenblas.dll" "$INSTDIR\bin\liblapack.dll"
    DetailPrint "Created openblas.dll and liblapack.dll from libopenblas.dll in bin/"
  ${Else}
    DetailPrint "Note: OpenBLAS DLLs not found (optional)"
  ${EndIf}
  
  ; Create launcher script (matching ZIP structure)
  SetOutPath "$INSTDIR"
  FileOpen $0 "$INSTDIR\start-kolosal-server.bat" w
  FileWrite $0 "@echo off$\r$\n"
  FileWrite $0 "REM Kolosal Server Launcher$\r$\n"
  FileWrite $0 "REM This script ensures DLLs in the bin folder are found$\r$\n"
  FileWrite $0 "$\r$\n"
  FileWrite $0 'set "SCRIPT_DIR=%~dp0"$\r$\n'
  FileWrite $0 'set "PATH=%SCRIPT_DIR%bin;%PATH%"$\r$\n'
  FileWrite $0 "$\r$\n"
  FileWrite $0 "echo Starting Kolosal Server...$\r$\n"
  FileWrite $0 "echo DLL search path: %SCRIPT_DIR%bin$\r$\n"
  FileWrite $0 "$\r$\n"
  FileWrite $0 '"%SCRIPT_DIR%kolosal-server.exe" %*$\r$\n'
  FileClose $0
  
  ; Documentation
  SetOutPath "$INSTDIR"
  File "..\README.md"
  File "..\LICENSE"
  
  ; Create README.txt (matching ZIP structure)
  FileOpen $0 "$INSTDIR\README.txt" w
  FileWrite $0 "Kolosal Server Portable Package$\r$\n"
  FileWrite $0 "$\r$\n"
  FileWrite $0 "QUICK START:$\r$\n"
  FileWrite $0 "------------$\r$\n"
  FileWrite $0 "Run start-kolosal-server.bat to start the server.$\r$\n"
  FileWrite $0 "Alternatively, you can run kolosal-server.exe directly from this directory.$\r$\n"
  FileWrite $0 "$\r$\n"
  FileWrite $0 "STRUCTURE:$\r$\n"
  FileWrite $0 "----------$\r$\n"
  FileWrite $0 "bin/       - All DLL dependencies$\r$\n"
  FileWrite $0 "config/    - Configuration files$\r$\n"
  FileWrite $0 "models/    - Place your model files here$\r$\n"
  FileWrite $0 "logs/      - Server logs will be written here$\r$\n"
  FileWrite $0 "data/      - Runtime data and indexes$\r$\n"
  FileWrite $0 "docs/      - Documentation$\r$\n"
  FileWrite $0 "static/    - Web UI files$\r$\n"
  FileWrite $0 "$\r$\n"
  FileWrite $0 "NOTES:$\r$\n"
  FileWrite $0 "------$\r$\n"
  FileWrite $0 "- All required DLL files are in the bin/ directory$\r$\n"
  FileWrite $0 "- The launcher script automatically adds bin/ to the PATH$\r$\n"
  FileWrite $0 "- If you have both llama-cpu.dll and llama-vulkan.dll, the server will use the appropriate one$\r$\n"
  FileWrite $0 "$\r$\n"
  FileClose $0
  
  ; Assets
  SetOutPath "$INSTDIR\assets"
  File /nonfatal "..\assets\icon.ico"
  File /nonfatal "..\assets\logo.png"
  
  ; Create necessary directories (matching ZIP structure)
  CreateDirectory "$INSTDIR\logs"
  CreateDirectory "$INSTDIR\data"
  CreateDirectory "$INSTDIR\data\faiss_index"
  CreateDirectory "$INSTDIR\models"
  CreateDirectory "$INSTDIR\config"
  
SectionEnd

Section "Configuration Files" SecConfig
  ; Install sample configs to config directory (matching ZIP structure)
  SetOutPath "$INSTDIR\config"
  
  ; Copy sample configuration files
  File "..\configs\config.yaml"
  File "..\configs\config.json"
  File /nonfatal "..\configs\config_rms.yaml"
  File /nonfatal "..\configs\local-retrieval-config.yaml"
  
  ; Check if inference engines are available in bin directory
  ${If} ${FileExists} "$INSTDIR\bin\llama-cpu.dll"
    DetailPrint "Found llama-cpu.dll in bin/ (CPU inference available)"
  ${Else}
    DetailPrint "WARNING: llama-cpu.dll not found in bin/! CPU inference will not work."
  ${EndIf}
  
  ${If} ${FileExists} "$INSTDIR\bin\llama-vulkan.dll"
    DetailPrint "Found llama-vulkan.dll in bin/ (Vulkan GPU support enabled)"
  ${Else}
    DetailPrint "Note: llama-vulkan.dll not found in bin/ (optional - GPU acceleration via Vulkan)"
  ${EndIf}
  
  ${If} ${FileExists} "$INSTDIR\bin\llama-cuda.dll"
    DetailPrint "Found llama-cuda.dll in bin/ (CUDA GPU support enabled)"
  ${Else}
    DetailPrint "Note: llama-cuda.dll not found in bin/ (optional - GPU acceleration via CUDA)"
  ${EndIf}
  
SectionEnd

Section "Documentation" SecDocs
  SetOutPath "$INSTDIR\docs"
  
  ; Copy all documentation
  File /nonfatal /r "..\docs\*.*"
  
SectionEnd

Section "Static Files (Web UI)" SecStatic
  ; Optional web UI files - copied from build output
  SetOutPath "$INSTDIR\static"
  
  ; Copy static web files from build output directory
  ; The static files are copied to build/Release/static during the build process
  !ifdef INCLUDE_STATIC_FILES
    File /r "..\build\Release\static\*.*"
    DetailPrint "Installed web UI static files"
  !else
    ; If static files don't exist at compile time, just create the directory
    CreateDirectory "$INSTDIR\static"
    DetailPrint "Static directory created (web UI files not included in this build)"
  !endif
  
SectionEnd

Section "Development Headers" SecHeaders
  SetOutPath "$INSTDIR\include"
  
  ; Copy header files for development
  File /nonfatal /r "..\include\*.*"
  
SectionEnd

Section -Post
  ; Write registry keys
  WriteRegStr HKLM "${PRODUCT_DIR_REGKEY}" "" "$INSTDIR\kolosal-server.exe"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "DisplayName" "$(^Name)"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "UninstallString" "$INSTDIR\uninstall.exe"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "DisplayIcon" "$INSTDIR\assets\icon.ico"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "DisplayVersion" "${PRODUCT_VERSION}"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "URLInfoAbout" "${PRODUCT_WEB_SITE}"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "Publisher" "${PRODUCT_PUBLISHER}"
  
  ; Calculate installed size
  ${GetSize} "$INSTDIR" "/S=0K" $0 $1 $2
  IntFmt $0 "0x%08X" $0
  WriteRegDWORD ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "EstimatedSize" "$0"
  
  ; Create uninstaller
  WriteUninstaller "$INSTDIR\uninstall.exe"
  
  ; Create start menu shortcuts
  CreateDirectory "$SMPROGRAMS\${PRODUCT_NAME}"
  CreateShortCut "$SMPROGRAMS\${PRODUCT_NAME}\Kolosal Server.lnk" "$INSTDIR\start-kolosal-server.bat" "" "$INSTDIR\assets\icon.ico"
  CreateShortCut "$SMPROGRAMS\${PRODUCT_NAME}\Configuration File.lnk" "$INSTDIR\config\config.yaml"
  CreateShortCut "$SMPROGRAMS\${PRODUCT_NAME}\Documentation.lnk" "$INSTDIR\README.md"
  CreateShortCut "$SMPROGRAMS\${PRODUCT_NAME}\Uninstall.lnk" "$INSTDIR\uninstall.exe"
  
  ; Desktop shortcut (if checked)
  ${NSD_GetState} $CreateDesktopIcon $0
  ${If} $0 == ${BST_CHECKED}
    CreateShortCut "$DESKTOP\Kolosal Server.lnk" "$INSTDIR\start-kolosal-server.bat" "" "$INSTDIR\assets\icon.ico"
  ${EndIf}
  
  ; Add to PATH (if checked)
  ${NSD_GetState} $AddToPath $0
  ${If} $0 == ${BST_CHECKED}
    ; Add to system PATH using registry
    ; Read current PATH
    ReadRegStr $0 HKLM "SYSTEM\CurrentControlSet\Control\Session Manager\Environment" "Path"
    ; Check if already in PATH
    ${StrStr} $1 $0 "$INSTDIR"
    StrCmp $1 "" 0 +3
      ; Not in PATH, so add it
      WriteRegExpandStr HKLM "SYSTEM\CurrentControlSet\Control\Session Manager\Environment" "Path" "$0;$INSTDIR"
      SendMessage ${HWND_BROADCAST} ${WM_WININICHANGE} 0 "STR:Environment" /TIMEOUT=5000
  ${EndIf}
  
  ; Auto-start (if checked)
  ${NSD_GetState} $AutoStart $0
  ${If} $0 == ${BST_CHECKED}
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Run" "KolosalServer" "$INSTDIR\start-kolosal-server.bat"
  ${EndIf}
  
SectionEnd

;--------------------------------
; Section Descriptions

!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
  !insertmacro MUI_DESCRIPTION_TEXT ${SecCore} "Core application files (required)"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecConfig} "Sample configuration files for server setup"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecDocs} "User and developer documentation"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecStatic} "Static web files for the web interface (optional)"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecHeaders} "Header files for development (optional)"
!insertmacro MUI_FUNCTION_DESCRIPTION_END

;--------------------------------
; Uninstaller Section

Section Uninstall
  ; Stop the service if running
  nsExec::ExecToStack 'taskkill /F /IM kolosal-server.exe'
  Pop $0
  
  ; Remove files and directories
  Delete "$INSTDIR\kolosal-server.exe"
  Delete "$INSTDIR\start-kolosal-server.bat"
  Delete "$INSTDIR\README.md"
  Delete "$INSTDIR\LICENSE"
  Delete "$INSTDIR\README.txt"
  Delete "$INSTDIR\uninstall.exe"
  
  ; Remove bin directory with all DLLs
  RMDir /r "$INSTDIR\bin"
  RMDir /r "$INSTDIR\assets"
  RMDir /r "$INSTDIR\docs"
  RMDir /r "$INSTDIR\static"
  RMDir /r "$INSTDIR\include"
  RMDir /r "$INSTDIR\config"
  
  ; Ask user if they want to keep data and logs
  MessageBox MB_YESNO|MB_ICONQUESTION "Do you want to keep your data, logs, and models?" IDYES KeepData
    RMDir /r "$INSTDIR\data"
    RMDir /r "$INSTDIR\logs"
    RMDir /r "$INSTDIR\models"
  KeepData:
  
  RMDir "$INSTDIR"
  
  ; Remove shortcuts
  Delete "$SMPROGRAMS\${PRODUCT_NAME}\*.*"
  RMDir "$SMPROGRAMS\${PRODUCT_NAME}"
  Delete "$DESKTOP\Kolosal Server.lnk"
  
  ; Remove registry keys
  DeleteRegKey ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}"
  DeleteRegKey HKLM "${PRODUCT_DIR_REGKEY}"
  DeleteRegValue HKLM "Software\Microsoft\Windows\CurrentVersion\Run" "KolosalServer"
  
  ; Remove from PATH
  ; Note: Manual PATH cleanup may be required
  ; Automatic PATH removal has been disabled to avoid complexity
  ; Users can manually remove the installation directory from PATH if needed
  
  SetAutoClose true
SectionEnd

;--------------------------------
; Functions

Function .onInit
  ; Initialize PROGRAMDATA variable from environment
  ReadEnvStr $PROGRAMDATA "PROGRAMDATA"
  ${If} $PROGRAMDATA == ""
    StrCpy $PROGRAMDATA "C:\ProgramData"
  ${EndIf}
  
  ; Check if 64-bit Windows
  ${IfNot} ${RunningX64}
    MessageBox MB_OK|MB_ICONSTOP "This application requires 64-bit Windows."
    Abort
  ${EndIf}
  
  ; Check if already installed
  ReadRegStr $R0 ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "UninstallString"
  StrCmp $R0 "" done
  
  MessageBox MB_OKCANCEL|MB_ICONEXCLAMATION \
  "${PRODUCT_NAME} is already installed. $\n$\nClick OK to remove the previous version or Cancel to cancel this installation." \
  IDOK uninst
  Abort
  
  uninst:
    ClearErrors
    ExecWait '$R0 /S _?=$INSTDIR'
    IfErrors no_remove_uninstaller done
    no_remove_uninstaller:
  
  done:
FunctionEnd

Function LaunchApplication
  ; Launch using the launcher script which handles DLL paths
  Exec "$INSTDIR\start-kolosal-server.bat"
FunctionEnd

Function un.onInit
  MessageBox MB_ICONQUESTION|MB_YESNO|MB_DEFBUTTON2 "Are you sure you want to uninstall ${PRODUCT_NAME}?" IDYES +2
  Abort
FunctionEnd

Function un.onUninstSuccess
  HideWindow
  MessageBox MB_ICONINFORMATION|MB_OK "${PRODUCT_NAME} has been successfully removed from your computer."
FunctionEnd
