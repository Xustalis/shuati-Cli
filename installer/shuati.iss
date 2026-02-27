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
Name: "contextmenu"; Description: "{cm:AddContextMenu}"; GroupDescription: "{cm:AdditionalTasks}"; Flags: unchecked

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

[Tasks]
Name: "addtopath"; Description: "{cm:AddToPath}"; GroupDescription: "{cm:AdditionalTasks}"
Name: "contextmenu"; Description: "{cm:AddContextMenu}"; GroupDescription: "{cm:AdditionalTasks}"; Flags: unchecked
