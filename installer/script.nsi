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
  
  SetOutPath "$INSTDIR"
  
  ; Main executable and libraries from build/Release
  File /nonfatal "..\build\Release\kolosal-server.exe"
  File /nonfatal "..\build\Release\*.dll"
  
  ; Alternative: from build/dist if using make dist
  File /nonfatal "..\build\dist\kolosal-server.exe"
  File /nonfatal "..\build\dist\*.dll"
  
  ; Create openblas.dll and liblapack.dll from libopenblas.dll if it exists
  ${If} ${FileExists} "$INSTDIR\libopenblas.dll"
    CopyFiles /SILENT "$INSTDIR\libopenblas.dll" "$INSTDIR\openblas.dll"
    CopyFiles /SILENT "$INSTDIR\libopenblas.dll" "$INSTDIR\liblapack.dll"
    DetailPrint "Created openblas.dll and liblapack.dll from libopenblas.dll"
  ${Else}
    DetailPrint "Note: OpenBLAS DLLs not found (optional)"
  ${EndIf}
  
  ; Documentation
  SetOutPath "$INSTDIR"
  File "..\README.md"
  File "..\LICENSE"
  
  ; Assets
  SetOutPath "$INSTDIR\assets"
  File "..\assets\icon.ico"
  
  ; Create necessary directories
  CreateDirectory "$INSTDIR\logs"
  CreateDirectory "$INSTDIR\data"
  CreateDirectory "$INSTDIR\data\faiss_index"
  CreateDirectory "$INSTDIR\models"
  CreateDirectory "$INSTDIR\configs"
  
SectionEnd

