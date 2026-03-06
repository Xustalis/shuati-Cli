; Shuati CLI Installer Script (Inno Setup 6)

#define MyAppName "Shuati CLI"
#define MyAppPublisher "Shuati CLI Team"
#define MyAppURL "https://github.com/Xustalis/shuati-Cli"
#define MyAppSupportURL "https://github.com/Xustalis/shuati-Cli/issues"
#define MyAppUpdatesURL "https://github.com/Xustalis/shuati-Cli/releases"

#if !Defined(MyAppVersion)
  #define EnvVersion GetEnv('SHUATI_VERSION')
  #if EnvVersion != ""
    #define MyAppVersion EnvVersion
  #else
    #define MyAppVersion "0.1.0"
  #endif
#endif

#if !Defined(MyOutputBaseFilename)
  #define EnvOutName GetEnv('SHUATI_OUTPUT_NAME')
  #if EnvOutName != ""
    #define MyOutputBaseFilename EnvOutName
  #else
    #define MyOutputBaseFilename "shuati-setup-x64-v" + MyAppVersion
  #endif
#endif

#if !Defined(SourceDir)
  #define EnvSourceDir GetEnv('SHUATI_SOURCE_DIR')
  #if EnvSourceDir != ""
    #define SourceDir EnvSourceDir
  #else
    #define SourceDir "..\dist"
  #endif
#endif

