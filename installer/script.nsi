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
!define MUI_HEADERIMAGE
!define MUI_HEADERIMAGE_BITMAP "..\assets\logo.png"
!define MUI_WELCOMEFINISHPAGE_BITMAP "..\assets\logo.png"

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
!define MUI_FINISHPAGE_SHOWREADME "$INSTDIR\POST_INSTALL_README.md"
!define MUI_FINISHPAGE_SHOWREADME_TEXT "View Post-Installation Guide"
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
  
  ; Optional: Copy OpenBLAS DLL if present (for BLAS acceleration)
  File /nonfatal "..\build\Release\openblas.dll"
  File /nonfatal "..\build\Release\libopenblas.dll"
  File /nonfatal "..\build\dist\openblas.dll"
  File /nonfatal "..\build\dist\libopenblas.dll"
  
  ; Optional: Copy from inference build directory if present
  File /nonfatal "..\inference\external\llama.cpp\build\bin\Release\openblas.dll"
  File /nonfatal "..\build\external\llama.cpp\bin\Release\openblas.dll"
  
  ; Create lib directory and copy libraries
  SetOutPath "$INSTDIR\lib"
  File /nonfatal /r "..\build\Release\lib\*.*"
  File /nonfatal /r "..\build\dist\lib\*.*"
  
  ; Documentation
  SetOutPath "$INSTDIR"
  File /nonfatal "..\README.md"
  File /nonfatal "..\LICENSE"
  File /nonfatal "..\changes.log"
  File /nonfatal "POST_INSTALL_README.md"
  File /nonfatal "cleanup-config.ps1"
  
  ; Assets
  SetOutPath "$INSTDIR\assets"
  File /nonfatal "..\assets\*.ico"
  File /nonfatal "..\assets\*.png"
  
  ; Create necessary directories
  CreateDirectory "$INSTDIR\logs"
  CreateDirectory "$INSTDIR\data"
  CreateDirectory "$INSTDIR\data\faiss_index"
  CreateDirectory "$INSTDIR\models"
  CreateDirectory "$INSTDIR\configs"
  
SectionEnd

Section "Configuration Files" SecConfig
  ; Backup old user config if it exists
  IfFileExists "$APPDATA\Kolosal\config.yaml" 0 +3
    CreateDirectory "$APPDATA\Kolosal\backup"
    CopyFiles "$APPDATA\Kolosal\config.yaml" "$APPDATA\Kolosal\backup\config.yaml.backup"
    DetailPrint "Backed up old user config to $APPDATA\Kolosal\backup\config.yaml.backup"
  
  ; Create ProgramData directory structure for system-wide config
  SetOutPath "$COMMONAPPDATA\Kolosal"
  CreateDirectory "$COMMONAPPDATA\Kolosal\bin"
  CreateDirectory "$COMMONAPPDATA\Kolosal\models"
  CreateDirectory "$COMMONAPPDATA\Kolosal\data"
  CreateDirectory "$COMMONAPPDATA\Kolosal\data\faiss_index"
  CreateDirectory "$COMMONAPPDATA\Kolosal\logs"
  
  ; Install fresh config to ProgramData (system location - higher priority)
  ${If} ${FileExists} "..\configs\config-install.yaml"
    File /oname=config.yaml "..\configs\config-install.yaml"
    DetailPrint "Installed fresh config to $COMMONAPPDATA\Kolosal\config.yaml"
  ${ElseIf} ${FileExists} "..\configs\config.yaml"
    File /oname=config.yaml "..\configs\config.yaml"
    DetailPrint "Installed fresh config to $COMMONAPPDATA\Kolosal\config.yaml"
  ${EndIf}
  
  ; Also install configs to installation directory for reference
  SetOutPath "$INSTDIR\configs"
  
  ; Copy sample configuration files
  File /nonfatal "..\configs\config.yaml"
  File /nonfatal "..\configs\config.json"
  File /nonfatal "..\configs\config_rms.yaml"
  File /nonfatal "..\configs\local-retrieval-config.yaml"
  
  ; Copy sample files with .default extension (always update these as reference)
  SetOutPath "$INSTDIR\configs"
  ${If} ${FileExists} "..\configs\config.yaml"
    File /oname=config.yaml.default "..\configs\config.yaml"
  ${EndIf}
  ${If} ${FileExists} "..\configs\config.json"
    File /oname=config.json.default "..\configs\config.json"
  ${EndIf}
  ${If} ${FileExists} "..\configs\config_rms.yaml"
    File /oname=config_rms.yaml.default "..\configs\config_rms.yaml"
  ${EndIf}
  ${If} ${FileExists} "..\configs\local-retrieval-config.yaml"
    File /oname=local-retrieval-config.yaml.default "..\configs\local-retrieval-config.yaml"
  ${EndIf}
  
  ; Copy inference engine DLLs to ProgramData\Kolosal\bin
  ; Use CopyFiles to avoid path expansion issues
  IfFileExists "..\build\Release\llama-cpu.dll" 0 +2
    CopyFiles /SILENT "..\build\Release\llama-cpu.dll" "$COMMONAPPDATA\Kolosal\bin\llama-cpu.dll"
  
  IfFileExists "..\build\Release\llama-vulkan.dll" 0 +2
    CopyFiles /SILENT "..\build\Release\llama-vulkan.dll" "$COMMONAPPDATA\Kolosal\bin\llama-vulkan.dll"
  
  IfFileExists "..\build\Release\llama-cuda.dll" 0 +2
    CopyFiles /SILENT "..\build\Release\llama-cuda.dll" "$COMMONAPPDATA\Kolosal\bin\llama-cuda.dll"
  
  ; Notify user about old config
  IfFileExists "$APPDATA\Kolosal\config.yaml" 0 +2
    MessageBox MB_OK|MB_ICONINFORMATION "Note: An old configuration was found in:$\n$APPDATA\Kolosal\config.yaml$\n$\nA backup has been created at:$\n$APPDATA\Kolosal\backup\config.yaml.backup$\n$\nA fresh configuration has been installed to:$\nC:\ProgramData\Kolosal\config.yaml$\n$\nTo use the fresh configuration, please delete the old config file at:$\n$APPDATA\Kolosal\config.yaml$\n$\nOr update it with the new paths from the fresh config."
  
SectionEnd

Section "Documentation" SecDocs
  SetOutPath "$INSTDIR\docs"
  
  ; Copy all documentation
  File /nonfatal /r "..\docs\*.*"
  
SectionEnd

Section "Static Files" SecStatic
  SetOutPath "$INSTDIR\static"
  
  ; Copy static web files
  File /nonfatal /r "..\static\*.*"
  
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
  CreateShortCut "$SMPROGRAMS\${PRODUCT_NAME}\Configuration Cleanup Tool.lnk" "powershell.exe" '-ExecutionPolicy Bypass -File "$INSTDIR\cleanup-config.ps1"' "$INSTDIR\assets\icon.ico"
  CreateShortCut "$SMPROGRAMS\${PRODUCT_NAME}\Configuration File.lnk" "$COMMONAPPDATA\Kolosal\config.yaml"
  CreateShortCut "$SMPROGRAMS\${PRODUCT_NAME}\Post-Installation Guide.lnk" "$INSTDIR\POST_INSTALL_README.md"
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
  !insertmacro MUI_DESCRIPTION_TEXT ${SecStatic} "Static web files for the web interface"
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
