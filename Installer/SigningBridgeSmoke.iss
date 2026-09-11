#ifndef SmokePayload
#error SmokePayload must identify the disposable smoke-test payload.
#endif

[Setup]
AppId={{D23051DC-AC48-4B3F-9409-7373B299397D}
AppName=Lean Beeftext Signing Bridge Smoke
AppVersion=1.0.0
AppPublisher=Lean Beeftext signing validation
DefaultDirName={autopf}\Lean Beeftext Signing Bridge Smoke
DisableProgramGroupPage=yes
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
OutputDir=_output
OutputBaseFilename=Lean-Beeftext-Signing-Bridge-Smoke
Compression=lzma2
SolidCompression=yes
Uninstallable=yes
SignTool=leanartifact
SignedUninstaller=yes

[Files]
Source: "{#SmokePayload}"; DestDir: "{app}"; Flags: ignoreversion
