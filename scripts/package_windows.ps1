# Stage Release artefacts, fill repo-root NEUROKORE-0.4.7-alpha, optionally compile Inno Setup.
# Usage (from repo root, after cmake --build build --config Release --target NeuroKore):
#   powershell -File scripts/package_windows.ps1

$ErrorActionPreference = "Stop"
if (-not $PSScriptRoot) { $PSScriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path }
$root = Split-Path -Parent $PSScriptRoot
$version = "0.5.0-alpha"
$stem = "NEUROKORE-$version"
$art = Join-Path $root "build\NeuroKore_artefacts\Release"
$stage = Join-Path $root "build\package\stage"
$out = Join-Path $root "build\package"
$dist = Join-Path $root $stem
$vst3 = $null
foreach ($cand in @(
        (Join-Path $art "VST3\$stem.vst3"),
        (Join-Path $art "VST3\NEUROKORE.vst3")
    )) {
    if (Test-Path $cand) { $vst3 = $cand; break }
}

Write-Host "Root: $root"
if (-not $vst3 -or -not (Test-Path $vst3)) {
    throw "VST3 not found (looked for $stem.vst3 and NEUROKORE.vst3) -- build Release first."
}

function Reset-Dir([string]$path) {
    if (Test-Path $path) { Remove-Item -Recurse -Force $path }
    New-Item -ItemType Directory -Force -Path $path | Out-Null
}

Reset-Dir $stage
New-Item -ItemType Directory -Force -Path $out | Out-Null

Copy-Item -Recurse -Force $vst3 (Join-Path $stage "$stem.vst3")
$exe = $null
foreach ($cand in @(
        (Join-Path $art "Standalone\$stem.exe"),
        (Join-Path $art "Standalone\NEUROKORE.exe")
    )) {
    if (Test-Path $cand) { $exe = $cand; break }
}
if ($exe) {
    Copy-Item -Force $exe (Join-Path $stage "$stem.exe")
}

Copy-Item -Force (Join-Path $root "LICENSE") (Join-Path $stage "LICENSE.txt")
Copy-Item -Force (Join-Path $root "installer\EULA.txt") (Join-Path $stage "EULA.txt")

$docsStage = Join-Path $stage "Docs"
New-Item -ItemType Directory -Force -Path $docsStage | Out-Null
@(
    "README.md",
    "docs\USER_MANUAL.md",
    "docs\DSL_REFERENCE.md",
    "resources\UserManual_en.txt",
    "UserManual DE.txt"
) | ForEach-Object {
    $src = Join-Path $root $_
    if (Test-Path $src) { Copy-Item -Force $src $docsStage }
}

@(
    "NEUROKORE $version by Neuroklast",
    "",
    "This folder is the Windows release kit.",
    "",
    "INSTALL",
    "  Preferred: run Installer\NEUROKORE-$version-Setup.exe (admin).",
    "  Manual VST3: copy VST3\$stem.vst3 to",
    "    C:\Program Files\Common Files\VST3\",
    "  Standalone: run Standalone\$stem.exe",
    "",
    "After install, rescan plug-ins in the DAW.",
    "Activate with a .lic via License in the plug-in.",
    "Import artist packs (.zip or a folder of .nrk) via Presets, Import.",
    "",
    "Oversampling defaults to 4x. Drop to 2x or 1x if the CPU is tight.",
    "",
    "See Docs\USER_MANUAL.md and EULA.txt."
) | Set-Content -Encoding UTF8 (Join-Path $stage "README.txt")

# Repo-root distribution folder (portable kit next to the source tree)
Reset-Dir $dist
New-Item -ItemType Directory -Force -Path (Join-Path $dist "VST3") | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $dist "Standalone") | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $dist "Installer") | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $dist "Docs") | Out-Null

Copy-Item -Recurse -Force (Join-Path $stage "$stem.vst3") (Join-Path $dist "VST3\$stem.vst3")
if (Test-Path (Join-Path $stage "$stem.exe")) {
    Copy-Item -Force (Join-Path $stage "$stem.exe") (Join-Path $dist "Standalone\$stem.exe")
}
Copy-Item -Force (Join-Path $stage "README.txt") (Join-Path $dist "README.txt")
Copy-Item -Force (Join-Path $stage "LICENSE.txt") (Join-Path $dist "LICENSE.txt")
Copy-Item -Force (Join-Path $stage "EULA.txt") (Join-Path $dist "EULA.txt")
Copy-Item -Force (Join-Path $root "installer\NeuroKore.iss") (Join-Path $dist "Installer\NeuroKore.iss")
Copy-Item -Force (Join-Path $root "installer\EULA.txt") (Join-Path $dist "Installer\EULA.txt")
Copy-Item -Force (Join-Path $stage "Docs\*") (Join-Path $dist "Docs")

$zip = Join-Path $out "NEUROKORE-$version-win64.zip"
if (Test-Path $zip) { Remove-Item -Force $zip }
Compress-Archive -Path (Join-Path $stage "*") -DestinationPath $zip -Force
Copy-Item -Force $zip (Join-Path $dist "NEUROKORE-$version-win64.zip")
Write-Host "Wrote $zip"

$iscc = @(
    "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe",
    "${env:ProgramFiles}\Inno Setup 6\ISCC.exe"
) | Where-Object { Test-Path $_ } | Select-Object -First 1

if ($iscc) {
    $iss = Join-Path $root "installer\NeuroKore.iss"
    & $iscc "/DNcStage=$stage" $iss
    $setup = Join-Path $out "NEUROKORE-$version-Setup.exe"
    if (Test-Path $setup) {
        Copy-Item -Force $setup (Join-Path $dist "Installer\NEUROKORE-$version-Setup.exe")
    }
    Write-Host "Installer compile finished."
} else {
    Write-Host "Inno Setup 6 (ISCC.exe) not found -- zip and portable folder are ready."
}

Write-Host "Portable kit: $dist"
