$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $root
$build = Join-Path $root 'build'
$exe = Join-Path $build 'Release\CoreModel.exe'
$backupDir = Join-Path $root 'KnownGood'
$backupExe = Join-Path $backupDir 'CoreModel.exe'
$log = Join-Path $root 'Update-and-Launch.log'

function Draw-Screen([int]$percent, [string]$phase, [string]$detail) {
  $width = 52
  $filled = [Math]::Min($width,[Math]::Max(0,[int][Math]::Floor($percent*$width/100)))
  $bar = ('#' * $filled) + ('.' * ($width-$filled))
  Clear-Host
  Write-Host ''
  Write-Host '  +--------------------------------------------------------------------+' -ForegroundColor DarkCyan
  Write-Host '  |                                                                    |' -ForegroundColor DarkCyan
  Write-Host '  |                         C O R E M O D E L                          |' -ForegroundColor Cyan
  Write-Host '  |                     UPDATE  /  BUILD  /  LAUNCH                    |' -ForegroundColor Gray
  Write-Host '  |                                                                    |' -ForegroundColor DarkCyan
  Write-Host '  +--------------------------------------------------------------------+' -ForegroundColor DarkCyan
  Write-Host ''
  Write-Host ("  [{0}] {1,3}%" -f $bar,$percent) -ForegroundColor Cyan
  Write-Host ''
  Write-Host ("  {0}" -f $phase) -ForegroundColor White
  Write-Host ("  {0}" -f $detail) -ForegroundColor DarkGray
  Write-Host ''
  Write-Host '  Previous working build is protected in KnownGood.' -ForegroundColor DarkCyan
  Write-Host '  Detailed output: Update-and-Launch.log' -ForegroundColor DarkGray
}
function Fail([string]$message) {
  Draw-Screen 0 'UPDATE STOPPED' $message
  Write-Host ''
  Write-Host ("  ERROR: {0}" -f $message) -ForegroundColor Red
  if (Test-Path $backupExe) { Write-Host ("  Known-good build: {0}" -f $backupExe) -ForegroundColor Yellow }
  if (Test-Path $log) {
    Write-Host ''
    Write-Host '  Last log lines:' -ForegroundColor Yellow
    Get-Content $log -Tail 10 | ForEach-Object { Write-Host ("  " + $_) -ForegroundColor Gray }
  }
  exit 1
}
function Run-Step([string]$label, [string]$command, [string[]]$arguments) {
  ("===== {0} =====" -f $label) | Out-File -FilePath $log -Append -Encoding utf8
  $stdoutFile = Join-Path $env:TEMP 'coremodel-launch-stdout.log'
  $stderrFile = Join-Path $env:TEMP 'coremodel-launch-stderr.log'
  try {
    $process = Start-Process -FilePath $command -ArgumentList $arguments -WorkingDirectory $root -Wait -PassThru -NoNewWindow -RedirectStandardOutput $stdoutFile -RedirectStandardError $stderrFile -ErrorAction Stop
    if (Test-Path $stdoutFile) { Get-Content $stdoutFile | Out-File -FilePath $log -Append -Encoding utf8 }
    if (Test-Path $stderrFile) { Get-Content $stderrFile | Out-File -FilePath $log -Append -Encoding utf8 }
    if ($process.ExitCode -ne 0) { Fail "$label failed (exit code $($process.ExitCode))" }
  } finally {
    Remove-Item $stdoutFile,$stderrFile -Force -ErrorAction SilentlyContinue
  }
}
try {
  Draw-Screen 5 'INITIALIZING' 'Checking development tools and local project...'
  foreach ($command in @('git','cmake')) {
    if (-not (Get-Command $command -ErrorAction SilentlyContinue)) { Fail "$command is not installed or not on PATH" }
  }
  if (-not (Test-Path (Join-Path $root '.git'))) { Fail 'Run this from a cloned Git repository' }
  $dirty = @(git status --porcelain --untracked-files=no)
  if ($LASTEXITCODE -ne 0) { Fail 'Unable to check Git working tree' }
  if ($dirty.Count -gt 0) { Fail 'Tracked local files have changes; commit or stash before updating' }
  '' | Set-Content -Path $log -Encoding utf8
  New-Item -ItemType Directory -Path $backupDir -Force | Out-Null
  if (Test-Path $exe) { Copy-Item $exe $backupExe -Force }

  Draw-Screen 20 'CHECKING FOR UPDATES' 'Pulling the latest CoreModel source from GitHub...'
  Run-Step 'Git update' 'git.exe' @('pull','--ff-only','origin','main')

  Draw-Screen 45 'CONFIGURING PROJECT' 'Preparing Visual Studio 2026 and CMake dependencies...'
  Run-Step 'CMake configure' 'cmake.exe' @('-S', $root, '-B', $build, '-G', 'Visual Studio 18 2026', '-A', 'x64')

  Draw-Screen 70 'BUILDING COREMODEL' 'Compiling C++ code. This may take a few minutes...'
  Run-Step 'Release build' 'cmake.exe' @('--build', $build, '--config', 'Release', '--parallel', '2')
  if (-not (Test-Path $exe)) { Fail 'Build completed but CoreModel.exe was not found' }

  Draw-Screen 95 'STARTING EDITOR' 'Launching CoreModel...'
  Start-Process -FilePath $exe -WorkingDirectory (Split-Path $exe)
  Draw-Screen 100 'READY' 'CoreModel launched successfully.'
  Start-Sleep -Seconds 2
} catch {
  Fail $_.Exception.Message
}
