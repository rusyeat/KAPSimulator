# vo_lines.csv-тағы берілген бөлімнің жолдарына Windows TTS орынбасар дауыс (ElevenLabs жазбасы келгенше)
# Файл аты ойындағыдай: VO_<FNV-1a(UTF-16)>_<m|f>.wav → External\VO (import_vo.py импорттайды)
param([string]$Csv = "D:\KAPSimulator\Saved\VO_Script\vo_lines.csv", [string]$Sec = "8", [string]$Out = "D:\KAPSimulator\External\VO")
Add-Type -TypeDefinition @"
using System; using System.Text;
public static class KapVoP {
    public static string Hash(string s) { uint h = 2166136261u; foreach (char c in s) { h ^= (uint)c; h *= 16777619u; } return h.ToString("x8"); }
    public static string Translit(string s) {
        var m = new StringBuilder();
        foreach (char c in s) {
            switch (c) {
                case 'ә': m.Append('а'); break; case 'Ә': m.Append('А'); break; case 'ғ': m.Append('г'); break; case 'Ғ': m.Append('Г'); break;
                case 'қ': m.Append('к'); break; case 'Қ': m.Append('К'); break; case 'ң': m.Append('н'); break; case 'Ң': m.Append('Н'); break;
                case 'ө': m.Append('ё'); break; case 'Ө': m.Append('Ё'); break; case 'ұ': m.Append('у'); break; case 'Ұ': m.Append('У'); break;
                case 'ү': m.Append('ю'); break; case 'Ү': m.Append('Ю'); break; case 'һ': m.Append('х'); break; case 'Һ': m.Append('Х'); break;
                case 'і': m.Append('и'); break; case 'І': m.Append('И'); break; case '«': case '»': case '(': case ')': break;
                default: m.Append(c); break;
            }
        }
        return m.ToString();
    }
}
"@ -Language CSharp
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
$n = 0
foreach ($r in (Import-Csv $Csv -Encoding UTF8 | Where-Object { $_.Sec -eq $Sec })) {
    $t = $r.Text
    $h = [KapVoP]::Hash($t)
    if ($h -ne $r.Hash) { Write-Host "HASH MISMATCH $t"; continue }
    $gs = if ($r.G -eq 'mf') { @('m', 'f') } else { @($r.G) }
    foreach ($g in $gs) {
        $path = Join-Path $Out "VO_${h}_$g.wav"
        if (Test-Path $path) { continue }
        $tts = [System.Security.SecurityElement]::Escape([KapVoP]::Translit($t))
        if ($tts.Trim('.', ' ') -eq '') { $tts = '...' }
        $rate = if ($g -eq 'm') { '0.92' } else { '0.98' }
        $pitch = if ($g -eq 'm') { '-6%' } else { '+0%' }
        $ssml = "<speak version='1.0' xmlns='http://www.w3.org/2001/10/synthesis' xml:lang='ru-RU'><prosody rate='$rate' pitch='$pitch'>$tts</prosody></speak>"
        $st = Await ($syn[$g].SynthesizeSsmlToStreamAsync($ssml)) ([Windows.Media.SpeechSynthesis.SpeechSynthesisStream])
        $rd = New-Object Windows.Storage.Streams.DataReader($st.GetInputStreamAt(0))
        $null = Await ($rd.LoadAsync([uint32]$st.Size)) ([uint32])
        $b = New-Object byte[] ([int]$st.Size); $rd.ReadBytes($b)
        [IO.File]::WriteAllBytes($path, $b)
        $n++
    }
}
"WROTE $n files"
