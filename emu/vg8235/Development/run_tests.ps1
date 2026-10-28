param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Debug',
    [switch]$Build
)

$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
$binDir = Join-Path $root "..\Build\x64\$Configuration"
$romDir = Join-Path $root 'VG8235\rom'

if ($Build) {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    $msbuild = & $vswhere -latest -requires Microsoft.Component.MSBuild `
        -find 'MSBuild\**\Bin\MSBuild.exe' | Select-Object -First 1
    & $msbuild (Join-Path $root 'VG8235.sln') /m /nologo /v:minimal `
        "/p:Configuration=$Configuration" /p:Platform=x64
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

$tests = @(
    @{ Name = 'z80bus_tests';         Arguments = @();     Timeout = 60 },
    @{ Name = 'videorec_tests';       Arguments = @();     Timeout = 60 },
    @{ Name = 'ppi8255_tests';        Arguments = @();     Timeout = 60 },
    @{ Name = 'ay8910_tests';         Arguments = @();     Timeout = 60 },
    @{ Name = 'v9938_tests';          Arguments = @();     Timeout = 60 },
    @{ Name = 'rp5c01_tests';         Arguments = @();     Timeout = 60 },
    @{ Name = 'wd2793_tests';         Arguments = @();     Timeout = 60 },
    @{ Name = 'cartridge_tests';      Arguments = @();     Timeout = 60 },
    @{ Name = 'usbslot_tests';        Arguments = @();     Timeout = 60 },
    @{ Name = 'printer_tests';        Arguments = @();     Timeout = 60 },
    # The boot tests need the system ROMs; without them the test runs everything else.
    @{ Name = 'vg8235_tests';         Arguments = $(if (Test-Path -LiteralPath (Join-Path $romDir 'VG8235-ROM1 - Basic2.0.bin')) { @($romDir) } else { @() }); Timeout = 120 }
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
