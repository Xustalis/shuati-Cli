; Shuati CLI Installer Script
; Optimized for professional Windows installation experience

#define MyAppName "Shuati CLI"
#define MyAppPublisher "Shuati CLI Team"
#define MyAppURL "https://github.com/shuati-cli/shuati-cli"
#define MyAppSupportURL "https://github.com/shuati-cli/shuati-cli/issues"
#define MyAppUpdatesURL "https://github.com/shuati-cli/shuati-cli/releases"

; Version handling - supports environment variable or defaults
#if !Defined(MyAppVersion)
  #define EnvVersion GetEnv('SHUATI_VERSION')
  #if EnvVersion != ""
    #define MyAppVersion EnvVersion
  #else
    #define MyAppVersion "0.0.1"
  #endif
#endif

; Output filename handling
#if !Defined(MyOutputBaseFilename)
  #define EnvOutName GetEnv('SHUATI_OUTPUT_NAME')
  #if EnvOutName != ""
    #define MyOutputBaseFilename EnvOutName
  #else
    #define MyOutputBaseFilename "shuati-cli-" + MyAppVersion + "-setup"
  #endif
#endif

; Source directory handling
#if !Defined(SourceDir)
  #define EnvSourceDir GetEnv('SHUATI_SOURCE_DIR')
  #if EnvSourceDir != ""
    #define SourceDir EnvSourceDir
  #else
    #define SourceDir "..\build\Release"
  #endif
#endif

