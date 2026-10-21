// KAP дыбыс құралы: WAV оқу/жазу, қадам/шертпе басталуын табу, кесу, тұйық цикл (crossfade), қалыпқа келтіру.
// PowerShell Add-Type арқылы компиляцияланады (C# 5 синтаксисі). Scripts/process_audio.ps1 қолданады.
using System;
using System.IO;
using System.Collections.Generic;

public static class KAPAudioTool
{
    // ---- WAV оқу: PCM 16/24/32, float32, extensible; арналар бөлек ----
    public static float[][] Read(string path, out int rate)
    {
        byte[] b = File.ReadAllBytes(path);
        if (b.Length < 12 || b[0] != 'R' || b[1] != 'I' || b[2] != 'F' || b[3] != 'F') throw new Exception("not RIFF: " + path);
        int pos = 12, fmtTag = 1, ch = 1, bits = 16; rate = 44100;
        int dataOff = -1, dataLen = 0;
        while (pos + 8 <= b.Length)
        {
            string id = System.Text.Encoding.ASCII.GetString(b, pos, 4);
            int len = BitConverter.ToInt32(b, pos + 4);
            int body = pos + 8;
            if (id == "fmt ")
            {
                fmtTag = BitConverter.ToUInt16(b, body);
                ch = BitConverter.ToUInt16(b, body + 2);
                rate = BitConverter.ToInt32(b, body + 4);
                bits = BitConverter.ToUInt16(b, body + 14);
                if (fmtTag == 0xFFFE && len >= 40) fmtTag = BitConverter.ToUInt16(b, body + 24);
            }
            else if (id == "data")
            {
                dataOff = body; dataLen = Math.Min(len, b.Length - body);
                if (len < 0 || len > b.Length - body) dataLen = b.Length - body; // үзілген файл
                break;
            }
            pos = body + len + (len & 1);
        }
        if (dataOff < 0) throw new Exception("no data chunk: " + path);
        int bps = bits / 8, frame = bps * ch, n = dataLen / frame;
        float[][] o = new float[ch][];
        for (int c = 0; c < ch; c++) o[c] = new float[n];
        for (int i = 0; i < n; i++)
            for (int c = 0; c < ch; c++)
            {
                int p = dataOff + i * frame + c * bps;
                float v;
                if (fmtTag == 3 && bits == 32) v = BitConverter.ToSingle(b, p);
                else if (bits == 16) v = BitConverter.ToInt16(b, p) / 32768f;
                else if (bits == 24) { int s = b[p] | (b[p + 1] << 8) | (b[p + 2] << 16); if ((s & 0x800000) != 0) s |= unchecked((int)0xFF000000); v = s / 8388608f; }
                else if (bits == 32) v = BitConverter.ToInt32(b, p) / 2147483648f;
                else v = (b[p] - 128) / 128f;
                o[c][i] = v;
            }
        return o;
    }

    public static float[] Mono(float[][] ch)
    {
        int n = ch[0].Length; float[] m = new float[n];
        for (int i = 0; i < n; i++) { float s = 0; for (int c = 0; c < ch.Length; c++) s += ch[c][i]; m[i] = s / ch.Length; }
        return m;
    }

    public static void Write(string path, float[] x, int rate)
    {
        Directory.CreateDirectory(Path.GetDirectoryName(path));
        using (var w = new BinaryWriter(File.Create(path)))
        {
            int n = x.Length;
            w.Write(System.Text.Encoding.ASCII.GetBytes("RIFF")); w.Write(36 + n * 2);
            w.Write(System.Text.Encoding.ASCII.GetBytes("WAVEfmt ")); w.Write(16); w.Write((short)1); w.Write((short)1);
            w.Write(rate); w.Write(rate * 2); w.Write((short)2); w.Write((short)16);
            w.Write(System.Text.Encoding.ASCII.GetBytes("data")); w.Write(n * 2);
            for (int i = 0; i < n; i++) { float v = Math.Max(-1f, Math.Min(1f, x[i])); w.Write((short)Math.Round(v * 32767f)); }
        }
    }

    // ---- көп арналы (стерео) 16-бит WAV ----
    public static void WriteMulti(string path, float[][] ch, int rate)
    {
        Directory.CreateDirectory(Path.GetDirectoryName(path));
        int nc = ch.Length, n = ch[0].Length;
        using (var w = new BinaryWriter(File.Create(path)))
        {
            w.Write(System.Text.Encoding.ASCII.GetBytes("RIFF")); w.Write(36 + n * 2 * nc);
            w.Write(System.Text.Encoding.ASCII.GetBytes("WAVEfmt ")); w.Write(16); w.Write((short)1); w.Write((short)nc);
            w.Write(rate); w.Write(rate * 2 * nc); w.Write((short)(2 * nc)); w.Write((short)16);
            w.Write(System.Text.Encoding.ASCII.GetBytes("data")); w.Write(n * 2 * nc);
            for (int i = 0; i < n; i++) for (int c = 0; c < nc; c++) { float v = Math.Max(-1f, Math.Min(1f, ch[c][i])); w.Write((short)Math.Round(v * 32767f)); }
        }
    }

