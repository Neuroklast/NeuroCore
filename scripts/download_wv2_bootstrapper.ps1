# Download Microsoft Edge WebView2 Evergreen Bootstrapper (x64) next to the Inno script.
# https://learn.microsoft.com/en-us/microsoft-edge/webview2/concepts/distribution
$ErrorActionPreference = "Stop"
if (-not $PSScriptRoot) { $PSScriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path }
$dest = Join-Path (Split-Path -Parent $PSScriptRoot) "installer\MicrosoftEdgeWebView2RuntimeInstallerX64.exe"
$url = "https://go.microsoft.com/fwlink/p/?LinkId=2124703"
Write-Host "Downloading WebView2 bootstrapper -> $dest"
Invoke-WebRequest -Uri $url -OutFile $dest -UseBasicParsing
if (-not (Test-Path $dest) -or (Get-Item $dest).Length -lt 100000) {
    throw "WebView2 bootstrapper download failed or file too small: $dest"
}
Write-Host ("OK {0:N0} bytes" -f (Get-Item $dest).Length)
