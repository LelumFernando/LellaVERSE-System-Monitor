; ═══════════════════════════════════════════════════════════════
; LellaVERSE System Monitor - Inno Setup Script
; Production Windows Installer for Universal Edition
; ═══════════════════════════════════════════════════════════════

#define MyAppName "LellaVERSE System Monitor"
#define MyAppVersion "2.1.0"
#define MyAppPublisher "LellaVERSE"
#define MyAppURL "https://github.com/lellaverse/LellaVERSE_System_Monitor"
#define MyAppExeName "LellaVERSE_System_Monitor.exe"

[Setup]
AppId={{C7B19D54-4E83-4E11-9A38-1E7C1D821C42}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
AllowNoIcons=yes
OutputDir=..\release
OutputBaseFilename=LellaVERSE_System_Monitor_Setup
SetupIconFile=..\resources\app.ico
UninstallDisplayIcon={app}\{#MyAppExeName}
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
PrivilegesRequired=admin
ArchitecturesInstallIn64BitMode=x64compatible
CloseApplications=yes
RestartApplications=no

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked
Name: "startupicon"; Description: "Automatically start LellaVERSE System Monitor on logon"; GroupDescription: "Startup options:"

[Files]
Source: "..\release\{#MyAppExeName}"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\resources\app.ico"; DestDir: "{app}\resources"; Flags: ignoreversion
Source: "..\resources\icon.png"; DestDir: "{app}\resources"; Flags: ignoreversion

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; IconFilename: "{app}\resources\app.ico"
Name: "{group}\{cm:UninstallProgram,{#MyAppName}}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; IconFilename: "{app}\resources\app.ico"; Tasks: desktopicon

[Run]
; Auto-register elevated Scheduled Task for logon startup if task selected
Filename: "schtasks.exe"; Parameters: "/create /sc onlogon /rl highest /tn ""LellaVERSE System Monitor"" /tr ""\""{app}\{#MyAppExeName}\"""" /f"; Flags: runhidden; Tasks: startupicon
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent

[UninstallRun]
Filename: "schtasks.exe"; Parameters: "/delete /tn ""LellaVERSE System Monitor"" /f"; Flags: runhidden
