using System.Collections.Concurrent;
using System.Diagnostics;
using System.Globalization;
using System.Text;
using Xunit;

namespace ForgeEngine.Tests;

/// <summary>Drives the real ForgeScriptHost process over the same line protocol the native engine uses.</summary>
public sealed class HostProtocolTests
{
    private sealed class Obj
    {
        public int Id;
        public string Name = "Object";
        public float X, Y, Z;
        public bool HasCollider;
        public int ParentId = -1;
        public bool Active = true;
        public string? Script;

        public string Line()
        {
            string F(float v) => v.ToString("R", CultureInfo.InvariantCulture);
            var fields = new List<string>
            {
                "OBJ", Id.ToString(CultureInfo.InvariantCulture), B64(Name), F(X), F(Y), F(Z), "0", "0", "0", "1", "1", "1",
                "1", "1", "0", "1", "1", "1", "1", "1", "10", "18", "30",
                HasCollider ? "1" : "0", "0", "0",
                B64("Untagged"), "0", "0", "1", "1", "1", "0", "0", "1", "1", "0",
                ParentId.ToString(CultureInfo.InvariantCulture), Active ? "1" : "0", "60", "0.05", "1000"
            };
            return string.Join('|', fields);
        }
    }

    private sealed class Host : IDisposable
    {
        private readonly Process _process;
        private readonly BlockingCollection<string> _lines = new();

        public Host()
        {
            string baseDir = AppContext.BaseDirectory.TrimEnd(Path.DirectorySeparatorChar);
            string framework = Path.GetFileName(baseDir);
            string configuration = Path.GetFileName(Path.GetDirectoryName(baseDir)!);
            string scripting = Path.GetFullPath(Path.Combine(baseDir, "..", "..", "..", ".."));
            string hostDll = Path.Combine(scripting, "ForgeScriptHost", "bin", configuration, framework, "ForgeScriptHost.dll");
            string fixtureDll = Path.Combine(scripting, "ForgeEngine.Tests.Fixtures", "bin", configuration, framework, "ForgeEngine.Tests.Fixtures.dll");
            Assert.True(File.Exists(hostDll), hostDll);
            Assert.True(File.Exists(fixtureDll), fixtureDll);

            var info = new ProcessStartInfo(Environment.GetEnvironmentVariable("DOTNET_HOST_PATH") ?? "dotnet")
            {
                RedirectStandardInput = true,
                RedirectStandardOutput = true,
                UseShellExecute = false,
                WorkingDirectory = Path.GetTempPath(),
                StandardOutputEncoding = Encoding.UTF8
            };
            info.ArgumentList.Add(hostDll);
            info.ArgumentList.Add(fixtureDll);
            _process = Process.Start(info)!;
            _process.StandardInput.AutoFlush = true;
            new Thread(() =>
            {
                string? line;
                while ((line = _process.StandardOutput.ReadLine()) is not null) _lines.Add(line);
                _lines.CompleteAdding();
            }) { IsBackground = true }.Start();

            while (true)
            {
                string line = Next();
                if (line.StartsWith("HOST|READY", StringComparison.Ordinal)) break;
                Assert.False(line.StartsWith("HOST|ERROR", StringComparison.Ordinal), line);
            }
        }

        private string Next()
        {
            Assert.True(_lines.TryTake(out var line, TimeSpan.FromSeconds(20)), "Host did not answer in time.");
            return line!;
        }

        public List<string> Frame(IEnumerable<Obj> objects, string keys = "", float scroll = 0, int width = 1280, int height = 720,
            float delta = 0.03f, IEnumerable<string>? extraLines = null)
        {
            var text = new StringBuilder();
            text.Append(FormattableString.Invariant($"FRAME|{delta}|0|0|0|0|0|{keys}|{scroll}|{width}|{height}\n"));
            foreach (var line in extraLines ?? Array.Empty<string>()) text.Append(line).Append('\n');
            foreach (var o in objects)
            {
                text.Append(o.Line()).Append('\n');
                if (o.Script is not null) text.Append($"SCRIPT|{o.Id}|{o.Script}|1\n");
            }
            text.Append("END\n");
            _process.StandardInput.Write(text.ToString());
            var response = new List<string>();
            string reply;
            while ((reply = Next()) != "END") response.Add(reply);
            return response;
        }

        public void Dispose()
        {
            try { _process.StandardInput.WriteLine("STOP"); if (!_process.WaitForExit(3000)) _process.Kill(); }
            catch { }
            _process.Dispose();
        }
    }

    private static string B64(string value) => Convert.ToBase64String(Encoding.UTF8.GetBytes(value));
    private static List<string> Logs(IEnumerable<string> lines)
        => lines.Where(l => l.StartsWith("LOG|", StringComparison.Ordinal))
            .Select(l => Encoding.UTF8.GetString(Convert.FromBase64String(l.Split('|')[2]))).ToList();

