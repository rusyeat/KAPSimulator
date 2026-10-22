# Тизер-трейлерді Movie Render Queue арқылы рендерлейді (ештеңе жүктемейді — движоктың өз ProRes/MP4 кодтағыштары).
# Алдын ала: Scripts/make_trailer_assets.py (LS_KAPTrailer + MPC_KAPTrailer активтері) бір рет орындалған болуы керек.
# Нәтиже: Saved/Trailer/KAP_Trailer_15s.mov (ProRes 422), .mp4 (H.264), ...MasterSubmixDefault.wav; соңында дыбыс деңгейі шығады.
param([string]$Project = "D:\KAPSimulator\KAPSimulator.uproject", [string]$Engine = "D:\ue5\UE_5.8")
$out = Join-Path (Split-Path $Project) "Saved\Trailer"
$log = Join-Path (Split-Path $Project) "Saved\Logs\kaptrailer_mrq.log"
Remove-Item $out -Recurse -Force -ErrorAction SilentlyContinue
Remove-Item $log -ErrorAction SilentlyContinue
$args = "`"$Project`" /Game/KAP/Maps/KAP_Steppe -game -LevelSequence=`"/Game/KAP/Cinematics/LS_KAPTrailer.LS_KAPTrailer`" " +
        "-MoviePipelineConfig=`"/Game/KAP/Cinematics/MPC_KAPTrailer.MPC_KAPTrailer`" -kaptrailer -deterministicaudio " +
        "-windowed -resx=1280 -resy=720 -nosplash -notexturestreaming -unattended -abslog=`"$log`""
$p = Start-Process (Join-Path $Engine "Engine\Binaries\Win64\UnrealEditor.exe") -ArgumentList $args -PassThru
$p.WaitForExit(1500000) | Out-Null
"exit " + $p.ExitCode
Select-String -Path $log -Pattern "KAPTRAILER (shot|event)|Fatal|Error:" | Where-Object { $_.Line -notmatch "Frame number" } | ForEach-Object { $_.Line -replace '^\[(.*?)\]\[.*?\]LogTemp: Display: ', '' }
Get-ChildItem $out -ErrorAction SilentlyContinue | Where-Object { $_.Extension -notin ".jpeg", ".jpg" } | ForEach-Object { "{0}  {1:N1} MB" -f $_.Name, ($_.Length / 1MB) }
# дыбыс деңгейі (0.5 с терезелер): RMS және шың, дБFS
$wav = Get-ChildItem $out -Filter *.wav -ErrorAction SilentlyContinue | Select-Object -First 1
if ($wav) {
    $b = [System.IO.File]::ReadAllBytes($wav.FullName); $ch = [BitConverter]::ToInt16($b, 22); $sr = [BitConverter]::ToInt32($b, 24)
    $p0 = 12; while ($p0 -lt $b.Length - 8) { $id = [System.Text.Encoding]::ASCII.GetString($b, $p0, 4); $sz = [BitConverter]::ToInt32($b, $p0 + 4); if ($id -eq "data") { $p0 += 8; break }; $p0 += 8 + $sz }
    $fr = $ch * 2; $win = [int]($sr * 0.5); $n = [int](($b.Length - $p0) / $fr); $top = 0
    for ($w = 0; $w -lt [Math]::Floor($n / $win); $w++) {
        $acc = 0.0; $pk = 0
        for ($i = 0; $i -lt $win; $i += 2) { $v = [BitConverter]::ToInt16($b, $p0 + ($w * $win + $i) * $fr); $acc += $v * $v; if ([Math]::Abs($v) -gt $pk) { $pk = [Math]::Abs($v) } }
        $rms = [Math]::Sqrt($acc / ($win / 2)); if ($pk -gt $top) { $top = $pk }
        "{0,4:N1}s  rms {1,6:N1} dB  peak {2,6:N1} dB" -f ($w * 0.5), (20 * [Math]::Log10([Math]::Max($rms, 1) / 32768)), (20 * [Math]::Log10([Math]::Max($pk, 1) / 32768))
    }
    $clip = 0; for ($i = $p0; $i -lt $b.Length - 1; $i += 2) { $v = [BitConverter]::ToInt16($b, $i); if ($v -ge 32700 -or $v -le -32700) { $clip++ } }
    "max peak {0:N1} dBFS, clipped samples {1}" -f (20 * [Math]::Log10([Math]::Max($top, 1) / 32768)), $clip
}
