$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $root
$build = Join-Path $root 'build'
$exe = Join-Path $build 'Release\CoreModel.exe'
$backupDir = Join-Path $root 'KnownGood'
$backupExe = Join-Path $backupDir 'CoreModel.exe'
function Fail([string]$message) {
  Write-Host "ERROR: $message" -ForegroundColor Red
  if (Test-Path $backupExe) {
    Write-Host "Previous executable: $backupExe" -ForegroundColor Yellow
  }
  Read-Host 'Press Enter to close'
  exit 1
}
try {
  foreach ($command in @('git','cmake')) {
    if (-not (Get-Command $command -ErrorAction SilentlyContinue)) { Fail "$command is not installed or not on PATH" }
  }
  if (-not (Test-Path (Join-Path $root '.git'))) { Fail 'Run this from a cloned Git repository' }
  $dirty = @(git status --porcelain --untracked-files=no)
  if ($LASTEXITCODE -ne 0) { Fail 'Unable to check Git working tree' }
  if ($dirty.Count -gt 0) { Fail 'Local tracked files have changes. Commit or stash them before updating.' }
  New-Item -ItemType Directory -Path $backupDir -Force | Out-Null
  if (Test-Path $exe) {
    Copy-Item $exe $backupExe -Force
    Write-Host 'Saved previous executable to KnownGood\CoreModel.exe'
  }
  Write-Host 'Checking GitHub updates...' -ForegroundColor Cyan
  & git pull --ff-only origin main
  if ($LASTEXITCODE -ne 0) { Fail 'Git pull failed; source was not reset or overwritten.' }
  # Reconfigure on every pull so new dependencies and assets are detected.
  Write-Host 'Configuring CMake (Visual Studio 2026)...' -ForegroundColor Cyan
  & cmake -S $root -B $build -G 'Visual Studio 18 2026' -A x64
  if ($LASTEXITCODE -ne 0) { Fail 'CMake configuration failed' }
  Write-Host 'Building CoreModel...' -ForegroundColor Cyan
  & cmake --build $build --config Release --parallel 2
  if ($LASTEXITCODE -ne 0) { Fail 'Compilation failed; previous executable is preserved.' }
  if (-not (Test-Path $exe)) { Fail 'Build succeeded but CoreModel.exe was not found' }
  Write-Host 'Build succeeded. Starting CoreModel...' -ForegroundColor Green
  Start-Process -FilePath $exe -WorkingDirectory (Split-Path $exe)
} catch {
  Fail $_.Exception.Message
}
