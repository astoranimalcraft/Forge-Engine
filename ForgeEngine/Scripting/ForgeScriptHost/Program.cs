using System.Globalization;
using System.Reflection;
using System.Runtime.Loader;
using System.Text;
using ForgeEngine;

internal static class Program
{
    private sealed class AttachedScript
    {
        public int ObjectId;
        public string ClassName = "";
        public bool Enabled;
    }
    private sealed class ScriptInstance
    {
        public int ObjectId;
        public string ClassName = "";
        public MonoBehaviour Behaviour = null!;
        public bool LastEnabled;
        public bool Awoken;
        public bool Started;
    }
    private sealed class ScriptLoadContext : AssemblyLoadContext
    {
        private readonly AssemblyDependencyResolver _resolver;
        public ScriptLoadContext(string mainAssemblyPath) : base("ForgeGameScripts", isCollectible: false)
            => _resolver = new AssemblyDependencyResolver(mainAssemblyPath);
        protected override Assembly? Load(AssemblyName assemblyName)
        {
            if (assemblyName.Name == typeof(MonoBehaviour).Assembly.GetName().Name)
                return typeof(MonoBehaviour).Assembly;
            string? path = _resolver.ResolveAssemblyToPath(assemblyName);
            return path is null ? null : LoadFromAssemblyPath(path);
        }
    }

    private static readonly Dictionary<string, Type> ScriptTypes = new(StringComparer.Ordinal);
    private static readonly Dictionary<string, ScriptInstance> Instances = new(StringComparer.Ordinal);
    private static readonly HashSet<string> MissingClassesReported = new(StringComparer.Ordinal);
    private static readonly object OutputLock = new();
    private static float FixedAccumulator;

    private static void Main(string[] args)
    {
        Console.InputEncoding = Encoding.UTF8;
        Console.OutputEncoding = Encoding.UTF8;
        Console.CancelKeyPress += (_, e) => { e.Cancel = true; };
        Debug.Sink = (level, message) => Emit("LOG", level, Convert.ToBase64String(Encoding.UTF8.GetBytes(message)));

        try
        {
            if (args.Length != 1) throw new InvalidOperationException("Usage: ForgeScriptHost <ForgeGameScripts.dll>");
            string scriptAssemblyPath = Path.GetFullPath(args[0]);
            if (!File.Exists(scriptAssemblyPath)) throw new FileNotFoundException("Compiled game scripts DLL was not found.", scriptAssemblyPath);
            var loadContext = new ScriptLoadContext(scriptAssemblyPath);
            Assembly assembly = loadContext.LoadFromAssemblyPath(scriptAssemblyPath);
            foreach (Type type in assembly.GetTypes())
            {
                if (!type.IsAbstract && !type.IsInterface && typeof(MonoBehaviour).IsAssignableFrom(type) && type.GetConstructor(Type.EmptyTypes) is not null)
                {
                    ScriptTypes[type.FullName ?? type.Name] = type;
                    if (!ScriptTypes.ContainsKey(type.Name)) ScriptTypes[type.Name] = type;
                }
            }
            Emit("HOST", "READY", ScriptTypes.Count.ToString(CultureInfo.InvariantCulture));
        }
        catch (Exception ex)
        {
            Emit("LOG", "ERROR", Convert.ToBase64String(Encoding.UTF8.GetBytes("Script host startup failed: " + ex)));
            Emit("HOST", "ERROR", Convert.ToBase64String(Encoding.UTF8.GetBytes(ex.GetBaseException().Message)));
            return;
        }

        string? line;
        while ((line = Console.ReadLine()) is not null)
        {
            if (line.Equals("STOP", StringComparison.Ordinal)) break;
            if (!line.StartsWith("FRAME|", StringComparison.Ordinal)) continue;
            try { ProcessFrame(line); }
            catch (Exception ex) { Emit("LOG", "ERROR", Convert.ToBase64String(Encoding.UTF8.GetBytes("Script frame failed: " + ex))); Emit("END"); }
            if (Application.quitRequested)
            {
                Emit("LOG", "INFO", Convert.ToBase64String(Encoding.UTF8.GetBytes("Application.Quit() requested; closing the script host.")));
                break;
            }
        }
        Application.isPlaying = false;
        foreach (var entry in Instances.Values.ToArray()) Destroy(entry);
    }

