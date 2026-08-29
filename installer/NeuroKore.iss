; NEUROKORE Windows installer (Inno Setup 6).
; Compiled by scripts/package_windows.ps1 after a Release build.
; VST3 always goes to Common Files. Standalone is optional.

#ifndef MyAppVersion
  #define MyAppVersion "0.6.4-beta"
#endif
#ifndef MyAppNumeric
  #define MyAppNumeric "0.6.4"
#endif
#ifndef NcStage
  #define NcStage "..\build\package\stage"
#endif

#define MyAppName "NEUROKORE"
#define MyAppPublisher "Neuroklast"
#define MyAppURL "https://neuroklast.net"
#define MyAppExeName "NEUROKORE-" + MyAppVersion + ".exe"
#define MyVst3Bundle "NEUROKORE.vst3"

[Setup]
AppId={{9C3E1B2A-7D54-4F10-9A6E-B0C0DE00A1B2}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
AppCopyright=Copyright (C) 2024–2026 Neuroklast
VersionInfoVersion={#MyAppNumeric}.0
VersionInfoCompany={#MyAppPublisher}
VersionInfoProductName={#MyAppName}
VersionInfoProductVersion={#MyAppNumeric}
VersionInfoCopyright=Copyright (C) 2024–2026 Neuroklast
VersionInfoDescription={#MyAppName} installer
DefaultDirName={autopf}\{#MyAppPublisher}\{#MyAppName}
DefaultGroupName={#MyAppPublisher}
DisableProgramGroupPage=yes
OutputDir=..\build\package
OutputBaseFilename=NEUROKORE-{#MyAppVersion}-Setup
SetupIconFile=..\resources\img\nk_logo.ico
UninstallDisplayIcon={app}\{#MyAppExeName}
UninstallDisplayName={#MyAppName}
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
WizardSizePercent=120
PrivilegesRequired=admin
ArchitecturesInstallIn64BitMode=x64compatible
ArchitecturesAllowed=x64compatible
MinVersion=10.0
LicenseFile=EULA.txt
CloseApplications=yes
RestartApplications=no
UsePreviousAppDir=yes
SetupLogging=yes

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "german"; MessagesFile: "compiler:Languages\German.isl"

[CustomMessages]
english.TypeFull=Full installation
english.TypePlugin=VST3 only
english.TypeCustom=Custom
english.CompVst3=VST3 plug-in (64-bit DAWs)
english.CompStandalone=Standalone application
english.NeedComponent=Select at least the VST3 plug-in or the standalone app.
english.WebView2Installing=Installing Microsoft Edge WebView2…
english.WebView2FailStart=Could not start the WebView2 installer. Install Microsoft Edge WebView2 Runtime from Microsoft, then run this setup again.
english.WebView2FailCode=WebView2 Runtime setup failed. Install it from https://go.microsoft.com/fwlink/p/?LinkId=2124703 and run this setup again.
english.WebView2Missing=NEUROKORE needs Microsoft Edge WebView2. Download it from Microsoft, install it, then run this setup again.
english.TaskDesktop=Create a desktop shortcut
german.TypeFull=Vollständige Installation
german.TypePlugin=Nur VST3
german.TypeCustom=Benutzerdefiniert
german.CompVst3=VST3-Plug-in (64-Bit-DAWs)
german.CompStandalone=Standalone-Anwendung
german.NeedComponent=Wähle mindestens das VST3-Plug-in oder die Standalone-App.
german.WebView2Installing=Microsoft Edge WebView2 wird installiert…
german.WebView2FailStart=WebView2-Installer konnte nicht starten. Microsoft Edge WebView2 Runtime installieren, dann Setup erneut ausführen.
german.WebView2FailCode=WebView2-Setup fehlgeschlagen. Von https://go.microsoft.com/fwlink/p/?LinkId=2124703 installieren, dann Setup erneut ausführen.
german.WebView2Missing=NEUROKORE braucht Microsoft Edge WebView2. Von Microsoft installieren, dann Setup erneut ausführen.
german.TaskDesktop=Desktopverknüpfung anlegen

[Types]
Name: "full"; Description: "{cm:TypeFull}"
Name: "plugin"; Description: "{cm:TypePlugin}"
Name: "custom"; Description: "{cm:TypeCustom}"; Flags: iscustom

[Components]
Name: "vst3"; Description: "{cm:CompVst3}"; Types: full plugin custom
Name: "standalone"; Description: "{cm:CompStandalone}"; Types: full custom

[Tasks]
Name: "desktopicon"; Description: "{cm:TaskDesktop}"; GroupDescription: "{cm:AdditionalIcons}"; Components: standalone; Flags: unchecked

[Files]
Source: "{#NcStage}\{#MyVst3Bundle}\*"; DestDir: "{commoncf64}\VST3\{#MyVst3Bundle}"; Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs restartreplace uninsrestartdelete
Source: "{#NcStage}\{#MyAppExeName}"; DestDir: "{app}"; Components: standalone; Flags: ignoreversion restartreplace
Source: "{#NcStage}\resources\*"; DestDir: "{app}\resources"; Components: standalone; Flags: ignoreversion recursesubdirs createallsubdirs skipifsourcedoesntexist
Source: "{#NcStage}\README.txt"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#NcStage}\LICENSE.txt"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#NcStage}\EULA.txt"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#NcStage}\Docs\*"; DestDir: "{app}\Docs"; Flags: ignoreversion recursesubdirs createallsubdirs skipifsourcedoesntexist
Source: "MicrosoftEdgeWebView2RuntimeInstallerX64.exe"; DestDir: "{tmp}"; Flags: dontcopy

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Components: standalone; Check: FileExists(ExpandConstant('{app}\{#MyAppExeName}'))
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Components: standalone; Tasks: desktopicon; Check: FileExists(ExpandConstant('{app}\{#MyAppExeName}'))
Name: "{group}\Uninstall {#MyAppName}"; Filename: "{uninstallexe}"

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#MyAppName}}"; Components: standalone; Flags: nowait postinstall skipifsilent skipifdoesntexist

[UninstallDelete]
; Keep %APPDATA%\NEUROKLAST\NEUROKORE (license, demo stamp, user presets).

[Code]
function WebView2Installed: Boolean;
begin
  Result :=
    RegKeyExists(HKLM, 'SOFTWARE\WOW6432Node\Microsoft\EdgeUpdate\Clients\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}') or
    RegKeyExists(HKLM, 'SOFTWARE\Microsoft\EdgeUpdate\Clients\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}') or
    RegKeyExists(HKCU, 'SOFTWARE\Microsoft\EdgeUpdate\Clients\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}') or
    RegKeyExists(HKCU, 'SOFTWARE\WOW6432Node\Microsoft\EdgeUpdate\Clients\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}');
end;

function NextButtonClick(CurPageID: Integer): Boolean;
begin
  Result := True;
  if CurPageID = wpSelectComponents then
  begin
    if (not WizardIsComponentSelected('vst3')) and (not WizardIsComponentSelected('standalone')) then
    begin
      MsgBox(ExpandConstant('{cm:NeedComponent}'), mbError, MB_OK);
      Result := False;
    end;
  end;
end;

procedure RemoveOldVersionedBundles;
var
  FindRec: TFindRec;
  Base: String;
begin
  Base := ExpandConstant('{commoncf64}\VST3\');
  if FindFirst(Base + 'NEUROKORE-*.vst3', FindRec) then
  try
    repeat
      if FindRec.Attributes and FILE_ATTRIBUTE_DIRECTORY <> 0 then
        DelTree(Base + FindRec.Name, True, True, True);
    until not FindNext(FindRec);
  finally
    FindClose(FindRec);
  end;
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssInstall then
    RemoveOldVersionedBundles;
end;

function PrepareToInstall(var NeedsRestart: Boolean): String;
var
  ResultCode: Integer;
  Bootstrapper: String;
begin
  Result := '';
  if WebView2Installed then
    Exit;
  WizardForm.StatusLabel.Caption := ExpandConstant('{cm:WebView2Installing}');
  try
    ExtractTemporaryFile('MicrosoftEdgeWebView2RuntimeInstallerX64.exe');
  except
  end;
  Bootstrapper := ExpandConstant('{tmp}\MicrosoftEdgeWebView2RuntimeInstallerX64.exe');
  if FileExists(Bootstrapper) then
  begin
    if not Exec(Bootstrapper, '/silent /install', '', SW_HIDE, ewWaitUntilTerminated, ResultCode) then
    begin
      Result := ExpandConstant('{cm:WebView2FailStart}');
      Exit;
    end;
    if (ResultCode <> 0) and (not WebView2Installed) then
    begin
      Result := ExpandConstant('{cm:WebView2FailCode}');
      Exit;
    end;
  end
  else
    Result := ExpandConstant('{cm:WebView2Missing}');
end;
