# Freesound (CC0) жазбаларын ойынға дайындау: қадамдарды кесу, генератор циклі, мылтық атыстары, Гейгер шертпелері.
# Шығыс: External/Audio/freesound/*.wav (import_audio.py импорттайды) және Source/.../Audio/KAPGeigerClicks.inl
Add-Type -Path "$PSScriptRoot\KAPAudioTool.cs"
# сандар C++ үшін нүктемен жазылсын (жүйе тілінде үтір болуы мүмкін)
[System.Threading.Thread]::CurrentThread.CurrentCulture = [Globalization.CultureInfo]::InvariantCulture
$src = "C:\Users\rusya\Downloads\soundef"
$out = "D:\KAPSimulator\External\Audio\freesound"
New-Item -ItemType Directory -Force $out | Out-Null
Get-ChildItem $out -Filter "*_*.wav" | Where-Object { $_.Name -ne "shooting_1_decoded.wav" } | Remove-Item

function Sec($t, $r) { [int]([double]$t * $r) }

# ---- 1. қадамдар (дымқыл құм, тау етігі): 24 жүру, жүгіру, секіріп түсу ----
$r = 0
$m = [KAPAudioTool]::Mono([KAPAudioTool]::Read("$src\564893__nox_sound__footsteps_mountain_boots_wet_sand_sequence_mono.wav", [ref]$r))
$walk = 0.06,1.03,2.21,3.42,4.63,5.77,6.75,7.78,8.83,9.82,10.85,12.03,13.02,14.09,15.13,16.29,17.47,18.56,19.69,20.76,21.77,22.89,24.05,25.16
$run = 27.29,27.68,28.07,28.44,28.83,29.20,29.64,30.01,30.41,30.81,31.21,31.60,32.00,32.40,32.82,33.20,33.59,34.01,34.41
$land = 36.83,39.33,41.97,44.68,47.21
$i = 0; foreach ($t in $walk) { $c = [KAPAudioTool]::Cut($m, (Sec ($t - 0.015) $r), (Sec ($t + 0.62) $r), (Sec 0.004 $r), (Sec 0.12 $r), 0.7); [KAPAudioTool]::Write("$out\StepSand_$i.wav", $c, $r); $i++ }
$i = 0; foreach ($t in $run) { $c = [KAPAudioTool]::Cut($m, (Sec ($t - 0.01) $r), (Sec ($t + 0.36) $r), (Sec 0.003 $r), (Sec 0.08 $r), 0.75); [KAPAudioTool]::Write("$out\StepSandRun_$i.wav", $c, $r); $i++ }
$i = 0; foreach ($t in $land) { $c = [KAPAudioTool]::Cut($m, (Sec ($t - 0.03) $r), (Sec ($t + 0.8) $r), (Sec 0.005 $r), (Sec 0.2 $r), 0.8); [KAPAudioTool]::Write("$out\LandSand_$i.wav", $c, $r); $i++ }
"steps: walk $($walk.Count) run $($run.Count) land $($land.Count)"

# ---- 2. дизель генераторы: тек сол микрофон (1.5 м; екі арнаны қосу тарақ сүзгі береді) ----
$ch = [KAPAudioTool]::Read("$src\516759__jameswrowles__small-diesel-generator.wav", [ref]$r)
$g = $ch[0]
[KAPAudioTool]::Write("$out\GenStart_0.wav", [KAPAudioTool]::Cut($g, 0, (Sec 8.0 $r), (Sec 0.01 $r), (Sec 1.2 $r), 0.9), $r)
[KAPAudioTool]::Write("$out\GenStop_0.wav", [KAPAudioTool]::Cut($g, (Sec 166.0 $r), $g.Length, (Sec 1.0 $r), (Sec 0.5 $r), 0.9), $r)
[KAPAudioTool]::Write("$out\Generator_0.wav", [KAPAudioTool]::Loop($g, (Sec 12.0 $r), (Sec 20.0 $r), (Sec 1.5 $r), 0.14), $r)
[KAPAudioTool]::Write("$out\Generator_1.wav", [KAPAudioTool]::Loop($g, (Sec 70.0 $r), (Sec 20.0 $r), (Sec 1.5 $r), 0.14), $r)
"generator: start, stop, 2 loops"

# ---- 3. мылтық (ашық далада, ұзақ жаңғырық): 10 жеке атыс ----
$gm = [KAPAudioTool]::Mono([KAPAudioTool]::Read("$out\shooting_1_decoded.wav", [ref]$r))
$shots = 0.67,3.12,6.10,10.23,13.61,17.64,20.64,23.62,26.46,29.72
for ($k = 0; $k -lt $shots.Count; $k++) {
  $t = $shots[$k]; $end = if ($k + 1 -lt $shots.Count) { [math]::Min($t + 2.4, $shots[$k + 1] - 0.06) } else { $t + 3.0 }
  [KAPAudioTool]::Write("$out\GunRifle_$k.wav", [KAPAudioTool]::Cut($gm, (Sec ($t - 0.004) $r), (Sec $end $r), (Sec 0.001 $r), (Sec 0.45 $r), 0.95), $r)
}
"rifle shots: $($shots.Count)"

# ---- 4. Гейгер: бос тұрған жеке шертпелер → 44.1 кГц, C++ массиві (синтездегі Пуассон шертпелеріне) ----
$gk = [KAPAudioTool]::Mono([KAPAudioTool]::Read("$src\555934__cookiespolicy__geiger-counter-hotspot-sweeping.wav", [ref]$r))
$cl = [KAPAudioTool]::IsolatedClicks($gk, $r, 0.0, 15.8, 0.03)
"isolated clicks: $($cl.Count)"
$N = 360; $sel = @(); $step = [math]::Max(1, [math]::Floor($cl.Count / 6))
for ($k = 0; $k -lt $cl.Count -and $sel.Count -lt 6; $k += $step) { $sel += $cl[$k] }
$sb = New-Object System.Text.StringBuilder
[void]$sb.AppendLine("// Автоматты түрде жасалған: Scripts/process_audio.ps1 — Гейгер санауышының нақты шертпелері (CC0, cookies+policy, freesound 555934)")
[void]$sb.AppendLine("static constexpr int32 GGeigerClickLen = $N;")
[void]$sb.AppendLine("static constexpr int32 GGeigerClickNum = $($sel.Count);")
[void]$sb.AppendLine("static const float GGeigerClicks[$($sel.Count)][$N] = {")
foreach ($o in $sel) {
  $c = [KAPAudioTool]::Cut($gk, $o - [int]($r * 0.0005), $o + [int]($r * 0.0095), 8, [int]($r * 0.003), 1.0)
  $c = [KAPAudioTool]::Resample($c, $r, 44100)
  $vals = for ($j = 0; $j -lt $N; $j++) { if ($j -lt $c.Length) { "{0:0.0####}f" -f $c[$j] } else { "0.f" } }
  [void]$sb.AppendLine("`t{ " + ($vals -join ", ") + " },")
}
[void]$sb.AppendLine("};")
[System.IO.File]::WriteAllText("D:\KAPSimulator\Source\KAPSimulator\KAP\Audio\KAPGeigerClicks.inl", $sb.ToString(), (New-Object System.Text.UTF8Encoding $true))
"geiger clicks: $($sel.Count) x $N"
Get-ChildItem $out -Filter *.wav | Measure-Object | ForEach-Object { "files: $($_.Count)" }