Section "Configuration Files" SecConfig
  ; Backup and remove old user config if it exists
  IfFileExists "$APPDATA\Kolosal\config.yaml" 0 +5
    CreateDirectory "$APPDATA\Kolosal\backup"
    CopyFiles "$APPDATA\Kolosal\config.yaml" "$APPDATA\Kolosal\backup\config.yaml.backup"
    Delete "$APPDATA\Kolosal\config.yaml"
    DetailPrint "Moved old user config to $APPDATA\Kolosal\backup\config.yaml.backup"
  
  ; Create ProgramData directory structure for system-wide config
  ; Note: NSIS uses $PROGRAMDATA for C:\ProgramData
  CreateDirectory "$PROGRAMDATA\Kolosal"
  CreateDirectory "$PROGRAMDATA\Kolosal\bin"
  CreateDirectory "$PROGRAMDATA\Kolosal\models"
  CreateDirectory "$PROGRAMDATA\Kolosal\data"
  CreateDirectory "$PROGRAMDATA\Kolosal\data\faiss_index"
  CreateDirectory "$PROGRAMDATA\Kolosal\logs"
  
  ; Install fresh config to ProgramData (system location - higher priority)
  SetOutPath "$PROGRAMDATA\Kolosal"
  File /oname=config.yaml "..\configs\config.yaml"
  DetailPrint "Installed fresh config to $PROGRAMDATA\Kolosal\config.yaml"
  
  ; Also copy config to installation directory (where the exe expects it)
  SetOutPath "$INSTDIR"
  File /oname=config.yaml "..\configs\config.yaml"
  DetailPrint "Installed config to $INSTDIR\config.yaml"
  
  ; Create kolosal subdirectory and copy config there as well (for kolosal/config.yaml path)
  CreateDirectory "$INSTDIR\kolosal"
  SetOutPath "$INSTDIR\kolosal"
  File /oname=config.yaml "..\configs\config.yaml"
  DetailPrint "Installed config to $INSTDIR\kolosal\config.yaml"
  
  ; Install sample configs to installation directory for reference
  SetOutPath "$INSTDIR\configs"
  
  ; Copy sample configuration files
  File "..\configs\config.yaml"
  File "..\configs\config.json"
  File "..\configs\config_rms.yaml"
  File "..\configs\local-retrieval-config.yaml"
  
  ; Copy inference engine DLLs to installation directory (same as exe)
  SetOutPath "$INSTDIR"
  
  ; Try multiple possible locations for inference engine DLLs
  ; Location 1: build/Release (MSVC multi-config)
  ; Location 2: build/bin (CMake single config)
  ; Location 3: inference/build/Release (standalone inference build)
  
  ; Copy CPU engine DLL (REQUIRED)
  File /nonfatal "..\build\Release\llama-cpu.dll"
  ${If} ${FileExists} "$INSTDIR\llama-cpu.dll"
    DetailPrint "Found llama-cpu.dll in build/Release"
  ${Else}
    File /nonfatal "..\build\bin\llama-cpu.dll"
    ${If} ${FileExists} "$INSTDIR\llama-cpu.dll"
      DetailPrint "Found llama-cpu.dll in build/bin"
    ${Else}
      File /nonfatal "..\inference\build\Release\llama-cpu.dll"
      ${If} ${FileExists} "$INSTDIR\llama-cpu.dll"
        DetailPrint "Found llama-cpu.dll in inference/build/Release"
      ${Else}
        DetailPrint "WARNING: llama-cpu.dll not found! CPU inference will not work."
      ${EndIf}
    ${EndIf}
  ${EndIf}
  
  ; Copy Vulkan engine DLL (optional but recommended for GPU support)
  File /nonfatal "..\build\Release\llama-vulkan.dll"
  ${If} ${FileExists} "$INSTDIR\llama-vulkan.dll"
    DetailPrint "Found llama-vulkan.dll in build/Release"
  ${Else}
    File /nonfatal "..\build\bin\llama-vulkan.dll"
    ${If} ${FileExists} "$INSTDIR\llama-vulkan.dll"
      DetailPrint "Found llama-vulkan.dll in build/bin"
    ${Else}
      File /nonfatal "..\inference\build\Release\llama-vulkan.dll"
      ${If} ${FileExists} "$INSTDIR\llama-vulkan.dll"
        DetailPrint "Found llama-vulkan.dll in inference/build/Release"
      ${Else}
        DetailPrint "Note: llama-vulkan.dll not found (optional - GPU acceleration via Vulkan)"
      ${EndIf}
    ${EndIf}
  ${EndIf}
  
  ; Copy CUDA engine DLL (optional)
  File /nonfatal "..\build\Release\llama-cuda.dll"
  ${If} ${FileExists} "$INSTDIR\llama-cuda.dll"
    DetailPrint "Found llama-cuda.dll in build/Release"
  ${Else}
    File /nonfatal "..\build\bin\llama-cuda.dll"
    ${If} ${FileExists} "$INSTDIR\llama-cuda.dll"
      DetailPrint "Found llama-cuda.dll in build/bin"
    ${Else}
      File /nonfatal "..\inference\build\Release\llama-cuda.dll"
      ${If} ${FileExists} "$INSTDIR\llama-cuda.dll"
        DetailPrint "Found llama-cuda.dll in inference/build/Release"
      ${Else}
        DetailPrint "Note: llama-cuda.dll not found (optional - GPU acceleration via CUDA)"
      ${EndIf}
    ${EndIf}
  ${EndIf}
  
  ; Copy Vulkan runtime DLLs if they exist (required for Vulkan support)
  File /nonfatal "..\build\Release\vulkan-1.dll"
  ${If} ${FileExists} "$INSTDIR\vulkan-1.dll"
    DetailPrint "Found vulkan-1.dll (Vulkan runtime)"
  ${Else}
    DetailPrint "Note: vulkan-1.dll not found (install Vulkan SDK or runtime for GPU support)"
  ${EndIf}
  
  ; Copy DLLs to both original name and libllama-*.dll naming convention in the executable directory
  ; Keep original names for runtime loading
  ${If} ${FileExists} "$INSTDIR\llama-cpu.dll"
    CopyFiles /SILENT "$INSTDIR\llama-cpu.dll" "$INSTDIR\libllama-cpu.dll"
    DetailPrint "Created libllama-cpu.dll (keeping original llama-cpu.dll)"
  ${EndIf}
  ${If} ${FileExists} "$INSTDIR\llama-vulkan.dll"
    CopyFiles /SILENT "$INSTDIR\llama-vulkan.dll" "$INSTDIR\libllama-vulkan.dll"
    DetailPrint "Created libllama-vulkan.dll (keeping original llama-vulkan.dll)"
  ${EndIf}
  ${If} ${FileExists} "$INSTDIR\llama-cuda.dll"
    CopyFiles /SILENT "$INSTDIR\llama-cuda.dll" "$INSTDIR\libllama-cuda.dll"
    DetailPrint "Created libllama-cuda.dll (keeping original llama-cuda.dll)"
  ${EndIf}
  
  ; Create lib directory and copy engine DLLs there as well (for fallback)
  CreateDirectory "$INSTDIR\lib"
  ${If} ${FileExists} "$INSTDIR\llama-cpu.dll"
    CopyFiles /SILENT "$INSTDIR\llama-cpu.dll" "$INSTDIR\lib\llama-cpu.dll"
    CopyFiles /SILENT "$INSTDIR\libllama-cpu.dll" "$INSTDIR\lib\libllama-cpu.dll"
    DetailPrint "Installed CPU engine to $INSTDIR\lib\ (both llama-cpu.dll and libllama-cpu.dll)"
  ${Else}
    DetailPrint "WARNING: CPU engine DLL not found! Please rebuild with CPU support."
    MessageBox MB_OK|MB_ICONEXCLAMATION "Warning: llama-cpu.dll not found!$\n$\nCPU inference will not work. Please rebuild the project or contact support."
  ${EndIf}
  ${If} ${FileExists} "$INSTDIR\llama-vulkan.dll"
    CopyFiles /SILENT "$INSTDIR\llama-vulkan.dll" "$INSTDIR\lib\llama-vulkan.dll"
    CopyFiles /SILENT "$INSTDIR\libllama-vulkan.dll" "$INSTDIR\lib\libllama-vulkan.dll"
    DetailPrint "Installed Vulkan engine to $INSTDIR\lib\ (both llama-vulkan.dll and libllama-vulkan.dll)"
    ; Copy vulkan-1.dll to lib as well if it exists
    ${If} ${FileExists} "$INSTDIR\vulkan-1.dll"
      CopyFiles /SILENT "$INSTDIR\vulkan-1.dll" "$INSTDIR\lib\vulkan-1.dll"
      DetailPrint "Installed Vulkan runtime to $INSTDIR\lib\vulkan-1.dll"
    ${EndIf}
  ${EndIf}
  ${If} ${FileExists} "$INSTDIR\llama-cuda.dll"
    CopyFiles /SILENT "$INSTDIR\llama-cuda.dll" "$INSTDIR\lib\llama-cuda.dll"
    CopyFiles /SILENT "$INSTDIR\libllama-cuda.dll" "$INSTDIR\lib\libllama-cuda.dll"
    DetailPrint "Installed CUDA engine to $INSTDIR\lib\ (both llama-cuda.dll and libllama-cuda.dll)"
  ${EndIf}
  
  ; Also copy to ProgramData\Kolosal\bin for backward compatibility
  CreateDirectory "$PROGRAMDATA\Kolosal\bin"
  ${If} ${FileExists} "$INSTDIR\lib\llama-cpu.dll"
    CopyFiles /SILENT "$INSTDIR\lib\llama-cpu.dll" "$PROGRAMDATA\Kolosal\bin\llama-cpu.dll"
    CopyFiles /SILENT "$INSTDIR\lib\libllama-cpu.dll" "$PROGRAMDATA\Kolosal\bin\libllama-cpu.dll"
    DetailPrint "Installed CPU engine support"
  ${EndIf}
  ${If} ${FileExists} "$INSTDIR\lib\llama-vulkan.dll"
    CopyFiles /SILENT "$INSTDIR\lib\llama-vulkan.dll" "$PROGRAMDATA\Kolosal\bin\llama-vulkan.dll"
    CopyFiles /SILENT "$INSTDIR\lib\libllama-vulkan.dll" "$PROGRAMDATA\Kolosal\bin\libllama-vulkan.dll"
    DetailPrint "Installed Vulkan engine support"
    ; Copy vulkan runtime DLL
    ${If} ${FileExists} "$INSTDIR\vulkan-1.dll"
      CopyFiles /SILENT "$INSTDIR\vulkan-1.dll" "$PROGRAMDATA\Kolosal\bin\vulkan-1.dll"
      DetailPrint "Installed Vulkan runtime support"
    ${EndIf}
  ${EndIf}
  ${If} ${FileExists} "$INSTDIR\lib\llama-cuda.dll"
    CopyFiles /SILENT "$INSTDIR\lib\llama-cuda.dll" "$PROGRAMDATA\Kolosal\bin\llama-cuda.dll"
    CopyFiles /SILENT "$INSTDIR\lib\libllama-cuda.dll" "$PROGRAMDATA\Kolosal\bin\libllama-cuda.dll"
    DetailPrint "Installed CUDA engine support"
  ${EndIf}
  
  ; Notify user about old config migration
  IfFileExists "$APPDATA\Kolosal\backup\config.yaml.backup" 0 +2
    MessageBox MB_OK|MB_ICONINFORMATION "Note: Your old configuration has been migrated.$\n$\nOld config backed up to:$\n$APPDATA\Kolosal\backup\config.yaml.backup$\n$\nNew config installed to:$\nC:\ProgramData\Kolosal\config.yaml$\n$\nPlease review and update the new configuration with your custom settings if needed."
  