    private static void ProcessFrame(string frameLine)
    {
        string[] frame = frameLine.Split('|');
        float delta = ParseFloat(frame, 1, 0.016f);
        float mouseX = ParseFloat(frame, 2, 0);
        float mouseY = ParseFloat(frame, 3, 0);
        float mousePositionX = ParseFloat(frame, 4, 0);
        float mousePositionY = ParseFloat(frame, 5, 0);
        int mouseButtons = ParseInt(frame, 6, 0);
        var keys = new HashSet<int>();
        if (frame.Length > 7 && !string.IsNullOrWhiteSpace(frame[7]))
            foreach (string key in frame[7].Split(',', StringSplitOptions.RemoveEmptyEntries))
                if (int.TryParse(key, NumberStyles.Integer, CultureInfo.InvariantCulture, out int value)) keys.Add(value);

        var incomingObjects = new List<RuntimeObjectData>();
        var attachments = new List<AttachedScript>();
        string? line;
        while ((line = Console.ReadLine()) is not null && line != "END")
        {
            string[] p = line.Split('|');
            if (p.Length == 0) continue;
            if (p[0] == "NEWID" && p.Length >= 3)
                Scene.RemapId(ParseInt(p, 1, int.MinValue), ParseInt(p, 2, -1));
            else if (p[0] == "OBJ" && p.Length >= 26)
            {
                var data = new RuntimeObjectData
                {
                    Id = ParseInt(p, 1, -1),
                    Name = DecodeBase64(p[2]),
                    Position = new Vector3(ParseFloat(p,3,0), ParseFloat(p,4,0), ParseFloat(p,5,0)),
                    Rotation = new Vector3(ParseFloat(p,6,0), ParseFloat(p,7,0), ParseFloat(p,8,0)),
                    Scale = new Vector3(ParseFloat(p,9,1), ParseFloat(p,10,1), ParseFloat(p,11,1)),
                    HasMeshRenderer = ParseInt(p,12,0) != 0,
                    MeshRendererEnabled = ParseInt(p,13,1) != 0,
                    HasCollider = ParseInt(p,23,0) != 0,
                    HasAudioSource = ParseInt(p,24,0) != 0,
                    HasCamera = ParseInt(p,25,0) != 0,
                    LightType = ParseInt(p,14,0),
                    LightEnabled = ParseInt(p,15,1) != 0,
                    LightColor = new Vector3(ParseFloat(p,16,1), ParseFloat(p,17,1), ParseFloat(p,18,1)),
                    LightIntensity = ParseFloat(p,19,1), LightRange = ParseFloat(p,20,10),
                    LightInnerAngle = ParseFloat(p,21,18), LightOuterAngle = ParseFloat(p,22,30)
                };
                if (p.Length >= 37)
                {
                    string tag = DecodeBase64(p[26]);
                    data.Tag = string.IsNullOrWhiteSpace(tag) ? "Untagged" : tag;
                    data.Layer = Mathf.Clamp(ParseInt(p, 27, 0), 0, 31);
                    data.ColliderType = Mathf.Clamp(ParseInt(p, 28, 0), 0, 2);
                    data.ColliderSize = new Vector3(ParseFloat(p,29,1), ParseFloat(p,30,1), ParseFloat(p,31,1));
                    data.IsTrigger = ParseInt(p,32,0) != 0;
                    data.HasRigidbody = ParseInt(p,33,0) != 0;
                    data.Mass = MathF.Max(0.01f, ParseFloat(p,34,1));
                    data.UseGravity = ParseInt(p,35,1) != 0;
                    data.IsKinematic = ParseInt(p,36,0) != 0;
                }
                incomingObjects.Add(data);
            }
            else if (p[0] == "SCRIPT" && p.Length >= 4)
                attachments.Add(new AttachedScript { ObjectId = ParseInt(p,1,-1), ClassName = p[2], Enabled = ParseInt(p,3,1) != 0 });
        }

        Scene.SyncObjects(incomingObjects);
        Application.isPlaying = true;
        Time.BeginFrame(delta);
        Input.BeginFrame(keys, mouseButtons, mouseX, mouseY, mousePositionX, mousePositionY);
        SynchronizeScripts(attachments);

        foreach (var instance in Instances.Values.ToArray())
        {
            if (Scene.FindById(instance.ObjectId) is null) { Destroy(instance); continue; }
            bool shouldRun = instance.Behaviour.enabled && instance.Behaviour.gameObject.activeSelf;
            if (shouldRun && !instance.LastEnabled)
            {
                if (!instance.Awoken)
                {
                    SafeCall(instance, "Awake", instance.Behaviour.Awake);
                    instance.Awoken = true;
                }
                SafeCall(instance, "OnEnable", instance.Behaviour.OnEnable);
                if (!instance.Started) { SafeCall(instance, "Start", instance.Behaviour.Start); instance.Started = true; }
            }
            else if (!shouldRun && instance.LastEnabled) SafeCall(instance, "OnDisable", instance.Behaviour.OnDisable);
            instance.LastEnabled = shouldRun;
        }

        const float fixedStep = 0.02f;
        FixedAccumulator = MathF.Min(FixedAccumulator + delta, fixedStep * 5.0f);
        int fixedSteps = 0;
        while (FixedAccumulator >= fixedStep && fixedSteps < 5)
        {
            Time.SetDeltaTimeFromHost(fixedStep);
            foreach (var instance in Instances.Values.ToArray())
                if (instance.LastEnabled && instance.Behaviour.enabled)
                    SafeCall(instance, "FixedUpdate", instance.Behaviour.FixedUpdate);
            Physics.SimulateRuntimeStep(fixedStep);
            FixedAccumulator -= fixedStep;
            fixedSteps++;
        }
        Time.SetDeltaTimeFromHost(delta);
        foreach (var instance in Instances.Values.ToArray())
        {
            if (!instance.LastEnabled || !instance.Behaviour.enabled) continue;
            SafeCall(instance, "Update", instance.Behaviour.Update);
            SafeCall(instance, "LateUpdate", instance.Behaviour.LateUpdate);
        }

        foreach (var instance in Instances.Values)
            Emit("SCRIPTSET", instance.ObjectId.ToString(CultureInfo.InvariantCulture), instance.ClassName,
                instance.Behaviour.enabled ? "1" : "0");

        foreach (var go in Scene.gameObjects)
        {
            var s = go.State;
            Emit("NAMESET", s.Id.ToString(CultureInfo.InvariantCulture),
                Convert.ToBase64String(Encoding.UTF8.GetBytes(s.Name)));
            Emit("SET", s.Id.ToString(CultureInfo.InvariantCulture),
                F(s.Position.x), F(s.Position.y), F(s.Position.z),
                F(s.Rotation.x), F(s.Rotation.y), F(s.Rotation.z),
                F(s.Scale.x), F(s.Scale.y), F(s.Scale.z),
                s.MeshRendererEnabled ? "1" : "0", s.LightEnabled ? "1" : "0",
                F(s.LightColor.x), F(s.LightColor.y), F(s.LightColor.z), F(s.LightIntensity),
                F(s.LightRange), F(s.LightInnerAngle), F(s.LightOuterAngle),
                Convert.ToBase64String(Encoding.UTF8.GetBytes(s.Tag)),
                s.Layer.ToString(CultureInfo.InvariantCulture),
                s.ColliderType.ToString(CultureInfo.InvariantCulture),
                F(s.ColliderSize.x), F(s.ColliderSize.y), F(s.ColliderSize.z),
                s.IsTrigger ? "1" : "0", s.HasRigidbody ? "1" : "0", F(s.Mass),
                s.UseGravity ? "1" : "0", s.IsKinematic ? "1" : "0");
        }

        foreach (var command in ForgeRuntimeCommands.Drain())
        {
            if (command.kind == 0)
                Emit("CMD", "INST", command.tempId.ToString(CultureInfo.InvariantCulture),
                    command.sourceId.ToString(CultureInfo.InvariantCulture),
                    Convert.ToBase64String(Encoding.UTF8.GetBytes(command.name)),
                    F(command.position.x), F(command.position.y), F(command.position.z),
                    F(command.rotationDegrees.x), F(command.rotationDegrees.y), F(command.rotationDegrees.z),
                    F(command.scale.x), F(command.scale.y), F(command.scale.z));
            else if (command.targetId >= 0)
                Emit("CMD", "DEST", command.targetId.ToString(CultureInfo.InvariantCulture));
        }
        Emit("END");
    }

