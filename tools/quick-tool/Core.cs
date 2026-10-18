// SPDX-License-Identifier: MIT
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Security.Cryptography;
using System.Text;
using System.Text.RegularExpressions;
using System.Web.Script.Serialization;

namespace BridgeQuickTool {
    public class StoredFile { public string Relative; public string Hash; public long Length; }
    public class ToolState {
        public int Schema = 1;
        public string SnapshotId;
        public string LastOperation;
        public List<StoredFile> Files = new List<StoredFile>();
        public Dictionary<string,string> Context = new Dictionary<string,string>();
    }
    public class Journal { public string Id; public string Action; public ToolState Snapshot; public List<StoredFile> Files; }
    public class LegacyFile { public string relative; public string hash; public string backup; }
    public class LegacyState { public string status; public string game; public List<LegacyFile> moved; }
    public class BridgeStatus { public string Kind; public string Text; public string LoaderPath; public bool OtherLoaderPresent; public bool CanEnable; public bool CanDisable; }
    public class ConfigDocument {
        public string Text;
        public string Hash;
        public Encoding TextEncoding;
        public bool Bom;
        static readonly Regex Entry = new Regex(@"^(?<head>[ \t]*(?<key>[\w.]+)[ \t]*=[ \t]*)(?<value>[^#;\r\n]*?)(?<tail>[ \t]*(?:[#;][^\r\n]*)?)(?=\r?$)", RegexOptions.Multiline);
        public static ConfigDocument Load(string path) {
            byte[] b = File.ReadAllBytes(path); Encoding enc; int skip = 0; bool bom = false;
            if (b.Length >= 3 && b[0] == 239 && b[1] == 187 && b[2] == 191) { enc = new UTF8Encoding(false,true); skip = 3; bom = true; }
            else if (b.Length >= 2 && b[0] == 255 && b[1] == 254) { enc = new UnicodeEncoding(false,false,true); skip = 2; bom = true; }
            else if (b.Length >= 2 && b[0] == 254 && b[1] == 255) { enc = new UnicodeEncoding(true,false,true); skip = 2; bom = true; }
            else {
                enc = new UTF8Encoding(false,true);
                try { enc.GetString(b); } catch (DecoderFallbackException) { enc = Encoding.GetEncoding(936,EncoderFallback.ExceptionFallback,DecoderFallback.ExceptionFallback); }
            }
            return new ConfigDocument { Text = enc.GetString(b,skip,b.Length-skip), Hash = Core.HashBytes(b), TextEncoding = enc, Bom = bom };
        }
        public string Get(string key) {
            var found = Entry.Matches(Text).Cast<Match>().Where(m => m.Groups["key"].Value.Equals(key,StringComparison.OrdinalIgnoreCase)).ToArray();
            if (found.Length > 1) throw new InvalidOperationException("配置存在重复键，请先检查：" + key);
            return found.Length == 0 ? null : found[0].Groups["value"].Value.Trim();
        }
        public byte[] Change(Dictionary<string,string> changes) {
            changes = new Dictionary<string,string>(changes,StringComparer.OrdinalIgnoreCase);
            foreach (var kv in changes) { Core.ValidateSetting(kv.Key,kv.Value); Get(kv.Key); }
            var remaining = new HashSet<string>(changes.Keys,StringComparer.OrdinalIgnoreCase);
            string result = Entry.Replace(Text, m => {
                string key = m.Groups["key"].Value;
                if (!changes.ContainsKey(key)) return m.Value;
                remaining.Remove(key);
                return m.Groups["head"].Value + changes[key] + m.Groups["tail"].Value;
            });
            string nl = Text.Contains("\r\n") ? "\r\n" : "\n";
            if (remaining.Count > 0) {
                if (result.Length > 0 && !result.EndsWith("\n") && !result.EndsWith("\r")) result += nl;
                result += "# Bridge Quick Tool" + nl;
                foreach (string key in remaining.OrderBy(x => x,StringComparer.Ordinal)) result += key + " = " + changes[key] + nl;
            }
            byte[] content = TextEncoding.GetBytes(result);
            if (!Bom) return content;
            byte[] prefix = TextEncoding is UTF8Encoding ? new byte[]{239,187,191} : ((UnicodeEncoding)TextEncoding).GetPreamble();
            // The decoder was constructed without a preamble. Preserve UTF-16 BOM explicitly.
            if (TextEncoding is UnicodeEncoding) prefix = TextEncoding.CodePage == 1201 ? new byte[]{254,255} : new byte[]{255,254};
            return prefix.Concat(content).ToArray();
        }
    }
    public sealed class Core {
        public const string Version = "1.1.0";
        public readonly string GameRoot, ToolRoot, StateRoot, ConfigPath;
        public Func<bool> RunningCheck;
        internal Action<string> Fault { get; set; }
        static readonly JavaScriptSerializer Json = new JavaScriptSerializer { MaxJsonLength = 8*1024*1024 };
        static readonly string[] Loaders = { "bin/d3d9.dll", "bin/dxvk_d3d9.dll" };
        const string Plugin = "bin/neko/plugins/L4D2BridgePlugin.dll";
        static readonly string[] ContextFiles = { "bin/.l4d2bridge/L4D2Bridge64.exe", "bin/.l4d2bridge/L4D2Bridge32.exe", "bin/.l4d2bridge/d3d9vk_x64.dll", "bin/.l4d2bridge/d3d9vk_x86.dll", "bin/left4neko.dll" };
        public static readonly string[] DiagnosticKeys = {
            "client.memoryMonitoring","server.memoryMonitoring","client.crashDiagnostics","server.crashDiagnostics",
            "client.exceptionDiagnosticsDetailed","server.exceptionDiagnosticsDetailed","client.apiWaitDiagnostics","server.apiWaitDiagnostics",
            "client.pageBlockDiagnostics","client.pageBlockDiagnosticsDetailed","client.dataDiagnostics","server.dataDiagnostics",
            "client.colorDiagnostics","server.colorDiagnostics","client.steamInputDiagnostics","server.steamInputDiagnostics",
            "server.steamWindowDiagnostics","client.steamWebDiagnostics","logApiCalls","logServerCommands","logAllCalls","logAllCommands"
        };
        public static readonly string[] PresenterKeys = { "server.presenterWindow","server.presenterInput","server.presenterOverlayKey","server.presenterHotkeyFallback","client.forceWindowed","client.hookMessagePump","client.overrideCustomWinHooks","client.DirectInput.forward.mousePolicy","client.DirectInput.forward.keyboardPolicy" };
        public Core(string gameRoot) {
            GameRoot = Path.GetFullPath(gameRoot).TrimEnd(Path.DirectorySeparatorChar);
            if (!File.Exists(Path.Combine(GameRoot,"left4dead2.exe")) || !Directory.Exists(Path.Combine(GameRoot,"bin"))) throw new InvalidOperationException("请选择含 left4dead2.exe 和 bin 的游戏目录。");
            ToolRoot = Path.Combine(GameRoot,"tools","BridgeQuickTool"); StateRoot = Path.Combine(ToolRoot,"state");
            ConfigPath = GamePath("bin/.l4d2bridge/bridge.conf"); RunningCheck = IsGameRunning;
            Guard(GameRoot,GameRoot); Guard(ToolRoot,GameRoot);
        }
        public static string HashBytes(byte[] bytes) { using (var sha = SHA256.Create()) return BitConverter.ToString(sha.ComputeHash(bytes)).Replace("-",""); }
        public static string HashFile(string path) { using (var s = new FileStream(path,FileMode.Open,FileAccess.Read,FileShare.Read)) using (var sha = SHA256.Create()) return BitConverter.ToString(sha.ComputeHash(s)).Replace("-",""); }
        public static bool IsGameRunning() {
            foreach (string name in new[]{"left4dead2","left4dead2_skeeto","L4D2Bridge32","L4D2Bridge64"}) {
                var ps = Process.GetProcessesByName(name); bool any = ps.Length > 0;
                foreach (var p in ps) p.Dispose(); if (any) return true;
            }
            return false;
        }
        public static void Guard(string path,string root) {
            string full = Path.GetFullPath(path), basePath = Path.GetFullPath(root).TrimEnd('\\','/');
            if (!full.Equals(basePath,StringComparison.OrdinalIgnoreCase) && !full.StartsWith(basePath+Path.DirectorySeparatorChar,StringComparison.OrdinalIgnoreCase)) throw new InvalidOperationException("路径超出允许目录。");
            string current = full;
            while (!String.IsNullOrEmpty(current)) {
                if ((File.Exists(current)||Directory.Exists(current)) && (File.GetAttributes(current)&FileAttributes.ReparsePoint)!=0) throw new InvalidOperationException("路径包含链接或重定向，请使用实际游戏目录："+current);
                current = Path.GetDirectoryName(current);
            }
        }
        public string GamePath(string relative) { string p = Path.GetFullPath(Path.Combine(GameRoot,relative.Replace('/',Path.DirectorySeparatorChar))); Guard(p,GameRoot); return p; }
        string StatePath(string relative) { string p = Path.GetFullPath(Path.Combine(StateRoot,relative.Replace('/',Path.DirectorySeparatorChar))); Guard(p,StateRoot); return p; }
        string StateFile { get { return StatePath("bridge-state.json"); } }
        string JournalFile { get { return StatePath("pending-operation.json"); } }
        public bool HasPending { get { return File.Exists(JournalFile); } }
        void Stopped() { if (RunningCheck()) throw new InvalidOperationException("请先正常退出游戏和 Bridge Host，再执行开关、保存或恢复设置。日志打包可以在游戏运行时使用。"); }
        void Ready() { Stopped(); if (HasPending) throw new InvalidOperationException("检测到未完成操作，请先点击“恢复未完成操作”。"); }
        static bool Allowed(string relative) { return Loaders.Contains(relative)||relative==Plugin; }
        void ValidateState(ToolState state) {
            if (state==null||state.Schema!=1||state.Files==null||state.Context==null||!Regex.IsMatch(state.SnapshotId??"",@"^[A-Za-z0-9_-]+$")) throw new InvalidOperationException("Bridge 备份记录无效。");
            if (state.Files.Count<1||state.Files.Count>2||state.Files.Select(x=>x.Relative).Distinct().Count()!=state.Files.Count||state.Files.Count(x=>Loaders.Contains(x.Relative))!=1) throw new InvalidOperationException("Bridge 备份文件列表无效。");
            foreach (var f in state.Files) if (!Allowed(f.Relative)||!Regex.IsMatch(f.Hash??"",@"^[A-Fa-f0-9]{64}$")) throw new InvalidOperationException("Bridge 备份路径或校验值无效。");
            if (state.Context.Count!=ContextFiles.Length||state.Context.Keys.Except(ContextFiles).Any()) throw new InvalidOperationException("Bridge 版本配对记录无效。");
        }
        ToolState ReadState() { if (!File.Exists(StateFile)) return null; var s = Json.Deserialize<ToolState>(File.ReadAllText(StateFile,Encoding.UTF8)); ValidateState(s); return s; }
        string BackupPath(ToolState s,StoredFile f) { return StatePath("snapshots/"+s.SnapshotId+"/"+f.Relative.Replace('/','_')); }
        Dictionary<string,string> Context() {
            var result = new Dictionary<string,string>();
            foreach (string rel in ContextFiles) { string p = GamePath(rel); result[rel] = File.Exists(p)?HashFile(p):null; }
            if (result[ContextFiles[0]]==null||result[ContextFiles[2]]==null) throw new InvalidOperationException("缺少 x64 Host 或后端，请先完整安装 Bridge。");
            return result;
        }
        void ValidateBackups(ToolState s) {
            ValidateState(s);
            foreach (var f in s.Files) if (!File.Exists(BackupPath(s,f))||HashFile(BackupPath(s,f))!=f.Hash) throw new InvalidOperationException("Bridge 备份损坏或被修改："+f.Relative);
            if (!IsBridgeDll(BackupPath(s,s.Files.First(x=>Loaders.Contains(x.Relative))))) throw new InvalidOperationException("备份不是可识别的 Bridge 客户端。");
        }
        void WriteJson(string path,object data) { AtomicBytes(path,new UTF8Encoding(false).GetBytes(Json.Serialize(data)),null); }
        static void AtomicBytes(string path,byte[] data,string backup) {
            Directory.CreateDirectory(Path.GetDirectoryName(path)); string temp = path+"."+Guid.NewGuid().ToString("N")+".tmp";
            try {
                using (var s = new FileStream(temp,FileMode.CreateNew,FileAccess.Write,FileShare.None)) { s.Write(data,0,data.Length); s.Flush(true); }
                if (File.Exists(path)) File.Replace(temp,path,backup); else File.Move(temp,path);
            } finally { if (File.Exists(temp)) File.Delete(temp); }
        }
        public BridgeStatus Status() {
            if (HasPending) return new BridgeStatus { Kind="pending",Text="有未完成的操作，需要恢复",CanEnable=false,CanDisable=false };
            if (File.Exists(GamePath("d3d9.dll"))) return new BridgeStatus {Kind="conflict",Text="根目录存在 d3d9.dll，请先核对加载方式"};
            ToolState state = ReadState();
            var active = Loaders.Where(x=>File.Exists(GamePath(x))).ToArray();
            // A historical -vulkan loader can remain beside the default mode.
            // Once imported/managed, the recorded entry determines this tool's
            // mode; another mode's DLL must not be enabled, removed or restored.
            if(state==null&&active.Length>1)return new BridgeStatus {Kind="conflict",Text="两种入口同时存在且没有管理记录，请先核对启动方式"};
            string managed=state!=null?state.Files.First(x=>Loaders.Contains(x.Relative)).Relative:active.FirstOrDefault();
            string mode=managed=="bin/dxvk_d3d9.dll"?"-vulkan 兼容入口":"默认 D3D9 入口";
            bool other=managed!=null&&Loaders.Any(x=>x!=managed&&File.Exists(GamePath(x)));
            if (managed!=null&&File.Exists(GamePath(managed))) {
                if (!IsBridgeDll(GamePath(managed))) return new BridgeStatus {Kind="conflict",Text="入口 DLL 不是已识别的 Bridge，工具不会移动它"};
                return new BridgeStatus {Kind="enabled",Text="Bridge 已启用（"+mode+"）",LoaderPath=managed,OtherLoaderPresent=other,CanDisable=true};
            }
            if (state==null) return new BridgeStatus {Kind="unavailable",Text="Bridge 入口未安装，且没有可恢复的停用备份"};
            ValidateBackups(state);
            return new BridgeStatus {Kind="disabled",Text="Bridge 已停用（"+mode+"）",LoaderPath=managed,OtherLoaderPresent=other,CanEnable=true};
        }
        public void ImportDisabled(string legacyPath) {
            Ready(); if (File.Exists(StateFile)) throw new InvalidOperationException("已有管理记录，不会覆盖。");
            LegacyState old = Json.Deserialize<LegacyState>(File.ReadAllText(legacyPath,Encoding.UTF8));
            if (old==null||old.status!="disabled-and-verified"||!Path.GetFullPath(old.game).TrimEnd('\\').Equals(GameRoot,StringComparison.OrdinalIgnoreCase)) throw new InvalidOperationException("旧备份不属于这个游戏目录。");
            var s = new ToolState { SnapshotId="import-"+Guid.NewGuid().ToString("N"),LastOperation="import",Context=Context() };
            foreach (var f in old.moved) {
                string rel = f.relative.Replace('\\','/');
                if (!Allowed(rel)||File.Exists(GamePath(rel))||HashFile(f.backup)!=f.hash.ToUpperInvariant()) throw new InvalidOperationException("旧备份校验失败或入口已经存在。");
                s.Files.Add(new StoredFile {Relative=rel,Hash=f.hash.ToUpperInvariant(),Length=new FileInfo(f.backup).Length});
            }
            ValidateState(s);
            for (int i=0;i<s.Files.Count;i++) { string target=BackupPath(s,s.Files[i]); Directory.CreateDirectory(Path.GetDirectoryName(target)); File.Copy(old.moved[i].backup,target,false); }
            ValidateBackups(s); WriteJson(StateFile,s); Audit("已接管旧停用备份；Bridge 保持关闭");
        }
        public void Disable() {
            Ready(); var status=Status(); if (!status.CanDisable) throw new InvalidOperationException(status.Text);
            string id="off-"+DateTime.UtcNow.ToString("yyyyMMddHHmmss")+"-"+Guid.NewGuid().ToString("N");
            var s=new ToolState {SnapshotId=id,LastOperation=id,Context=Context()};
            foreach (string rel in new[]{status.LoaderPath,Plugin}) if (File.Exists(GamePath(rel))) s.Files.Add(new StoredFile {Relative=rel,Hash=HashFile(GamePath(rel)),Length=new FileInfo(GamePath(rel)).Length});
            ValidateState(s);
            foreach (var f in s.Files) { string p=BackupPath(s,f); Directory.CreateDirectory(Path.GetDirectoryName(p)); File.Copy(GamePath(f.Relative),p,false); }
            ValidateBackups(s); var journal=new Journal {Id=id,Action="disable",Snapshot=s,Files=s.Files}; WriteJson(JournalFile,journal);
            try {
                Stopped();
                foreach (var f in s.Files) { string p=GamePath(f.Relative); if (HashFile(p)!=f.Hash) throw new InvalidOperationException("入口文件发生变化，已中止。"); File.Delete(p); if (Fault!=null) Fault("disable-file"); }
                WriteJson(StateFile,s); File.Delete(JournalFile); Audit("已停用 Bridge；配置保持不变");
            } catch { RecoverPending(); throw; }
        }
        public void Enable() {
            Ready(); var status=Status(); if (!status.CanEnable) throw new InvalidOperationException(status.Text);
            ToolState s=ReadState(); ValidateBackups(s);
            var now=Context(); foreach (var kv in s.Context) if (now[kv.Key]!=kv.Value) throw new InvalidOperationException("停用后 Host、后端或 L4N 发生了更新。请完整安装同一版本 Bridge，避免混用旧入口："+kv.Key);
            foreach (var f in s.Files) if (File.Exists(GamePath(f.Relative))) throw new InvalidOperationException("目标已有文件，工具不会覆盖："+f.Relative);
            string id="on-"+Guid.NewGuid().ToString("N"); WriteJson(JournalFile,new Journal {Id=id,Action="enable",Snapshot=s,Files=s.Files});
            try {
                Stopped();
                foreach (var f in s.Files) { File.Copy(BackupPath(s,f),GamePath(f.Relative),false); if (HashFile(GamePath(f.Relative))!=f.Hash) throw new IOException("恢复文件校验失败。"); if (Fault!=null) Fault("enable-file"); }
                s.LastOperation=id; WriteJson(StateFile,s); File.Delete(JournalFile); Audit("已启用 Bridge；下次正常启动游戏生效");
            } catch { RecoverPending(); throw; }
        }
        public void RecoverPending() {
            Stopped(); if (!HasPending) return;
            Journal j=Json.Deserialize<Journal>(File.ReadAllText(JournalFile,Encoding.UTF8));
            if (j==null||j.Snapshot==null||j.Files==null||!new[]{"enable","disable"}.Contains(j.Action)) throw new InvalidOperationException("操作记录损坏，请保留备份后检查。");
            ValidateBackups(j.Snapshot);
            if (j.Files.Count!=j.Snapshot.Files.Count||j.Files.Any(f=>!j.Snapshot.Files.Any(x=>x.Relative==f.Relative&&x.Hash==f.Hash))) throw new InvalidOperationException("恢复操作列表无效。");
            ToolState committed=ReadState();
            if (committed==null||committed.LastOperation!=j.Id) {
                foreach (var f in j.Files) {
                    string p=GamePath(f.Relative);
                    if (File.Exists(p)&&HashFile(p)!=f.Hash) throw new InvalidOperationException("恢复目标出现新文件，不会覆盖："+f.Relative);
                    if (j.Action=="disable"&&!File.Exists(p)) File.Copy(BackupPath(j.Snapshot,f),p,false);
                    if (j.Action=="enable"&&File.Exists(p)) File.Delete(p);
                }
            }
            File.Delete(JournalFile); Audit("已恢复未完成操作");
        }
        public static void ValidateSetting(string key,string value) {
            if (DiagnosticKeys.Contains(key)||PresenterKeys.Contains(key)&&key!="server.presenterOverlayKey"&&!key.StartsWith("client.DirectInput.forward.")||new[]{"forceX64Server","client.testX86Server","client.vbibMappedShadow"}.Contains(key)) {
                if (value!="True"&&value!="False") throw new InvalidOperationException("无效布尔设置："+key); return;
            }
            if (key=="client.ipcBatchSize"&&new[]{"1","2","4","8","16","32"}.Contains(value)) return;
            if (key=="client.pageBlockRetentionPolicy"&&new[]{"keep","learned-aggressive","drop"}.Contains(value)) return;
            if (key=="logLevel"&&new[]{"Info","Debug","Warn","Error"}.Contains(value)) return;
            if (key=="server.presenterOverlayKey"&&value=="36") return;
            if (key.StartsWith("client.DirectInput.forward.")&&PresenterKeys.Contains(key)&&value=="0") return;
            throw new InvalidOperationException("不支持的设置或数值："+key);
        }
        public static Dictionary<string,string> Presenter(bool enable) {
            var d=new Dictionary<string,string>();
            if (enable) { foreach (string k in new[]{"server.presenterWindow","server.presenterInput","client.forceWindowed","client.hookMessagePump","client.overrideCustomWinHooks"}) d[k]="True"; d["server.presenterOverlayKey"]="36"; d["server.presenterHotkeyFallback"]="False"; d["client.DirectInput.forward.mousePolicy"]="0"; d["client.DirectInput.forward.keyboardPolicy"]="0"; }
            else foreach (string k in new[]{"server.presenterWindow","client.hookMessagePump","client.overrideCustomWinHooks"}) d[k]="False";
            return d;
        }
        public ConfigDocument ReadConfig() { return ConfigDocument.Load(ConfigPath); }
        public string SaveConfig(ConfigDocument loaded,Dictionary<string,string> changes) {
            Ready(); if (changes.Count==0) return null;
            if (changes.ContainsKey("client.testX86Server")&&changes["client.testX86Server"]=="True"&&(!File.Exists(GamePath(ContextFiles[1]))||!File.Exists(GamePath(ContextFiles[3])))) throw new InvalidOperationException("缺少配套 x86 Host 或后端，未修改配置。");
            if (HashFile(ConfigPath)!=loaded.Hash) throw new InvalidOperationException("配置已被其他程序修改。请刷新后重新选择，未覆盖外部修改。");
            byte[] bytes=loaded.Change(changes); if (HashBytes(bytes)==loaded.Hash) return null;
            string backup=StatePath("config-backups/bridge-"+DateTime.UtcNow.ToString("yyyyMMdd-HHmmss")+"-"+Guid.NewGuid().ToString("N").Substring(0,8)+".conf");
            Directory.CreateDirectory(Path.GetDirectoryName(backup));
            if (HashFile(ConfigPath)!=loaded.Hash) throw new InvalidOperationException("保存前配置又发生了变化，请刷新。");
            AtomicBytes(ConfigPath,bytes,backup); if (HashFile(backup)!=loaded.Hash) throw new IOException("配置备份校验失败，请保留文件检查。");
            Audit("已保存设置："+String.Join(", ",changes.Keys.OrderBy(x=>x))); return backup;
        }
        public string[] ConfigBackups() { string p=StatePath("config-backups"); return Directory.Exists(p)?Directory.GetFiles(p,"bridge-*.conf").OrderByDescending(x=>x,StringComparer.Ordinal).ToArray():new string[0]; }
        public void RestoreConfig(string backup,string expectedCurrent) {
            Ready(); Guard(backup,StatePath("config-backups"));
            if (!ConfigBackups().Contains(backup)) throw new InvalidOperationException("请选择工具自动保存的配置备份。");
            if (HashFile(ConfigPath)!=expectedCurrent) throw new InvalidOperationException("当前配置已变化，请先刷新。");
            var old=ConfigDocument.Load(backup); string saved=StatePath("config-backups/bridge-"+DateTime.UtcNow.ToString("yyyyMMdd-HHmmss")+"-"+Guid.NewGuid().ToString("N").Substring(0,8)+".conf");
            AtomicBytes(ConfigPath,File.ReadAllBytes(backup),saved); Audit("已恢复配置备份："+Path.GetFileName(backup));
        }
        public void Audit(string text) {
            try { Directory.CreateDirectory(StateRoot); File.AppendAllText(StatePath("tool.log"),DateTimeOffset.Now.ToString("o")+" "+text+Environment.NewLine,new UTF8Encoding(false)); } catch { /* audit failure cannot alter a completed operation */ }
        }
        public static bool IsBridgeDll(string path) {
            try {
                byte[] b=File.ReadAllBytes(path); if (b.Length<512||b.Length>64*1024*1024||BitConverter.ToUInt16(b,0)!=0x5a4d) return false;
                int pe=BitConverter.ToInt32(b,0x3c); if (pe<64||pe>b.Length-256||BitConverter.ToUInt32(b,pe)!=0x4550||BitConverter.ToUInt16(b,pe+4)!=0x14c) return false;
                int count=BitConverter.ToUInt16(b,pe+6),size=BitConverter.ToUInt16(b,pe+20),opt=pe+24,sections=opt+size;
                if (count<1||count>96||sections+count*40>b.Length||BitConverter.ToUInt16(b,opt)!=0x10b) return false;
                Func<uint,int> offset=rva=> { for(int i=0;i<count;i++){int s=sections+i*40; uint va=BitConverter.ToUInt32(b,s+12),raw=BitConverter.ToUInt32(b,s+16),ptr=BitConverter.ToUInt32(b,s+20); if(rva>=va&&(ulong)rva-va<raw){long at=(long)rva-va+ptr; if(at>=0&&at<b.Length)return (int)at;}} throw new InvalidDataException(); };
                int exp=offset(BitConverter.ToUInt32(b,opt+96)); if (exp>b.Length-40) return false;
                uint n=BitConverter.ToUInt32(b,exp+24); if (n>4096) return false;
                int names=offset(BitConverter.ToUInt32(b,exp+32)); bool create=false,control=false;
                for(uint i=0;i<n;i++){int at=offset(BitConverter.ToUInt32(b,checked(names+(int)i*4))); int end=Array.IndexOf(b,(byte)0,at,Math.Min(160,b.Length-at)); if(end<0)return false; string name=Encoding.ASCII.GetString(b,at,end-at); create|=name=="Direct3DCreate9";control|=name=="L4D2BridgeControl";}
                return create&&control;
            } catch { return false; }
        }
        static string Decode(byte[] data) {
            if(data.Length>=2&&data[0]==255&&data[1]==254)return Encoding.Unicode.GetString(data,2,data.Length-2);
            if(data.Length>=2&&data[0]==254&&data[1]==255)return Encoding.BigEndianUnicode.GetString(data,2,data.Length-2);
            try{return new UTF8Encoding(false,true).GetString(data).TrimStart('\ufeff');}catch(DecoderFallbackException){return Encoding.GetEncoding(936).GetString(data);}
        }
        internal static string DecodeLog(byte[] data,byte[] prefix,bool tail) {
            if(!tail)return Decode(data);
            // A tail can start in the middle of UTF-8/GBK characters, or lose
            // the UTF-16 BOM. Preserve the source encoding and skip that line.
            if(prefix.Length>=2&&((prefix[0]==255&&prefix[1]==254)||(prefix[0]==254&&prefix[1]==255))){
                Encoding enc=prefix[0]==255?Encoding.Unicode:Encoding.BigEndianUnicode;
                string value=enc.GetString(data);int line=value.IndexOf('\n');return line>=0&&line<4096?value.Substring(line+1):value;
            }
            int cut=Array.IndexOf(data,(byte)10,0,Math.Min(data.Length,4096));
            if(cut>=0)data=data.Skip(cut+1).ToArray();
            if(prefix.Length>=3&&prefix[0]==239&&prefix[1]==187&&prefix[2]==191)return Encoding.UTF8.GetString(data);
            return Decode(data);
        }
        public string Redact(string text) {
            text=Regex.Replace(text,Regex.Escape(GameRoot),"%GAME%",RegexOptions.IgnoreCase);
            string user=Environment.GetFolderPath(Environment.SpecialFolder.UserProfile);
            if(!String.IsNullOrEmpty(user))text=Regex.Replace(text,Regex.Escape(user),"%USERPROFILE%",RegexOptions.IgnoreCase);
            text=Regex.Replace(text,"(?im)(\\b(?:rcon_password|password|passwd|[\\w-]*token|api[_-]?key|authorization|secret)[\"']?\\s*[=:]\\s*)[^\\r\\n]*","$1<redacted>");
            return text;
        }
        public IEnumerable<string> LogCandidates() {
            string[] roots={"","bin","bin/.l4d2bridge","left4dead2","bin/neko/plugins","skeeto_update"};
            var result=new HashSet<string>(StringComparer.OrdinalIgnoreCase);
            foreach(string rel in roots){string folder=GamePath(rel);if(!Directory.Exists(folder))continue;foreach(string file in Directory.GetFiles(folder,"*.log",SearchOption.TopDirectoryOnly))result.Add(file);}
            foreach(string rel in new[]{"bin/.l4d2bridge/logs","left4dead2/logs","skeeto_update/logs"}){
                string folder=GamePath(rel);if(!Directory.Exists(folder))continue;
                foreach(string f in Directory.GetFiles(folder,"*.log",SearchOption.TopDirectoryOnly))result.Add(f);
            }
            string own=StatePath("tool.log");if(File.Exists(own))result.Add(own);
            return result.Where(p=>{try{Guard(p,GameRoot);return true;}catch{return false;}}).OrderByDescending(p=>File.GetLastWriteTimeUtc(p));
        }
        static void ZipText(ZipArchive zip,string path,string text) {
            var entry=zip.CreateEntry(path,CompressionLevel.Optimal);using(var s=new StreamWriter(entry.Open(),new UTF8Encoding(false)))s.Write(text);
        }
        public string PackLogs(int days) {
            if(days!=1&&days!=3&&days!=7&&days!=30)throw new ArgumentException("无效日志时间范围。");
            string reports=Path.Combine(ToolRoot,"reports");Guard(reports,GameRoot);Directory.CreateDirectory(reports);
            string target=Path.Combine(reports,"L4D2-Bridge-logs-"+DateTime.Now.ToString("yyyyMMdd-HHmmss")+"-"+Guid.NewGuid().ToString("N").Substring(0,4)+".zip"),temp=target+".tmp";
            var items=new List<object>();var skipped=new List<object>();long total=0;int count=0;
            try{
                using(var file=new FileStream(temp,FileMode.CreateNew,FileAccess.Write,FileShare.None))using(var zip=new ZipArchive(file,ZipArchiveMode.Create)){
                    foreach(string p in LogCandidates()){
                        string rel=p.Substring(GameRoot.Length+1).Replace('\\','/');
                        if(File.GetLastWriteTimeUtc(p)<DateTime.UtcNow.AddDays(-days)){skipped.Add(new{path=rel,reason="outside time range"});continue;}
                        if(count>=120||total>=64L*1024*1024){skipped.Add(new{path=rel,reason="bundle limit"});continue;}
                        try{
                            byte[] data;byte[] prefix=new byte[4];long length;int got=0;
                            using(var s=new FileStream(p,FileMode.Open,FileAccess.Read,FileShare.ReadWrite|FileShare.Delete)){
                                length=s.Length;s.Read(prefix,0,prefix.Length);
                                int take=(int)Math.Min(Math.Min(length,4L*1024*1024),64L*1024*1024-total);long start=Math.Max(0,length-take);
                                if(start>0&&((prefix[0]==255&&prefix[1]==254)||(prefix[0]==254&&prefix[1]==255))&&(start&1)!=0){start++;take--;}
                                data=new byte[take];s.Seek(start,SeekOrigin.Begin);
                                while(got<take){int n=s.Read(data,got,take-got);if(n==0)break;got+=n;}
                            }
                            if(got!=data.Length)Array.Resize(ref data,got);
                            bool truncated=length>data.Length;string content=Redact(DecodeLog(data,prefix,truncated));
                            ZipText(zip,"logs/"+rel,(truncated?"[Only the last 4 MiB or remaining bundle budget was captured.]\n":"")+content);
                            items.Add(new{path=rel,originalBytes=length,capturedBytes=data.Length,truncated=truncated,lastWriteUtc=File.GetLastWriteTimeUtc(p).ToString("o")});total+=data.Length;count++;
                        }catch(Exception e){skipped.Add(new{path=rel,reason=e.GetType().Name});}
                    }
                    foreach(string rel in new[]{"bin/.l4d2bridge/bridge.conf","dxvk.conf"}){
                        string p=GamePath(rel);if(!File.Exists(p))continue;
                        if(new FileInfo(p).Length>1024*1024){skipped.Add(new{path=rel,reason="configuration too large"});continue;}
                        ZipText(zip,"config/"+rel,Redact(Decode(File.ReadAllBytes(p))));
                    }
                    var binaries=new List<object>();ToolState saved=null;BridgeStatus bridge;
                    try{saved=ReadState();bridge=Status();}catch(Exception e){bridge=new BridgeStatus{Kind="error",Text=Redact(e.Message)};}
                    foreach(string rel in ContextFiles.Concat(Loaders).Concat(new[]{Plugin,"bin/neko/plugins/client_tab_adapter.dll","skeeto.dll","bin/wininet.dll"})){
                        string p=GamePath(rel);bool active=File.Exists(p);if(!active&&saved!=null){var f=saved.Files.FirstOrDefault(x=>x.Relative==rel);if(f!=null)p=BackupPath(saved,f);}
                        if(File.Exists(p))binaries.Add(new{path=rel,active=active,bytes=new FileInfo(p).Length,sha256=HashFile(p),version=FileVersionInfo.GetVersionInfo(p).FileVersion});
                    }
                    var manifest=new{toolVersion=Version,createdUtc=DateTime.UtcNow.ToString("o"),rangeDays=days,bridge=bridge,gameRunning=RunningCheck(),os=Environment.OSVersion.ToString(),is64BitOS=Environment.Is64BitOperatingSystem,logs=items,skipped=skipped,binaries=binaries,limits=new{perFileBytes=4*1024*1024,totalLogBytes=64*1024*1024,maxFiles=120},note="Local snapshot of existing files. Live logs may change while copied; no diagnostics were enabled. No dumps, databases, caches, executable payloads or Steam userdata are included. Common credentials and this user's paths are redacted; review before sharing."};
                    ZipText(zip,"manifest.json",Json.Serialize(manifest));
                    ZipText(zip,"README.txt","L4D2 Bridge diagnostic bundle\nExisting logs and Bridge/DXVK configuration only. No automatic upload.\nLarge logs contain their tail; see manifest.json for omissions and capture times.\nRedaction is best-effort: player names, server information and other identifying text may remain. Review before sharing.\n");
                }
                File.Move(temp,target);Audit("已打包日志："+Path.GetFileName(target));return target;
            }finally{if(File.Exists(temp))File.Delete(temp);}
        }
    }
}
