param([switch]$Iap, [string]$KeilRoot = 'D:/Keil_v5')
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
Push-Location $projectRoot
try {
    foreach ($generator in @('gen_ui_text.py','gen_font.py','sync_project_files.py')) {
        python (Join-Path $PSScriptRoot $generator)
        if ($LASTEXITCODE) { throw ('Generator failed: ' + $generator) }
    }
    $buildTargets = ,@('keilproj/1.uvprojx','keilproj/build.log','keilproj/Objects/lcdtest')
    if ($Iap) {
        python (Join-Path $PSScriptRoot 'make_iap_projects.py')
        if ($LASTEXITCODE) { throw 'IAP project generation failed' }
        $buildTargets += ,@('keilproj/iap/boot.uvprojx','keilproj/iap/boot-build.log','keilproj/iap/ObjectsBoot/lbj_boot')
        $buildTargets += ,@('keilproj/iap/app.uvprojx','keilproj/iap/app-build.log','keilproj/iap/ObjectsApp/lbj_app')
    }
    foreach ($buildTarget in $buildTargets) {
        $projectFile = Join-Path $projectRoot $buildTarget[0]
        $logFile = Join-Path $projectRoot $buildTarget[1]
        Start-Process -FilePath (Join-Path $KeilRoot 'UV4/UV4.exe') -ArgumentList '-b',$projectFile,'-j0','-o',$logFile -Wait -WindowStyle Hidden
        $logText = Get-Content -LiteralPath $logFile -Raw
        if ($logText -notmatch '0 Error\(s\), 0 Warning\(s\)') {
            Get-Content -LiteralPath $logFile -Tail 25
            throw ('Build failed: ' + $projectFile)
        }
        $artifactBase = Join-Path $projectRoot $buildTarget[2]
        & (Join-Path $KeilRoot 'ARM/ARMCC/Bin/fromelf.exe') --bin --output ($artifactBase + '.bin') ($artifactBase + '.axf')
        if ($LASTEXITCODE) { throw 'BIN export failed' }
        & (Join-Path $KeilRoot 'ARM/ARMCC/Bin/fromelf.exe') --i32 --output ($artifactBase + '.hex') ($artifactBase + '.axf')
        if ($LASTEXITCODE) { throw 'HEX export failed' }
        Get-Content -LiteralPath $logFile -Tail 5
    }
    Copy-Item -LiteralPath 'keilproj/Objects/lcdtest.hex' -Destination 'keilproj/Objects/lbj_rx.hex'
    if ($Iap) {
        python (Join-Path $PSScriptRoot 'pack_iap_install.py')
        if ($LASTEXITCODE) { throw 'IAP package export failed' }
        python (Join-Path $PSScriptRoot 'make_iap_installer.py')
        if ($LASTEXITCODE) { throw 'IAP installer project generation failed' }
        $installerProject = Join-Path $projectRoot 'keilproj/iap/install.uvprojx'
        $installerLog = Join-Path $projectRoot 'keilproj/iap/install-build.log'
        Start-Process -FilePath (Join-Path $KeilRoot 'UV4/UV4.exe') -ArgumentList '-b',$installerProject,'-j0','-o',$installerLog -Wait -WindowStyle Hidden
        if ((Get-Content -LiteralPath $installerLog -Raw) -notmatch '0 Error\(s\), 0 Warning\(s\)') {
            Get-Content -LiteralPath $installerLog -Tail 20
            throw 'IAP complete installer build failed'
        }
        & (Join-Path $KeilRoot 'ARM/ARMCC/Bin/fromelf.exe') --bin --output 'keilproj/iap/ObjectsInstall/lbj_install.bin' 'keilproj/iap/ObjectsInstall/lbj_install.axf'
        if ($LASTEXITCODE) { throw 'IAP installer BIN export failed' }
        if ((Get-FileHash 'keilproj/iap/ObjectsInstall/lbj_install.bin').Hash -ne (Get-FileHash 'keilproj/iap/release/first-install.bin').Hash) {
            throw 'IAP installer AXF does not match the verified complete BIN'
        }
        python (Join-Path $PSScriptRoot 'write_build_info.py')
        if ($LASTEXITCODE) { throw 'Build identity export failed' }
    }
    Write-Output 'Build/export complete. No hardware was flashed.'
} finally { Pop-Location }
