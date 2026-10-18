param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Debug',
    [switch]$Build
)

$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
$binDir = Join-Path $root "..\Build\x64\$Configuration"
$rom = Join-Path $root 'SVI328\rom\svi111.rom'
# Disk BASIC image for the optional disk test (medium folder of the MSX projects).
$diskBasic = Join-Path $root '..\..\2100 - MSX - MEDIEN\SVIDSK\Svi328DiskBasicSS\S328DBSS.DSK'
# CP/M 2.24 system disk in the media folder.
$cpm = Join-Path $root '..\..\2100 - MSX - MEDIEN\SVIDSK\Cpm224SS\CPM224SS.DSK'
# CP/M 2.27 for the SVI-608M, double-sided (boots with the SASI hard disk).
$cpm227 = Join-Path $root '..\..\2100 - MSX - MEDIEN\SVIDSK\Cpm227Svi608mDS\CPM227DS.DSK'
$characterRom = Join-Path $root '..\..\svi3x8\SVI806\rom\svi806.rom'
$bootRom = Join-Path $root '..\..\svi3x8\LANIF01\rom\svi609.rom'
$expanderArguments = @($rom, $characterRom,
    $(if (Test-Path $diskBasic) { $diskBasic } else { '-' }),
    $(if (Test-Path $cpm) { $cpm } else { '-' }))
$svi605bhdArguments = @($rom, $(if (Test-Path $cpm227) { $cpm227 } else { '-' }))
$svi608mArguments = @($rom, $characterRom, $bootRom,
    $(if (Test-Path $cpm227) { $cpm227 } else { '-' }))

if ($Build) {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    $msbuild = & $vswhere -latest -requires Microsoft.Component.MSBuild `
        -find 'MSBuild\**\Bin\MSBuild.exe' | Select-Object -First 1
    & $msbuild (Join-Path $root 'SVI328.sln') /m /nologo /v:minimal `
        "/p:Configuration=$Configuration" /p:Platform=x64
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

$tests = @(
    @{ Name = 'z80bus_tests';   Arguments = @();     Timeout = 60 },
    @{ Name = 'videorec_tests'; Arguments = @();     Timeout = 60 },
    @{ Name = 'ppi8255_tests';  Arguments = @();     Timeout = 60 },
    @{ Name = 'ay8910_tests';   Arguments = @();     Timeout = 60 },
    @{ Name = 'tms99x8_tests';  Arguments = @();     Timeout = 60 },
    @{ Name = 'tms9918a_tests'; Arguments = @();     Timeout = 60 },
    @{ Name = 'tms9929a_tests'; Arguments = @();     Timeout = 60 },
    @{ Name = 'printer_tests';  Arguments = @();     Timeout = 60 },
    @{ Name = 'fd1793_tests';   Arguments = @();     Timeout = 120 },
    @{ Name = 'ins8250_tests';  Arguments = @();     Timeout = 60 },
    @{ Name = 'mc6845_tests';   Arguments = @();     Timeout = 60 },
    @{ Name = 'mc6821_tests';   Arguments = @();     Timeout = 60 },
    @{ Name = 'z8530_tests';    Arguments = @();     Timeout = 60 },
    @{ Name = 'wd1002shd_tests'; Arguments = @();    Timeout = 60 },
    @{ Name = 'svi328_tests';   Arguments = @($rom); Timeout = 180 },
    @{ Name = 'svi801_tests'; Arguments = @(); Timeout = 60 },
    @{ Name = 'svi802_tests'; Arguments = @(); Timeout = 60 },
    @{ Name = 'svi803_tests'; Arguments = @(); Timeout = 60 },
    @{ Name = 'svi805_tests'; Arguments = @(); Timeout = 60 },
    @{ Name = 'svi806_tests'; Arguments = @($characterRom); Timeout = 60 },
    @{ Name = 'svi807_tests'; Arguments = @(); Timeout = 60 },
    @{ Name = 'lanhd01_tests'; Arguments = @(); Timeout = 60 },
    @{ Name = 'lanif01_tests'; Arguments = @(); Timeout = 60 },
    @{ Name = 'svi605b_tests';  Arguments = $expanderArguments; Timeout = 240 },
    @{ Name = 'svi608m_tests';   Arguments = $svi608mArguments; Timeout = 240 },
    @{ Name = 'svi609_tests';     Arguments = $svi608mArguments; Timeout = 240 },
    @{ Name = 'svi605bhd_tests';   Arguments = $svi605bhdArguments; Timeout = 240 },
    @{ Name = 'svi602_tests';    Arguments = @($rom); Timeout = 60 },
    @{ Name = 'svi601_tests';    Arguments = @($rom, $(if (Test-Path $diskBasic) { $diskBasic } else { '-' })); Timeout = 240 }
)

$failed = 0
$skipped = 0
foreach ($test in $tests) {
    # Original ROM images are not part of the source; a test whose ROM is missing is skipped.
    $missingRoms = @($test.Arguments | Where-Object { $_ -match '\.(rom|bin)$' -and -not (Test-Path -LiteralPath $_) })
    if ($missingRoms.Count -gt 0) {
        Write-Host ("{0,-22} SKIPPED      ROM missing: {1}" -f $test.Name, (($missingRoms | ForEach-Object { Split-Path $_ -Leaf }) -join ', ')) -ForegroundColor Yellow
        $skipped++
        continue
    }
    $exe = Join-Path $binDir ($test.Name + '.exe')
    if (-not (Test-Path $exe)) {
        Write-Host ("{0,-22} MISSING  {1}" -f $test.Name, $exe) -ForegroundColor Red
        $failed++
        continue
    }
    $info = New-Object System.Diagnostics.ProcessStartInfo
    $info.FileName = $exe
    $info.Arguments = ($test.Arguments | ForEach-Object { '"' + $_ + '"' }) -join ' '
    $info.UseShellExecute = $false
    $info.RedirectStandardOutput = $true
    $info.RedirectStandardError = $true
    $watch = [System.Diagnostics.Stopwatch]::StartNew()
    $process = [System.Diagnostics.Process]::Start($info)
    $stdout = $process.StandardOutput.ReadToEndAsync()
    $stderr = $process.StandardError.ReadToEndAsync()
    if (-not $process.WaitForExit($test.Timeout * 1000)) {
        $process.Kill()
        $process.WaitForExit()
        $result = 'TIMEOUT'
    } elseif ($process.ExitCode -eq 0) {
        $result = 'PASSED'
    } else {
        $result = "FAILED ($($process.ExitCode))"
    }
    $watch.Stop()
    $color = if ($result -eq 'PASSED') { 'Green' } else { 'Red' }
    Write-Host ("{0,-22} {1,-12} {2,7:N1} s" -f $test.Name, $result, $watch.Elapsed.TotalSeconds) `
        -ForegroundColor $color
    if ($result -ne 'PASSED') {
        $failed++
        $output = ($stdout.Result + $stderr.Result).Trim()
        if ($output) { Write-Host $output }
    }
}

Write-Host ("{0} of {1} tests passed, {2} skipped" -f ($tests.Count - $failed - $skipped), $tests.Count, $skipped)
exit $failed