    private static void SynchronizeScripts(List<AttachedScript> attachments)
    {
        var desired = attachments.Where(a => a.ObjectId >= 0 && !string.IsNullOrWhiteSpace(a.ClassName))
            .GroupBy(a => Key(a.ObjectId, a.ClassName)).ToDictionary(g => g.Key, g => g.Last());
        foreach (string key in Instances.Keys.Where(k => !desired.ContainsKey(k)).ToArray()) Destroy(Instances[key]);

        foreach (var attach in desired.Values)
        {
            string key = Key(attach.ObjectId, attach.ClassName);
            if (!Instances.TryGetValue(key, out var instance))
            {
                Type? type = ResolveScriptType(attach.ClassName);
                if (type is null)
                {
                    if (MissingClassesReported.Add(attach.ClassName)) Debug.LogError($"Script class '{attach.ClassName}' was not found in ForgeGameScripts.dll. Check the class name and recompile.");
                    continue;
                }
                if (Scene.FindById(attach.ObjectId) is not { } go) continue;
                try
                {
                    var behaviour = (MonoBehaviour)Activator.CreateInstance(type)!;
                    go.AttachComponent(behaviour);
                    behaviour.enabled = attach.Enabled;
                    instance = new ScriptInstance { ObjectId=attach.ObjectId, ClassName=attach.ClassName, Behaviour=behaviour, LastEnabled=false };
                    Instances[key] = instance;
                    Debug.Log($"Attached {attach.ClassName} to '{go.name}' (ID {attach.ObjectId}).");
                }
                catch (Exception ex) { Debug.LogError($"Could not create script '{attach.ClassName}': {ex.GetBaseException().Message}"); continue; }
            }
            instance.Behaviour.enabled = attach.Enabled;
        }
    }

