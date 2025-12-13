# LanguageOne Multi-Version Package Script
# Create separate packages for each engine version for Fab approval

param(
    [string]$OutputDir = "Packages\Release"
)

$PluginName = "LanguageOne"
$PluginSourceDir = "LanguageOne"

# Supported engine versions
$EngineVersions = @(
    @{ Version = "4.26"; VersionString = "4.26.0" },
    @{ Version = "4.27"; VersionString = "4.27.0" },
    @{ Version = "5.0"; VersionString = "5.0.0" },
    @{ Version = "5.1"; VersionString = "5.1.0" },
    @{ Version = "5.2"; VersionString = "5.2.0" },
    @{ Version = "5.3"; VersionString = "5.3.0" },
    @{ Version = "5.4"; VersionString = "5.4.0" },
    @{ Version = "5.5"; VersionString = "5.5.0" },
    @{ Version = "5.6"; VersionString = "5.6.0" },
    @{ Version = "5.7"; VersionString = "5.7.0" }
)

Write-Host "================================================" -ForegroundColor Cyan
Write-Host " LanguageOne Multi-Version Package Script" -ForegroundColor Cyan
Write-Host "================================================" -ForegroundColor Cyan
Write-Host ""

# Read original uplugin file
$UpluginPath = "$PluginSourceDir\$PluginName.uplugin"
$OriginalUplugin = Get-Content $UpluginPath -Raw -Encoding UTF8 | ConvertFrom-Json

# Create output directory
if (!(Test-Path $OutputDir)) {
    New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null
}

$ExcludeDirs = @("Binaries", "Intermediate", ".vs", "Saved", "DerivedDataCache")
$ExcludeFiles = @("*.pdb", "*.exp", "*.lib", "*.obj")

foreach ($EngineVer in $EngineVersions) {
    $Version = $EngineVer.Version
    $VersionString = $EngineVer.VersionString
    
    Write-Host "================================================" -ForegroundColor Yellow
    Write-Host " Packaging for UE $Version" -ForegroundColor Yellow
    Write-Host "================================================" -ForegroundColor Yellow
    
    # Create temp directory
    $TempDir = "$OutputDir\Temp_UE$Version"
    if (Test-Path $TempDir) {
        Remove-Item -Path $TempDir -Recurse -Force
    }
    New-Item -ItemType Directory -Path $TempDir -Force | Out-Null
    
    # Copy plugin files
    Write-Host "  Copying plugin files..." -ForegroundColor Green
    $ExcludeDirsParam = $ExcludeDirs | ForEach-Object { "/XD `"$_`"" }
    $ExcludeFilesParam = $ExcludeFiles | ForEach-Object { "/XF `"$_`"" }
    $RobocopyCmd = "robocopy `"$PluginSourceDir`" `"$TempDir\$PluginName`" /E /NFL /NDL /NJH /NJS $ExcludeDirsParam $ExcludeFilesParam"
    Invoke-Expression $RobocopyCmd | Out-Null
    
    if ($LASTEXITCODE -ge 8) {
        Write-Host "  File copy error!" -ForegroundColor Red
        exit 1
    }
    
    # Modify uplugin file, add EngineVersion
    Write-Host "  Setting engine version: $VersionString" -ForegroundColor Green
    $UpluginFilePath = "$TempDir\$PluginName\$PluginName.uplugin"
    $upluginObj = Get-Content $UpluginFilePath -Raw -Encoding UTF8 | ConvertFrom-Json
    
    # Add EngineVersion field
    $upluginObj | Add-Member -NotePropertyName "EngineVersion" -NotePropertyValue $VersionString -Force
    
    # Save modified JSON with formatting (UTF8 without BOM)
    $jsonContent = $upluginObj | ConvertTo-Json -Depth 10
    # Replace escaped characters back to original (e.g., \u0026 -> &)
    $jsonContent = $jsonContent -replace '\\u0026', '&'
    $utf8NoBom = New-Object System.Text.UTF8Encoding $false
    [System.IO.File]::WriteAllText($UpluginFilePath, $jsonContent, $utf8NoBom)
    
    # Create ZIP package
    $ZipName = "${PluginName}_UE${Version}_v$($OriginalUplugin.VersionName).zip"
    $ZipPath = "$OutputDir\$ZipName"
    
    Write-Host "  Creating ZIP: $ZipName" -ForegroundColor Green
    if (Test-Path $ZipPath) {
        Remove-Item $ZipPath -Force
    }
    Compress-Archive -Path "$TempDir\*" -DestinationPath $ZipPath -Force
    
    # Clean up temp directory
    Remove-Item -Path $TempDir -Recurse -Force
    
    Write-Host "  Done: $ZipName" -ForegroundColor Green
    Write-Host ""
}

Write-Host "================================================" -ForegroundColor Green
Write-Host " All Packages Created Successfully!" -ForegroundColor Green
Write-Host "================================================" -ForegroundColor Green
Write-Host ""
Write-Host "Output Directory: $OutputDir" -ForegroundColor Cyan
Write-Host ""
Write-Host "Created Packages:" -ForegroundColor Yellow
Get-ChildItem -Path $OutputDir -Filter "*.zip" | ForEach-Object {
    $sizeMB = [math]::Round($_.Length / 1MB, 2)
    Write-Host "  - $($_.Name) - $sizeMB MB" -ForegroundColor Gray
}
Write-Host ""
Write-Host "Upload each package to Fab for its corresponding engine version" -ForegroundColor Cyan
Write-Host ""

