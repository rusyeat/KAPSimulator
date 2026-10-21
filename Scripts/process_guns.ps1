# Мылтық жазбаларын жеке атыстарға кесу: бұрын бір атыс 4–15 атысы бар бүкіл файлды ойнататын (қабаттасу).
# Әр файлдағы атыс «топтарын» (жаңғырығымен) табамыз: күшті басталу, алдыңғысынан MinGap сек кейін.
# Шығыс: External/Audio/guns_cut/<Bank>_<i>.wav → import_freesound.py сияқты импорттау (import_guns.py)
Add-Type -Path "$PSScriptRoot\KAPAudioTool.cs"
[System.Threading.Thread]::CurrentThread.CurrentCulture = [Globalization.CultureInfo]::InvariantCulture
$src = "D:\KAPSimulator\External\Audio\guns\x_gunshots\sounds"
$out = "D:\KAPSimulator\External\Audio\guns_cut"
New-Item -ItemType Directory -Force $out | Out-Null
Get-ChildItem $out -Filter *.wav | Remove-Item

function Cut-Shots($file, $bank, $minGap, $tail, $fadeOut, $target, $thr = 0.55, $back = 0.2)
{
  $r = 0
  $m = [KAPAudioTool]::Mono([KAPAudioTool]::Read("$src\$file", [ref]$r))
  $on = [KAPAudioTool]::Onsets($m, $r, 0.003, 6.0, 0.06, 0.05)
  $pk = @($on | ForEach-Object { [KAPAudioTool]::Peak($m, $_, $_ + [int]($r * 0.05)) })
  $mx = ($pk | Measure-Object -Maximum).Maximum
  $keep = @(); $last = -1e9
  for ($k = 0; $k -lt $on.Count; $k++) {
    $t = $on[$k] / $r
    if ($pk[$k] -ge $thr * $mx -and ($t - $last) -gt $minGap) { $keep += $on[$k]; $last = $t }
  }
  $i = 0
  for ($k = 0; $k -lt $keep.Count; $k++) {
    # атыс тобының басына шегіну (алдындағы 0.2 с ішіндегі әлсіз басталу — шабуыл бөлігі)
    $st = $keep[$k]; foreach ($o in $on) { if ($o -lt $keep[$k] -and $o -ge $keep[$k] - [int]($r * $back) -and $o -lt $st) { $st = $o } }
    $a = $st - [int]($r * 0.004)
    $endT = $keep[$k] / $r + $tail
    if ($k + 1 -lt $keep.Count) { $endT = [math]::Min($endT, $keep[$k + 1] / $r - 0.04) }
    # келесі (әлсіз) оқиғадан бұрын тоқтау: затвор, гильза — басқа дыбыс
    foreach ($o in $on) { $to = $o / $r; if ($to -gt $keep[$k] / $r + 0.35 -and $to -lt $endT) { $j = [array]::IndexOf($on, $o); if ($pk[$j] -lt 0.55 * $mx) { $endT = $to - 0.02; break } } }
    $c = [KAPAudioTool]::Cut($m, $a, [int]($endT * $r), [int]($r * 0.001), [int]($r * $fadeOut), $target)
    [KAPAudioTool]::Write("$out\$($bank)_$i.wav", $c, $r)
    "{0}_{1}: {2:N2}s..{3:N2}s" -f $bank, $i, ($keep[$k] / $r), $endT
    $i++
  }
}
Cut-Shots "cz.wav" "GunPM" 0.6 0.9 0.35 0.9
Cut-Shots "sks.wav" "GunAK" 0.3 0.75 0.3 0.9 0.55 0.0
Cut-Shots "mosin.wav" "GunSniper" 1.2 1.3 0.45 0.92 0.78 0.0
Cut-Shots "shotty.wav" "GunShotgun" 2.0 0.7 0.25 0.92 0.55 0.0
