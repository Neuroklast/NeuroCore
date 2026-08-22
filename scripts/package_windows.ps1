# Stage a clean Windows kit and compile the Inno Setup installer.
# Usage (repo root, after Release NeuroKore_All):
#   powershell -File scripts/package_windows.ps1
# Fails if Inno Setup 6 or the WebView2 bootstrapper cannot be obtained.
# Optional signing: NEUROKORE_SIGN_PFX + NEUROKORE_SIGN_PASSWORD, or NEUROKORE_SIGN_THUMBPRINT.

$ErrorActionPreference = "Stop"
if (-not $PSScriptRoot) { $PSScriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path }
$root = Split-Path -Parent $PSScriptRoot
$version = "0.6.0-beta"
$numeric = "0.6.0"
$stem = "NEUROKORE-$version"
$bundleName = "NEUROKORE.vst3"
$art = Join-Path $root "build\NeuroKore_artefacts\Release"
$stage = Join-Path $root "build\package\stage"
$out = Join-Path $root "build\package"
$dist = Join-Path $root $stem
$sign = Join-Path $root "scripts\sign_windows.ps1"

function Reset-Dir([string]$path) {
    if (Test-Path $path) { Remove-Item -Recurse -Force $path }
    New-Item -ItemType Directory -Force -Path $path | Out-Null
}

function Find-Iscc {
    @(
        "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe",
        "${env:ProgramFiles}\Inno Setup 6\ISCC.exe",
        "$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe"
    ) | Where-Object { Test-Path $_ } | Select-Object -First 1
}

function Invoke-Sign([string]$path) {
    if (Test-Path $sign) {
        & $sign -Path $path
    }
}

$issText = Get-Content (Join-Path $root "installer\NeuroKore.iss") -Raw
if ($issText -notmatch 'VST3\\\{#MyVst3Bundle\}' -and $issText -notmatch 'VST3\\NEUROKORE\.vst3') {
    throw "installer/NeuroKore.iss must install to VST3\NEUROKORE.vst3 (stable bundle name)."
}
if ($issText -match 'NEUROKORE-0\.[0-9].*\\.vst3') {
    throw "installer/NeuroKore.iss still has a versioned VST3 DestDir."
}

$iscc = Find-Iscc
if (-not $iscc) {
    throw "Inno Setup 6 (ISCC.exe) not found. Install it, then re-run. https://jrsoftware.org/isinfo.php"
}

$srcBundle = $null
foreach ($cand in @(
        (Join-Path $art "VST3\$bundleName"),
        (Join-Path $art "VST3\$stem.vst3")
    )) {
    if (Test-Path $cand) { $srcBundle = $cand; break }
}
if (-not $srcBundle) {
    throw "VST3 bundle not found under $art\VST3 - build Release NeuroKore_All first."
}

$inner = $null
$binDir = Join-Path $srcBundle "Contents\x86_64-win"
if (Test-Path $binDir) {
    $inner = Get-ChildItem $binDir -Filter "$stem.vst3" -File | Select-Object -First 1
    if (-not $inner) {
        $inner = Get-ChildItem $binDir -Filter "*.vst3" -File | Sort-Object LastWriteTime -Descending | Select-Object -First 1
    }
}
if (-not $inner) {
    throw "No VST3 binary inside $srcBundle\Contents\x86_64-win"
}

$exe = $null
foreach ($cand in @(
        (Join-Path $art "Standalone\$stem.exe"),
        (Join-Path $art "Standalone\NEUROKORE.exe")
    )) {
    if (Test-Path $cand) { $exe = $cand; break }
}

Write-Host "Root: $root"
Write-Host "VST3 binary: $($inner.FullName)"
if ($exe) { Write-Host "Standalone: $exe" }

Reset-Dir $stage
New-Item -ItemType Directory -Force -Path $out | Out-Null

$stageBundle = Join-Path $stage $bundleName
$stageBin = Join-Path $stageBundle "Contents\x86_64-win"
New-Item -ItemType Directory -Force -Path $stageBin | Out-Null
Copy-Item -Force $inner.FullName (Join-Path $stageBin $inner.Name)

$resSrc = Join-Path $root "resources"
$stageRes = Join-Path $stageBin "resources"
if (Test-Path $resSrc) {
    Copy-Item -Recurse -Force $resSrc $stageRes
}

if ($exe) {
    Copy-Item -Force $exe (Join-Path $stage $stem.exe)
    Copy-Item -Recurse -Force $resSrc (Join-Path $stage "resources")
}

Copy-Item -Force (Join-Path $root "LICENSE") (Join-Path $stage "LICENSE.txt")
Copy-Item -Force (Join-Path $root "installer\EULA.txt") (Join-Path $stage "EULA.txt")

