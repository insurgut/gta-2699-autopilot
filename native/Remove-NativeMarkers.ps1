[CmdletBinding()]
param([string]$ClientPath)
$ErrorActionPreference='Stop'
try {
    if(Get-Process -Name 'District9_b2699_GameProcess' -ErrorAction SilentlyContinue){throw 'Сначала полностью закрой игру District. Затем повтори удаление.'}
    if(!$ClientPath){$ClientPath=Join-Path $env:LOCALAPPDATA 'DISTRICT 9 RP\FiveM.app'}
    if(!(Test-Path (Join-Path $ClientPath 'CitizenGame.dll')) -or !(Test-Path (Join-Path $ClientPath 'scripthookv.dll'))){
        Add-Type -AssemblyName System.Windows.Forms
        $dialog=New-Object System.Windows.Forms.FolderBrowserDialog
        $dialog.Description='Выбери FiveM.app внутри папки DISTRICT 9 RP (где CitizenGame.dll и scripthookv.dll)'
        if($dialog.ShowDialog() -ne 'OK'){throw 'Папка клиента не выбрана.'}
        $ClientPath=$dialog.SelectedPath
    }
    if(!(Test-Path (Join-Path $ClientPath 'CitizenGame.dll')) -or !(Test-Path (Join-Path $ClientPath 'scripthookv.dll'))){throw 'В выбранной папке нет ожидаемых компонентов CitizenFX. Файлы не изменены.'}
    $plugins=Join-Path $ClientPath 'plugins'
    $target=Join-Path $plugins 'DistrictNativeMarkers.asi'
    if(Test-Path $target){Remove-Item -LiteralPath $target}
    if(Test-Path ($target+'.previous')){Remove-Item -LiteralPath ($target+'.previous')}
    Write-Host 'Плагин игровых маркеров удалён. Остальные файлы клиента сохранены.'
    exit 0
} catch {Write-Host $_.Exception.Message -ForegroundColor Red;exit 1}
