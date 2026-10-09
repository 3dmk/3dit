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
$form.BackColor = [System.Drawing.Color]::FromArgb(23,27,36)
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
$status.ForeColor = [System.Drawing.Color]::FromArgb(178,193,216)
$status.Location = New-Object System.Drawing.Point(35,106)
$status.Size = New-Object System.Drawing.Size(410,30)
$form.Controls.Add($status)
$bar = New-Object System.Windows.Forms.ProgressBar
$bar.Style = 'Marquee'
$bar.MarqueeAnimationSpeed = 25
$bar.Location = New-Object System.Drawing.Point(35,154)
$bar.Size = New-Object System.Drawing.Size(410,8)
$form.Controls.Add($bar)
$timer = New-Object System.Windows.Forms.Timer
$timer.Interval = 250
$timer.Add_Tick({ if(Test-Path -LiteralPath $SignalFile){$timer.Stop();$form.Close()} })
$form.Add_Shown({$timer.Start()})
[System.Windows.Forms.Application]::Run($form)
$timer.Dispose()
$form.Dispose()
