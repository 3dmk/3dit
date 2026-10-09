# CoreModel updater splash. Runs in a separate STA PowerShell process.
param([Parameter(Mandatory=$true)][string]$SignalFile)
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
# Custom flat loading line: charcoal track and moving graphite segment.
# This avoids the bright Windows-themed ProgressBar control.
$line = New-Object System.Windows.Forms.Panel
$line.Location = New-Object System.Drawing.Point(35,154)
$line.Size = New-Object System.Drawing.Size(410,5)
$line.BackColor = [System.Drawing.Color]::FromArgb(53,53,53)
$form.Controls.Add($line)
$segment = New-Object System.Windows.Forms.Panel
$segment.Location = New-Object System.Drawing.Point(0,0)
$segment.Size = New-Object System.Drawing.Size(100,5)
$segment.BackColor = [System.Drawing.Color]::FromArgb(105,105,105)
$line.Controls.Add($segment)
$progressX = -100
$timer = New-Object System.Windows.Forms.Timer
$timer.Interval = 30
$timer.Add_Tick({
  if(Test-Path -LiteralPath $SignalFile){$timer.Stop();$form.Close();return}
  $script:progressX += 4
  if($script:progressX -gt 410){$script:progressX = -100}
  $segment.Left = $script:progressX
})
$form.Add_Shown({$timer.Start()})
[System.Windows.Forms.Application]::Run($form)
$timer.Dispose()
$form.Dispose()