[Setup]
AppId={{E2A00C8F-5C4D-4B2D-9BCE-3B3B2F4F6A9E}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppSupportURL}
AppUpdatesURL={#MyAppUpdatesURL}

DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=no

PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible

OutputDir=.
OutputBaseFilename={#MyOutputBaseFilename}

Compression=lzma2/ultra64
SolidCompression=yes
LZMAUseSeparateProcess=yes
LZMANumBlockThreads=4

ChangesEnvironment=yes
SetupLogging=yes

WizardStyle=modern
WizardSizePercent=120
WizardResizable=yes

VersionInfoVersion={#MyAppVersion}
VersionInfoCompany={#MyAppPublisher}
VersionInfoDescription={#MyAppName} Setup
VersionInfoTextVersion={#MyAppVersion}
VersionInfoCopyright=Copyright (C) 2025-2026 {#MyAppPublisher}

AllowNoIcons=yes
AllowUNCPath=no
MinVersion=10.0
UninstallDisplayName={#MyAppName}
UninstallFilesDir={app}\uninstall
LanguageDetectionMethod=uilanguage

[Languages]
Name: "en"; MessagesFile: "compiler:Default.isl"
Name: "zh_CN"; MessagesFile: "{#SourcePath}\Languages\ChineseSimplified.isl"

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

[Files]
Source: "{#SourceDir}\shuati.exe"; DestDir: "{app}"; Flags: ignoreversion; Components: main
Source: "{#SourceDir}\resources\*"; DestDir: "{app}\resources"; Flags: ignoreversion recursesubdirs createallsubdirs; Components: resources
Source: "{#SourceDir}\README.md"; DestDir: "{app}"; Flags: ignoreversion; Components: docs
Source: "{#SourceDir}\CHANGELOG.md"; DestDir: "{app}"; Flags: ignoreversion; Components: docs
Source: "..\LICENSE"; DestDir: "{app}"; Flags: ignoreversion; Components: docs
Source: "{#SourceDir}\*.dll"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist; Components: main

[Icons]
Name: "{autoprograms}\{#MyAppName}"; Filename: "{app}\shuati.exe"; Comment: "Shuati CLI - Algorithm Practice Tool"

[Registry]
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Uninstall\{#MyAppName}_is1"; ValueType: string; ValueName: "DisplayIcon"; ValueData: "{app}\shuati.exe"; Flags: uninsdeletevalue
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Uninstall\{#MyAppName}_is1"; ValueType: string; ValueName: "Publisher"; ValueData: "{#MyAppPublisher}"; Flags: uninsdeletevalue
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Uninstall\{#MyAppName}_is1"; ValueType: string; ValueName: "URLInfoAbout"; ValueData: "{#MyAppURL}"; Flags: uninsdeletevalue
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Uninstall\{#MyAppName}_is1"; ValueType: string; ValueName: "HelpLink"; ValueData: "{#MyAppSupportURL}"; Flags: uninsdeletevalue
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Uninstall\{#MyAppName}_is1"; ValueType: string; ValueName: "URLUpdateInfo"; ValueData: "{#MyAppUpdatesURL}"; Flags: uninsdeletevalue

[CustomMessages]
AddToPath=Add to PATH environment variable
zh_CN.AddToPath=添加到 PATH 环境变量
AdditionalTasks=Additional tasks:
zh_CN.AdditionalTasks=附加任务:

[Code]
const
  WM_SETTINGCHANGE = $001A;
  SMTO_ABORTIFHUNG = $0002;

function SendMessageTimeout(hWnd: HWND; Msg: UINT; wParam: Longint; lParam: string; fuFlags, uTimeout: UINT; var lpdwResult: DWORD): UINT;
  external 'SendMessageTimeoutW@user32.dll stdcall';

procedure BroadcastEnvChange();
var
  R: DWORD;
begin
  SendMessageTimeout(HWND_BROADCAST, WM_SETTINGCHANGE, 0, 'Environment', SMTO_ABORTIFHUNG, 5000, R);
end;

function NormalizePath(const S: string): string;
begin
  Result := Trim(S);
  if (Length(Result) > 0) and (Result[Length(Result)] = '\') then
    Delete(Result, Length(Result), 1);
end;

function ContainsPath(const PathList, Candidate: string): Boolean;
begin
  Result := Pos(';' + Lowercase(NormalizePath(Candidate)) + ';',
                ';' + Lowercase(NormalizePath(PathList)) + ';') > 0;
end;

procedure ModifyPath(const Dir: string; DoAdd: Boolean);
var
  OldPath, NewPath, Part: string;
  P: Integer;
  NDir: string;
  R: DWORD;
begin
  if not RegQueryStringValue(HKCU, 'Environment', 'Path', OldPath) then
    OldPath := '';

  NDir := Lowercase(NormalizePath(Dir));

  if DoAdd then begin
    if not ContainsPath(OldPath, Dir) then begin
      if OldPath = '' then
        NewPath := Dir
      else
        NewPath := OldPath + ';' + Dir;
      RegWriteExpandStringValue(HKCU, 'Environment', 'Path', NewPath);
      BroadcastEnvChange();
    end;
  end else begin
    NewPath := '';
    while OldPath <> '' do begin
      P := Pos(';', OldPath);
      if P = 0 then begin
        Part := OldPath;
        OldPath := '';
      end else begin
        Part := Copy(OldPath, 1, P - 1);
        Delete(OldPath, 1, P);
      end;
      if Lowercase(NormalizePath(Part)) <> NDir then begin
        if NewPath = '' then
          NewPath := Part
        else
          NewPath := NewPath + ';' + Part;
      end;
    end;
    RegWriteExpandStringValue(HKCU, 'Environment', 'Path', NewPath);
    BroadcastEnvChange();
  end;
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if (CurStep = ssPostInstall) and WizardIsTaskSelected('addtopath') then
    ModifyPath(ExpandConstant('{app}'), True);
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
  if CurUninstallStep = usUninstall then
    ModifyPath(ExpandConstant('{app}'), False);
end;

function InitializeSetup(): Boolean;
var
  Version: string;
begin
  Result := True;
  if RegQueryStringValue(HKCU,
    'Software\Microsoft\Windows\CurrentVersion\Uninstall\{#MyAppName}_is1',
    'DisplayVersion', Version) then
    Log('Upgrading from: ' + Version);
end;