[Setup]
; Application identification
AppId={{E2A00C8F-5C4D-4B2D-9BCE-3B3B2F4F6A9E}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppSupportURL}
AppUpdatesURL={#MyAppUpdatesURL}

; Default installation settings
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=no

; Privilege and architecture settings
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible

; Output settings
OutputDir=..\dist
OutputBaseFilename={#MyOutputBaseFilename}

; Compression settings - optimized for size and speed
Compression=lzma2/ultra64
SolidCompression=yes
LZMAUseSeparateProcess=yes
LZMANumBlockThreads=4

; UI settings
WizardStyle=modern
WizardSizePercent=120
WizardResizable=yes

; Version info for the installer itself
VersionInfoVersion={#MyAppVersion}
VersionInfoCompany={#MyAppPublisher}
VersionInfoDescription={#MyAppName} Setup
VersionInfoTextVersion={#MyAppVersion}
VersionInfoCopyright=Copyright (C) 2024 {#MyAppPublisher}

; Other settings
AllowNoIcons=yes
AllowUNCPath=no
MinVersion=6.1sp1
UninstallDisplayIcon={app}\shuati.exe
UninstallDisplayName={#MyAppName}
UninstallFilesDir={app}\uninstall
SetupIconFile=assets\icon.ico

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "chinesesimplified"; MessagesFile: "compiler:Languages\ChineseSimplified.isl"

[Types]
Name: "full"; Description: "Full installation"
Name: "compact"; Description: "Compact installation"
Name: "custom"; Description: "Custom installation"; Flags: iscustom

[Components]
Name: "main"; Description: "Main application"; Types: full compact custom; Flags: fixed
Name: "resources"; Description: "Resource files (compiler rules, templates)"; Types: full compact
Name: "docs"; Description: "Documentation files"; Types: full

[Tasks]
Name: "addtopath"; Description: "{cm:AddToPath}"; GroupDescription: "{cm:AdditionalTasks}"
Name: "contextmenu"; Description: "{cm:AddContextMenu}"; GroupDescription: "{cm:AdditionalTasks}"; Flags: unchecked
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalTasks}"; Flags: unchecked

[Files]
; Main executable
Source: "{#SourceDir}\shuati.exe"; DestDir: "{app}"; Flags: ignoreversion; Components: main

; Resource files
Source: "{#SourceDir}\resources\*"; DestDir: "{app}\resources"; Flags: ignoreversion recursesubdirs createallsubdirs; Components: resources

; Documentation
Source: "..\README.md"; DestDir: "{app}"; Flags: ignoreversion; Components: docs
Source: "..\CHANGELOG.md"; DestDir: "{app}"; Flags: ignoreversion; Components: docs
Source: "..\LICENSE"; DestDir: "{app}"; Flags: ignoreversion; Components: docs

; Visual C++ Redistributables (optional, for systems that need them)
Source: "..\build\vcpkg_installed\x64-windows\bin\*.dll"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist; Components: main

[Icons]
Name: "{autoprograms}\{#MyAppName}"; Filename: "{app}\shuati.exe"; Comment: "Shuati CLI - Algorithm Practice Tool"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\shuati.exe"; Tasks: desktopicon; Comment: "Shuati CLI - Algorithm Practice Tool"

[Run]
; Show help after installation
Filename: "{cmd}"; Parameters: "/c ""{app}\shuati.exe"" --help && pause"; Flags: postinstall nowait skipifsilent shellexec; Description: "{cm:LaunchAfterInstall}"

[Registry]
; Context menu entries for Windows Explorer
Root: HKCU; Subkey: "Software\Classes\Directory\Background\shell\ShuatiCLI"; ValueType: string; ValueName: ""; ValueData: "Open Shuati CLI Here"; Tasks: contextmenu; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\Classes\Directory\Background\shell\ShuatiCLI"; ValueType: string; ValueName: "Icon"; ValueData: "{app}\shuati.exe"; Tasks: contextmenu; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\Classes\Directory\Background\shell\ShuatiCLI\command"; ValueType: string; ValueName: ""; ValueData: "cmd.exe /s /k pushd ""%V"" && ""{app}\shuati.exe"""; Tasks: contextmenu; Flags: uninsdeletekey

Root: HKCU; Subkey: "Software\Classes\Directory\shell\ShuatiCLI"; ValueType: string; ValueName: ""; ValueData: "Open Shuati CLI Here"; Tasks: contextmenu; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\Classes\Directory\shell\ShuatiCLI"; ValueType: string; ValueName: "Icon"; ValueData: "{app}\shuati.exe"; Tasks: contextmenu; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\Classes\Directory\shell\ShuatiCLI\command"; ValueType: string; ValueName: ""; ValueData: "cmd.exe /s /k pushd ""%1"" && ""{app}\shuati.exe"""; Tasks: contextmenu; Flags: uninsdeletekey

; Application registration for Windows "Apps & Features"
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Uninstall\{#MyAppName}_is1"; ValueType: string; ValueName: "DisplayIcon"; ValueData: "{app}\shuati.exe"; Flags: uninsdeletevalue
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Uninstall\{#MyAppName}_is1"; ValueType: string; ValueName: "Publisher"; ValueData: "{#MyAppPublisher}"; Flags: uninsdeletevalue
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Uninstall\{#MyAppName}_is1"; ValueType: string; ValueName: "URLInfoAbout"; ValueData: "{#MyAppURL}"; Flags: uninsdeletevalue
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Uninstall\{#MyAppName}_is1"; ValueType: string; ValueName: "HelpLink"; ValueData: "{#MyAppSupportURL}"; Flags: uninsdeletevalue
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Uninstall\{#MyAppName}_is1"; ValueType: string; ValueName: "URLUpdateInfo"; ValueData: "{#MyAppUpdatesURL}"; Flags: uninsdeletevalue

[CustomMessages]
; English messages
english.AddToPath=Add Shuati CLI to PATH environment variable
english.AddContextMenu=Add "Open Shuati CLI Here" to Windows Explorer context menu
english.CreateDesktopIcon=Create a &desktop icon
english.AdditionalTasks=Additional tasks:
english.LaunchAfterInstall=View Shuati CLI help

; Chinese Simplified messages
chinesesimplified.AddToPath=将 Shuati CLI 添加到 PATH 环境变量
chinesesimplified.AddContextMenu=在 Windows 资源管理器右键菜单中添加"在此处打开 Shuati CLI"
chinesesimplified.CreateDesktopIcon=创建桌面快捷方式(&D)
chinesesimplified.AdditionalTasks=附加任务:
chinesesimplified.LaunchAfterInstall=查看 Shuati CLI 帮助信息

[Code]
const
  WM_SETTINGCHANGE = $001A;
  SMTO_ABORTIFHUNG = $0002;
  
function SendMessageTimeout(hWnd: HWND; Msg: UINT; wParam: Longint; lParam: string; fuFlags, uTimeout: UINT; var lpdwResult: DWORD): UINT;
  external 'SendMessageTimeoutW@user32.dll stdcall';

function NormalizePath(const S: string): string;
begin
  Result := Trim(S);
  if (Length(Result) > 0) and (Result[Length(Result)] = '\') then
    Delete(Result, Length(Result), 1);
end;

function ContainsPath(const PathList: string; const Candidate: string): Boolean;
var
  P: string;
  C: string;
begin
  P := ';' + Lowercase(NormalizePath(PathList)) + ';';
  C := ';' + Lowercase(NormalizePath(Candidate)) + ';';
  Result := Pos(C, P) > 0;
end;

procedure AddPath(const Dir: string);
var
  OldPath: string;
  NewPath: string;
  R: DWORD;
begin
  if not RegQueryStringValue(HKCU, 'Environment', 'Path', OldPath) then
    OldPath := '';

  if not ContainsPath(OldPath, Dir) then begin
    if OldPath = '' then
      NewPath := Dir
    else
      NewPath := OldPath + ';' + Dir;
      
    if RegWriteExpandStringValue(HKCU, 'Environment', 'Path', NewPath) then begin
      SendMessageTimeout(HWND_BROADCAST, WM_SETTINGCHANGE, 0, 'Environment', SMTO_ABORTIFHUNG, 5000, R);
    end;
  end;
end;

procedure RemovePath(const Dir: string);
var
  OldPath: string;
  NewPath: string;
  P: Integer;
  Part: string;
  R: DWORD;
  QDir: string;
begin
  if not RegQueryStringValue(HKCU, 'Environment', 'Path', OldPath) then
    Exit;

  NewPath := '';
  QDir := Lowercase(NormalizePath(Dir));
  
  while OldPath <> '' do begin
    P := Pos(';', OldPath);
    if P = 0 then begin
      Part := OldPath;
      OldPath := '';
    end else begin
      Part := Copy(OldPath, 1, P - 1);
      Delete(OldPath, 1, P);
    end;
    
    if Lowercase(NormalizePath(Part)) <> QDir then begin
      if NewPath = '' then
        NewPath := Part
      else
        NewPath := NewPath + ';' + Part;
    end;
  end;
  
  if RegWriteExpandStringValue(HKCU, 'Environment', 'Path', NewPath) then begin
      SendMessageTimeout(HWND_BROADCAST, WM_SETTINGCHANGE, 0, 'Environment', SMTO_ABORTIFHUNG, 5000, R);
  end;
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if (CurStep = ssPostInstall) and WizardIsTaskSelected('addtopath') then begin
    AddPath(ExpandConstant('{app}'));
  end;
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
  if CurUninstallStep = usUninstall then begin
    RemovePath(ExpandConstant('{app}'));
  end;
end;

function InitializeSetup(): Boolean;
var
  Version: string;
begin
  Result := True;
  
  ; Check if already installed
  if RegQueryStringValue(HKCU, 'Software\Microsoft\Windows\CurrentVersion\Uninstall\{#MyAppName}_is1', 'DisplayVersion', Version) then begin
    Log('Previous version detected: ' + Version);
  end;
end;

function NextButtonClick(CurPageID: Integer): Boolean;
begin
  Result := True;
  
  ; Custom validation can be added here
  case CurPageID of
    wpSelectDir: begin
      ; Validate installation directory
      if Pos(' ', ExpandConstant('{app}')) > 0 then begin
        ; Warn about spaces in path but allow it
        Log('Warning: Installation path contains spaces');
      end;
    end;
  end;
end;
