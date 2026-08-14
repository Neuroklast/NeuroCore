; NEUROKORE Windows installer (Inno Setup 6).
; Built by scripts/package_windows.ps1 after a Release CMake build.
; Installs the VST3 bundle where hosts scan, plus the Standalone app.

#define MyAppName "NEUROKORE"
#define MyAppVersion "0.9.0"
#define MyAppPublisher "Neuroklast"
#define MyAppURL "https://neuroklast.net"
#define MyAppExeName "NEUROKORE.exe"

#ifndef NcStage
  #define NcStage "..\build\package\stage"
#endif

[Setup]
AppId={{9C3E1B2A-7D54-4F10-9A6E-B0C0DE00A1B2}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
DefaultDirName={autopf}\{#MyAppPublisher}\{#MyAppName}
DefaultGroupName={#MyAppPublisher}
DisableProgramGroupPage=yes
OutputDir=..\build\package
OutputBaseFilename=NEUROKORE-{#MyAppVersion}-Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
PrivilegesRequired=admin
ArchitecturesInstallIn64BitMode=x64
ArchitecturesAllowed=x64
UninstallDisplayIcon={app}\{#MyAppExeName}
LicenseFile=EULA.txt

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Shortcuts:"; Flags: unchecked

[Files]
; VST3 is a folder bundle — hosts look in Common Files\VST3
Source: "{#NcStage}\NEUROKORE.vst3\*"; DestDir: "{commoncf64}\VST3\NEUROKORE.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#NcStage}\{#MyAppExeName}"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#NcStage}\README.txt"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#NcStage}\LICENSE.txt"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#NcStage}\EULA.txt"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#NcStage}\Docs\*"; DestDir: "{app}\Docs"; Flags: ignoreversion recursesubdirs createallsubdirs skipifsourcedoesntexist

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Check: FileExists(ExpandConstant('{app}\{#MyAppExeName}'))
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon; Check: FileExists(ExpandConstant('{app}\{#MyAppExeName}'))

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "Launch {#MyAppName}"; Flags: nowait postinstall skipifsilent skipifdoesntexist
