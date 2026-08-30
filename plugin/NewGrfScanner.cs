using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Text;

namespace LauncherV2.Plugins.OpenTTD;

// Reads a NewGRF's identity from the file itself: container v1/v2, then
// Action 8 (grfid+name) with Action 14 overrides. GRFIDs are byte-swapped
// in v2 headers.

/// What a NewGRF says about itself. Null fields = not stated.
public sealed record NewGrfInfo(
    string  Path,
    string? GrfId,
    string? Name,
    string? Description,
    uint?   Version,
    uint?   MinVersion,
    string? Url,
    string? Error)
{
    public string FileName => System.IO.Path.GetFileName(Path);

    /// We got an identity out of it — a GRFID, which is what we match on.
    public bool IsReadable => Error == null && GrfId != null;

    /// Label for the UI: "Iron Horse 4.14.1 (build 8948)".
    public string Display =>
        Name == null   ? FileName
      : Version == null ? Name
                        : $"{Name} (build {Version})";
}

public static class NewGrfScanner
{
    private static readonly byte[] V2Magic = { 0x00, 0x00, (byte)'G', (byte)'R', (byte)'F',
                                               0x82, 0x0D, 0x0A, 0x1A, 0x0A };

    // Action 8 and 14 sit at the very front — Iron Horse's are inside the first
    // 3 KB of a 43 MB file. Reading the whole set to answer "which version is
    // this" would cost a second per scan for nothing.
    private const int HeadBytes   = 256 * 1024;
    private const int MaxRecords  = 256;

    /// Read one .grf. Never throws — unreadable files come back with Error set.
    public static NewGrfInfo Scan(string path)
    {
        try
        {
            byte[] b = ReadHead(path, HeadBytes);
            return ReadFromBytes(b, b.Length, path);
        }
        catch (Exception ex)
        {
            // A malformed set is a fact about the player's install, not a crash.
            return Fail(path, ex.Message);
        }
    }

    ///
    /// The identity in a buffer that already holds the start of a NewGRF.
    ///
    /// Split out from Scan so a set inside a .tar can be read without being
    /// unpacked first -- the bytes are the same, only where they came from
    /// differs. `label` is what the UI shows as the path.
    ///
    private static NewGrfInfo ReadFromBytes(byte[] b, int length, string label)
    {
        string path = label;
        try
        {
            if (length < 16)
                return Fail(path, "file too short to be a NewGRF");
            if (b.Length > length) Array.Resize(ref b, length);

            bool v2 = StartsWith(b, V2Magic);
            int  pos = v2 ? 15 : 0;          // v2: 10 magic + 4 offset + 1 compression

            string? grfId = null, name = null, desc = null, url = null;
            uint? version = null, minVersion = null;

            for (int rec = 0; rec < MaxRecords; rec++)
            {
                int size, headerLen;
                if (v2)
                {
                    if (pos + 5 > b.Length) break;
                    size = (int)ReadU32(b, pos);
                    headerLen = 5;
                }
                else
                {
                    if (pos + 3 > b.Length) break;
                    size = b[pos] | (b[pos + 1] << 8);
                    headerLen = 3;
                }
                if (size <= 0) break;                       // end of sprite section

                byte info = b[pos + headerLen - 1];
                int  dataAt = pos + headerLen;
                if (dataAt + size > b.Length) break;         // truncated by our read cap

                if (info == 0xFF && size >= 1)
                {
                    switch (b[dataAt])
                    {
                        case 0x08:
                            ParseAction8(b, dataAt, size, ref grfId, ref name, ref desc);
                            break;
                        case 0x14:
                            ParseAction14(b, dataAt, size, ref version, ref minVersion, ref url);
                            break;
                    }
                }

                pos = dataAt + size;

                // Everything we want is in the opening records; once we have the
                // identity and a version there is no reason to keep walking.
                if (grfId != null && version != null) break;
            }

            return grfId == null
                ? Fail(path, v2 ? "no Action 8 found in the first records"
                                : "container v1, no Action 8 found in the first records")
                : new NewGrfInfo(path, grfId, name, desc, version, minVersion, url, null);
        }
        catch (Exception ex)
        {
            // A malformed set is a fact about the player's install, not a crash.
            return Fail(path, ex.Message);
        }
    }

