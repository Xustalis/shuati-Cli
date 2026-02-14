#if !Defined(MyAppVersion)
  #define EnvVersion GetEnv('SHUATI_VERSION')
  #if EnvVersion != ""
    #define MyAppVersion EnvVersion
  #else
    #define MyAppVersion "0.0.0"
  #endif
#endif

#if !Defined(MyOutputBaseFilename)
  #define EnvOutName GetEnv('SHUATI_OUTPUT_NAME')
  #if EnvOutName != ""
    #define MyOutputBaseFilename EnvOutName
  #else
    #define MyOutputBaseFilename "shuati-cli-setup"
  #endif
#endif

[Setup]
AppId={{E2A00C8F-5C4D-4B2D-9BCE-3B3B2F4F6A9E}
AppName=Shuati CLI
AppVersion={#MyAppVersion}
AppPublisher=Shuati CLI
DefaultDirName={autopf}\shuati-cli
PrivilegesRequired=lowest
DisableProgramGroupPage=yes
OutputDir=..
OutputBaseFilename={#MyOutputBaseFilename}
Compression=lzma
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
WizardStyle=modern

[Tasks]
Name: addtopath; Description: Add shuati-cli to PATH
Name: contextmenu; Description: "Add 'Open Shuati CLI Here' to context menu"; Flags: unchecked

[Files]
Source: "..\dist\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\Shuati CLI"; Filename: "{app}\shuati.exe"

[Run]
Filename: "{cmd}"; Parameters: "/k ""{app}\shuati.exe"" --help"; Flags: postinstall nowait skipifsilent

[Registry]
Root: HKCU; Subkey: "Software\Classes\Directory\Background\shell\Shuati CLI"; ValueType: string; ValueName: ""; ValueData: "Open Shuati CLI Here"; Tasks: contextmenu; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\Classes\Directory\Background\shell\Shuati CLI\icon"; ValueType: string; ValueName: ""; ValueData: "{app}\shuati.exe"; Tasks: contextmenu; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\Classes\Directory\Background\shell\Shuati CLI\command"; ValueType: string; ValueName: ""; ValueData: "cmd.exe /k ""{app}\shuati.exe"""; Tasks: contextmenu; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\Classes\Directory\shell\Shuati CLI"; ValueType: string; ValueName: ""; ValueData: "Open Shuati CLI Here"; Tasks: contextmenu; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\Classes\Directory\shell\Shuati CLI\icon"; ValueType: string; ValueName: ""; ValueData: "{app}\shuati.exe"; Tasks: contextmenu; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\Classes\Directory\shell\Shuati CLI\command"; ValueType: string; ValueName: ""; ValueData: "cmd.exe /k ""pushd ""%V"" && ""{app}\shuati.exe"""""; Tasks: contextmenu; Flags: uninsdeletekey

; Removed [Registry] entry for PATH to prevent Inno Setup from restoring the old PATH on uninstall, 
; which would wipe out changes made by other installers in the meantime. 
; Path management is now handled entirely in [Code].

[Code]
const
  WM_SETTINGCHANGE = $001A;
  SMTO_ABORTIFHUNG = $0002;

function SendMessageTimeout(hWnd: HWND; Msg: UINT; wParam: Longint; lParam: string; fuFlags, uTimeout: UINT; var lpdwResult: DWORD): UINT;
  external 'SendMessageTimeoutW@user32.dll stdcall';

function NormalizePath(const S: string): string;
begin
  Result := Trim(S);
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
