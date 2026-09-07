#define MyAppName "Lean Beeftext"
#define MyAppVersion "1.0.0"
#define MyAppPublisher "Jubal Slone"
#define MyAppURL "https://github.com/jubalslone/Beeftext"
#define MyAppExeName "LeanBeeftext.exe"

[Setup]
AppId={{499E5EE9-ECC6-455E-B78A-EDF581715A80}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL=https://github.com/jubalslone/Beeftext/issues
AppUpdatesURL=https://github.com/jubalslone/Beeftext/releases
DefaultDirName={autopf}\Lean Beeftext
DefaultGroupName=Lean Beeftext
DisableProgramGroupPage=yes
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
OutputDir=_output
OutputBaseFilename=Lean-Beeftext-Setup-1.0.0
SetupIconFile=..\Beeftext\Resources\Icons\LeanBeeftextApp.ico
UninstallDisplayIcon={app}\{#MyAppExeName}
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
CloseApplications=yes
CloseApplicationsFilter={#MyAppExeName}
ForceCloseApplications=no
RestartApplications=no
RestartIfNeededByRun=no
ChangesAssociations=no
ChangesEnvironment=no
UsePreviousAppDir=yes
SetupLogging=yes
VersionInfoVersion=1.0.0.0
VersionInfoCompany={#MyAppPublisher}
VersionInfoDescription={#MyAppName} installer
VersionInfoProductName={#MyAppName}
VersionInfoProductVersion={#MyAppVersion}

[Tasks]
Name: "desktopicon"; Description: "Create a &desktop shortcut"; GroupDescription: "Additional shortcuts:"; Flags: unchecked

[Files]
Source: "_staging\installed\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\Lean Beeftext"; Filename: "{app}\{#MyAppExeName}"
Name: "{autodesktop}\Lean Beeftext"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "Launch Lean Beeftext"; Flags: nowait postinstall skipifsilent runasoriginaluser

[Code]
const
	LeanUninstallKey = 'Software\Microsoft\Windows\CurrentVersion\Uninstall\{499E5EE9-ECC6-455E-B78A-EDF581715A80}_is1';

function NextVersionPart(var Remaining: String): Integer;
var
	DotPosition: Integer;
	Part: String;
begin
	DotPosition := Pos('.', Remaining);
	if DotPosition = 0 then
	begin
		Part := Remaining;
		Remaining := '';
	end
	else
	begin
		Part := Copy(Remaining, 1, DotPosition - 1);
		Delete(Remaining, 1, DotPosition);
	end;
	Result := StrToIntDef(Part, 0);
end;

function CompareVersions(LeftVersion, RightVersion: String): Integer;
var
	Index: Integer;
	LeftPart: Integer;
	RightPart: Integer;
begin
	Result := 0;
	for Index := 1 to 4 do
	begin
		LeftPart := NextVersionPart(LeftVersion);
		RightPart := NextVersionPart(RightVersion);
		if LeftPart < RightPart then
		begin
			Result := -1;
			exit;
		end;
		if LeftPart > RightPart then
		begin
			Result := 1;
			exit;
		end;
	end;
end;

function InitializeSetup(): Boolean;
var
	InstalledVersion: String;
begin
	Result := True;
	if RegQueryStringValue(HKLM64, LeanUninstallKey, 'DisplayVersion', InstalledVersion) and
		(CompareVersions(InstalledVersion, '{#MyAppVersion}') > 0) then
	begin
		MsgBox('A newer version of Lean Beeftext (' + InstalledVersion + ') is already installed. Setup will not downgrade it to {#MyAppVersion}.',
			mbError, MB_OK);
		Result := False;
	end;
end;
