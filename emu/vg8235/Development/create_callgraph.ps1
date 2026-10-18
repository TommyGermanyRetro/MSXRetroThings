param()

# Builds the function tree of the emulator with Doxygen and Graphviz: CallGraph\html\index.html shows every
# function of the machine, the frontend and the components with its call and caller graph.

$ErrorActionPreference = 'Stop'
$developmentRoot = $PSScriptRoot

$doxygen = (Get-Command doxygen.exe -ErrorAction SilentlyContinue).Source
if (-not $doxygen) {
    $candidate = Join-Path $env:ProgramFiles 'doxygen\bin\doxygen.exe'
    if (Test-Path -LiteralPath $candidate) { $doxygen = $candidate }
}
$dot = (Get-Command dot.exe -ErrorAction SilentlyContinue).Source
if (-not $dot) {
    $candidate = Join-Path $env:ProgramFiles 'Graphviz\bin\dot.exe'
    if (Test-Path -LiteralPath $candidate) { $dot = $candidate }
}
if (-not $doxygen) { throw 'Doxygen was not found. Install it with: winget install DimitriVanHeesch.Doxygen' }
if (-not $dot) { throw 'Graphviz was not found. Install it with: winget install Graphviz.Graphviz' }

$output = Join-Path $developmentRoot 'CallGraph\html'
if (Test-Path -LiteralPath $output) { Remove-Item -LiteralPath $output -Recurse -Force }

$env:GRAPHVIZ_BIN = Split-Path -Parent $dot
Push-Location $developmentRoot
try {
    $ErrorActionPreference = 'Continue'
    & $doxygen 'CallGraph\Doxyfile' 2>&1 | ForEach-Object { Write-Host $_ }
    if ($LASTEXITCODE -ne 0) { throw "Doxygen failed with exit code $LASTEXITCODE" }
}
finally {
    Pop-Location
}

$index = Join-Path $output 'index.html'
if (-not (Test-Path -LiteralPath $index)) { throw "Doxygen did not create $index" }

# Doxygen_index.html in the Development folder opens the function tree.
$title = (Select-String -LiteralPath (Join-Path $developmentRoot 'CallGraph\Doxyfile') -Pattern '^PROJECT_NAME\s*=\s*"(.*)"').Matches[0].Groups[1].Value
$entry = Join-Path $developmentRoot 'Doxygen_index.html'
$page = @"
<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta http-equiv="refresh" content="0; url=CallGraph/html/index.html">
<title>$title - function tree</title>
</head>
<body>
<p><a href="CallGraph/html/index.html">$title - function tree</a></p>
</body>
</html>
"@
[System.IO.File]::WriteAllText($entry, $page, (New-Object System.Text.UTF8Encoding($false)))
Write-Host "Created function tree: $entry"
