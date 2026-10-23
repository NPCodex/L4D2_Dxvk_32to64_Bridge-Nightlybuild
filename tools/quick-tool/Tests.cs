// SPDX-License-Identifier: MIT
using System;
using System.Collections.Generic;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Text;
using System.Web.Script.Serialization;
namespace BridgeQuickTool {
    static class Tests {
        static string root,loader,plugin;
        static readonly List<string> passed=new List<string>();
        static readonly UTF8Encoding Utf8=new UTF8Encoding(false);
        static void Assert(bool ok,string message){if(!ok)throw new Exception(message);}
        static void Reject(Action action,string message){bool rejected=false;try{action();}catch(InvalidOperationException){rejected=true;}Assert(rejected,message);}
        static Core Fixture(string label){
            string game=Path.Combine(root,label,"游戏 目录");Directory.CreateDirectory(Path.Combine(game,"bin",".l4d2bridge"));Directory.CreateDirectory(Path.Combine(game,"bin","neko","plugins"));
            File.WriteAllText(Path.Combine(game,"left4dead2.exe"),"fixture only",Utf8);
            foreach(string file in new[]{"L4D2Bridge64.exe","L4D2Bridge32.exe","d3d9vk_x64.dll","d3d9vk_x86.dll"})File.WriteAllText(Path.Combine(game,"bin",".l4d2bridge",file),file,Utf8);
            File.WriteAllText(Path.Combine(game,"bin","left4neko.dll"),"preserve L4N",Utf8);File.WriteAllText(Path.Combine(game,"bin","neko","plugins","client_tab_adapter.dll"),"preserve custom Tab",Utf8);
            File.Copy(loader,Path.Combine(game,"bin","d3d9.dll"));File.Copy(plugin,Path.Combine(game,"bin","neko","plugins","L4D2BridgePlugin.dll"));
            File.WriteAllText(Path.Combine(game,"bin",".l4d2bridge","bridge.conf"),"# 我的配置\r\nclient.ipcBatchSize = 8 # 保留注释\r\nforceX64Server = True\r\nclient.testX86Server = False\r\nunknown.option = 保留\r\nlogLevel = Info\r\n",Utf8);
            var core=new Core(game);core.RunningCheck=()=>false;return core;
        }
        static void Test(string name,Action action){action();passed.Add(name);Console.WriteLine("PASS "+name);}
        static int Main(string[] args){
            try{
                root=Path.GetFullPath(args[0]);loader=Path.GetFullPath(args[1]);plugin=Path.GetFullPath(args[2]);Directory.CreateDirectory(root);
                Test("Bridge PE identity and non-Bridge rejection",()=>{Assert(Core.IsBridgeDll(loader),"real Bridge was not recognized");string bad=Path.Combine(root,"not-bridge.dll");File.WriteAllText(bad,"MZ Direct3DCreate9 L4D2BridgeControl");Assert(!Core.IsBridgeDll(bad),"raw strings accepted as PE");});
                Test("Disable/enable roundtrip preserves configuration, L4N and custom Tab",()=>{
                    var c=Fixture("roundtrip");var names=new[]{"bin/d3d9.dll","bin/neko/plugins/L4D2BridgePlugin.dll","bin/neko/plugins/client_tab_adapter.dll","bin/left4neko.dll","bin/.l4d2bridge/bridge.conf"};var hashes=names.ToDictionary(x=>x,x=>Core.HashFile(c.GamePath(x)));
                    c.Disable();Assert(c.Status().Kind=="disabled"&&!File.Exists(c.GamePath(names[0])),"disable failed");c.Enable();Assert(c.Status().Kind=="enabled","enable failed");foreach(string rel in names)Assert(Core.HashFile(c.GamePath(rel))==hashes[rel],"roundtrip altered "+rel);
                });
                Test("Running game blocks changes; logging remains available",()=>{var c=Fixture("running");var doc=c.ReadConfig();string before=doc.Hash;c.RunningCheck=()=>true;Reject(()=>c.Disable(),"running disable allowed");Reject(()=>c.SaveConfig(doc,new Dictionary<string,string>{{"client.ipcBatchSize","16"}}),"running save allowed");Assert(Core.HashFile(c.ConfigPath)==before,"blocked save wrote data");Assert(File.Exists(c.PackLogs(7)),"cannot pack while running");});
                Test("Partial disable/enable recover transactionally",()=>{
                    var c=Fixture("rollback");string hash=Core.HashFile(c.GamePath("bin/d3d9.dll"));c.Fault=x=>{throw new InvalidOperationException("injected interruption");};Reject(()=>c.Disable(),"fault was ignored");Assert(c.Status().Kind=="enabled"&&!c.HasPending&&Core.HashFile(c.GamePath("bin/d3d9.dll"))==hash,"partial disable not restored");c.Fault=null;c.Disable();c.Fault=x=>{throw new InvalidOperationException("injected interruption");};Reject(()=>c.Enable(),"fault was ignored");Assert(c.Status().Kind=="disabled"&&!c.HasPending&&!File.Exists(c.GamePath("bin/d3d9.dll")),"partial enable not restored");
                });
                Test("Recover durable journal after a simulated process interruption",()=>{
                    var c=Fixture("durable-journal");c.Disable();string stateFile=Path.Combine(c.StateRoot,"bridge-state.json");var serializer=new JavaScriptSerializer();var state=serializer.Deserialize<ToolState>(File.ReadAllText(stateFile));string journal=Path.Combine(c.StateRoot,"pending-operation.json");
                    var client=state.Files.First(f=>f.Relative=="bin/d3d9.dll");string saved=Path.Combine(c.StateRoot,"snapshots",state.SnapshotId,"bin_d3d9.dll");File.Copy(saved,c.GamePath(client.Relative));File.WriteAllText(journal,serializer.Serialize(new Journal{Id="interrupted-enable",Action="enable",Snapshot=state,Files=state.Files}));
                    var reopened=new Core(c.GameRoot);reopened.RunningCheck=()=>false;reopened.RecoverPending();Assert(reopened.Status().Kind=="disabled","durable enable not rolled back");reopened.Enable();state=serializer.Deserialize<ToolState>(File.ReadAllText(stateFile));File.WriteAllText(journal,serializer.Serialize(new Journal{Id="interrupted-disable",Action="disable",Snapshot=state,Files=state.Files}));File.Delete(c.GamePath(client.Relative));
                    reopened=new Core(c.GameRoot);reopened.RunningCheck=()=>false;reopened.RecoverPending();Assert(reopened.Status().Kind=="enabled"&&Core.HashFile(c.GamePath(client.Relative))==client.Hash,"durable disable not restored");
                });
                Test("Reject changed Host pairing, tampered backups and unknown active DLL",()=>{
                    var c=Fixture("pairing");c.Disable();File.AppendAllText(c.GamePath("bin/.l4d2bridge/L4D2Bridge64.exe"),"new version");Reject(()=>c.Enable(),"mixed host accepted");Assert(!File.Exists(c.GamePath("bin/d3d9.dll")),"paired rejection created client");
                    var d=Fixture("tamper");d.Disable();string saved=Directory.GetFiles(Path.Combine(d.StateRoot,"snapshots"),"bin_d3d9.dll",SearchOption.AllDirectories).Single();File.AppendAllText(saved,"changed");Reject(()=>d.Enable(),"tampered backup accepted");
                    var e=Fixture("unknown");File.WriteAllText(e.GamePath("bin/d3d9.dll"),"some other loader");Reject(()=>e.Disable(),"non-bridge file moved");Assert(File.ReadAllText(e.GamePath("bin/d3d9.dll"))=="some other loader","non-bridge overwritten");
                });
                Test("Adopt an updated active bundle without restoring an old snapshot",()=>{var c=Fixture("updated");c.Disable();File.Copy(loader,c.GamePath("bin/d3d9.dll"));File.Copy(plugin,c.GamePath("bin/neko/plugins/L4D2BridgePlugin.dll"));File.AppendAllText(c.GamePath("bin/.l4d2bridge/L4D2Bridge64.exe"),"updated");c.Disable();c.Enable();Assert(c.Status().Kind=="enabled","new active bundle not adopted");});
                Test("Inactive -vulkan loader never overrides recorded default mode",()=>{
                    var c=Fixture("legacy-alternate");c.Disable();File.Copy(loader,c.GamePath("bin/dxvk_d3d9.dll"));string old=Core.HashFile(c.GamePath("bin/dxvk_d3d9.dll"));var status=c.Status();Assert(status.Kind=="disabled"&&status.LoaderPath=="bin/d3d9.dll"&&status.OtherLoaderPresent,"unused alternate reported as active");c.Enable();Assert(c.Status().Kind=="enabled","default mode did not enable");c.Disable();Assert(c.Status().Kind=="disabled"&&Core.HashFile(c.GamePath("bin/dxvk_d3d9.dll"))==old,"unused alternate was changed");
                    var d=Fixture("ambiguous");File.Copy(loader,d.GamePath("bin/dxvk_d3d9.dll"));Assert(d.Status().Kind=="conflict","unrecorded ambiguous mode accepted");Reject(()=>d.Disable(),"ambiguous entry moved");
                });
                Test("UTF-8/UTF-16/GBK, comments, CRLF and unknown keys survive edits",()=>{
                    int i=0;foreach(Encoding enc in new Encoding[]{new UTF8Encoding(false),new UTF8Encoding(true),new UnicodeEncoding(false,true),new UnicodeEncoding(true,true),Encoding.GetEncoding(936)}){
                        var c=Fixture("encoding"+(i++));string text="# 中文标题\r\nCLIENT.IPCBATCHSIZE = 8  # 注释\r\nunknown.option = 原样保留\r\n";File.WriteAllText(c.ConfigPath,text,enc);var loaded=c.ReadConfig();string originalHash=loaded.Hash;var backup=c.SaveConfig(loaded,new Dictionary<string,string>{{"client.ipcBatchSize","16"},{"client.vbibMappedShadow","True"}});
                        var next=c.ReadConfig();Assert(next.Get("client.ipcBatchSize")=="16"&&next.Get("client.vbibMappedShadow")=="True","setting not changed");Assert(next.Text.Contains("CLIENT.IPCBATCHSIZE = 16  # 注释\r\n")&&next.Text.Contains("unknown.option = 原样保留\r\n"),"format or unknown key changed");Assert(Core.HashFile(backup)==originalHash,"backup is not original bytes");Assert(next.TextEncoding.CodePage==loaded.TextEncoding.CodePage&&next.Bom==loaded.Bom,"encoding changed");c.RestoreConfig(backup,next.Hash);Assert(Core.HashFile(c.ConfigPath)==originalHash,"restore not byte-exact");
                    }
                });
                Test("No-op, stale document, duplicate managed keys and invalid values",()=>{
                    var c=Fixture("config-guards");var loaded=c.ReadConfig();Assert(c.SaveConfig(loaded,new Dictionary<string,string>{{"client.ipcBatchSize","8"}})==null,"no-op rewrote config");Assert(c.ConfigBackups().Length==0,"no-op made backup");File.AppendAllText(c.ConfigPath,"# external edit\r\n");Reject(()=>c.SaveConfig(loaded,new Dictionary<string,string>{{"client.ipcBatchSize","16"}}),"stale config overwritten");loaded=c.ReadConfig();Reject(()=>c.SaveConfig(loaded,new Dictionary<string,string>{{"client.ipcBatchSize","1000"}}),"invalid batch accepted");Reject(()=>c.SaveConfig(loaded,new Dictionary<string,string>{{"rcon_password","bad"}}),"unmanaged key accepted");File.AppendAllText(c.ConfigPath,"client.ipcBatchSize = 4\r\n");Reject(()=>c.ReadConfig().Get("client.ipcBatchSize"),"duplicate key accepted");
                });
                Test("Presenter scope and diagnostic settings remain explicit",()=>{var c=Fixture("presenter");File.AppendAllText(c.ConfigPath,"client.steamOverlayInput = False\r\nserver.presenterInput = True\r\n");c.SaveConfig(c.ReadConfig(),Core.Presenter(false));var d=c.ReadConfig();Assert(d.Get("server.presenterWindow")=="False"&&d.Get("server.presenterInput")=="True"&&d.Get("client.steamOverlayInput")=="False","presenter disable altered unrelated flags");c.SaveConfig(d,Core.Presenter(true));Assert(c.ReadConfig().Get("client.hookMessagePump")=="True","presenter enable incomplete");});
                Test("ZIP logs are bounded, redacted, local-only and include no dumps/databases",()=>{
                    var c=Fixture("logs");File.WriteAllText(c.GamePath("current.log"),"header\n"+new string('a',5*1024*1024)+"\n"+Environment.GetFolderPath(Environment.SpecialFolder.UserProfile)+"\\user\n"+c.GameRoot+"\\bin\nrcon_password = never-share\nAuthorization: Bearer never-share2\n{\"token\":\"never-share3\"}\n",Utf8);File.WriteAllText(c.GamePath("old.log"),"old marker",Utf8);File.SetLastWriteTimeUtc(c.GamePath("old.log"),DateTime.UtcNow.AddDays(-60));File.WriteAllText(c.GamePath("crash.mdmp"),"memory dump",Utf8);File.WriteAllText(c.GamePath("bin/.l4d2bridge/resource-retention.db"),"db",Utf8);string before=Core.HashFile(c.ConfigPath);string zip=c.PackLogs(7);
                    using(var z=ZipFile.OpenRead(zip)){
                        Assert(z.Entries.Any(e=>e.FullName=="manifest.json"),"no manifest");Assert(!z.Entries.Any(e=>e.FullName.EndsWith(".dll")||e.FullName.EndsWith(".exe")||e.FullName.EndsWith(".db")||e.FullName.EndsWith(".mdmp")||e.FullName.EndsWith("old.log")),"included payload/dump/db/old log");
                        string body;using(var r=new StreamReader(z.GetEntry("logs/current.log").Open()))body=r.ReadToEnd();Assert(body.Length<4*1024*1024+200&&!body.Contains("never-share")&&!body.Contains(c.GameRoot)&&body.Contains("%USERPROFILE%")&&body.Contains("%GAME%"),"redaction or limit failed");
                        using(var r=new StreamReader(z.GetEntry("manifest.json").Open()))Assert(r.ReadToEnd().Contains("\"truncated\":true"),"tail not disclosed");
                    }
                    Assert(Core.HashFile(c.ConfigPath)==before,"pack changed diagnostics/config");
                });
                Test("Legacy disabled backup import keeps Bridge off and restores verified pair",()=>{
                    var c=Fixture("import");string path=Path.Combine(root,"legacy.json"),a=Path.Combine(root,"legacy-client.dll"),b=Path.Combine(root,"legacy-plugin.dll");File.Copy(c.GamePath("bin/d3d9.dll"),a);File.Copy(c.GamePath("bin/neko/plugins/L4D2BridgePlugin.dll"),b);File.Delete(c.GamePath("bin/d3d9.dll"));File.Delete(c.GamePath("bin/neko/plugins/L4D2BridgePlugin.dll"));var legacy=new LegacyState {game=c.GameRoot,status="disabled-and-verified",moved=new List<LegacyFile>{new LegacyFile{relative="bin/d3d9.dll",backup=a,hash=Core.HashFile(a)},new LegacyFile{relative="bin/neko/plugins/L4D2BridgePlugin.dll",backup=b,hash=Core.HashFile(b)}}};File.WriteAllText(path,new JavaScriptSerializer().Serialize(legacy),Utf8);c.ImportDisabled(path);Assert(c.Status().Kind=="disabled","import enabled Bridge");c.Enable();Assert(Core.HashFile(c.GamePath("bin/d3d9.dll"))==Core.HashFile(a),"import restore hash wrong");c.Disable();
                });
                Test("Reject traversal and malformed state paths",()=>{var c=Fixture("paths");Reject(()=>c.GamePath("../outside.dll"),"path escaped game");c.Disable();string state=Path.Combine(c.StateRoot,"bridge-state.json");string value=File.ReadAllText(state);File.WriteAllText(state,value.Replace("\"SnapshotId\":\"off-","\"SnapshotId\":\"../off-"));Reject(()=>c.Status(),"state traversal accepted");});
                Test("Unicode log tails remain readable and broken state can be diagnosed",()=>{
                    var c=Fixture("unicode-tail");File.WriteAllText(c.GamePath("unicode.log"),new string('中',3*1024*1024)+"\n保留末尾章节信息\n",Encoding.Unicode);c.Disable();File.WriteAllText(Path.Combine(c.StateRoot,"bridge-state.json"),"invalid json");string path=c.PackLogs(7);using(var z=ZipFile.OpenRead(path))using(var r=new StreamReader(z.GetEntry("logs/unicode.log").Open()))Assert(r.ReadToEnd().Contains("保留末尾章节信息"),"UTF16 tail is unreadable");
                });
                var preview=Fixture("preview");preview.Disable();
                File.WriteAllText(Path.Combine(root,"results.json"),new JavaScriptSerializer().Serialize(new {passed=passed,previewGame=preview.GameRoot}),Utf8);
                Console.WriteLine("ALL "+passed.Count+" TEST GROUPS PASSED");return 0;
            }catch(Exception e){Console.Error.WriteLine(e.ToString());return 1;}
        }
    }
}
