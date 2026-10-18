; KITSUNE EQ - Windows installer (Inno Setup 6, modern wizard)
; Built by CI:  iscc /DAppVersion=1.0.0 /DBuildDir=<abs path to build\KitsuneEQ_artefacts\Release> /DOutDir=<abs out dir> KitsuneEQ.iss

#ifndef AppVersion
  #define AppVersion "1.0.0"
#endif
#ifndef BuildDir
  #define BuildDir "..\..\build\KitsuneEQ_artefacts\Release"
#endif
#ifndef OutDir
  #define OutDir "..\..\dist"
#endif

#define AppName      "KITSUNE EQ"
#define Publisher    "prodbyperke"
#define Vst3Bundle   "KITSUNE EQ.vst3"

[Setup]
AppId={{6F3B2C0E-4B7A-4E55-9D1A-5A1E0E0B0E01}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher={#Publisher}
VersionInfoVersion={#AppVersion}
DefaultDirName={commonpf64}\{#Publisher}\{#AppName}
DisableDirPage=yes
DefaultGroupName={#Publisher}
DisableProgramGroupPage=yes
OutputDir={#OutDir}
OutputBaseFilename=KitsuneEQ-{#AppVersion}-Windows-Setup
SetupIconFile=KitsuneEQ.ico
UninstallDisplayIcon={app}\KitsuneEQ.ico
UninstallDisplayName={#AppName}
WizardStyle=modern
WizardImageFile=wizard-large.bmp
WizardSmallImageFile=wizard-small.bmp
WizardImageBackColor=$180A0A
Compression=lzma2/ultra
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
CloseApplications=yes
RestartApplications=no
ChangesAssociations=no

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Types]
Name: "full"; Description: "Standard (VST3 plugin)"
Name: "custom"; Description: "Custom"; Flags: iscustom

[Components]
Name: "vst3"; Description: "VST3 plugin  (installs to Common Files\VST3)"; Types: full custom; Flags: fixed
Name: "standalone"; Description: "Standalone application (optional)"; Types: custom

[Files]
; VST3 bundle -> C:\Program Files\Common Files\VST3\KITSUNE EQ.vst3   (the folder every DAW incl. FL Studio scans)
Source: "{#BuildDir}\VST3\{#Vst3Bundle}\*"; DestDir: "{commoncf64}\VST3\{#Vst3Bundle}"; Components: vst3; \
    Flags: ignoreversion recursesubdirs createallsubdirs restartreplace uninsrestartdelete
; Standalone app
Source: "{#BuildDir}\Standalone\{#AppName}.exe"; DestDir: "{app}"; Components: standalone; Flags: ignoreversion restartreplace uninsrestartdelete
; Icon used by the uninstaller entry
Source: "KitsuneEQ.ico"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\{#Publisher}\{#AppName}"; Filename: "{app}\{#AppName}.exe"; IconFilename: "{app}\KitsuneEQ.ico"; Components: standalone
Name: "{autoprograms}\{#Publisher}\Uninstall {#AppName}"; Filename: "{uninstallexe}"

[Messages]
WelcomeLabel2=This will install [name/ver] on your computer.%n%nThe VST3 plugin is installed to the standard folder:%nC:\Program Files\Common Files\VST3%n%nClose your DAW before continuing.
FinishedLabel=KITSUNE EQ has been installed.%n%nFL Studio: Options > Manage plugins > "Find installed plugins" (enable "Verify plugins" for the first scan), then find KITSUNE EQ in the plugin list under prodbyperke.
