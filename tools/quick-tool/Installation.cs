// SPDX-License-Identifier: MIT
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;

namespace BridgeQuickTool {
    public sealed class CheckItem { public string Level,Text; }
    public sealed class InstallReport {
        public string CreatedUtc=DateTime.UtcNow.ToString("o");
        public List<CheckItem> Items=new List<CheckItem>();
        public bool HasErrors {get{return Items.Any(i=>i.Level=="ERROR");}}
        public void Add(string level,string text){Items.Add(new CheckItem{Level=level,Text=text});}
        public override string ToString(){return "Installation check / 安装自检  "+CreatedUtc+Environment.NewLine+String.Join(Environment.NewLine,Items.Select(i=>"["+i.Level+"] "+i.Text))+Environment.NewLine+"This verifies installation/startup evidence, not FPS or gameplay stability. / 不代表帧率或地图稳定性验收。";}
    }
    public static class Installation {
        public static ushort Machine(string path){Core.Guard(path,Path.GetDirectoryName(Path.GetFullPath(path)));using(var file=new FileStream(path,FileMode.Open,FileAccess.Read,FileShare.ReadWrite|FileShare.Delete))using(var read=new BinaryReader(file)){if(file.Length<64||read.ReadUInt16()!=0x5a4d)throw new InvalidDataException("Not a PE file: "+Path.GetFileName(path));file.Position=60;uint offset=read.ReadUInt32();if(offset>file.Length-24)throw new InvalidDataException("Invalid PE header.");file.Position=offset;if(read.ReadUInt32()!=0x4550)throw new InvalidDataException("Invalid PE signature.");return read.ReadUInt16();}}
        static bool Pe(Core core,InstallReport report,string relative,ushort expected,bool required){string p=core.GamePath(relative);if(!File.Exists(p)){report.Add(required?"ERROR":"WARN","Missing / 未安装："+relative);return false;}try{if(Machine(p)!=expected){report.Add("ERROR","Wrong PE architecture / 位数不符："+relative);return false;}report.Add("OK",relative+" ("+(expected==0x14c?"x86":"x64")+")");return true;}catch(Exception e){report.Add("ERROR",relative+": "+e.Message);return false;}}
        public static string BuildId(string path){var info=new FileInfo(path);if(info.Length>64L*1024*1024)throw new InvalidDataException("Bridge binary exceeds inspection limit.");var ids=Regex.Matches(Encoding.ASCII.GetString(File.ReadAllBytes(path)),@"l4d2-\d+\.\d+(?:\.\d+)?\+[a-fA-F0-9]{16}\b").Cast<Match>().Select(m=>m.Value).Distinct().ToArray();return ids.Length==1?ids[0]:null;}
        public static bool Flag(string options,string flag){return SteamSupport.Args(options).Any(a=>a.Equals(flag,StringComparison.OrdinalIgnoreCase));}
        public static InstallReport Check(Core core,string[] registeredGames,string options,string steamError){
            var report=new InstallReport();
            try{
                Core.Guard(core.GameRoot,core.GameRoot);Pe(core,report,"left4dead2.exe",0x14c,true);
                if(registeredGames==null)report.Add("WARN","Steam install location could not be read / 无法核对 Steam 安装目录。");
                else if(registeredGames.Length==1&&!String.Equals(Path.GetFullPath(registeredGames[0]).TrimEnd('\\'),core.GameRoot.TrimEnd('\\'),StringComparison.OrdinalIgnoreCase))report.Add("ERROR","Steam App 550 points to a different installation / Steam 将启动另一个游戏目录。");
                else if(registeredGames.Any(p=>String.Equals(Path.GetFullPath(p).TrimEnd('\\'),core.GameRoot.TrimEnd('\\'),StringComparison.OrdinalIgnoreCase))&&registeredGames.Length==1)report.Add("OK","Game folder matches Steam App 550 / 游戏目录与 Steam 安装记录一致。");
                else report.Add("ERROR","Steam App 550 has no unique matching installation / Steam 安装记录缺失或存在多个目标，无法确认启动目录。");
                if(!String.IsNullOrEmpty(steamError))report.Add("WARN","Steam launch options unavailable / 无法读取启动项："+steamError);
                var status=core.Status();bool bridge=status.Kind=="enabled";
                if(status.Kind=="conflict"||status.Kind=="pending")report.Add("ERROR",status.Text);
                else report.Add("OK",bridge?"Bridge mode / Bridge 模式":"No-Bridge mode; installed non-Bridge components remain / 无 Bridge 模式，保留已安装的非 Bridge 组件。");
                bool vulkan=options!=null&&Flag(options,"-vulkan");
                if(options==null)report.Add("WARN","Renderer flags were not verified / 尚未核对渲染启动参数。");
                else if(bridge&&status.LoaderPath=="bin/d3d9.dll"&&vulkan)report.Add("ERROR","Default Bridge requires removing -vulkan / 默认 Bridge 入口要求移除 -vulkan。");
                else if(bridge&&status.LoaderPath=="bin/dxvk_d3d9.dll"&&!vulkan)report.Add("ERROR","Recorded compatibility loader requires -vulkan / 已记录的兼容入口需要 -vulkan。");
                else if(!bridge&&vulkan){string alt=core.GamePath("bin/dxvk_d3d9.dll");if(File.Exists(alt)&&Core.IsBridgeDll(alt))report.Add("ERROR","-vulkan would load the remaining alternate Bridge entry / -vulkan 会加载遗留 Bridge 入口。");else report.Add("WARN","-vulkan selects an external renderer; native D3D9 optimization-only mode is not verified / -vulkan 对应其他渲染器，未验证为原生优化模式。");}
                else report.Add("OK","No known renderer-flag conflict / 未发现已知渲染参数冲突。");
                if(options!=null){var args=SteamSupport.Args(options);if(args.Any(a=>a.EndsWith(".bat",StringComparison.OrdinalIgnoreCase)||a.IndexOf("start_skeeto",StringComparison.OrdinalIgnoreCase)>=0||a=="%command%"))report.Add("WARN","Custom launcher command found; Steam may run a different entry / 启动项包含包装启动器，需人工核对。");}
                if(bridge){
                    string[] paths={status.LoaderPath,"bin/.l4d2bridge/L4D2Bridge32.exe","bin/.l4d2bridge/L4D2Bridge64.exe","bin/.l4d2bridge/d3d9vk_x86.dll","bin/.l4d2bridge/d3d9vk_x64.dll"};
                    bool all=true;for(int i=0;i<paths.Length;i++)all&=Pe(core,report,paths[i],(ushort)(i==2||i==4?0x8664:0x14c),true);
                    if(all){var ids=paths.Take(3).Select(p=>BuildId(core.GamePath(p))).ToArray();if(ids.All(id=>id!=null)&&ids.Distinct().Count()!=1)report.Add("ERROR","Client / Host build identities differ / Client 与 Host 构建标识不匹配。");else if(ids.Any(id=>id==null))report.Add("WARN","A binary has no unique build marker; exact pairing cannot be proven / 部分二进制缺少唯一构建标识，无法证明精确版本配套。");else report.Add("OK","Client and both Hosts share build identity / Client 与两个 Host 构建标识一致："+ids[0]);}
                    try{var config=core.ReadConfig();string host=config.Get("client.testX86Server")=="True"?"x86":"x64";report.Add("OK","Bridge configuration readable / Bridge 配置可读；Host="+host);}catch(Exception e){report.Add("ERROR","Bridge configuration: "+e.Message);}
                }
                Pe(core,report,"bin/left4neko.dll",0x14c,false);
                string flex=core.GamePath("bin/studiorender.dll");if(File.Exists(flex)){string hash=Core.HashFile(flex);report.Add(hash.Equals("03964dedcf8b7f4ebde24cd3d0738873d37c075a7a9b313dad001bb937f9d1b6",StringComparison.OrdinalIgnoreCase)?"OK":"WARN",hash.Equals("03964dedcf8b7f4ebde24cd3d0738873d37c075a7a9b313dad001bb937f9d1b6",StringComparison.OrdinalIgnoreCase)?"Known ThinFlex repair installed / 已安装已知 ThinFlex 修复":"ThinFlex patch identity not confirmed; file retained / ThinFlex 修复身份未确认，保留原文件。");}
                else report.Add("WARN","bin/studiorender.dll is missing / 缺少游戏引擎文件 bin/studiorender.dll。");
                report.Add("INFO","No-Bridge keeps L4N/ThinFlex and other existing mods; it does not install missing components or claim a performance improvement / 无 Bridge 模式保留现有组件，不下载缺失组件，也不保证性能提升。");
            }catch(Exception e){report.Add("ERROR",e.Message);}
            return report;
        }
        public static void ApplyMode(Core core,string mode){if(mode!="bridge"&&mode!="no-bridge")throw new ArgumentException("Unknown mode.");if(core.RunningCheck()||core.HasPending)throw new InvalidOperationException("Exit the game/Hosts and recover unfinished operations before changing modes.");var status=core.Status();if(mode=="bridge"){if(status.Kind=="enabled")return;if(status.CanEnable)core.Enable();else throw new InvalidOperationException("A matching Bridge backup is required; this tool does not download or invent a Bridge installation. "+status.Text);}else{if(status.CanDisable)core.Disable();else if(status.Kind!="disabled"&&status.Kind!="unavailable")throw new InvalidOperationException(status.Text);}}
    }
}