    [Fact]
    public void CoroutinesRunAcrossFrames()
    {
        using var host = new Host();
        var scene = new[] { new Obj { Id = 0, Name = "Probe", Script = "CoroutineProbe" } };
        Assert.Contains("co:start", Logs(host.Frame(scene)));
        Assert.Contains("co:next-frame", Logs(host.Frame(scene)));
        Assert.DoesNotContain("co:space", Logs(host.Frame(scene)));
        Assert.Contains("co:space", Logs(host.Frame(scene, keys: "32")));
    }

    [Fact]
    public void InstantiateAndDestroyUseStableIds()
    {
        using var host = new Host();
        var scene = new List<Obj>
        {
            new() { Id = 0, Name = "Spawner", Script = "Spawner" },
            new() { Id = 1, Name = "Template" },
            new() { Id = 2, Name = "Victim1" },
            new() { Id = 3, Name = "Victim2" },
            new() { Id = 4, Name = "Survivor" }
        };
        var output = host.Frame(scene);
        var instantiates = output.Where(l => l.StartsWith("CMD|INST|", StringComparison.Ordinal)).Select(l => l.Split('|')).ToList();
        var destroys = output.Where(l => l.StartsWith("CMD|DEST|", StringComparison.Ordinal)).Select(l => int.Parse(l.Split('|')[2], CultureInfo.InvariantCulture)).ToList();

        Assert.Equal(2, instantiates.Count);
        Assert.All(instantiates, p => Assert.Equal("1", p[3]));
        int spawnedTemp = int.Parse(instantiates[0][2], CultureInfo.InvariantCulture);
        int discardedTemp = int.Parse(instantiates[1][2], CultureInfo.InvariantCulture);
        Assert.True(spawnedTemp < -1 && discardedTemp < -1);
        Assert.Equal(new[] { 2, 3, discardedTemp }, destroys);

        // Native side: clone got stable id 10, victims are gone, discarded clone was destroyed.
        scene.RemoveAll(o => o.Id is 2 or 3);
        scene.Add(new Obj { Id = 10, Name = "Template" });
        var next = host.Frame(scene, extraLines: new[] { $"NEWID|{spawnedTemp}|10" });
        Assert.Contains($"NAMESET|10|{B64("Spawned")}", next);
        Assert.Contains($"NAMESET|4|{B64("Survivor")}", next);
        Assert.DoesNotContain(next, l => l.StartsWith("CMD|", StringComparison.Ordinal));
    }

    [Fact]
    public void MouseScrollAndScreenSizeReachScripts()
    {
        using var host = new Host();
        var scene = new[] { new Obj { Id = 0, Name = "Input", Script = "InputProbe" } };
        host.Frame(scene);
        Assert.Contains("scroll:1:1:1024x600", Logs(host.Frame(scene, scroll: 1, width: 1024, height: 600)));
    }

    [Fact]
    public void DestroyedScriptIsNotRecreated()
    {
        using var host = new Host();
        var scene = new[] { new Obj { Id = 0, Name = "Holder", Script = "SelfRemover" } };
        var first = host.Frame(scene);
        Assert.Contains("self-removed", Logs(first));
        Assert.Contains("SCRIPTSET|0|SelfRemover|0", first);
        var second = host.Frame(scene);
        Assert.DoesNotContain(Logs(second), l => l.StartsWith("Attached SelfRemover", StringComparison.Ordinal));
        Assert.Contains("SCRIPTSET|0|SelfRemover|0", second);
    }

    [Fact]
    public void CollisionExitIsDispatched()
    {
        using var host = new Host();
        var a = new Obj { Id = 0, Name = "A", Y = 5, HasCollider = true, Script = "CollisionProbe" };
        var b = new Obj { Id = 1, Name = "B", X = 0.5f, Y = 5, HasCollider = true };
        var logs = Logs(host.Frame(new[] { a, b }));
        Assert.Contains("collision:enter:B", logs);
        b.X = 10;
        logs = Logs(host.Frame(new[] { a, b }));
        Assert.Contains("collision:exit:B", logs);
        Assert.DoesNotContain("collision:enter:B", logs);
    }

    [Fact]
    public void SetActiveAndParentAreSentBack()
    {
        using var host = new Host();
        var scene = new[]
        {
            new Obj { Id = 0, Name = "Hidden", Script = "Hider" },
            new Obj { Id = 1, Name = "Child", ParentId = 0 }
        };
        host.Frame(scene);
        var output = host.Frame(scene, keys: "72");
        string[] hidden = output.First(l => l.StartsWith("SET|0|", StringComparison.Ordinal)).Split('|');
        string[] child = output.First(l => l.StartsWith("SET|1|", StringComparison.Ordinal)).Split('|');
        Assert.Equal(36, hidden.Length);
        Assert.Equal("0", hidden[32]);
        Assert.Equal("0", child[31]);
        Assert.Equal("1", child[32]);
    }
}
