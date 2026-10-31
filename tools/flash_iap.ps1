param([string]$KeilRoot = 'D:/Keil_v5')
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
Push-Location $projectRoot
try {
    $axfPath = Join-Path $projectRoot 'keilproj/iap/ObjectsInstall/lbj_install.axf'
    $checkPath = Join-Path $projectRoot 'keilproj/iap/ObjectsInstall/install-check.bin'
    $imagePath = Join-Path $projectRoot 'keilproj/iap/release/first-install.bin'
    & (Join-Path $KeilRoot 'ARM/ARMCC/Bin/fromelf.exe') --bin --output $checkPath $axfPath
    if ($LASTEXITCODE) { throw 'Cannot validate IAP installer AXF' }
    if ((Get-Item -LiteralPath $checkPath).Length -ne 0x20000 -or
        (Get-FileHash -LiteralPath $checkPath).Hash -ne (Get-FileHash -LiteralPath $imagePath).Hash) {
        throw 'Installer AXF is stale or does not match the complete IAP image; rebuild first'
    }
    python (Join-Path $PSScriptRoot 'verify_iap_artifacts.py')
    if ($LASTEXITCODE) { throw 'IAP image verification failed' }
    $flashStamp = [DateTimeOffset]::UtcNow.ToOffset([TimeSpan]::FromHours(8)).ToString('yyyyMMdd-HHmmss')
    $logFile = Join-Path $projectRoot ('keilproj/iap/install-flash-' + $flashStamp + '.log')
    $projectFile = Join-Path $projectRoot 'keilproj/iap/install.uvprojx'
    Start-Process -FilePath (Join-Path $KeilRoot 'UV4/UV4.exe') -ArgumentList '-f',$projectFile,'-j0','-o',$logFile -Wait -WindowStyle Hidden
    $logText = Get-Content -LiteralPath $logFile -Raw
    Get-Content -LiteralPath $logFile -Tail 10
    if ($logText -notmatch 'Verify OK' -or $logText -match 'Target DLL.*cancel|Flash Download failed|Error:') {
        throw ('IAP flash did not verify: ' + $logFile)
    }
    $state = [ordered]@{
        hardware_flashed = $true
        flash_verified = $true
        installed_at = [DateTimeOffset]::UtcNow.ToOffset([TimeSpan]::FromHours(8)).ToString('o')
        installer_sha256 = (Get-FileHash -LiteralPath $imagePath).Hash
        flash_log = $logFile
        rear_contact_continuity_verified_by_user = $true
        boot_runtime_validated = $false
        uart_hardware_tested = $false
    }
    ConvertTo-Json -InputObject $state | Set-Content 'keilproj/iap/release/installed-state.json' -Encoding UTF8
} finally { Pop-Location }