SectionEnd

Section "Documentation" SecDocs
  SetOutPath "$INSTDIR\docs"
  
  ; Copy all documentation
  File /nonfatal /r "..\docs\*.*"
  
SectionEnd

Section "!Static Files (Web UI)" SecStatic
  SectionIn RO  ; Read-only, required for web UI
  SetOutPath "$INSTDIR\static"
  
  ; Copy static web files - REQUIRED for web UI to work
  File /r "..\static\*.*"
  
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
  CreateShortCut "$SMPROGRAMS\${PRODUCT_NAME}\Kolosal Server.lnk" "$INSTDIR\kolosal-server.exe" "" "$INSTDIR\assets\icon.ico"
  CreateShortCut "$SMPROGRAMS\${PRODUCT_NAME}\Configuration File.lnk" "$PROGRAMDATA\Kolosal\config.yaml"
  CreateShortCut "$SMPROGRAMS\${PRODUCT_NAME}\Documentation.lnk" "$INSTDIR\README.md"
  CreateShortCut "$SMPROGRAMS\${PRODUCT_NAME}\Uninstall.lnk" "$INSTDIR\uninstall.exe"
  
  ; Desktop shortcut (if checked)
  ${NSD_GetState} $CreateDesktopIcon $0
  ${If} $0 == ${BST_CHECKED}
    CreateShortCut "$DESKTOP\Kolosal Server.lnk" "$INSTDIR\kolosal-server.exe" "" "$INSTDIR\assets\icon.ico"
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
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Run" "KolosalServer" "$INSTDIR\kolosal-server.exe"
  ${EndIf}
  