$docsStage = Join-Path $stage "Docs"
New-Item -ItemType Directory -Force -Path $docsStage | Out-Null
@(
    "docs\manual\NEUROKORE.md",
    "docs\USER_MANUAL.md",
    "resources\UserManual_en.txt",
    "UserManual DE.txt"
) | ForEach-Object {
    $src = Join-Path $root $_
    if (Test-Path $src) { Copy-Item -Force $src $docsStage }
}

@(
    "NEUROKORE $version by Neuroklast",
    "",
    "Windows install",
    "  Run Installer\NEUROKORE-$version-Setup.exe as administrator.",
    "  Pick VST3 (all 64-bit DAWs) and/or Standalone.",
    "  If WebView2 is missing, the setup installs it first.",
    "",
    "VST3 path",
    "  C:\Program Files\Common Files\VST3\NEUROKORE.vst3",
    "  Rescan plug-ins in the DAW. Older NEUROKORE-<version>.vst3 folders are removed.",
    "",
    "Standalone",
    "  Start menu: Neuroklast\NEUROKORE",
    "",
    "Your presets and license stay in %APPDATA%\NEUROKLAST\NEUROKORE when you uninstall.",
    "",
    "See Docs\NEUROKORE.md and EULA.txt."
) | Set-Content -Encoding UTF8 (Join-Path $stage "README.txt")

Get-ChildItem $stageBin -Filter "*.vst3" -File | ForEach-Object { Invoke-Sign $_.FullName }
if ($exe) { Invoke-Sign (Join-Path $stage "$stem.exe") }

$wv2 = Join-Path $root "scripts\download_wv2_bootstrapper.ps1"
& $wv2
$wv2Exe = Join-Path $root "installer\MicrosoftEdgeWebView2RuntimeInstallerX64.exe"
if (-not (Test-Path $wv2Exe)) {
    throw "WebView2 bootstrapper missing after download: $wv2Exe"
}

$iss = Join-Path $root "installer\NeuroKore.iss"
& $iscc "/DNcStage=$stage" "/DMyAppVersion=$version" "/DMyAppNumeric=$numeric" $iss
if ($LASTEXITCODE -ne 0) {
    throw "ISCC failed ($LASTEXITCODE)"
}

$setup = Join-Path $out "NEUROKORE-$version-Setup.exe"
if (-not (Test-Path $setup)) {
    throw "Setup.exe was not written to $setup"
}
Invoke-Sign $setup

$hash = (Get-FileHash -Algorithm SHA256 $setup).Hash.ToLower()
$shaFile = Join-Path $out "NEUROKORE-$version-Setup.exe.sha256"
"$hash  NEUROKORE-$version-Setup.exe" | Set-Content -Encoding ASCII $shaFile
Write-Host "SHA256 $hash"

Reset-Dir $dist
New-Item -ItemType Directory -Force -Path (Join-Path $dist "VST3") | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $dist "Standalone") | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $dist "Installer") | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $dist "Docs") | Out-Null

Copy-Item -Recurse -Force $stageBundle (Join-Path $dist "VST3\$bundleName")
if (Test-Path (Join-Path $stage "$stem.exe")) {
    Copy-Item -Force (Join-Path $stage "$stem.exe") (Join-Path $dist "Standalone\$stem.exe")
    if (Test-Path (Join-Path $stage "resources")) {
        Copy-Item -Recurse -Force (Join-Path $stage "resources") (Join-Path $dist "Standalone\resources")
    }
}
Copy-Item -Force (Join-Path $stage "README.txt") (Join-Path $dist "README.txt")
Copy-Item -Force (Join-Path $stage "LICENSE.txt") (Join-Path $dist "LICENSE.txt")
Copy-Item -Force (Join-Path $stage "EULA.txt") (Join-Path $dist "EULA.txt")
Copy-Item -Force (Join-Path $stage "Docs\*") (Join-Path $dist "Docs")
Copy-Item -Force $setup (Join-Path $dist "Installer\NEUROKORE-$version-Setup.exe")
Copy-Item -Force $shaFile (Join-Path $dist "Installer\NEUROKORE-$version-Setup.exe.sha256")

$zip = Join-Path $out "NEUROKORE-$version-win64.zip"
if (Test-Path $zip) { Remove-Item -Force $zip }
Compress-Archive -Path (Join-Path $stage "*") -DestinationPath $zip -Force
Copy-Item -Force $zip (Join-Path $dist "NEUROKORE-$version-win64.zip")

Write-Host "Setup: $setup"
Write-Host "Portable kit: $dist"
