# AUDIO_DEPLOY_HANDOFF кестелерінен -kapaudiocheck деректерін жасау: Source/KAPSimulator/KAP/Audio/KAPAudioCheckData.inl
# Іске қосу: powershell -File Scripts/gen_audio_check.ps1 [-Handoff <папка>] [-Durations <all_203_files.csv>]
param(
	[string]$Handoff = "C:\Users\rusya\Documents\Codex\2026-10-21\js\work\Audio_Claude_Handoff",
	[string]$Durations = "C:\Users\rusya\Downloads\all_203_files.csv"
)
$root = Split-Path -Parent $PSScriptRoot
$s = Import-Csv (Join-Path $Handoff "SFX_Music_file_mapping.csv") -Encoding UTF8
$v = Import-Csv (Join-Path $Handoff "VO_203_file_mapping.csv") -Encoding UTF8
$d = @{}
if (Test-Path $Durations) { Import-Csv $Durations -Encoding UTF8 | ForEach-Object { $d[$_.'Файл'] = $_.'Ұзақтығы секунд' } }
$inv = [Globalization.CultureInfo]::InvariantCulture
function Esc([string]$t) { return $t.Replace('\', '\\').Replace('"', '\"').Replace("`r", '').Replace("`n", '\n') }
$sb = New-Object System.Text.StringBuilder
[void]$sb.AppendLine('// Автоматты түрде жасалған (Scripts/gen_audio_check.ps1): AUDIO_DEPLOY_HANDOFF кестелері бойынша дайын EXE-дегі дыбыс тексеруі (-kapaudiocheck)')
[void]$sb.AppendLine('struct FKAPBankCheck { const TCHAR* Bank; int32 Count; bool bLoop; };')
[void]$sb.AppendLine('static const FKAPBankCheck GKAPBankChecks[] = {')
$s | Group-Object bank | Sort-Object Name | ForEach-Object {
	$loop = if (($_.Group | Select-Object -First 1).loop -eq 'True') { 'true' } else { 'false' }
	[void]$sb.AppendLine("`t{ TEXT(`"$($_.Name)`"), $($_.Count), $loop },")
}
[void]$sb.AppendLine('};')
[void]$sb.AppendLine('struct FKAPVoCheck { const TCHAR* Asset; bool bReady; float Dur; };')
[void]$sb.AppendLine('static const FKAPVoCheck GKAPVoChecks[] = {')
foreach ($r in $v) {
	$n = $r.unreal_asset.Split('/')[-1]
	$rd = if ($r.ready_wav_exists -eq 'True') { 'true' } else { 'false' }
	$du = if ($d[$r.file]) { [double]::Parse($d[$r.file], $inv).ToString('0.0##', $inv) } else { '0.0' }
	[void]$sb.AppendLine("`t{ TEXT(`"$n`"), $rd, ${du}f },")
}
[void]$sb.AppendLine('};')
[void]$sb.AppendLine('struct FKAPVoLine { const TCHAR* Text; bool bFemale; };')
[void]$sb.AppendLine('static const FKAPVoLine GKAPVoLines[] = {')
Get-Content (Join-Path $root "Scripts\vo_lines.tsv") -Encoding UTF8 | Select-Object -Skip 1 | ForEach-Object {
	$c = $_.Split("`t")
	if ($c.Count -ge 6 -and $c[5]) {
		# tsv-те жол үзілісі «\n» болып жазылған — ойын мәтінінде нақты жол үзілісі бар (хэш соған есептеледі)
		$txt = $c[5].Replace('\n', "`n")
		# рация: дауыс жынысын ойын мәтін ұзындығымен таңдайды (AKAPGameMode::Radio: Len % 3 == 0 → әйел)
		$fem = if ($c[2] -eq 'f') { $true } elseif ($c[2] -eq 'radio') { ($txt.Length % 3) -eq 0 } else { $false }
		$f = if ($fem) { 'true' } else { 'false' }
		[void]$sb.AppendLine("`t{ TEXT(`"$(Esc $txt)`"), $f },")
	}
}
[void]$sb.AppendLine('};')
$out = Join-Path $root "Source\KAPSimulator\KAP\Audio\KAPAudioCheckData.inl"
[IO.File]::WriteAllText($out, $sb.ToString(), (New-Object System.Text.UTF8Encoding($true)))
Write-Output "KAPAUDIO check data -> $out"
