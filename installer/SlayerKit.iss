#ifndef AppVersion
  #define AppVersion "1.0.0"
#endif
#ifndef BuildDir
  #define BuildDir "..\build"
#endif

[Setup]
AppId={{6B7C2E1A-5D44-4C0B-9E2F-51A8B0D3C7E1}
AppName=Slayer Kit
AppVersion={#AppVersion}
AppVerName=Slayer Kit {#AppVersion}
AppPublisher=yns_bnl
AppPublisherURL=https://github.com/ybyns2/slayer-kit
AppSupportURL=https://github.com/ybyns2/slayer-kit/issues
AppUpdatesURL=https://github.com/ybyns2/slayer-kit/releases
DefaultDirName={autopf}\Slayer Kit
DefaultGroupName=Slayer Kit
DisableProgramGroupPage=yes
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0.19041
OutputDir=..\dist
OutputBaseFilename=SlayerKit-Setup
SetupIconFile=..\res\app.ico
UninstallDisplayIcon={app}\SlayerKit.exe
UninstallDisplayName=Slayer Kit by yns_bnl
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
CloseApplications=force
RestartApplications=no
LicenseFile=..\LICENSE
VersionInfoVersion={#AppVersion}
VersionInfoCompany=yns_bnl
VersionInfoDescription=Installateur Slayer Kit

[Languages]
Name: "fr"; MessagesFile: "compiler:Languages\French.isl"

[Tasks]
Name: "desktopicon"; Description: "Créer un raccourci sur le Bureau"; GroupDescription: "Raccourcis :"

[Files]
Source: "{#BuildDir}\SlayerKit.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\LICENSE"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\Slayer Kit"; Filename: "{app}\SlayerKit.exe"
Name: "{group}\Désinstaller Slayer Kit"; Filename: "{uninstallexe}"
Name: "{autodesktop}\Slayer Kit"; Filename: "{app}\SlayerKit.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\SlayerKit.exe"; Description: "Lancer Slayer Kit"; Flags: nowait postinstall runascurrentuser shellexec

[UninstallRun]
Filename: "{sys}\schtasks.exe"; Parameters: "/Delete /F /TN ""SlayerKit"""; Flags: runhidden; RunOnceId: "RemoveTask"
Filename: "{sys}\taskkill.exe"; Parameters: "/F /IM SlayerKit.exe"; Flags: runhidden; RunOnceId: "KillApp"

[UninstallDelete]
Type: filesandordirs; Name: "{localappdata}\SlayerKit\temp"
