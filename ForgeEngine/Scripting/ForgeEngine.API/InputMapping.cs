using System.Text.Json;

namespace ForgeEngine;

/// <summary>
/// Loads the project input mapping from Config/Inputs.json so designers can rebind keys,
/// axes and commands without touching code. Scripts keep using Input.GetAxis / Input.GetButton.
/// </summary>
public static class InputMap
{
    public enum AxisSource { Keys, MouseDeltaX, MouseDeltaY, MouseScroll, MousePositionX, MousePositionY }

    internal sealed class AxisBinding
    {
        public string Name = "";
        public int[] NegativeKeys = Array.Empty<int>();
        public int[] PositiveKeys = Array.Empty<int>();
        public AxisSource Source = AxisSource.Keys;
        public float Smoothing;
    }

    internal sealed class ButtonBinding
    {
        public string Name = "";
        public int[] Keys = Array.Empty<int>();
        public int[] MouseButtons = Array.Empty<int>();
    }

    private static readonly Dictionary<string, AxisBinding> Axes = new(StringComparer.OrdinalIgnoreCase);
    private static readonly Dictionary<string, ButtonBinding> Buttons = new(StringComparer.OrdinalIgnoreCase);
    private static bool _loaded;
    private static string _path = string.Empty;

    public static bool isLoaded => _loaded;
    public static string filePath => _path;
    public static IEnumerable<string> axisNames => Axes.Keys;
    public static IEnumerable<string> buttonNames => Buttons.Keys;

    /// <summary>Searches Config/Inputs.json from the current directory and upward (host runs in Project/Scripts).</summary>
    public static bool Load(bool force = false)
    {
        if (_loaded && !force) return true;
        Axes.Clear();
        Buttons.Clear();
        _path = FindFile() ?? string.Empty;
        if (_path.Length == 0) { _loaded = false; return false; }
        try
        {
            using var document = JsonDocument.Parse(File.ReadAllText(_path));
            if (document.RootElement.TryGetProperty("axes", out var axes) && axes.ValueKind == JsonValueKind.Array)
                foreach (var element in axes.EnumerateArray()) ReadAxis(element);
            if (document.RootElement.TryGetProperty("buttons", out var buttons) && buttons.ValueKind == JsonValueKind.Array)
                foreach (var element in buttons.EnumerateArray()) ReadButton(element);
            _loaded = Axes.Count > 0 || Buttons.Count > 0;
        }
        catch (Exception ex)
        {
            Debug.LogError($"InputMap could not parse '{_path}': {ex.Message}");
            _loaded = false;
        }
        return _loaded;
    }

    private static string? FindFile()
    {
        var candidates = new List<string>();
        try
        {
            var directory = new DirectoryInfo(AppContext.BaseDirectory);
            while (directory is not null) { candidates.Add(Path.Combine(directory.FullName, "Config", "Inputs.json")); directory = directory.Parent; }
            candidates.Add(Path.Combine(Environment.CurrentDirectory, "Config", "Inputs.json"));
            candidates.Add(Path.Combine(Environment.CurrentDirectory, "..", "Config", "Inputs.json"));
            foreach (var candidate in candidates)
                if (File.Exists(candidate)) return candidate;
        }
        catch { }
        return null;
    }

    private static void ReadAxis(JsonElement element)
    {
        if (!element.TryGetProperty("name", out var nameElement)) return;
        var binding = new AxisBinding { Name = nameElement.GetString() ?? "" };
        if (string.IsNullOrWhiteSpace(binding.Name)) return;
        if (element.TryGetProperty("source", out var sourceElement))
            binding.Source = sourceElement.GetString() switch
            {
                "mouseDeltaX" => AxisSource.MouseDeltaX,
                "mouseDeltaY" => AxisSource.MouseDeltaY,
                "mouseScroll" => AxisSource.MouseScroll,
                "mousePositionX" => AxisSource.MousePositionX,
                "mousePositionY" => AxisSource.MousePositionY,
                _ => AxisSource.Keys
            };
        binding.NegativeKeys = ReadKeys(element, "negative");
        binding.PositiveKeys = ReadKeys(element, "positive");
        if (element.TryGetProperty("smoothing", out var smoothingElement) &&
            smoothingElement.ValueKind == JsonValueKind.Number)
            binding.Smoothing = smoothingElement.GetSingle();
        Axes[binding.Name] = binding;
    }

