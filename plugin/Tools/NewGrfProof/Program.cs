// What the launcher tells a player about their NewGRF sets.
//
// ⚠⚠ THE BUG THIS EXISTS FOR
//
// A player downloaded three sets, and the launcher went on saying
//
//     SHARK: MISSING — this seed needs it
//     Open OpenTTD, go to Check Online Content, and install SHARK.
//
// three launches in a row, while all six .tar files sat in
// content_download\newgrf. The verdict was built from what the GAME reported
// loaded and phrased as though it were about the disk. OpenTTD loads a NewGRF
// only once it is ticked in its own window, so "not loaded" and "not
// downloaded" are different facts and need different instructions.
//
// ⚠ The sets themselves cannot be fetched here. BaNaNaS hands packages over
// OpenTTD's own TCP protocol — its HTTP API returns metadata with no download
// address — and the authors publish no .tar on GitHub. So the archive below is
// BUILT to the format the scanner parses, and said to be built rather than
// passed off as somebody's release. Point OPENTTD_NEWGRF_DIR at a real
// content_download\newgrf folder and the same checks run against real files.
using System.Text;
using LauncherV2.Plugins.OpenTTD;

int pass = 0, fail = 0;
void Check(string what, bool ok, string detail = "")
{
    Console.WriteLine($"  {(ok ? "ok  " : "FAIL")} {what}{(detail.Length > 0 ? "  " + detail : "")}");
    if (ok) pass++; else fail++;
}

// --- a NewGRF, built to the format ------------------------------------------

static byte[] Grf(string grfIdHex, string name)
{
    var body = new List<byte>();
    body.Add(0x08);                                   // Action 8
    body.Add(0x08);                                   // grf container version
    body.AddRange(Convert.FromHexString(grfIdHex));   // the id itself
    body.AddRange(Encoding.ASCII.GetBytes(name)); body.Add(0);
    body.AddRange(Encoding.ASCII.GetBytes("built by a proof")); body.Add(0);

    var f = new List<byte>();
    f.AddRange(new byte[] { 0x00, 0x00, (byte)'G', (byte)'R', (byte)'F',
                            0x82, 0x0D, 0x0A, 0x1A, 0x0A });   // v2 magic
    f.AddRange(BitConverter.GetBytes((uint)0));                // data offset
    f.Add(0);                                                  // compression
    f.AddRange(BitConverter.GetBytes((uint)body.Count));       // record size
    f.Add(0xFF);                                               // pseudo-sprite
    f.AddRange(body);
    f.AddRange(BitConverter.GetBytes((uint)0));                // end of section
    return f.ToArray();
}

// A tar is 512-byte headers, each followed by contents padded to 512.
static void AddToTar(List<byte> tar, string name, byte[] data)
{
    var h = new byte[512];
    Encoding.ASCII.GetBytes(name).CopyTo(h, 0);
    Encoding.ASCII.GetBytes("0000644\0").CopyTo(h, 100);
    Encoding.ASCII.GetBytes(Convert.ToString(data.Length, 8).PadLeft(11, '0') + "\0").CopyTo(h, 124);
    Encoding.ASCII.GetBytes("00000000000\0").CopyTo(h, 136);
    for (int i = 148; i < 156; i++) h[i] = (byte)' ';          // checksum field
    h[156] = (byte)'0';                                        // a regular file
    int sum = 0; foreach (byte b in h) sum += b;
    Encoding.ASCII.GetBytes(Convert.ToString(sum, 8).PadLeft(6, '0') + "\0 ").CopyTo(h, 148);
    tar.AddRange(h);
    tar.AddRange(data);
    int pad = (512 - data.Length % 512) % 512;
    tar.AddRange(new byte[pad]);
}

string dir = Path.Combine(Path.GetTempPath(), "newgrf_proof");
if (Directory.Exists(dir)) Directory.Delete(dir, true);
Directory.CreateDirectory(dir);

var wanted = new (string GrfId, string Name)[]
{
    ("4a44bbb1", "SHARK"),
    ("444a5901", "Vactrain Set"),
    ("485a0101", "Hover Vehicles"),
};

Console.WriteLine("a set inside a .tar, which is how the game's own downloader leaves them");
foreach (var (id, name) in wanted)
{
    var tar = new List<byte>();
    AddToTar(tar, name.Replace(' ', '_') + ".grf", Grf(id, name));
    tar.AddRange(new byte[1024]);                              // two zero blocks
    File.WriteAllBytes(Path.Combine(dir, $"{id}-{name.Replace(' ', '_')}.tar"), tar.ToArray());
}

var onDisk = NewGrfScanner.ScanFolder(dir);
Check("the scanner reads them without unpacking", onDisk.Count == wanted.Length,
      $"{onDisk.Count} found");
foreach (var (id, name) in wanted)
    Check($"  and identifies {name}", NewGrfScanner.Find(onDisk, id) != null, id);

// If a real install is at hand, the same scan runs over real releases.
string? realDir = Environment.GetEnvironmentVariable("OPENTTD_NEWGRF_DIR");
if (realDir is { Length: > 0 } && Directory.Exists(realDir))
{
    var real = NewGrfScanner.ScanFolder(realDir);
    Check("a real content_download folder scans too", real.Count > 0,
          $"{real.Count} sets in {realDir}");
}
else
{
    // ⚠ Said out loud rather than skipped in silence.
    Check("no real folder given (set OPENTTD_NEWGRF_DIR to also check one)", true);
}

Console.WriteLine("\nthe verdict the player is given");
var required = wanted.Select(w => new NewGrfRequirement(w.GrfId, w.Name, 0)).ToList();

// The exact situation from the report: every file present, the game reporting
// nothing loaded because none of them is ticked.
var notEnabled = NewGrfRequirements.Evaluate(
    Array.Empty<NewGrfInfo>(), required, onDisk);
Check("a downloaded set is NOT called missing",
      notEnabled.All(r => r.State == NewGrfState.NotEnabled),
      string.Join(", ", notEnabled.Select(r => r.State.ToString()).Distinct()));
Check("and the advice says to enable it, not to download it",
      notEnabled.All(r => r.Advice is { } a
                       && a.Contains("already have")
                       && !a.Contains("Check Online Content")));
// The offer only fetches what is genuinely absent; that filter is what stopped
// the launcher downloading the same three sets a fourth time.
Check("nothing in that state is offered for download",
      !notEnabled.Any(r => r.State == NewGrfState.Missing));

var absent = NewGrfRequirements.Evaluate(
    Array.Empty<NewGrfInfo>(), required, Array.Empty<NewGrfInfo>());
Check("a set that really is absent is still MISSING",
      absent.All(r => r.State == NewGrfState.Missing));
Check("and its advice sends the player to the content service",
      absent.All(r => r.Advice?.Contains("Check Online Content") == true));

var loaded = wanted.Select(w => new NewGrfInfo(
    Path: "", GrfId: w.GrfId, Name: null, Description: null,
    Version: 1, MinVersion: null, Url: null, Error: null));
var fine = NewGrfRequirements.Evaluate(loaded, required, onDisk);
Check("a set the game has loaded is satisfied", fine.All(r => r.Ok));
Check("so the player is shown no message at all",
      NewGrfRequirements.Explain(fine) == null);

try { Directory.Delete(dir, true); } catch { }
Console.WriteLine($"\n{pass} ok, {fail} fejl");
return fail == 0 ? 0 : 1;