    /// Every .grf under a folder, sorted by filename. Missing folder = empty.
    public static IReadOnlyList<NewGrfInfo> ScanFolder(string folder)
    {
        var found = new List<NewGrfInfo>();
        if (string.IsNullOrEmpty(folder) || !Directory.Exists(folder)) return found;

        foreach (string f in Directory.EnumerateFiles(folder, "*.grf", SearchOption.AllDirectories))
            found.Add(Scan(f));

        // ⚠ Sets the player downloads through the game do NOT arrive as a .grf
        // in this folder. OpenTTD writes them to content_download/newgrf/ as a
        // .tar, and scans that path itself (fileio.cpp scans ".tar" for
        // NEWGRF_DIR; newgrf_config.cpp runs TarScanner). Looking only for
        // loose .grf files meant a set could be installed, visible to the game,
        // and still reported missing here -- so the launcher would offer to
        // fetch what the player already had.
        foreach (string t in Directory.EnumerateFiles(folder, "*.tar", SearchOption.AllDirectories))
            found.AddRange(ScanTar(t));

        found.Sort((a, b) => string.CompareOrdinal(a.FileName, b.FileName));
        return found;
    }

    ///
    /// Every NewGRF inside a .tar, read without unpacking it.
    ///
    /// tar is a flat sequence of 512-byte headers, each followed by the file
    /// contents padded to 512. Only the .grf members are read, and only their
    /// first HeadBytes, so a 40 MB archive costs the same as a loose file.
    ///
    public static IReadOnlyList<NewGrfInfo> ScanTar(string tarPath)
    {
        var found = new List<NewGrfInfo>();
        try
        {
            using var fs = File.OpenRead(tarPath);
            var header = new byte[512];

            while (fs.Read(header, 0, 512) == 512)
            {
                // Two zero blocks end the archive; one is enough to stop on.
                bool empty = true;
                for (int i = 0; i < 512 && empty; i++) empty = header[i] == 0;
                if (empty) break;

                string name = Encoding.ASCII.GetString(header, 0, 100).TrimEnd('\0', ' ');
                // Size is octal text, NUL/space padded.
                string sizeText = Encoding.ASCII.GetString(header, 124, 12).Trim('\0', ' ');
                long size = 0;
                foreach (char c in sizeText)
                {
                    if (c < '0' || c > '7') break;
                    size = size * 8 + (c - '0');
                }

                long dataStart = fs.Position;
                if (size > 0 && name.EndsWith(".grf", StringComparison.OrdinalIgnoreCase))
                {
                    int take = (int)Math.Min(size, HeadBytes);
                    var head = new byte[take];
                    int got = fs.Read(head, 0, take);
                    if (got > 0)
                    {
                        var info = ReadFromBytes(head, got, $"{tarPath}!{name}");
                        if (info.IsReadable) found.Add(info);
                    }
                }

                // Members are padded to a 512-byte boundary.
                long next = dataStart + ((size + 511) / 512) * 512;
                if (next <= dataStart && size > 0) break;   // malformed; do not spin
                fs.Seek(next, SeekOrigin.Begin);
            }
        }
        catch (Exception ex)
        {
            found.Add(new NewGrfInfo(tarPath, null, null, null, null, null, null,
                                     "Could not read archive: " + ex.Message));
        }
        return found;
    }

    /// Find an installed set by GRFID, case-insensitively. Null when absent.
    public static NewGrfInfo? Find(IEnumerable<NewGrfInfo> installed, string grfId)
    {
        foreach (var g in installed)
            if (g.GrfId != null && string.Equals(g.GrfId, grfId, StringComparison.OrdinalIgnoreCase))
                return g;
        return null;
    }

    // --- Action 8: version byte, GRFID, name, description ---

    private static void ParseAction8(byte[] b, int at, int size,
                                     ref string? grfId, ref string? name, ref string? desc)
    {
        if (grfId != null || size < 7) return;               // first one wins
        int p = at + 2;                                      // skip action byte + grf version
        grfId = Convert.ToHexString(b, p, 4).ToLowerInvariant();
        p += 4;
        name = ReadCString(b, ref p, at + size);
        desc = ReadCString(b, ref p, at + size);
    }

    // --- Action 14: a chunk tree; we want the top-level INFO values ---