    private static void ReadButton(JsonElement element)
    {
        if (!element.TryGetProperty("name", out var nameElement)) return;
        var binding = new ButtonBinding { Name = nameElement.GetString() ?? "" };
        if (string.IsNullOrWhiteSpace(binding.Name)) return;
        var keys = new List<int>();
        var mouse = new List<int>();
        if (element.TryGetProperty("keys", out var keyArray) && keyArray.ValueKind == JsonValueKind.Array)
        {
            foreach (var raw in keyArray.EnumerateArray())
            {
                string keyName = raw.GetString() ?? "";
                if (TryMouse(keyName, out int mouseIndex)) { mouse.Add(mouseIndex); continue; }
                if (KeyNameToVirtualKey(keyName) is int vk) keys.Add(vk);
                else Debug.LogWarning($"InputMap: unknown key '{keyName}' for button '{binding.Name}'.");
            }
        }
        binding.Keys = keys.ToArray();
        binding.MouseButtons = mouse.ToArray();
        Buttons[binding.Name] = binding;
    }

    private static int[] ReadKeys(JsonElement element, string property)
    {
        var result = new List<int>();
        if (!element.TryGetProperty(property, out var array) || array.ValueKind != JsonValueKind.Array) return result.ToArray();
        foreach (var raw in array.EnumerateArray())
        {
            string keyName = raw.GetString() ?? "";
            if (TryMouse(keyName, out _)) continue;
            if (KeyNameToVirtualKey(keyName) is int vk) result.Add(vk);
            else Debug.LogWarning($"InputMap: unknown key '{keyName}' on '{property}' of axis.");
        }
        return result.ToArray();
    }

    private static bool TryMouse(string keyName, out int button)
    {
        switch (keyName.Trim().ToLowerInvariant())
        {
            case "mouseleft": button = 0; return true;
            case "mouseright": button = 1; return true;
            case "mousemiddle": button = 2; return true;
            default: button = -1; return false;
        }
    }

    /// <summary>Maps friendly key names (Space, Enter, LeftShift, F5, W, UpArrow, 0..9) to Win32 virtual-key codes.</summary>
    public static int? KeyNameToVirtualKey(string keyName)
    {
        if (string.IsNullOrWhiteSpace(keyName)) return null;
        string key = keyName.Trim();
        if (key.Length == 1)
        {
            char c = char.ToUpperInvariant(key[0]);
            if (c is >= 'A' and <= 'Z') return c;
            if (c is >= '0' and <= '9') return c;
        }
        switch (key.ToLowerInvariant())
        {
            case "space": return 32;
            case "enter": case "return": return 13;
            case "escape": case "esc": return 27;
            case "tab": return 9;
            case "backspace": case "back": return 8;
            case "delete": case "del": return 46;
            case "insert": return 45;
            case "home": return 36;
            case "end": return 35;
            case "pageup": case "prior": return 33;
            case "pagedown": case "next": return 34;
            case "leftarrow": case "left": return 37;
            case "uparrow": case "up": return 38;
            case "rightarrow": case "right": return 39;
            case "downarrow": case "down": return 40;
            case "shift": return 16;
            case "leftshift": return 160;
            case "rightshift": return 161;
            case "ctrl": case "control": return 17;
            case "leftctrl": case "leftcontrol": return 162;
            case "rightctrl": case "rightcontrol": return 163;
            case "alt": case "leftalt": return 164;
            case "rightalt": return 165;
            case "capslock": return 20;
            case "comma": return 188;
            case "period": return 190;
            case "slash": return 191;
            case "semicolon": return 186;
            case "quote": return 222;
            case "minus": return 189;
            case "equals": return 187;
            case "leftbracket": return 219;
            case "rightbracket": return 221;
            case "backslash": return 220;
            case "backquote": return 192;
        }
        if (key.ToLowerInvariant().StartsWith("f") && key.Length is >= 2 and <= 3 &&
            int.TryParse(key[1..], out int fn) && fn is >= 1 and <= 12)
            return 111 + fn;
        return null;
    }

    internal static bool TryGetAxisBinding(string name, out AxisBinding binding)
    {
        binding = null!;
        if (Axes.TryGetValue(name, out var found)) { binding = found; return true; }
        return false;
    }

    internal static bool TryGetButtonBinding(string name, out ButtonBinding binding)
    {
        binding = null!;
        if (Buttons.TryGetValue(name, out var found)) { binding = found; return true; }
        return false;
    }
}
