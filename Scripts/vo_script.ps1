# vo_lines.tsv → файл аттары (ойындағыдай FNV-1a/UTF-16) + бастапқы кодта бар-жоғын тексеру + markdown кестелер
param([string]$Tsv = "D:\KAPSimulator\Scripts\vo_lines.tsv", [string]$Src = "D:\KAPSimulator\Source\KAPSimulator\KAP", [string]$Out = "D:\KAPSimulator\Saved\VO_Script")
Add-Type -TypeDefinition @"
public static class KapVoH { public static string Hash(string s) { uint h = 2166136261u; foreach (char c in s) { h ^= (uint)c; h *= 16777619u; } return h.ToString("x8"); } }
"@ -Language CSharp
New-Item -ItemType Directory -Force $Out | Out-Null
$all = ""
foreach ($f in Get-ChildItem $Src -Recurse -Include *.cpp,*.h) { $all += [IO.File]::ReadAllText($f.FullName) }
# тек «қосылатын» нұсқалар (босс, мұнара, финал себебі) — кодта бөліктері ғана бар
$variantOk = @('Босс жойылды: ', 'Сигнал мұнарасы қосылды: ', '«Қашу». ')
$rows = @()
$bad = 0
foreach ($l in [IO.File]::ReadAllLines($Tsv, [Text.Encoding]::UTF8)) {
    if ($l.StartsWith('#') -or $l.Trim() -eq '') { continue }
    $c = $l.Split("`t")
    if ($c.Count -lt 7) { Write-Host "BADROW $l"; $bad++; continue }
    $text = $c[5].Replace('\n', "`n")
    $esc = $c[5]   # кодта \n әріптік күйде тұрады
    $found = $all.Contains('"' + $esc + '"') -or $all.Contains($esc)
    if (-not $found) { $isVar = $false; foreach ($v in $variantOk) { if ($esc.StartsWith($v)) { $isVar = $true } }; if (-not $isVar) { Write-Host "NOTFOUND [$($c[1])] $esc"; $bad++ } }
    $g = $c[2]; $spk = $c[1]
    # рация хабарламасы: ойын дауысты ұзындық бойынша таңдайды (Len % 3 == 0 → әйел)
    if ($g -eq 'radio') { $g = if (($text.Length % 3) -eq 0) { 'f' } else { 'm' }; $spk = if ($g -eq 'f') { 'Кеніш диспетчері (әйел)' } else { 'Кеніш диспетчері (ер)' } }
    if ($spk -eq 'Радио') { $spk = 'Кеніш диспетчері (ер)' }
    $h = [KapVoH]::Hash($text)
    $files = if ($g -eq 'mf') { "VO_${h}_m.wav, VO_${h}_f.wav" } else { "VO_${h}_$g.wav" }
    $rows += [pscustomobject]@{ Sec = $c[0]; Speaker = $spk; G = $g; Ctx = $c[3]; Emo = $c[4]; Text = $text; Tagged = $c[6]; Hash = $h; Files = $files }
}
"ROWS $($rows.Count) BAD $bad"
$rows | Export-Csv "$Out\vo_lines.csv" -NoTypeInformation -Encoding UTF8
# markdown: бөлім → кесте
function Cell([string]$s) { return $s.Replace('|', '/').Replace("`n", ' ') }
foreach ($sec in ($rows | Select-Object -ExpandProperty Sec -Unique)) {
    $md = "| № | Кім | Қай кезде | Эмоция | Файл | ElevenLabs-қа (көшіріп қойыңыз) |`n| --- | --- | --- | --- | --- | --- |`n"
    $i = 0
    foreach ($r in ($rows | Where-Object { $_.Sec -eq $sec })) {
        $i++
        $gl = if ($r.G -eq 'mf') { ' (ер + әйел)' } elseif ($r.G -eq 'f' -and $r.Speaker -notlike '*әйел*' -and $r.Speaker -notlike 'Ана*' -and $r.Speaker -notlike 'Планшет*' -and $r.Speaker -notlike '*қызы*') { ' (әйел)' } else { '' }
        $md += "| $i | $(Cell $r.Speaker)$gl | $(Cell $r.Ctx) | $(Cell $r.Emo) | ``$(Cell $r.Files)`` | $(Cell $r.Tagged) |`n"
    }
    [IO.File]::WriteAllText("$Out\sec$sec.md", $md, [Text.UTF8Encoding]::new($false))
}
# кейіпкер бойынша сан (ер+әйел жолдары екі файл)
$rows | Group-Object Speaker | ForEach-Object { $n = $_.Count; $fl = ($_.Group | ForEach-Object { if ($_.G -eq 'mf') { 2 } else { 1 } } | Measure-Object -Sum).Sum; "{0}`t{1}`t{2}" -f $_.Name, $n, $fl } | Out-File "$Out\counts.tsv" -Encoding utf8
"FILES " + (($rows | ForEach-Object { if ($_.G -eq 'mf') { 2 } else { 1 } } | Measure-Object -Sum).Sum)
