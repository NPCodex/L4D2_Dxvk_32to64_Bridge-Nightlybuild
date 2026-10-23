// SPDX-License-Identifier: MIT
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Web.Script.Serialization;

namespace BridgeQuickTool {
    public sealed class ProcessEvidence { public bool Game,Host,UnexpectedHost,Unreadable; }
    public sealed class StartupCheck {
        public int Schema=1;
        public DateTime StartedUtc,LastObservedUtc;
        public string HostName,Result;
        public string GameRoot;
        public bool Bridge,SeenGame,SeenHost,Unreadable,Gap,Complete,UnexpectedHost;
        public int FreshLogs;
        public Dictionary<string,string> Baseline=new Dictionary<string,string>();
        static readonly JavaScriptSerializer Json=new JavaScriptSerializer{MaxJsonLength=1024*1024};
        public static string Stamp(string path){var info=new FileInfo(path);return info.LastWriteTimeUtc.Ticks+":"+info.Length;}
        public static StartupCheck Begin(Core core){var s=new StartupCheck{GameRoot=core.GameRoot,StartedUtc=DateTime.UtcNow,LastObservedUtc=DateTime.UtcNow,Bridge=core.Status().Kind=="enabled"};s.HostName=s.Bridge&&core.ReadConfig().Get("client.testX86Server")=="True"?"L4D2Bridge32":"L4D2Bridge64";foreach(string p in core.LogCandidates())s.Baseline[p.Substring(core.GameRoot.Length+1)]=Stamp(p);return s;}
        internal static bool Matches(Core core,string name,string path,DateTime started,DateTime requested){if(!new[]{"left4dead2","left4dead2_skeeto","L4D2Bridge32","L4D2Bridge64"}.Contains(name))return false;string expected=core.GamePath(name.StartsWith("L4D2Bridge",StringComparison.OrdinalIgnoreCase)?"bin/.l4d2bridge/"+name+".exe":name+".exe");return Path.GetFullPath(path).Equals(expected,StringComparison.OrdinalIgnoreCase)&&started>=requested.AddSeconds(-2);}
        public static ProcessEvidence Probe(Core core,StartupCheck session){var result=new ProcessEvidence();foreach(string name in new[]{"left4dead2","left4dead2_skeeto","L4D2Bridge32","L4D2Bridge64"})foreach(var process in Process.GetProcessesByName(name))using(process){try{if(!Matches(core,name,process.MainModule.FileName,process.StartTime.ToUniversalTime(),session.StartedUtc))continue;if(name.StartsWith("left4dead2",StringComparison.OrdinalIgnoreCase))result.Game=true;else if(session.Bridge&&name==session.HostName)result.Host=true;else result.UnexpectedHost=true;}catch{result.Unreadable=true;}}return result;}
        public static int Fresh(Core core,StartupCheck session){int count=0;foreach(string path in core.LogCandidates()){if(path.StartsWith(core.ToolRoot+Path.DirectorySeparatorChar,StringComparison.OrdinalIgnoreCase))continue;string previous;string key=path.Substring(core.GameRoot.Length+1);if(File.GetLastWriteTimeUtc(path)>=session.StartedUtc&&(!session.Baseline.TryGetValue(key,out previous)||previous!=Stamp(path)))count++;}return count;}
        public void Observe(ProcessEvidence evidence,int freshLogs,DateTime now){
            if(Complete)return;
            if(now-LastObservedUtc>TimeSpan.FromSeconds(8))Gap=true;
            LastObservedUtc=now;SeenGame|=evidence.Game;SeenHost|=evidence.Host;UnexpectedHost|=evidence.UnexpectedHost;Unreadable|=evidence.Unreadable;FreshLogs=Math.Max(FreshLogs,freshLogs);
            bool due=now-StartedUtc>=TimeSpan.FromSeconds(90);
            if(UnexpectedHost){Complete=true;Result="CHECK REQUIRED: an unexpected Bridge Host was observed / 检测到不符合当前模式的 Host。";}
            else if(evidence.Game&&(!Bridge||SeenHost)&&FreshLogs>0&&now-StartedUtc>=TimeSpan.FromSeconds(10)){Complete=true;Result="STARTUP OBSERVED: matching game"+(Bridge?" and Host":"")+", fresh logs="+FreshLogs+". / 已观察到对应游戏进程"+(Bridge?"与 Host":"")+"及新日志。";}
            else if(due){Complete=true;Result=!SeenGame?"NOT OBSERVED: no matching new game process within 90 s. Steam dialogs or a manual cancellation may explain this; it is not a crash diagnosis. / 90 秒内未观察到新游戏进程，不据此判定崩溃。":"PARTIAL: game observed, but expected Host/log evidence is incomplete. / 已观察到游戏，Host 或日志证据不足。";}
            else Result="Observing startup (up to 90 s) / 正在观察启动：game="+SeenGame+", host="+SeenHost+", fresh logs="+FreshLogs;
            if(Complete){if(Gap)Result+="\r\nObservation had a gap; continuous monitoring was not available. / 观察有中断。";if(Unreadable)Result+="\r\nSome process paths were unreadable; those processes were not treated as matches. / 部分进程路径不可读，未计为匹配。";Result+="\r\nNo FPS, map, rendering or long-term stability validation was performed. / 未验证帧率、地图、渲染或长期稳定性。";}
        }
        static string PathFor(Core core){string path=Path.Combine(core.StateRoot,"startup-check.json");Core.Guard(path,core.GameRoot);return path;}
        public void Save(Core core){string path=PathFor(core);Directory.CreateDirectory(core.StateRoot);string stage=path+"."+Guid.NewGuid().ToString("N")+".tmp";try{File.WriteAllText(stage,Json.Serialize(this),new System.Text.UTF8Encoding(false));if(File.Exists(path))File.Replace(stage,path,null);else File.Move(stage,path);}finally{if(File.Exists(stage))File.Delete(stage);}}
        public static StartupCheck Load(Core core){string path=PathFor(core);if(!File.Exists(path))return null;if(new FileInfo(path).Length>1024*1024)throw new InvalidDataException("Startup receipt too large.");var s=Json.Deserialize<StartupCheck>(File.ReadAllText(path));if(s==null||s.Schema!=1||!String.Equals(s.GameRoot,core.GameRoot,StringComparison.OrdinalIgnoreCase)||s.Baseline==null||s.Baseline.Count>1000||s.StartedUtc>DateTime.UtcNow.AddMinutes(1)||s.HostName!="L4D2Bridge32"&&s.HostName!="L4D2Bridge64")throw new InvalidDataException("Invalid startup receipt or different game folder.");return s;}
    }
}