    private static void ParseAction14(byte[] b, int at, int size,
                                      ref uint? version, ref uint? minVersion, ref string? url)
    {
        int end = at + size;
        int p   = at + 1;                                    // skip action byte

        // The tree root is a branch; we only care about the one named INFO.
        while (p < end)
        {
            byte type = b[p++];
            if (type == 0 || p + 4 > end) return;
            string id = Ascii(b, p, 4); p += 4;

            if (type == (byte)'C' && id == "INFO")
            {
                ParseInfo(b, ref p, end, ref version, ref minVersion, ref url);
                return;
            }
            if (!SkipChunk(b, ref p, end, type)) return;
        }
    }

    private static void ParseInfo(byte[] b, ref int p, int end,
                                  ref uint? version, ref uint? minVersion, ref string? url)
    {
        while (p < end)
        {
            byte type = b[p++];
            if (type == 0 || p + 4 > end) return;            // end of INFO
            string id = Ascii(b, p, 4); p += 4;

            switch (type)
            {
                case (byte)'B':
                {
                    if (p + 2 > end) return;
                    int len = b[p] | (b[p + 1] << 8); p += 2;
                    if (p + len > end) return;
                    uint? v = len is > 0 and <= 4 ? ReadLe(b, p, len) : null;
                    if (id == "VRSN" && version == null)    version    = v;
                    if (id == "MINV" && minVersion == null) minVersion = v;
                    p += len;
                    break;
                }
                case (byte)'T':
                {
                    if (p >= end) return;
                    byte lang = b[p++];
                    int  s = p;
                    while (p < end && b[p] != 0) p++;
                    string text = Encoding.UTF8.GetString(b, s, p - s);
                    if (p < end) p++;
                    // 0x7F is the set's own default language. Anything else is a
                    // translation, and taking the first one would leave us
                    // reporting an Esperanto name to a Danish user.
                    if (id == "URL_" && (url == null || lang == 0x7F)) url = text.Trim();
                    break;
                }
                case (byte)'C':
                    // PARA and friends describe the set's parameters, not the set.
                    if (!SkipBranch(b, ref p, end)) return;
                    break;
                default:
                    return;                                  // unknown type: length unknown, stop
            }
        }
    }

    private static bool SkipChunk(byte[] b, ref int p, int end, byte type)
    {
        switch (type)
        {
            case (byte)'B':
                if (p + 2 > end) return false;
                int len = b[p] | (b[p + 1] << 8); p += 2;
                if (p + len > end) return false;
                p += len; return true;
            case (byte)'T':
                if (p >= end) return false;
                p++;                                          // language
                while (p < end && b[p] != 0) p++;
                if (p < end) p++;
                return true;
            case (byte)'C':
                return SkipBranch(b, ref p, end);
            default:
                return false;
        }
    }

    private static bool SkipBranch(byte[] b, ref int p, int end)
    {
        int guard = 0;
        while (p < end && guard++ < 4096)
        {
            byte type = b[p++];
            if (type == 0) return true;
            if (p + 4 > end) return false;
            p += 4;                                           // chunk id
            if (!SkipChunk(b, ref p, end, type)) return false;
        }
        return false;
    }

    // --- helpers ---

    private static NewGrfInfo Fail(string path, string why)
        => new(path, null, null, null, null, null, null, why);

    private static string? ReadCString(byte[] b, ref int p, int end)
    {
        int s = p;
        while (p < end && b[p] != 0) p++;
        string v = Encoding.UTF8.GetString(b, s, p - s).Trim();
        if (p < end) p++;
        return v.Length == 0 ? null : v;
    }

    private static uint ReadLe(byte[] b, int at, int len)
    {
        uint v = 0;
        for (int i = len - 1; i >= 0; i--) v = (v << 8) | b[at + i];
        return v;
    }

    private static uint ReadU32(byte[] b, int at)
        => (uint)(b[at] | (b[at + 1] << 8) | (b[at + 2] << 16) | (b[at + 3] << 24));

    private static string Ascii(byte[] b, int at, int len)
        => Encoding.ASCII.GetString(b, at, len);

    private static bool StartsWith(byte[] b, byte[] prefix)
    {
        if (b.Length < prefix.Length) return false;
        for (int i = 0; i < prefix.Length; i++) if (b[i] != prefix[i]) return false;
        return true;
    }

    private static byte[] ReadHead(string path, int max)
    {
        using var fs = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.ReadWrite);
        int want = (int)Math.Min(max, fs.Length);
        byte[] buf = new byte[want];
        int got = 0;
        while (got < want)
        {
            int n = fs.Read(buf, got, want - got);
            if (n <= 0) break;
            got += n;
        }
        return got == want ? buf : buf[..got];
    }
}
