# CoreModel reliable console updater. Run via Update-and-Launch.cmd.
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$log = Join-Path $root 'Update-and-Launch.log'
$build = Join-Path $root 'build'
$exe = Join-Path $build 'Release\CoreModel.exe'
$knownGood = Join-Path $root 'KnownGood\CoreModel.exe'
Set-Location -LiteralPath $root
"CoreModel update started: $(Get-Date -Format o)" | Set-Content -LiteralPath $log
function Status([string]$message) {
  Write-Host ("[CoreModel] " + $message) -ForegroundColor Cyan
  Add-Content -LiteralPath $log -Value ("[CoreModel] " + $message)
}
function Step([string]$name,[string]$program,[string[]]$arguments) {
  Status $name
  # Native programs can write harmless warnings to stderr. Capture output as text,
  # and decide success from the actual process exit code, not PowerShell error records.
  $output = & $program @arguments 2>&1
  $exitCode = $LASTEXITCODE
  foreach ($entry in $output) {
    $line = [string]$entry
    Write-Host $line
    Add-Content -LiteralPath $log -Value $line
  }
  if ($exitCode -ne 0) { throw "$name failed with exit code $exitCode" }
}

try {
  Status 'Checking project and tools'
  if (-not (Test-Path (Join-Path $root '.git'))) { throw 'Project folder is not a Git clone.' }
  foreach ($name in @('git.exe','cmake.exe')) {
    if (-not (Get-Command $name -ErrorAction SilentlyContinue)) { throw "$name was not found on PATH." }
  }
  if ((Test-Path $exe) -and -not (Test-Path $knownGood)) {
    New-Item -ItemType Directory -Force -Path (Split-Path $knownGood) | Out-Null
    Copy-Item -LiteralPath $exe -Destination $knownGood -Force
  }
  Step 'Checking GitHub for latest source' 'git.exe' @('fetch','origin','main')
  $dirty = @(& git.exe status --porcelain --untracked-files=no)
  if ($LASTEXITCODE -ne 0) { throw 'Git status failed.' }
  if ($dirty.Count -gt 0) {
    Status 'Local tracked edits detected; refusing to silently launch an outdated editor.'
    Status 'Run: git diff -- main.cpp'
    Status 'If your local edit is already on GitHub, first back up main.cpp, then run: git restore main.cpp'
    throw 'Update blocked by local edits. Back up or commit your edits, then rerun the launcher.'
  }
  Step 'Updating source from GitHub' 'git.exe' @('merge','--ff-only','origin/main')
  Status 'Running the build for this launch; unchanged dependencies may be reused'
  if (Test-Path (Join-Path $build 'CMakeCache.txt')) {
    Step 'Configuring CMake (existing build)' 'cmake.exe' @('-S',$root,'-B',$build)
  } else {
    Step 'Configuring CMake (Visual Studio 2026 x64)' 'cmake.exe' @('-S',$root,'-B',$build,'-G','Visual Studio 18 2026','-A','x64')
  }
  $beforeBuild = if (Test-Path $exe) { (Get-Item -LiteralPath $exe).LastWriteTimeUtc } else { [datetime]::MinValue }
  Step 'Building CoreModel Release' 'cmake.exe' @('--build',$build,'--config','Release','--parallel','2')
  if (-not (Test-Path $exe)) { throw "Build finished but executable is missing: $exe" }
  $afterBuild = (Get-Item -LiteralPath $exe).LastWriteTimeUtc
  $revision = (& git.exe rev-parse --short HEAD | Select-Object -First 1)
  Status ("Build succeeded. Source revision: {0}; EXE modified (UTC): {1:o}" -f $revision,$afterBuild)
  if ($afterBuild -eq $beforeBuild) { Status 'Executable unchanged: build system found no changes requiring relinking.' }
  New-Item -ItemType Directory -Force -Path (Split-Path $knownGood) | Out-Null
  Status 'Build completed. Starting the executable directly (not Visual Studio).'
  # Run the executable with PowerShell's call operator, matching the known-working command.
  # A successful build is a candidate; do not overwrite an existing KnownGood here.
  Set-Location -LiteralPath $root
  & $exe
  $editorExit = $LASTEXITCODE
  if ($null -ne $editorExit -and $editorExit -ne 0) { throw "CoreModel.exe exited with code $editorExit" }
  Status 'CoreModel.exe closed normally.'
  exit 0
} catch {
  $reason = $_.Exception.Message
  Write-Host ("[CoreModel] ERROR: " + $reason) -ForegroundColor Red
  Add-Content -LiteralPath $log -Value ("ERROR: " + $reason)
  Write-Host ("Log: " + $log) -ForegroundColor Yellow
  if (Test-Path $knownGood) {
    Write-Host 'The previous known-good executable is available in KnownGood.' -ForegroundColor Yellow
  }
  exit 1
}
