$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $root
$build = Join-Path $root 'build'
$exe = Join-Path $build 'Release\CoreModel.exe'
$backupDir = Join-Path $root 'KnownGood'
$backupExe = Join-Path $backupDir 'CoreModel.exe'
$log = Join-Path $root 'Update-and-Launch.log'

Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
[System.Windows.Forms.Application]::EnableVisualStyles()
$form = New-Object System.Windows.Forms.Form
$form.Text = 'CoreModel - Starting'
$form.Size = New-Object System.Drawing.Size(540,310)
$form.StartPosition = 'CenterScreen'
$form.FormBorderStyle = 'FixedDialog'
$form.MaximizeBox = $false
$form.BackColor = [System.Drawing.Color]::FromArgb(17,25,37)
$form.ForeColor = [System.Drawing.Color]::White
$form.Font = New-Object System.Drawing.Font('Segoe UI',10)
$heading = New-Object System.Windows.Forms.Label
$heading.Text = 'CoreModel'
$heading.Font = New-Object System.Drawing.Font('Segoe UI Semibold',26)
$heading.ForeColor = [System.Drawing.Color]::FromArgb(235,242,251)
$heading.SetBounds(32,28,460,56)
$form.Controls.Add($heading)
$subtitle = New-Object System.Windows.Forms.Label
$subtitle.Text = 'Preparing your 3D workspace'
$subtitle.ForeColor = [System.Drawing.Color]::FromArgb(145,167,190)
$subtitle.SetBounds(35,90,460,26)
$form.Controls.Add($subtitle)
$statusLabel = New-Object System.Windows.Forms.Label
$statusLabel.Text = 'Initializing...'
$statusLabel.SetBounds(35,141,455,25)
$form.Controls.Add($statusLabel)
$progress = New-Object System.Windows.Forms.ProgressBar
$progress.SetBounds(35,177,455,12)
$progress.Style = 'Continuous'
$progress.Maximum = 100
$form.Controls.Add($progress)
$detailLabel = New-Object System.Windows.Forms.Label
$detailLabel.ForeColor = [System.Drawing.Color]::FromArgb(145,167,190)
$detailLabel.SetBounds(35,203,455,46)
$form.Controls.Add($detailLabel)
$form.Show()
[System.Windows.Forms.Application]::DoEvents()
function Draw-Screen([int]$percent, [string]$phase, [string]$detail) {
  $progress.Value = [Math]::Max(0,[Math]::Min(100,$percent))
  $statusLabel.Text = $phase
  $detailLabel.Text = $detail
  [System.Windows.Forms.Application]::DoEvents()
}
function Fail([string]$message) {
  Draw-Screen 0 'UPDATE STOPPED' $message
  $tail = if(Test-Path $log) { (Get-Content $log -Tail 12) -join [Environment]::NewLine } else { '' }
  $form.Hide()
  [System.Windows.Forms.MessageBox]::Show(("CoreModel could not finish updating.`r`n`r`n" + $message + "`r`n`r`nPrevious executable preserved in KnownGood.`r`n`r`nLog: " + $log + "`r`n`r`n" + $tail),'CoreModel Update Error','OK','Error') | Out-Null
  $form.Close()
  exit 1
}
function Run-Step([string]$label, [string]$command, [string[]]$arguments) {
  ("===== {0} =====" -f $label) | Out-File -FilePath $log -Append -Encoding utf8
  $stdoutFile = [System.IO.Path]::GetTempFileName()
  $stderrFile = [System.IO.Path]::GetTempFileName()
  try {
    $startInfo = New-Object System.Diagnostics.ProcessStartInfo
    $startInfo.FileName = (Get-Command $command -ErrorAction Stop).Source
    $startInfo.WorkingDirectory = $root
    $startInfo.UseShellExecute = $false
    $startInfo.CreateNoWindow = $true
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true
    # ProcessStartInfo.Arguments is one command-line string on Windows PowerShell 5.1.
    # Quote each argument independently, preserving generator names and paths with spaces.
    $quoted = foreach ($arg in $arguments) {
      if ($arg -match '[\s"]') { '"' + ($arg -replace '(\\*)"', '$1$1\"' -replace '(\\+)$', '$1$1') + '"' }
      else { $arg }
    }
    $startInfo.Arguments = ($quoted -join ' ')
    $process = New-Object System.Diagnostics.Process
    $process.StartInfo = $startInfo
    if (-not $process.Start()) { Fail "$label could not start" }
    $outTask = $process.StandardOutput.ReadToEndAsync()
    $errTask = $process.StandardError.ReadToEndAsync()
    $process.WaitForExit()
    [System.IO.File]::WriteAllText($stdoutFile,$outTask.Result)
    [System.IO.File]::WriteAllText($stderrFile,$errTask.Result)
    Get-Content $stdoutFile | Out-File -FilePath $log -Append -Encoding utf8
    Get-Content $stderrFile | Out-File -FilePath $log -Append -Encoding utf8
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
  $form.Close()
} catch {
  Fail $_.Exception.Message
}
