# LanguageOne 打包脚本 - Package Script
# 用途：创建源码分发包，无版本弹窗
# Purpose: Create source distribution package without version warning

param(
    [string]$OutputDir = "Packages\Release"
)

$PluginName = "LanguageOne"
$PluginSourceDir = "LanguageOne"
$Timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$OutputPath = "$OutputDir\${PluginName}_$Timestamp"

Write-Host "================================================" -ForegroundColor Cyan
Write-Host " LanguageOne Package Script" -ForegroundColor Cyan
Write-Host "================================================" -ForegroundColor Cyan
Write-Host ""

# 创建输出目录
if (!(Test-Path $OutputDir)) {
    New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null
}

if (Test-Path $OutputPath) {
    Write-Host "Removing existing output directory..." -ForegroundColor Yellow
    Remove-Item -Path $OutputPath -Recurse -Force
}

Write-Host "Creating package in: $OutputPath" -ForegroundColor Green
New-Item -ItemType Directory -Path $OutputPath -Force | Out-Null

# 复制插件文件，排除编译产物
Write-Host "Copying plugin files..." -ForegroundColor Green

$ExcludeDirs = @("Binaries", "Intermediate", ".vs", "Saved", "DerivedDataCache")
$ExcludeFiles = @("*.pdb", "*.exp", "*.lib", "*.obj")

# 使用 robocopy 复制（排除指定目录）
$ExcludeDirsParam = $ExcludeDirs | ForEach-Object { "/XD `"$_`"" }
$ExcludeFilesParam = $ExcludeFiles | ForEach-Object { "/XF `"$_`"" }

$RobocopyCmd = "robocopy `"$PluginSourceDir`" `"$OutputPath\$PluginName`" /E /NFL /NDL /NJH /NJS $ExcludeDirsParam $ExcludeFilesParam"
Invoke-Expression $RobocopyCmd | Out-Null

if ($LASTEXITCODE -ge 8) {
    Write-Host "Error during file copy!" -ForegroundColor Red
    exit 1
}

Write-Host ""
Write-Host "================================================" -ForegroundColor Green
Write-Host " Package Created Successfully!" -ForegroundColor Green
Write-Host "================================================" -ForegroundColor Green
Write-Host ""
Write-Host "Output: $OutputPath" -ForegroundColor Cyan
Write-Host ""
Write-Host "Files included:" -ForegroundColor Yellow
Get-ChildItem -Path "$OutputPath\$PluginName" -Recurse -File | Select-Object -First 10 | ForEach-Object {
    Write-Host "  - $($_.FullName.Replace($OutputPath + '\', ''))" -ForegroundColor Gray
}
$TotalFiles = (Get-ChildItem -Path "$OutputPath\$PluginName" -Recurse -File).Count
Write-Host "  ... and $($TotalFiles - 10) more files" -ForegroundColor Gray
Write-Host ""

# 创建 ZIP 包
Write-Host "Creating ZIP archives..." -ForegroundColor Green

# 1. 带时间戳的归档版本（用于备份）
$ZipPath = "$OutputDir\${PluginName}_$Timestamp.zip"
Compress-Archive -Path "$OutputPath\*" -DestinationPath $ZipPath -Force
Write-Host "  [Archive] $ZipPath" -ForegroundColor Gray

# 2. 固定名称版本（用于 Fab/GitHub Release）
$ReleaseZipPath = "$OutputDir\${PluginName}.zip"
Compress-Archive -Path "$OutputPath\*" -DestinationPath $ReleaseZipPath -Force
Write-Host "  [Release] $ReleaseZipPath" -ForegroundColor Green

Write-Host ""
Write-Host "================================================" -ForegroundColor Green
Write-Host " Package Created Successfully!" -ForegroundColor Green
Write-Host "================================================" -ForegroundColor Green
Write-Host ""
Write-Host "发布文件 | Release File:" -ForegroundColor Cyan
Write-Host "  $ReleaseZipPath" -ForegroundColor Yellow
Write-Host ""
Write-Host "备份文件 | Archive File:" -ForegroundColor Cyan
Write-Host "  $ZipPath" -ForegroundColor Gray
Write-Host ""
Write-Host "上传到 Fab/GitHub 时使用: $ReleaseZipPath" -ForegroundColor Green
Write-Host ""

# 清理临时文件夹
Write-Host "Cleaning up temporary files..." -ForegroundColor Yellow
Remove-Item -Path $OutputPath -Recurse -Force
Write-Host "Temporary folder removed." -ForegroundColor Gray
Write-Host ""

