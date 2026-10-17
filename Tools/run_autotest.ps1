#	@tool 自動キャプチャを実行し、基準画像があれば比較する
#	@tool 使い方: powershell -File Tools\run_autotest.ps1 [-Configuration DevelopEditor] [-Name current] [-Baseline baseline] [-Frames 180]
#	@tool -ExtraArguments で起動引数を追加できる（例: -ExtraArguments '-model_editor_view'）
#	@tool 出力先は Captures\<Name>。-Name baseline で実行すると基準画像を作り直す
param(
	[string]$Configuration = 'DevelopEditor',
	[string]$Name = 'current',
	[string]$Baseline = 'baseline',
	[int]$Frames = 180,
	[int]$TimeoutSeconds = 540,
	[string[]]$ExtraArguments = @()
)

$root = Split-Path $PSScriptRoot -Parent
$exe = Join-Path $root "x64\$Configuration\TowerDefenceGame.exe"
$output = Join-Path $root "Captures\$Name"

if (-not (Test-Path $exe))
{
	Write-Error "実行ファイルがありません: $exe"
	exit 2
}
if (Test-Path $output)
{
	Remove-Item -Recurse -Force $output
}

$process = Start-Process -FilePath $exe -ArgumentList (@('-autotest', "Captures\$Name", '-autotest_frames', $Frames) + $ExtraArguments) -WorkingDirectory $root -PassThru
if (-not $process.WaitForExit($TimeoutSeconds * 1000))
{
	$process.Kill()
	Write-Error "時間切れで終了しました"
	exit 3
}
if ($process.ExitCode -ne 0)
{
	Write-Error ("異常終了しました（終了コード {0:X8}）" -f $process.ExitCode)
	if (Test-Path (Join-Path $output 'progress.txt')) { Get-Content (Join-Path $output 'progress.txt') | Select-Object -Last 3 }
	exit 4
}

Get-Content (Join-Path $output 'report.txt')

$baselinePath = Join-Path $root "Captures\$Baseline"
if ($Name -ne $Baseline -and (Test-Path $baselinePath))
{
	& (Join-Path $PSScriptRoot 'compare_captures.ps1') -Baseline $baselinePath -Target $output
	exit $LASTEXITCODE
}
exit 0
