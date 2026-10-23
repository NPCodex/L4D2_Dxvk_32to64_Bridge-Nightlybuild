// SPDX-License-Identifier: MIT
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace BridgeQuickTool {
    public sealed class Choice {
        public string Value,Text;
        public Choice(string value,string text) { Value=value;Text=text; }
        public override string ToString() { return Text; }
    }
    public sealed class MainForm : Form {
        readonly Core core;
        readonly HoverHelp help;
        readonly LaunchPane launcher;
        ConfigDocument loaded;
        Label status,pathLabel,feedback,configInfo,exportInfo;
        Button enable,disable,recover,save,openZip;
        ComboBox host,policy,batch,presenter,level,days,backups;
        readonly List<CheckBox> checks=new List<CheckBox>();
        readonly List<Control> actions=new List<Control>();
        bool busy;
        string lastZip;
        public MainForm(Core service) {
            core=service;Text="L4D2 Bridge 快捷工具";Font=new Font("Microsoft YaHei UI",10F);help=new HoverHelp(this);
            AutoScaleMode=AutoScaleMode.Font;ClientSize=new Size(1000,760);MinimumSize=new Size(920,720);StartPosition=FormStartPosition.CenterScreen;
            var root=new TableLayoutPanel {Dock=DockStyle.Fill,ColumnCount=1,RowCount=4,Padding=new Padding(20,14,20,12),BackColor=Color.FromArgb(247,248,250)};
            root.RowStyles.Add(new RowStyle(SizeType.Absolute,45));root.RowStyles.Add(new RowStyle(SizeType.Absolute,108));root.RowStyles.Add(new RowStyle(SizeType.Percent,100));root.RowStyles.Add(new RowStyle(SizeType.Absolute,62));Controls.Add(root);
            var title=new Label {Text="L4D2 Bridge 快捷工具",Font=new Font(Font.FontFamily,19,FontStyle.Bold),AutoSize=true,Dock=DockStyle.Fill};root.Controls.Add(title,0,0);
            var top=new TableLayoutPanel {Dock=DockStyle.Fill,ColumnCount=1,RowCount=3};top.RowStyles.Add(new RowStyle(SizeType.Absolute,28));top.RowStyles.Add(new RowStyle(SizeType.Absolute,24));top.RowStyles.Add(new RowStyle(SizeType.Absolute,48));root.Controls.Add(top,0,1);
            status=new Label {AutoSize=true,Font=new Font(Font,FontStyle.Bold),Dock=DockStyle.Fill};top.Controls.Add(status);
            pathLabel=new Label {Text=core.GameRoot,ForeColor=Color.DimGray,AutoEllipsis=true,Dock=DockStyle.Fill};top.Controls.Add(pathLabel);
            var switches=Flow();top.Controls.Add(switches);
            enable=Button(switches,"启用 Bridge",()=>Run(()=>core.Enable(),"Bridge 已启用，下次正常启动游戏生效。"));
            disable=Button(switches,"停用 Bridge",()=>Run(()=>core.Disable(),"Bridge 已停用；配置和其他插件保持不变。"));
            Button(switches,"启动游戏",LaunchGame);
            Button(switches,"刷新状态",UpdateStatus);
            recover=Button(switches,"恢复未完成操作",()=>Run(()=>core.RecoverPending(),"已恢复未完成操作。"));
            var tabs=new TabControl {Dock=DockStyle.Fill,ShowToolTips=true};root.Controls.Add(tabs,0,2);
            var settings=Page(tabs,"常用设置");var table=Grid(settings);
            host=Combo(table,"Host 架构",new[]{new Choice("x64","x64 Host（默认）"),new Choice("x86","x86 Host"),new Choice("","保持原配置")});
            policy=Combo(table,"内存保留策略",new[]{new Choice("learned-aggressive","Learned-aggressive"),new Choice("keep","Keep（保留 backing）"),new Choice("drop","Drop（实验选项）"),new Choice("","保持原配置")});
            batch=Combo(table,"IPC 批次",new[]{"1","2","4","8","16","32",""}.Select(x=>new Choice(x,x==""?"保持原配置":x)).ToArray());
            Check(table,"静态 VB/IB 映射",new[]{"client.vbibMappedShadow"});
            presenter=Combo(table,"ReShade / Overlay 呈现窗口",new[]{new Choice("","保持当前配置"),new Choice("on","启用呈现窗口及配套输入钩子"),new Choice("off","停用呈现窗口及配套输入钩子")});
            Note(table,"呈现窗口开关不会关闭整个 Bridge，也不会停用全部 Steam 适配。它与顶部的 Bridge 启停分开。",54);
            level=Combo(table,"普通日志级别",new[]{"Info","Debug","Warn","Error",""}.Select(x=>new Choice(x,x==""?"保持原配置":x)).ToArray());
            Note(table,"设置仅在点击保存时写入，并自动备份。Host、IPC、映射、Presenter 和诊断设置需完整重启游戏及 Host。",50);
            var settingActions=Flow();Row(table,"",settingActions,46);
            save=Button(settingActions,"保存设置",Save);
            Button(settingActions,"重新读取配置",()=>{if(MessageBox.Show(this,"重新读取会放弃未保存的选择。继续？","重新读取",MessageBoxButtons.OKCancel,MessageBoxIcon.Question)==DialogResult.OK)LoadConfig();});
            Button(settingActions,"打开配置文件",()=>Open(core.ConfigPath));
            configInfo=new Label {AutoSize=false,Dock=DockStyle.Fill,ForeColor=Color.DimGray};Row(table,"",configInfo,48);
            var logs=Page(tabs,"日志与诊断");var logGrid=Grid(logs);
            Note(logGrid,"一键打包现有 Bridge、DXVK 及相关插件日志，并附上配置和版本校验信息。ZIP 保存在本地，不会上传。",58);
            days=Combo(logGrid,"日志范围",new[]{new Choice("1","最近 24 小时"),new Choice("3","最近 3 天"),new Choice("7","最近 7 天"),new Choice("30","最近 30 天")});days.SelectedIndex=2;
            var packActions=Flow();Row(logGrid,"",packActions,46);
            Button(packActions,"一键打包日志",Pack);
            openZip=Button(packActions,"定位最近的 ZIP",()=>Locate(lastZip));openZip.Enabled=false;
            exportInfo=new Label {Dock=DockStyle.Fill,ForeColor=Color.DimGray};Row(logGrid,"",exportInfo,53);
            Note(logGrid,"每份日志最多取末尾 4 MiB，总计最多 64 MiB / 120 份。包内不含转储、数据库、缓存或 Steam 用户数据；会遮盖常见口令和本机用户路径。分享前仍请检查玩家名、服务器等信息。",76);
            Note(logGrid,"以下选项只在保存后启用。已有日志不会因关闭诊断而删除；打包本身不改变诊断设置。",48);
            Check(logGrid,"内存诊断（Client + Host）",new[]{"client.memoryMonitoring","server.memoryMonitoring"});
            Check(logGrid,"崩溃历史（Client + Host）",new[]{"client.crashDiagnostics","server.crashDiagnostics"});
            Check(logGrid,"详细异常信息",new[]{"client.exceptionDiagnosticsDetailed","server.exceptionDiagnosticsDetailed"});
            Check(logGrid,"API 等待诊断",new[]{"client.apiWaitDiagnostics","server.apiWaitDiagnostics"});
            Check(logGrid,"PageBlock 诊断",new[]{"client.pageBlockDiagnostics"});
            Check(logGrid,"逐调用详细日志（开销较高）",new[]{"logApiCalls","logServerCommands","logAllCalls","logAllCommands"});
            var diagActions=Flow();Row(logGrid,"",diagActions,46);
            Button(diagActions,"选择内存 / 崩溃排查模式",()=>Preset(true));
            Button(diagActions,"选择关闭全部详细诊断",()=>Preset(false));
            Button(diagActions,"保存设置",Save);
            var history=Page(tabs,"备份与记录");var historyGrid=Grid(history);
            Note(historyGrid,"Bridge 开关管理状态栏标明的加载方式及官方菜单插件，不修改 Steam 启动项。另一加载方式的遗留 DLL 保留不动；使用 -vulkan 时需对应兼容入口。停用后若 Host / 后端 / L4N 更新，工具会拒绝恢复旧入口。",86);
            backups=Combo(historyGrid,"配置备份",new Choice[0]);
            var historyActions=Flow();Row(historyGrid,"",historyActions,46);
            Button(historyActions,"恢复选中的配置备份",Restore);
            Button(historyActions,"打开工具数据目录",()=>OpenFolder(core.ToolRoot));
            Button(historyActions,"打开日志包目录",()=>OpenFolder(Path.Combine(core.ToolRoot,"reports")));
            Note(historyGrid,"恢复配置会先备份当前配置。切换 Bridge 或保存/恢复配置前，需正常退出游戏和 Host；工具不会强制结束它们。",58);
            Note(historyGrid,"工具完全本地运行，不自动更新、不联网、不驻留、不修改游戏启动项。开关和保存操作有日志记录。",56);
            var monitor=Page(tabs,"实时日志");var logView=new LogMonitor(core,help);monitor.Controls.Add(logView);
            tabs.SelectedIndexChanged+=(s,e)=>logView.SetActive(Visible&&tabs.SelectedTab==monitor);
            VisibleChanged+=(s,e)=>logView.SetActive(Visible&&tabs.SelectedTab==monitor);
            var launchPage=Page(tabs,"模式 / 启动 / 自检");launchPage.ToolTipText="Bridge 与无 Bridge 模式、Steam 启动项、安装位置和启动后检查。";launcher=new LaunchPane(core,help,()=>{UpdateStatus();LoadConfig();});launchPage.Controls.Add(launcher);
            tabs.SelectedTab=launchPage;
            Shown+=(s,e)=>{if(!rendering)launcher.InitialCheck();};
            feedback=new Label {Dock=DockStyle.Fill,ForeColor=Color.FromArgb(55,65,81),Padding=new Padding(0,10,0,0)};root.Controls.Add(feedback,0,3);
            FormClosing+=(s,e)=>{if(busy){e.Cancel=true;MessageBox.Show(this,"正在完成操作，请稍等。","Bridge 快捷工具");}};
            help.Add(status,"状态栏");help.Add(pathLabel,"游戏目录");help.Add(configInfo,"当前配置说明");help.Add(exportInfo,"日志包位置");help.Add(feedback,"操作结果");help.DisabledControls(this);
            UpdateStatus();LoadConfig();
        }
        static FlowLayoutPanel Flow() { return new FlowLayoutPanel {Dock=DockStyle.Fill,AutoSize=false,WrapContents=false,Padding=new Padding(0,4,0,0)}; }
        TabPage Page(TabControl tabs,string title) {var page=new TabPage(title){BackColor=Color.White,AutoScroll=true,Padding=new Padding(14),ToolTipText=HelpText.For(title)};tabs.TabPages.Add(page);return page;}
        TableLayoutPanel Grid(Control parent) {var grid=new TableLayoutPanel{Dock=DockStyle.Top,AutoSize=true,ColumnCount=2,Padding=new Padding(2)};grid.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute,230));grid.ColumnStyles.Add(new ColumnStyle(SizeType.Percent,100));parent.Controls.Add(grid);return grid;}
        void Row(TableLayoutPanel table,string label,Control control,int height) {int row=table.RowCount++;table.RowStyles.Add(new RowStyle(SizeType.Absolute,height));var l=new Label{Text=label,Dock=DockStyle.Fill,TextAlign=ContentAlignment.MiddleLeft};table.Controls.Add(l,0,row);control.Dock=DockStyle.Fill;control.Margin=new Padding(3,5,3,5);table.Controls.Add(control,1,row);if(label!=""){help.Add(l,label);help.Add(control,label);}}
        void Note(TableLayoutPanel table,string text,int height) {int row=table.RowCount++;table.RowStyles.Add(new RowStyle(SizeType.Absolute,height));var label=new Label{Text=text,Dock=DockStyle.Fill,ForeColor=Color.DimGray,Padding=new Padding(0,7,0,0)};table.Controls.Add(label,0,row);table.SetColumnSpan(label,2);help.AddText(label,text);}
        ComboBox Combo(TableLayoutPanel table,string label,Choice[] choices){var combo=new ComboBox{DropDownStyle=ComboBoxStyle.DropDownList};combo.Items.AddRange(choices);if(choices.Length>0)combo.SelectedIndex=0;Row(table,label,combo,42);help.Combo(combo,label);actions.Add(combo);return combo;}
        void Check(TableLayoutPanel table,string label,string[] keys){var check=new CheckBox{Text=label,ThreeState=true,Tag=keys,AutoSize=true};checks.Add(check);Row(table,"",check,38);help.Add(check,label);actions.Add(check);}
        Button Button(Control parent,string text,Action action){var b=new Button{Text=text,AutoSize=true,Height=32,MinimumSize=new Size(100,32),Padding=new Padding(8,0,8,0),Margin=new Padding(0,0,10,0)};b.Click+=(s,e)=>{try{if(launcher!=null&&launcher.MutationsBlocked&&new[]{"启用 Bridge","停用 Bridge","保存设置","恢复选中的配置备份","恢复未完成操作"}.Contains(text))throw new InvalidOperationException("自检或启动观察正在进行，请等待检查完成后再修改配置或模式。");action();}catch(Exception ex){Error(ex);}};parent.Controls.Add(b);help.Add(b,text);actions.Add(b);return b;}
        void LaunchGame(){
            var tabs=launcher.Parent.Parent as TabControl;if(tabs!=null)tabs.SelectedTab=(TabPage)launcher.Parent;
            launcher.Launch();
        }
        void Error(Exception ex){feedback.Text="操作未完成："+ex.Message;MessageBox.Show(this,ex.Message,"操作未完成",MessageBoxButtons.OK,MessageBoxIcon.Warning);UpdateStatus();}
        void Run(Action action,string message){action();feedback.Text=message;UpdateStatus();RefreshBackups();}
        void Open(string path){Process.Start(new ProcessStartInfo(path){UseShellExecute=true});}
        void OpenFolder(string path){Core.Guard(path,core.GameRoot);Directory.CreateDirectory(path);Open(path);}
        void Locate(string path){if(String.IsNullOrEmpty(path)||!File.Exists(path))throw new IOException("尚无日志包。");Process.Start("explorer.exe","/select,\""+path+"\"");}
        string Selected(ComboBox box){return box.SelectedItem is Choice?((Choice)box.SelectedItem).Value:"";}
        void Select(ComboBox box,string value){int i=0;for(;i<box.Items.Count;i++)if(((Choice)box.Items[i]).Value==value)break;box.SelectedIndex=i<box.Items.Count?i:box.Items.Count-1;box.Tag=Selected(box);}
        void UpdateStatus(){
            try{var s=core.Status();status.Text=s.Text+(core.RunningCheck()?"  ·  游戏 / Host 正在运行":"  ·  游戏 / Host 已退出");status.ForeColor=s.Kind=="enabled"?Color.FromArgb(20,115,65):s.Kind=="disabled"?Color.FromArgb(65,85,120):Color.DarkOrange;enable.Enabled=!busy&&s.CanEnable;disable.Enabled=!busy&&s.CanDisable;recover.Visible=core.HasPending;}
            catch(Exception ex){status.Text="状态需要检查："+ex.Message;status.ForeColor=Color.Firebrick;enable.Enabled=false;disable.Enabled=false;}
        }
        void LoadConfig(){
            closeAllDiagnostics=false;
            try{
                loaded=core.ReadConfig();Select(host,loaded.Get("forceX64Server")=="True"?(loaded.Get("client.testX86Server")=="True"?"x86":"x64"):"");
                Select(policy,loaded.Get("client.pageBlockRetentionPolicy"));Select(batch,loaded.Get("client.ipcBatchSize"));Select(level,loaded.Get("logLevel"));presenter.SelectedIndex=0;
                foreach(var c in checks){var values=((string[])c.Tag).Select(k=>loaded.Get(k)).ToArray();c.CheckState=values.All(v=>String.Equals(v,"True",StringComparison.OrdinalIgnoreCase))?CheckState.Checked:values.All(v=>String.Equals(v,"False",StringComparison.OrdinalIgnoreCase))?CheckState.Unchecked:CheckState.Indeterminate;c.AccessibleDescription=c.CheckState.ToString();}
                configInfo.Text="已读取 bridge.conf。方框中的横线表示混合或未显式配置；保持不动就不会改写。";save.Enabled=true;feedback.Text="设置尚未更改。开关与设置保存均需先退出游戏。";
            }catch(Exception ex){loaded=null;save.Enabled=false;configInfo.Text="读取失败："+ex.Message;}
            RefreshBackups();
        }
        Dictionary<string,string> Changes(){
            if(loaded==null)throw new InvalidOperationException("配置没有成功读取，请先检查配置文件。");
            var result=new Dictionary<string,string>(StringComparer.OrdinalIgnoreCase);
            if(Selected(host)!=(string)host.Tag&&Selected(host)!=""){result["forceX64Server"]="True";result["client.testX86Server"]=Selected(host)=="x86"?"True":"False";}
            foreach(var pair in new[]{Tuple.Create(policy,"client.pageBlockRetentionPolicy"),Tuple.Create(batch,"client.ipcBatchSize"),Tuple.Create(level,"logLevel")})if(Selected(pair.Item1)!=(string)pair.Item1.Tag&&Selected(pair.Item1)!="")result[pair.Item2]=Selected(pair.Item1);
            if(Selected(presenter)!="")foreach(var kv in Core.Presenter(Selected(presenter)=="on"))result[kv.Key]=kv.Value;
            foreach(var c in checks)if(c.CheckState!=CheckState.Indeterminate&&c.CheckState.ToString()!=c.AccessibleDescription)foreach(string key in (string[])c.Tag)result[key]=c.Checked?"True":"False";
            if(closeAllDiagnostics)foreach(string key in Core.DiagnosticKeys)result[key]="False";
            foreach(string key in result.Keys.ToArray())if(String.Equals(loaded.Get(key),result[key],StringComparison.OrdinalIgnoreCase))result.Remove(key);
            return result;
        }
        bool closeAllDiagnostics;
        void Preset(bool troubleshooting){
            closeAllDiagnostics=!troubleshooting;
            foreach(var c in checks){var keys=(string[])c.Tag;c.CheckState=troubleshooting&&(keys.Contains("client.memoryMonitoring")||keys.Contains("client.crashDiagnostics"))?CheckState.Checked:CheckState.Unchecked;}
            if(troubleshooting)level.SelectedIndex=0;
            feedback.Text=troubleshooting?"已选择内存 / 崩溃历史诊断。点击保存后，重启并复现问题，再打包日志。":"已选择关闭详细诊断。点击保存后重启生效。";
        }
        void Save(){var changes=Changes();string backup=core.SaveConfig(loaded,changes);closeAllDiagnostics=false;LoadConfig();feedback.Text=backup==null?"没有需要保存的修改。":"设置已保存并自动备份。请完整重启游戏和 Host 后再测试。";}
        void RefreshBackups(){if(backups==null)return;backups.Items.Clear();foreach(string p in core.ConfigBackups())backups.Items.Add(new Choice(p,Path.GetFileName(p)));if(backups.Items.Count>0)backups.SelectedIndex=0;}
        void Restore(){string selected=Selected(backups);if(selected=="")throw new InvalidOperationException("尚无配置备份。");if(MessageBox.Show(this,"将恢复所选备份中的完整 Bridge 配置，并先备份当前文件。继续？","恢复配置",MessageBoxButtons.OKCancel,MessageBoxIcon.Question)!=DialogResult.OK)return;core.RestoreConfig(selected,Core.HashFile(core.ConfigPath));closeAllDiagnostics=false;LoadConfig();feedback.Text="配置备份已恢复，下次完整启动生效。";}
        async void Pack(){
            if(busy)return;int range=Int32.Parse(Selected(days));busy=true;foreach(var c in actions)c.Enabled=false;feedback.Text="正在打包现有日志，请稍等……";
            try{lastZip=await Task.Run(()=>core.PackLogs(range));exportInfo.Text=lastZip;feedback.Text="日志 ZIP 已保存到本地。分享前请检查包内内容。";}
            catch(Exception ex){Error(ex);}
            finally{busy=false;foreach(var c in actions)c.Enabled=true;save.Enabled=loaded!=null;openZip.Enabled=lastZip!=null;UpdateStatus();}
        }
        bool rendering;
        protected override bool ShowWithoutActivation {get{return rendering;}}
        public void Render(string path,int tab){
            rendering=true;ShowInTaskbar=false;Opacity=0;Show();
            var tabs=Controls[0].Controls.OfType<TabControl>().Single();tabs.SelectedIndex=tab;
            PerformLayout();Application.DoEvents();
            using(var bitmap=new Bitmap(Width,Height)){DrawToBitmap(bitmap,new Rectangle(Point.Empty,Size));bitmap.Save(path,System.Drawing.Imaging.ImageFormat.Png);}Hide();
        }
    }
    static class Program {
        [STAThread] static int Main(string[] args){
            try{
                Application.EnableVisualStyles();Application.SetCompatibleTextRenderingDefault(false);
                string game=null,import=null,preview=null;bool pack=false;int days=7,tab=0;
                for(int i=0;i<args.Length;i++){switch(args[i]){case "--game-root":game=args[++i];break;case "--import-disabled":import=args[++i];break;case "--render-preview":preview=args[++i];break;case "--preview-tab":tab=Int32.Parse(args[++i]);break;case "--export-logs":pack=true;break;case "--days":days=Int32.Parse(args[++i]);break;default:throw new ArgumentException("未知参数："+args[i]);}}
                if(game==null){var d=new DirectoryInfo(AppDomain.CurrentDomain.BaseDirectory);while(d!=null&&!File.Exists(Path.Combine(d.FullName,"left4dead2.exe")))d=d.Parent;if(d!=null)game=d.FullName;else{var games=SteamSupport.Games(SteamSupport.FindSteam());if(games.Length==1)game=games[0];else{if(args.Length>0)throw new InvalidOperationException("无法唯一确认游戏目录，请指定 --game-root。");Application.EnableVisualStyles();using(var picker=new FolderBrowserDialog{Description="选择 L4D2 游戏根目录（含 left4dead2.exe 和 bin）"}){if(picker.ShowDialog()!=DialogResult.OK)return 0;game=picker.SelectedPath;}}}}
                var core=new Core(game);
                bool created;using(var mutex=new Mutex(true,"Local\\L4D2BridgeQuickTool-"+Core.HashBytes(System.Text.Encoding.UTF8.GetBytes(core.GameRoot.ToUpperInvariant())).Substring(0,16),out created)){
                    if(!created)throw new InvalidOperationException("这个游戏目录的快捷工具已经打开，请使用已有窗口。");
                    if(import!=null){core.ImportDisabled(import);return 0;}
                    if(pack){core.PackLogs(days);return 0;}
                    using(var form=new MainForm(core)){if(preview!=null){form.Render(preview,tab);return 0;}Application.Run(form);}return 0;
                }
            }catch(Exception ex){if(args.Length>0){try{Console.Error.WriteLine(ex.ToString());}catch{}return 1;}MessageBox.Show(ex.Message,"Bridge 快捷工具",MessageBoxButtons.OK,MessageBoxIcon.Error);return 1;}
        }
    }
}
