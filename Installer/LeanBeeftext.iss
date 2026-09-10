#define MyAppName "Lean Beeftext"
#define MyAppVersion "1.0.0"
#define MyAppPublisher "Jubal Slone"
#define MyAppURL "https://github.com/jubalslone/lean-beeftext"
#define MyAppExeName "LeanBeeftext.exe"

[Setup]
AppId={{499E5EE9-ECC6-455E-B78A-EDF581715A80}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL=https://github.com/jubalslone/lean-beeftext/issues
AppUpdatesURL=https://github.com/jubalslone/lean-beeftext/releases
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

[Messages]
ConfirmUninstall=Are you sure you want to remove Lean Beeftext and its installed components?%n%nYour Lean Beeftext user data will not be removed.
UninstalledAll=Lean Beeftext was successfully removed.%n%nYour user data was kept.

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
	LeanProcessNotRunning = 0;
	LeanProcessRunning = 1;
	LeanProcessStateUnknown = 2;

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

{ XMiLib's single-instance key is implemented with QSharedMemory, not a Windows named mutex,
  so Inno AppMutex cannot observe it. Query only the exact installed executable path instead. }
function InstalledLeanProcessState(): Integer;
var
	Locator: Variant;
	Services: Variant;
	Processes: Variant;
	Process: Variant;
	ProcessPathValue: Variant;
	ProcessPath: String;
	ExpectedPath: String;
	Index: Integer;
begin
	Result := LeanProcessStateUnknown;
	ExpectedPath := ExpandConstant('{app}\{#MyAppExeName}');
	try
		Locator := CreateOleObject('WbemScripting.SWbemLocator');
		Services := Locator.ConnectServer('.', 'root\CIMV2');
		Processes := Services.ExecQuery(
			'SELECT ExecutablePath FROM Win32_Process WHERE Name = ''{#MyAppExeName}''');
		Result := LeanProcessNotRunning;
		for Index := 0 to Processes.Count - 1 do
		begin
			Process := Processes.ItemIndex(Index);
			ProcessPathValue := Process.ExecutablePath;
			if VarIsNull(ProcessPathValue) then
			begin
				Log('Could not determine the full path of a running {#MyAppExeName}; uninstall fails closed.');
				Result := LeanProcessStateUnknown;
				exit;
			end;
			ProcessPath := ProcessPathValue;
			if SameText(ProcessPath, ExpectedPath) then
			begin
				Result := LeanProcessRunning;
				exit;
			end;
		end;
	except
		Log('Could not query running Lean Beeftext processes: ' + GetExceptionMessage);
		Result := LeanProcessStateUnknown;
	end;
end;

function InitializeUninstall(): Boolean;
var
	ProcessState: Integer;
begin
	Result := False;
	while True do
	begin
		ProcessState := InstalledLeanProcessState();
		if ProcessState = LeanProcessNotRunning then
		begin
			Result := True;
			exit;
		end;
		if UninstallSilent() then
		begin
			Log('Silent uninstall aborted because the installed Lean Beeftext process is running or could not be checked safely.');
			exit;
		end;
		if SuppressibleMsgBox(
			'Lean Beeftext is currently running.' + #13#10 + #13#10 +
			'Please close Lean Beeftext before uninstalling it.',
			mbError, MB_RETRYCANCEL, IDCANCEL) <> IDRETRY then
			exit;
	end;
end;