    // ---- стерео тұйық цикл: соңғы Xf үлгі басымен тең қуатты араласады (барлық арнаға бірдей) ----
    public static float[][] LoopMulti(float[][] ch, int xf)
    {
        int nc = ch.Length, len = ch[0].Length - xf;
        float[][] o = new float[nc][];
        for (int c = 0; c < nc; c++)
        {
            o[c] = new float[len];
            Array.Copy(ch[c], 0, o[c], 0, len);
            for (int i = 0; i < xf; i++)
            {
                double t = (double)i / xf;
                o[c][i] = ch[c][i] * (float)Math.Sin(t * Math.PI / 2) + ch[c][len + i] * (float)Math.Cos(t * Math.PI / 2);
            }
        }
        return o;
    }

    // ---- RMS қабықшасы (Win сек терезе) ----
    public static float[] Envelope(float[] x, int rate, float win)
    {
        int W = Math.Max(1, (int)(rate * win)), n = x.Length / W;
        float[] e = new float[n];
        for (int k = 0; k < n; k++) { double s = 0; for (int i = k * W; i < (k + 1) * W; i++) s += x[i] * x[i]; e[k] = (float)Math.Sqrt(s / W); }
        return e;
    }

    // ---- басталулар: қабықша жергілікті фоннан Ratio есе жоғары, кемінде MinGap сек аралықпен ----
    public static List<int> Onsets(float[] x, int rate, float win, float ratio, float minGap, float absMin)
    {
        float[] e = Envelope(x, rate, win);
        var res = new List<int>();
        int W = (int)(rate * win), gap = (int)(minGap / win), last = -100000;
        int back = (int)(0.25f / win);
        for (int k = 1; k < e.Length; k++)
        {
            float floor = 1e9f;
            for (int j = Math.Max(0, k - back); j < k; j++) floor = Math.Min(floor, e[j]);
            if (e[k] > absMin && e[k] > floor * ratio && e[k] > e[k - 1] && k - last > gap) { res.Add(k * W); last = k; }
        }
        return res;
    }

    public static float Peak(float[] x, int a, int b) { float m = 0; for (int i = Math.Max(0, a); i < Math.Min(x.Length, b); i++) m = Math.Max(m, Math.Abs(x[i])); return m; }

    // ---- кесу: a..b, басы/соңы жұмсақ, шыңы Target-ке дейін ----
    public static float[] Cut(float[] x, int a, int b, int fadeIn, int fadeOut, float target)
    {
        a = Math.Max(0, a); b = Math.Min(x.Length, b);
        float[] o = new float[b - a];
        Array.Copy(x, a, o, 0, o.Length);
        for (int i = 0; i < Math.Min(fadeIn, o.Length); i++) o[i] *= (float)i / fadeIn;
        for (int i = 0; i < Math.Min(fadeOut, o.Length); i++) o[o.Length - 1 - i] *= (float)i / fadeOut;
        if (target > 0) { float p = Peak(o, 0, o.Length); if (p > 1e-4f) for (int i = 0; i < o.Length; i++) o[i] *= target / p; }
        return o;
    }

    // ---- тұйық цикл: x[a .. a+len+xf], соңғы xf басымен тең қуатты араласады ----
    public static float[] Loop(float[] x, int a, int len, int xf, float targetRms)
    {
        float[] o = new float[len];
        for (int i = 0; i < len; i++) o[i] = x[a + i];
        for (int i = 0; i < xf; i++)
        {
            double t = (double)i / xf;
            float gIn = (float)Math.Sin(t * Math.PI / 2), gOut = (float)Math.Cos(t * Math.PI / 2);
            o[i] = x[a + i] * gIn + x[a + len + i] * gOut;
        }
        if (targetRms > 0)
        {
            double s = 0; for (int i = 0; i < len; i++) s += o[i] * o[i];
            float r = (float)Math.Sqrt(s / len);
            if (r > 1e-5f) { float g = targetRms / r; float p = Peak(o, 0, len) * g; if (p > 0.95f) g *= 0.95f / p; for (int i = 0; i < len; i++) o[i] *= g; }
        }
        return o;
    }

    // ---- сызықтық қайта дискреттеу ----
    public static float[] Resample(float[] x, int from, int to)
    {
        int n = (int)((long)x.Length * to / from);
        float[] o = new float[n];
        for (int i = 0; i < n; i++)
        {
            double p = (double)i * from / to; int a = (int)p; double f = p - a;
            float v0 = x[Math.Min(a, x.Length - 1)], v1 = x[Math.Min(a + 1, x.Length - 1)];
            o[i] = (float)(v0 + (v1 - v0) * f);
        }
        return o;
    }

    // ---- Гейгер шертпелері: аралары бос (Iso сек) жеке шертпелерді табу ----
    public static List<int> IsolatedClicks(float[] x, int rate, float t0, float t1, float iso)
    {
        var on = Onsets(x, rate, 0.001f, 4.0f, 0.003f, 0.03f);
        var res = new List<int>();
        int isoN = (int)(iso * rate);
        for (int k = 0; k < on.Count; k++)
        {
            float t = (float)on[k] / rate;
            if (t < t0 || t > t1) continue;
            bool ok = (k == 0 || on[k] - on[k - 1] > isoN) && (k == on.Count - 1 || on[k + 1] - on[k] > isoN);
            if (ok) res.Add(on[k]);
        }
        return res;
    }

    // ---- шертпелер тығыздығы (сек сайын), Гейгер үшін ----
    public static float[] ClickRate(float[] x, int rate, float hop)
    {
        var on = Onsets(x, rate, 0.002f, 3.0f, 0.004f, 0.02f);
        int H = (int)(rate * hop), n = x.Length / H;
        float[] r = new float[n];
        foreach (int o in on) { int k = o / H; if (k < n) r[k] += 1f / hop; }
        return r;
    }
}
