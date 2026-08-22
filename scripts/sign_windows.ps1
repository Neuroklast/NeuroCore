# Authenticode-sign a file when a cert is configured. No-op otherwise.
# Env (first match wins):
#   NEUROKORE_SIGN_PFX + NEUROKORE_SIGN_PASSWORD  (PFX path)
#   NEUROKORE_SIGN_THUMBPRINT                     (CurrentUser/My or LocalMachine/My)
param(
    [Parameter(Mandatory = $true)][string]$Path
)
$ErrorActionPreference = "Stop"
if (-not (Test-Path $Path)) {
    throw "Nothing to sign: $Path"
}

$signtool = @(
    "${env:ProgramFiles(x86)}\Windows Kits\10\bin\*\x64\signtool.exe",
    "${env:ProgramFiles}\Windows Kits\10\bin\*\x64\signtool.exe"
) | ForEach-Object { Get-Item $_ -ErrorAction SilentlyContinue } |
    Sort-Object FullName -Descending |
    Select-Object -First 1 -ExpandProperty FullName

if (-not $signtool) {
    Write-Host "signtool.exe not found - skip signing $Path"
    return
}

$timestamp = "http://timestamp.digicert.com"
$args = @("sign", "/fd", "SHA256", "/td", "SHA256", "/tr", $timestamp, "/v")

if ($env:NEUROKORE_SIGN_PFX) {
    $args += @("/f", $env:NEUROKORE_SIGN_PFX)
    if ($env:NEUROKORE_SIGN_PASSWORD) {
        $args += @("/p", $env:NEUROKORE_SIGN_PASSWORD)
    }
} elseif ($env:NEUROKORE_SIGN_THUMBPRINT) {
    $args += @("/sha1", $env:NEUROKORE_SIGN_THUMBPRINT, "/sm")
} else {
    Write-Host "No NEUROKORE_SIGN_PFX or NEUROKORE_SIGN_THUMBPRINT - skip signing $Path"
    return
}

$args += $Path
& $signtool @args
if ($LASTEXITCODE -ne 0) {
    throw "signtool failed ($LASTEXITCODE) for $Path"
}
Write-Host "Signed $Path"
