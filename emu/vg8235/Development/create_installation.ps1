param(
    # Builds the setup without the ROM images (for publishing): the user places them in the ROM folder.
    [switch]$NoRoms
)

$ErrorActionPreference = 'Stop'

$projectRoot = $PSScriptRoot
$solution = Join-Path $projectRoot 'VG8235.sln'
$installerSource = Join-Path $projectRoot 'installer\Installer.cs'
$csc = Join-Path $env:SystemRoot 'Microsoft.NET\Framework64\v4.0.30319\csc.exe'
$msbuild = (Get-Command msbuild.exe -ErrorAction SilentlyContinue).Source

if (-not $msbuild) {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (Test-Path -LiteralPath $vswhere) {
        $vsPath = & $vswhere -latest -products '*' -requires Microsoft.Component.MSBuild -property installationPath
        if ($LASTEXITCODE -ne 0) {
            throw "vswhere failed with exit code $LASTEXITCODE"
        }
        if ($vsPath) {
            $candidate = Join-Path $vsPath 'MSBuild\Current\Bin\MSBuild.exe'
            if (Test-Path -LiteralPath $candidate) {
                $msbuild = $candidate
            }
        }
    }
}

if (-not $msbuild) {
    throw 'MSBuild was not found. Install Visual Studio 2022 with Desktop development with C++.'
}
if (-not (Test-Path -LiteralPath $csc -PathType Leaf)) {
    $csc = Join-Path $env:SystemRoot 'Microsoft.NET\Framework\v4.0.30319\csc.exe'
}
if (-not (Test-Path -LiteralPath $csc -PathType Leaf)) {
    throw 'The .NET Framework C# compiler was not found.'
}

& $msbuild $solution /m /p:Configuration=Release /p:Platform=x64
if ($LASTEXITCODE -ne 0) {
    throw "Release build failed with exit code $LASTEXITCODE"
}

$exe = Join-Path $projectRoot '..\Build\x64\Release\vg8235.exe'
$romDirectory = Join-Path $projectRoot 'VG8235\rom'
$romFiles = @(
    'VG8235-ROM1 - Basic2.0.bin',
    'VG8235-ROM2 - Basic2.0.bin',
    'VG8235-ROM3 - Disc.rom'
)
$distributionRoot = Join-Path $projectRoot '..\Distribution'
$setupPath = Join-Path $distributionRoot 'VG8235-Setup.exe'
if ($NoRoms) {
    $setupPath = Join-Path $distributionRoot 'VG8235-Setup-NoRoms.exe'
    $romFiles = @()
}

foreach ($requiredFile in @($exe, $installerSource)) {
    if (-not (Test-Path -LiteralPath $requiredFile -PathType Leaf)) {
        throw "Required distribution file is missing: $requiredFile"
    }
}
foreach ($romFile in $romFiles) {
    $path = Join-Path $romDirectory $romFile
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Required system ROM is missing: $path"
    }
}

New-Item -ItemType Directory -Path $distributionRoot -Force | Out-Null
$compilerArguments = @(
    '/nologo',
    '/target:winexe',
    "/out:$setupPath",
    '/reference:System.Windows.Forms.dll',
    '/reference:System.Drawing.dll',
    "/resource:$exe,vg8235.exe",
    "/resource:$(Join-Path $projectRoot '..\Distribution\MANUAL.md'),MANUAL.md"
)
foreach ($romFile in $romFiles) {
    $path = Join-Path $romDirectory $romFile
    $compilerArguments += "/resource:$path,$romFile"
}
# Blank disks contain no third-party code and are part of every setup.
foreach ($disk in Get-ChildItem -LiteralPath (Join-Path $projectRoot 'installer\disks') -Filter *.DSK) {
    $compilerArguments += "/resource:$($disk.FullName),disk.$($disk.Name)"
}
$compilerArguments += $installerSource

& $csc @compilerArguments
if ($LASTEXITCODE -ne 0) {
    throw "Setup compilation failed with exit code $LASTEXITCODE"
}
if (-not (Test-Path -LiteralPath $setupPath -PathType Leaf)) {
    throw "The C# compiler did not create the setup executable: $setupPath"
}

Write-Host "Created Windows setup executable: $setupPath"
