# LanguageOne: compile the plugin against every installed UE 5.1-5.8 with RunUAT BuildPlugin
# Engine locations come from the Epic launcher registry keys. Exit code is non-zero if any build fails.

param(
    [string[]]$Versions = @("5.1", "5.2", "5.3", "5.4", "5.5", "5.6", "5.7", "5.8")
)

$Plugin = Join-Path $PSScriptRoot "LanguageOne\LanguageOne.uplugin"
$Failed = @()

# powershell -File passes "5.1,5.8" as one string
foreach ($Version in ($Versions -split "," | ForEach-Object { $_.Trim() } | Where-Object { $_ })) {
    $Install = (Get-ItemProperty "HKLM:\SOFTWARE\EpicGames\Unreal Engine\$Version" -ErrorAction SilentlyContinue).InstalledDirectory
    if (-not $Install) {
        Write-Host "UE $Version  not installed, skipped" -ForegroundColor DarkGray
        continue
    }

    # Keep the output path short: UBT fails once intermediate paths pass 260 characters
    $OutDir = Join-Path $env:TEMP ("l1b" + $Version.Replace(".", ""))
    $Log = "$OutDir.log"
    & "$Install\Engine\Build\BatchFiles\RunUAT.bat" BuildPlugin -Plugin="$Plugin" -Package="$OutDir" -TargetPlatforms=Win64 -Rocket *> $Log
    $ExitCode = $LASTEXITCODE

    if ($ExitCode -eq 0) {
        Write-Host "UE $Version  OK" -ForegroundColor Green
    }
    else {
        Write-Host "UE $Version  FAILED (exit $ExitCode), log: $Log" -ForegroundColor Red
        $Failed += $Version
    }
    Select-String -Path $Log -Pattern "error C|: error|warning C" | Select-Object -First 5 | ForEach-Object { Write-Host "    $($_.Line.Trim())" -ForegroundColor Yellow }
}

if ($Failed.Count -gt 0) {
    Write-Host "Failed: $($Failed -join ', ')" -ForegroundColor Red
    exit 1
}
exit 0