    private static Type? ResolveScriptType(string name)
    {
        if (ScriptTypes.TryGetValue(name, out var type)) return type;
        return ScriptTypes.Values.FirstOrDefault(t => string.Equals(t.Name, name, StringComparison.Ordinal));
    }
    private static void Destroy(ScriptInstance instance)
    {
        string key = Key(instance.ObjectId, instance.ClassName);
        if (!Instances.Remove(key)) return;
        if (instance.LastEnabled) SafeCall(instance, "OnDisable", instance.Behaviour.OnDisable);
        SafeCall(instance, "OnDestroy", instance.Behaviour.OnDestroy);
        instance.Behaviour.gameObject.DetachComponent(instance.Behaviour);
    }
    private static void SafeCall(ScriptInstance instance, string method, Action callback)
    {
        try { callback(); }
        catch (Exception ex) { Debug.LogError($"{instance.ClassName}.{method}() on '{instance.Behaviour.gameObject.name}': {ex.GetBaseException().Message}"); }
    }
    private static string Key(int id, string name) => id.ToString(CultureInfo.InvariantCulture) + "\u001f" + name;
    private static int ParseInt(string[] fields, int index, int fallback) => fields.Length > index && int.TryParse(fields[index], NumberStyles.Integer, CultureInfo.InvariantCulture, out int v) ? v : fallback;
    private static float ParseFloat(string[] fields, int index, float fallback) => fields.Length > index && float.TryParse(fields[index], NumberStyles.Float, CultureInfo.InvariantCulture, out float v) && float.IsFinite(v) ? v : fallback;
    private static string F(float value) => (float.IsFinite(value) ? value : 0f).ToString("R", CultureInfo.InvariantCulture);
    private static string DecodeBase64(string value) { try { return Encoding.UTF8.GetString(Convert.FromBase64String(value)); } catch { return "GameObject"; } }
    private static void Emit(params string[] parts) { lock (OutputLock) { Console.WriteLine(string.Join('|', parts)); Console.Out.Flush(); } }
}
