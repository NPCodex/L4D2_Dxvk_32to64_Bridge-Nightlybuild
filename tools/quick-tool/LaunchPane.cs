// SPDX-License-Identifier: MIT
using System;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace BridgeQuickTool {
    internal sealed class LaunchPane : UserControl {
        readonly Core core;
        readonly Action changed;
        readonly ComboBox mode;
        readonly TextBox options,report;
        readonly Label account,feedback;
        readonly CheckBox afterLaunch;
        readonly Timer timer=new Timer{Interval=2000};
        SteamOptions loaded;
        StartupCheck session;
        bool working,polling;
        public bool LaunchPending {get{return session!=null&&!session.Complete;}}
        public bool MutationsBlocked {get{return working||LaunchPending;}}
        public LaunchPane(Core service,HoverHelp help,Action onChanged){
            core=service;changed=onChanged;Dock=DockStyle.Fill;AutoScroll=true;
            var grid=new TableLayoutPanel{Dock=DockStyle.Top,AutoSize=true,ColumnCount=1};grid.ColumnStyles.Add(new ColumnStyle(SizeType.Percent,100));Controls.Add(grid);
            Add(grid,new Label{Text="Bridge / 无 Bridge 模式；保留已安装的 L4N、ThinFlex 和其他模组。此工具不下载缺失组件。",ForeColor=Color.DimGray},44);
            mode=new ComboBox{DropDownStyle=ComboBoxStyle.DropDownList};mode.Items.AddRange(new object[]{new Choice("bridge","原版 Bridge（沿用当前 Host / 后端设置）"),new Choice("no-bridge","无 Bridge：保留 L4N / ThinFlex 等现有优化")});mode.SelectedIndex=0;help.AddText(mode,"两种运行模式：Bridge 使用已安装的配套 Client / Host；无 Bridge 只停用已识别的 Bridge 入口和菜单插件，保留 L4N / ThinFlex 等其他组件。不会凭空安装缺失的优化，也不承诺性能提升。");Add(grid,mode,40);
            var switches=Flow();Button(switches,"应用运行模式",()=>{if(working||LaunchPending)throw new InvalidOperationException("Check in progress.");Installation.ApplyMode(core,((Choice)mode.SelectedItem).Value);changed();feedback.Text="模式已应用，下次启动生效；请自检并核对启动项。";RunCheck(false);},help,"退出游戏与 Host 后应用所选模式。切换 Bridge 有校验与备份；无 Bridge 模式保留其他组件。Steam 启动项另行保存，未保存的设置不会自动写入。");Button(switches,"安装自检",()=>RunCheck(false),help,"检查 Steam 安装记录、游戏目录、PE 位数、Client / Host 构建标识、渲染参数和已有组件。自检不安装或修复文件。打开工具时也会自动执行。");Button(switches,"停止启动检查",()=>{if(LaunchPending){session.Complete=true;session.Result="Startup observation stopped by user / 用户已停止启动观察；游戏未被关闭。";session.Save(core);timer.Stop();feedback.Text=session.Result;}},help,"仅停止本工具的启动观察，不关闭游戏或 Host。未完成检查不会记作成功。");Add(grid,switches,43);
            account=new Label{Text="Steam launch options / Steam 启动项：尚未读取",ForeColor=Color.DimGray,AutoEllipsis=true};Add(grid,account,40);help.AddText(account,"只读取当前 Steam 账户的 App 550 启动项。账户不能唯一确认时会停止，不猜测账户。切换 Steam 账户后请重新读取。");
            options=new TextBox{Multiline=true,ScrollBars=ScrollBars.Vertical,MaxLength=4096};Add(grid,options,76);help.AddText(options,"App 550 的完整启动项。保存只改这一项，保留其他游戏、注释和文件编码；保存需先退出 Steam。不会自动移除参数或设置 RCON。Bridge 默认入口需移除 -vulkan，现有兼容入口则需对应参数。");
            var edits=Flow();Button(edits,"读取 Steam 启动项",()=>RunCheck(true),help,"重新读取当前账户启动项，会放弃此框尚未保存的编辑。打开页面的普通自检不会覆盖你的编辑。");Button(edits,"保存 Steam 启动项",SaveOptions,help,"先退出 Steam。核对账户和文件未改变，备份完整 localconfig.vdf 后，只修改 App 550 的 LaunchOptions。备份保存在 state/steam-launch-backups，不会打入日志包；可能包含账户设置，请保留在本机。");Button(edits,"打开自检报告目录",()=>{string p=ReportDirectory();Directory.CreateDirectory(p);Process.Start(new ProcessStartInfo(p){UseShellExecute=true});},help,"查看本机保存的安装自检报告。报告不会自动上传。");Add(grid,edits,43);
            afterLaunch=new CheckBox{Text="通过本工具启动后自动检查（最多 90 秒）",Checked=true};Add(grid,afterLaunch,34);help.AddText(afterLaunch,"启动前自检始终执行。勾选后，在本工具保持打开期间观察新游戏进程、预期 Host 和新日志；关闭工具后停止，重开会标记观察中断。无法据此保证帧率、地图、渲染或稳定性。");
            report=new TextBox{Multiline=true,ReadOnly=true,ScrollBars=ScrollBars.Both,WordWrap=false,Font=new Font("Consolas",9.5f)};Add(grid,report,260);help.AddText(report,"自检结果中 OK 为已核对项目，WARN 为缺失或证据不足，ERROR 为已发现的冲突。启动后检查仅报告观察到的证据；未观察到进程不直接等于崩溃。");
            feedback=new Label{ForeColor=Color.DimGray};Add(grid,feedback,65);help.AddText(feedback,"显示模式、启动项保存和启动后检查结果。报告会遮盖当前游戏和用户路径；日志与报告都只保存在本机。");
            timer.Tick+=(s,e)=>PollStartup();
            try{var status=core.Status();if(status.Kind=="disabled"||status.Kind=="unavailable")mode.SelectedIndex=1;}catch{}
        }
        static FlowLayoutPanel Flow(){return new FlowLayoutPanel{Dock=DockStyle.Fill,WrapContents=false};}
        static void Add(TableLayoutPanel grid,Control control,int height){int row=grid.RowCount++;grid.RowStyles.Add(new RowStyle(SizeType.Absolute,height));control.Dock=DockStyle.Fill;control.Margin=new Padding(3,4,3,4);grid.Controls.Add(control,0,row);}
        void Button(Control parent,string text,Action action,HoverHelp help,string tip){var b=new Button{Text=text,AutoSize=true,Height=32,Margin=new Padding(0,3,9,0)};b.Click+=(s,e)=>{try{action();}catch(Exception ex){feedback.Text=ex.Message;MessageBox.Show(this,ex.Message,"操作未完成",MessageBoxButtons.OK,MessageBoxIcon.Warning);}};parent.Controls.Add(b);help.AddText(b,tip);}
        string ReportDirectory(){string p=Path.Combine(core.ToolRoot,"reports","checks");Core.Guard(p,core.GameRoot);return p;}
        void StoreReport(string text){string dir=ReportDirectory();Directory.CreateDirectory(dir);string path=Path.Combine(dir,"install-"+DateTime.UtcNow.ToString("yyyyMMdd-HHmmss")+"-"+Guid.NewGuid().ToString("N").Substring(0,6)+".txt");File.WriteAllText(path,core.Redact(text),new System.Text.UTF8Encoding(false));}
        InstallReport Scan(out SteamOptions current){string[] games=null;string error=null;current=null;try{games=SteamSupport.Games(SteamSupport.FindSteam());current=SteamSupport.Current();}catch(Exception ex){error=ex.Message;}return Installation.Check(core,games,current==null?null:current.Value,error);}
        public void InitialCheck(){RunCheck(true);try{session=StartupCheck.Load(core);if(session!=null){feedback.Text=session.Result;if(!session.Complete){session.Gap=true;timer.Start();PollStartup();}}}catch(Exception ex){feedback.Text="启动检查记录无法读取："+ex.Message;}}
        async void RunCheck(bool reloadOptions){if(working)return;if(reloadOptions&&loaded!=null&&options.Text!=loaded.Value&&MessageBox.Show(this,"重新读取将放弃未保存的启动项。继续？","读取启动项",MessageBoxButtons.OKCancel,MessageBoxIcon.Question)!=DialogResult.OK)return;working=true;try{SteamOptions current=null;var result=await Task.Run(()=>Scan(out current));if(IsDisposed)return;if(reloadOptions||loaded==null){loaded=current;options.Text=loaded==null?"":loaded.Value;account.Text=loaded==null?"Steam 启动项不可用；详见自检结果。":"Steam App 550 · account "+loaded.AccountId+" · "+loaded.Path;}report.Text=result.ToString();StoreReport(result.ToString());feedback.Text=result.HasErrors?"发现冲突，请先处理 ERROR 项。":"安装检查完成；WARN 项表示未确认的条件。";}catch(Exception ex){if(!IsDisposed)feedback.Text="自检未完成："+ex.Message;}finally{working=false;}}
        void SaveOptions(){if(working||session!=null&&!session.Complete)throw new InvalidOperationException("请等待正在进行的自检或启动观察完成。");if(loaded==null)throw new InvalidOperationException("请先读取当前 Steam 启动项。");var current=SteamSupport.Current();if(!String.Equals(current.Path,loaded.Path,StringComparison.OrdinalIgnoreCase))throw new InvalidOperationException("Steam 账户已改变，请重新读取。");string backup=SteamSupport.Save(core,loaded,options.Text,SteamSupport.SteamRunning);loaded=SteamSupport.Read(loaded.SteamRoot,loaded.AccountId);feedback.Text=backup==null?"启动项未改变。":"已保存 App 550 启动项。原文件已备份："+backup;RunCheck(false);}
        public async void Launch(){
            if(working){feedback.Text="正在自检，请稍后启动。";return;}working=true;
            try{
                if(LaunchPending)throw new InvalidOperationException("上一次启动仍在观察中，请等待或停止启动检查后再试。");if(core.HasPending)throw new InvalidOperationException("请先恢复未完成操作。");if(core.RunningCheck())throw new InvalidOperationException("游戏或 Host 已在运行；请从 Steam 切回游戏。");if(loaded!=null&&options.Text!=loaded.Value)throw new InvalidOperationException("启动项有未保存编辑，请保存或重新读取后再启动。");
                SteamOptions current=null;var audit=await Task.Run(()=>Scan(out current));if(IsDisposed)return;report.Text=audit.ToString();StoreReport(audit.ToString());if(audit.HasErrors||current==null)throw new InvalidOperationException("启动前自检未通过，或 Steam 启动项目标尚未确认。请检查结果。");
                if(core.RunningCheck())throw new InvalidOperationException("游戏或 Host 已启动，未重复发出启动请求。");
                session=afterLaunch.Checked?StartupCheck.Begin(core):null;if(session!=null)session.Save(core);
                try{Process.Start(new ProcessStartInfo("steam://rungameid/550"){UseShellExecute=true});}catch{if(session!=null){session.Complete=true;session.Result="Steam launch request failed / Steam 启动请求失败。";session.Save(core);}throw;}
                feedback.Text="已请求 Steam 启动 App 550，沿用已保存的启动项。";if(session!=null)timer.Start();
            }catch(Exception ex){if(!IsDisposed){feedback.Text=ex.Message;MessageBox.Show(this,ex.Message,"启动未完成",MessageBoxButtons.OK,MessageBoxIcon.Warning);}}finally{working=false;}
        }
        async void PollStartup(){if(polling||session==null||session.Complete||IsDisposed){if(session!=null&&session.Complete)timer.Stop();return;}polling=true;try{var evidence=await Task.Run(()=>StartupCheck.Probe(core,session));int fresh=await Task.Run(()=>StartupCheck.Fresh(core,session));if(IsDisposed)return;session.Observe(evidence,fresh,DateTime.UtcNow);session.Save(core);feedback.Text=session.Result;if(session.Complete){timer.Stop();StoreReport(session.Result);}}catch(Exception ex){if(!IsDisposed)feedback.Text="启动观察未完成："+ex.Message;}finally{polling=false;}}
        protected override void Dispose(bool disposing){if(disposing)timer.Dispose();base.Dispose(disposing);}
    }
}
