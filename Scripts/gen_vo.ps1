# Ойын кодындағы барлық сөйлемді жинап, Windows TTS (Pavel — ер, Irina — әйел) арқылы WAV етіп жазады.
# Файл аты: VO_<FNV-1a(UTF-16) hex>_m.wav / _f.wav — ойын дәл осы хэшпен табады (UKAPAudio::VoHash).
param([string]$Src = "D:\KAPSimulator\Source\KAPSimulator\KAP", [string]$Out = "D:\KAPSimulator\External\VO", [switch]$ListOnly)

Add-Type -TypeDefinition @"
using System; using System.Text;
public static class KapVo {
    public static string Hash(string s) { uint h = 2166136261u; foreach (char c in s) { h ^= (uint)c; h *= 16777619u; } return h.ToString("x8"); }
    public static string Translit(string s) {
        var m = new StringBuilder();
        foreach (char c in s) {
            switch (c) {
                case 'ә': m.Append('а'); break; case 'Ә': m.Append('А'); break;
                case 'ғ': m.Append('г'); break; case 'Ғ': m.Append('Г'); break;
                case 'қ': m.Append('к'); break; case 'Қ': m.Append('К'); break;
                case 'ң': m.Append('н'); break; case 'Ң': m.Append('Н'); break;
                case 'ө': m.Append('ё'); break; case 'Ө': m.Append('Ё'); break;
                case 'ұ': m.Append('у'); break; case 'Ұ': m.Append('У'); break;
                case 'ү': m.Append('ю'); break; case 'Ү': m.Append('Ю'); break;
                case 'һ': m.Append('х'); break; case 'Һ': m.Append('Х'); break;
                case 'і': m.Append('и'); break; case 'І': m.Append('И'); break;
                case '«': case '»': break;
                case '№': m.Append("номер "); break;
                case '\n': m.Append(". "); break;
                default: m.Append(c); break;
            }
        }
        return m.ToString();
    }
}
"@ -Language CSharp

function Unescape([string]$s) { return $s.Replace('\n', "`n").Replace('\"', '"').Replace('\\', '\') }
$lit = '"((?:[^"\\]|\\.)*)"'
$lines = New-Object System.Collections.Generic.List[string]
function AddIf([string]$t) {
    $u = Unescape $t
    if ($u -notmatch '[\u0400-\u04FF]') { return }
    if ($u.Length -lt 6) { return }
    if (-not ($u -match ' ' -or $u -match '[!?.…]$')) { return }
    if ($u -match '%[ds]') { return }   # Printf үлгілері — динамикалық
    if (-not $lines.Contains($u)) { $lines.Add($u) }
}
# NPC: барлық сөйлемдер (Bark, айқай, тапсырмалар)
foreach ($f in @("$Src\NPC\KAPNPC.cpp", "$Src\NPC\KAPNPCTasks.cpp")) {
    foreach ($m in [regex]::Matches([IO.File]::ReadAllText($f), "TEXT\($lit\)")) { AddIf $m.Groups[1].Value }
}
# GameMode: субтитр, рация, диалог, кіріспе сөйлемдері
$gm = [IO.File]::ReadAllLines("$Src\Core\KAPGameMode.cpp")
foreach ($l in $gm) {
    foreach ($m in [regex]::Matches($l, "Subtitle\(TEXT\($lit\)")) { AddIf $m.Groups[1].Value }
    foreach ($m in [regex]::Matches($l, "Radio\(TEXT\($lit\)")) { AddIf $m.Groups[1].Value }
    foreach ($m in [regex]::Matches($l, "\{\s*\d+,\s*TEXT\($lit\)")) { AddIf $m.Groups[1].Value }
    if ($l -match 'Dialog\(') { foreach ($m in [regex]::Matches($l, "TEXT\($lit\)")) { AddIf $m.Groups[1].Value } }
}
"LINES " + $lines.Count
if ($ListOnly) { $lines | ForEach-Object { (([KapVo]::Hash($_)) + "  " + $_.Replace("`n", " / ")) }; return }

Add-Type -AssemblyName System.Runtime.WindowsRuntime
$null = [Windows.Media.SpeechSynthesis.SpeechSynthesizer, Windows.Media.SpeechSynthesis, ContentType = WindowsRuntime]
$null = [Windows.Storage.Streams.DataReader, Windows.Storage.Streams, ContentType = WindowsRuntime]
$asTask = ([System.WindowsRuntimeSystemExtensions].GetMethods() | Where-Object { $_.Name -eq 'AsTask' -and $_.GetParameters().Count -eq 1 -and $_.GetParameters()[0].ParameterType.Name -eq 'IAsyncOperation`1' })[0]
function Await($op, [Type]$t) { $task = $asTask.MakeGenericMethod($t).Invoke($null, @($op)); $task.Wait(); $task.Result }
$voices = [Windows.Media.SpeechSynthesis.SpeechSynthesizer]::AllVoices
$syn = @{ m = New-Object Windows.Media.SpeechSynthesis.SpeechSynthesizer; f = New-Object Windows.Media.SpeechSynthesis.SpeechSynthesizer }
$syn.m.Voice = $voices | Where-Object { $_.DisplayName -like '*Pavel*' } | Select-Object -First 1
$syn.f.Voice = $voices | Where-Object { $_.DisplayName -like '*Irina*' } | Select-Object -First 1
New-Item -ItemType Directory -Force $Out | Out-Null
$manifest = New-Object System.Collections.Generic.List[string]
$n = 0
foreach ($t in $lines) {
    $h = [KapVo]::Hash($t)
    $tts = [System.Security.SecurityElement]::Escape([KapVo]::Translit($t))
    foreach ($g in @('m', 'f')) {
        $path = Join-Path $Out "VO_${h}_$g.wav"
        if (Test-Path $path) { continue }
        $rate = if ($g -eq 'm') { '0.92' } else { '0.98' }
        $pitch = if ($g -eq 'm') { '-6%' } else { '+0%' }
        $ssml = "<speak version='1.0' xmlns='http://www.w3.org/2001/10/synthesis' xml:lang='ru-RU'><prosody rate='$rate' pitch='$pitch'>$tts</prosody></speak>"
        $st = Await ($syn[$g].SynthesizeSsmlToStreamAsync($ssml)) ([Windows.Media.SpeechSynthesis.SpeechSynthesisStream])
        $r = New-Object Windows.Storage.Streams.DataReader($st.GetInputStreamAt(0))
        $null = Await ($r.LoadAsync([uint32]$st.Size)) ([uint32])
        $b = New-Object byte[] ([int]$st.Size); $r.ReadBytes($b)
        [IO.File]::WriteAllBytes($path, $b)
        $n++
    }
    $manifest.Add("$h`t" + $t.Replace("`n", " / "))
}
[IO.File]::WriteAllLines((Join-Path $Out "manifest.tsv"), $manifest, (New-Object Text.UTF8Encoding $true))
"WROTE $n files"