SectionEnd

;--------------------------------
; Section Descriptions

!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
  !insertmacro MUI_DESCRIPTION_TEXT ${SecCore} "Core application files (required)"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecConfig} "Sample configuration files for server setup"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecDocs} "User and developer documentation"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecStatic} "Static web files for the web interface (required)"
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
  Delete "$INSTDIR\*.dll"
  Delete "$INSTDIR\llama-cpu.dll"
  Delete "$INSTDIR\llama-vulkan.dll"
  Delete "$INSTDIR\llama-cuda.dll"
  Delete "$INSTDIR\libllama-cpu.dll"
  Delete "$INSTDIR\libllama-vulkan.dll"
  Delete "$INSTDIR\libllama-cuda.dll"
  Delete "$INSTDIR\vulkan-1.dll"
  Delete "$INSTDIR\README.md"
  Delete "$INSTDIR\LICENSE"
  Delete "$INSTDIR\changes.log"
  Delete "$INSTDIR\config.yaml"
  Delete "$INSTDIR\config.json"
  Delete "$INSTDIR\uninstall.exe"
  
  RMDir /r "$INSTDIR\lib"
  RMDir /r "$INSTDIR\assets"
  RMDir /r "$INSTDIR\docs"
  RMDir /r "$INSTDIR\static"
  RMDir /r "$INSTDIR\include"
  RMDir /r "$INSTDIR\configs"
  RMDir /r "$INSTDIR\kolosal"
  
  ; Also remove ProgramData inference engines
  Delete "$PROGRAMDATA\Kolosal\bin\llama-cpu.dll"
  Delete "$PROGRAMDATA\Kolosal\bin\llama-vulkan.dll"
  Delete "$PROGRAMDATA\Kolosal\bin\llama-cuda.dll"
  Delete "$PROGRAMDATA\Kolosal\bin\libllama-cpu.dll"
  Delete "$PROGRAMDATA\Kolosal\bin\libllama-vulkan.dll"
  Delete "$PROGRAMDATA\Kolosal\bin\libllama-cuda.dll"
  Delete "$PROGRAMDATA\Kolosal\bin\vulkan-1.dll"
  RMDir "$PROGRAMDATA\Kolosal\bin"
  
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
  ; Check if application can start (test for missing DLLs)
  ExecWait '"$INSTDIR\kolosal-server.exe" --version' $0
  ${If} $0 != 0
    MessageBox MB_OK|MB_ICONINFORMATION \
      "Kolosal Server has been installed successfully.$\n$\n\
      Note: If you encounter missing DLL errors, you may need to install:$\n\
      - Visual C++ Redistributable (https://aka.ms/vs/17/release/vc_redist.x64.exe)$\n\
      - OpenBLAS (optional, for CPU acceleration)$\n$\n\
      Please check the logs directory for more information."
  ${Else}
    Exec "$INSTDIR\kolosal-server.exe"
  ${EndIf}
FunctionEnd

Function un.onInit
  MessageBox MB_ICONQUESTION|MB_YESNO|MB_DEFBUTTON2 "Are you sure you want to uninstall ${PRODUCT_NAME}?" IDYES +2
  Abort
FunctionEnd

Function un.onUninstSuccess
  HideWindow
  MessageBox MB_ICONINFORMATION|MB_OK "${PRODUCT_NAME} has been successfully removed from your computer."
FunctionEnd
