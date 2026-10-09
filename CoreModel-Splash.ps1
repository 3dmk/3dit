# CoreModel updater splash. Runs in a separate STA PowerShell process.
param([Parameter(Mandatory=$true)][string]$SignalFile, [Parameter(Mandatory=$true)][string]$ProgressFile)
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
[System.Windows.Forms.Application]::EnableVisualStyles()
$form = New-Object System.Windows.Forms.Form
$form.Text = 'CoreModel'
$form.FormBorderStyle = 'None'
$form.StartPosition = 'CenterScreen'
$form.Size = New-Object System.Drawing.Size(480,220)
$form.BackColor = [System.Drawing.Color]::FromArgb(30,30,30)
$form.TopMost = $true
$heading = New-Object System.Windows.Forms.Label
$heading.Text = 'COREMODEL'
$heading.Font = New-Object System.Drawing.Font('Segoe UI',26,[System.Drawing.FontStyle]::Bold)
$heading.ForeColor = [System.Drawing.Color]::White
$heading.Location = New-Object System.Drawing.Point(32,36)
$heading.AutoSize = $true
$form.Controls.Add($heading)
$status = New-Object System.Windows.Forms.Label
$status.Text = 'Checking for updates and preparing editor...'
$status.Font = New-Object System.Drawing.Font('Segoe UI',11)
$status.ForeColor = [System.Drawing.Color]::FromArgb(170,170,170)
$status.Location = New-Object System.Drawing.Point(35,106)
$status.Size = New-Object System.Drawing.Size(410,30)
$form.Controls.Add($status)
# One-way progress: fills left to right, never loops.
$line = New-Object System.Windows.Forms.Panel
$line.Location = New-Object System.Drawing.Point(35,154)
$line.Size = New-Object System.Drawing.Size(410,5)
$line.BackColor = [System.Drawing.Color]::FromArgb(53,53,53)
$form.Controls.Add($line)
$fill = New-Object System.Windows.Forms.Panel
$fill.Location = New-Object System.Drawing.Point(0,0)
$fill.Size = New-Object System.Drawing.Size(0,5)
$fill.BackColor = [System.Drawing.Color]::FromArgb(105,105,105)
$line.Controls.Add($fill)
$script:progress = 0
$timer = New-Object System.Windows.Forms.Timer
$timer.Interval = 120
$timer.Add_Tick({
  if(Test-Path -LiteralPath $ProgressFile){
    try {
      $parts = ([System.IO.File]::ReadAllText($ProgressFile)).Trim().Split('|',2)
      $target = [int]$parts[0]
      $script:progress = [Math]::Max($script:progress,[Math]::Min(100,[Math]::Max(0,$target)))
      if($parts.Length -gt 1){$status.Text = $parts[1]}
      $fill.Width = [int][Math]::Round(410*$script:progress/100)
    } catch { }
  }
  if(Test-Path -LiteralPath $SignalFile){
    $script:progress=100
    $fill.Width=410
    $timer.Stop()
    $form.Close()
  }
})
$form.Add_Shown({$timer.Start()})
[System.Windows.Forms.Application]::Run($form)
$timer.Dispose()
$form.Dispose()
