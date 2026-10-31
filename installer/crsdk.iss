; instalador do CRS Development Kit (Inno Setup 6). Não rodar direto: o CMake passa as pastas e a versão
;   cmake --build build --config Release --target installer
; SourceDir = pasta montada pelo cmake --install; OutputDir = onde sai o setup.exe

#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif

[Setup]
; identifica a engine no Windows (atualizar por cima, desinstalar): nunca trocar
AppId={{714543FC-5575-4602-AC25-DF378CDA24BD}
AppName=CRS Development Kit
AppVersion={#AppVersion}
AppPublisher=Carlos Rodrigo Schimanski
AppPublisherURL=https://github.com/CarlosRodrigoSchimanski/crs-development-kit
; instala só pro usuário (sem pedir administrador), em %LOCALAPPDATA%\Programs: a engine grava
; cenas, imgui.ini e componentes dentro da própria pasta, e em Arquivos de Programas não poderia
PrivilegesRequired=lowest
DefaultDirName={autopf}\CRS Development Kit
DefaultGroupName=CRS Development Kit
DisableProgramGroupPage=yes
UninstallDisplayIcon={app}\CRSDK.exe
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
SourceDir={#SourceDir}
OutputDir={#OutputDir}
OutputBaseFilename=CRSDK-{#AppVersion}-setup
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern

[Languages]
Name: "pt"; MessagesFile: "compiler:Languages\BrazilianPortuguese.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"

[Files]
; a pasta inteira do cmake --install (exe, Python portátil, runtime do C++, assets, shaders)
Source: "*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\CRS Development Kit"; Filename: "{app}\CRSDK.exe"; WorkingDir: "{app}"
Name: "{group}\{cm:UninstallProgram,CRS Development Kit}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\CRS Development Kit"; Filename: "{app}\CRSDK.exe"; WorkingDir: "{app}"; Tasks: desktopicon

[UninstallDelete]
; criado pela engine ao rodar (posição das janelas do editor), não pelo instalador
Type: files; Name: "{app}\imgui.ini"

[Run]
Filename: "{app}\CRSDK.exe"; Description: "{cm:LaunchProgram,CRS Development Kit}"; WorkingDir: "{app}"; Flags: nowait postinstall skipifsilent
