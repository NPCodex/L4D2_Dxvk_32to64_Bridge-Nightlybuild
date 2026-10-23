// SPDX-License-Identifier: MIT
using System;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace BridgeQuickTool {
    public sealed class LogSnapshot {
        public string Text;
        public long Length;
        public DateTime ModifiedUtc;
        public int CapturedBytes;
    }
    public static class LogTail {
        public const int MaxBytes=256*1024;
        public static LogSnapshot Read(Core core,string path) {
            Core.Guard(path,core.GameRoot);
            if(!path.EndsWith(".log",StringComparison.OrdinalIgnoreCase))throw new IOException("请选择日志文件。");
            byte[] prefix=new byte[4],data;long length,start;int got=0;
            using(var stream=new FileStream(path,FileMode.Open,FileAccess.Read,FileShare.ReadWrite|FileShare.Delete)){
                length=stream.Length;stream.Read(prefix,0,4);start=Math.Max(0,length-MaxBytes);
                if(start>0&&(prefix[0]==255&&prefix[1]==254||prefix[0]==254&&prefix[1]==255)&&(start&1)!=0)start++;
                data=new byte[(int)Math.Min(MaxBytes,length-start)];stream.Position=start;
                while(got<data.Length){int n=stream.Read(data,got,data.Length-got);if(n==0)break;got+=n;}
            }
            if(got<data.Length)Array.Resize(ref data,got);
            string text=Core.DecodeLog(data,prefix,start>0).Replace("\0","");
            var lines=text.Split('\n');bool limited=lines.Length>2000;
            if(limited)text=String.Join("\n",lines.Skip(lines.Length-2000));
            if(start>0||limited)text="[仅显示末尾最多 256 KiB / 2000 行，原日志未修改]\r\n"+text;
            return new LogSnapshot{Text=core.Redact(text),Length=length,ModifiedUtc=File.GetLastWriteTimeUtc(path),CapturedBytes=got};
        }
    }
    internal sealed class LogMonitor : UserControl {
        readonly Core core;
        readonly ComboBox files;
        readonly RichTextBox output;
        readonly Label info;
        readonly Button pause;
        readonly CheckBox follow;
        readonly Timer timer=new Timer{Interval=1000};
        bool paused,reading,refreshing,active;
        int generation;
        string shown;
        [StructLayout(LayoutKind.Sequential)] struct ScrollPoint { public int X,Y; }
        [DllImport("user32.dll",CharSet=CharSet.Auto)] static extern IntPtr SendMessage(IntPtr window,int message,IntPtr unused,ref ScrollPoint point);
        public LogMonitor(Core service,HoverHelp help){
            core=service;Dock=DockStyle.Fill;
            var grid=new TableLayoutPanel{Dock=DockStyle.Fill,ColumnCount=1,RowCount=5};
            grid.RowStyles.Add(new RowStyle(SizeType.Absolute,48));grid.RowStyles.Add(new RowStyle(SizeType.Absolute,40));grid.RowStyles.Add(new RowStyle(SizeType.Absolute,44));grid.RowStyles.Add(new RowStyle(SizeType.Percent,100));grid.RowStyles.Add(new RowStyle(SizeType.Absolute,44));Controls.Add(grid);
            var note=new Label{Dock=DockStyle.Fill,Text="实时只读查看已有日志，每秒检查一次。切到其他页或关闭工具后停止刷新；不会自动开启诊断。",ForeColor=Color.DimGray};grid.Controls.Add(note,0,0);help.AddText(note,note.Text);
            files=new ComboBox{Dock=DockStyle.Fill,DropDownStyle=ComboBoxStyle.DropDownList};grid.Controls.Add(files,0,1);help.Combo(files,"监视日志文件");
            var buttons=new FlowLayoutPanel{Dock=DockStyle.Fill,WrapContents=false};grid.Controls.Add(buttons,0,2);
            Add(buttons,"刷新日志列表",RefreshFiles,help);
            pause=Add(buttons,"暂停监视",()=>{paused=!paused;pause.Text=paused?"继续监视":"暂停监视";help.Add(pause,pause.Text);Watching();},help);
            Add(buttons,"定位日志文件",()=>{string path=Selected();if(path==null||!File.Exists(path))throw new IOException("当前没有可定位的日志。");Core.Guard(path,core.GameRoot);System.Diagnostics.Process.Start("explorer.exe","/select,\""+path+"\"");},help);
            follow=new CheckBox{Text="自动滚到底部",Checked=true,AutoSize=true,Margin=new Padding(8,7,0,0)};buttons.Controls.Add(follow);help.Add(follow,follow.Text);
            output=new RichTextBox{Dock=DockStyle.Fill,ReadOnly=true,WordWrap=false,DetectUrls=false,Font=new Font("Consolas",10),BackColor=Color.FromArgb(25,29,36),ForeColor=Color.FromArgb(222,228,237),BorderStyle=BorderStyle.FixedSingle,HideSelection=false};grid.Controls.Add(output,0,3);help.Add(output,"日志内容");
            info=new Label{Dock=DockStyle.Fill,ForeColor=Color.DimGray,Padding=new Padding(0,6,0,0)};grid.Controls.Add(info,0,4);help.Add(info,"监视状态");
            files.SelectedIndexChanged+=(s,e)=>{generation++;shown=null;output.Clear();if(!refreshing)Poll();};
            timer.Tick+=(s,e)=>Poll();VisibleChanged+=(s,e)=>Watching();
            RefreshFiles();
        }
        Button Add(Control parent,string text,Action action,HoverHelp help){var b=new Button{Text=text,AutoSize=true,Height=32,Margin=new Padding(0,3,8,0)};b.Click+=(s,e)=>{try{action();}catch(Exception ex){info.Text="操作未完成："+ex.Message;}};parent.Controls.Add(b);help.Add(b,text);return b;}
        string Selected(){var choice=files.SelectedItem as Choice;return choice==null?null:choice.Value;}
        void RefreshFiles(){
            string selected=Selected();refreshing=true;
            try{var paths=core.LogCandidates().ToArray();files.Items.Clear();foreach(string p in paths)files.Items.Add(new Choice(p,p.Substring(core.GameRoot.Length+1)));int index=Array.FindIndex(paths,p=>String.Equals(p,selected,StringComparison.OrdinalIgnoreCase));if(paths.Length>0)files.SelectedIndex=index<0?0:index;else info.Text="尚无日志文件。游戏产生日志后，请刷新列表。";}
            catch(Exception ex){info.Text="读取列表失败："+ex.Message;}
            finally{refreshing=false;generation++;shown=null;Poll();}
        }
        internal void SetActive(bool value){active=value;Watching();}
        void Watching(){timer.Enabled=active&&Visible&&!paused;if(timer.Enabled)Poll();else info.Text=paused?"监视已暂停，原日志仍可继续写入。":"切回此页后继续监视。";}
        async void Poll(){
            if(reading||paused||!active||!Visible||IsDisposed)return;
            string path=Selected();if(path==null)return;reading=true;int request=generation;
            try{
                var snapshot=await Task.Run(()=>LogTail.Read(core,path));
                if(IsDisposed||request!=generation||paused||!active||!Visible)return;
                if(shown!=snapshot.Text){
                    var point=new ScrollPoint();int selection=output.SelectionStart,length=output.SelectionLength;
                    if(!follow.Checked)SendMessage(output.Handle,0x04dd,IntPtr.Zero,ref point);
                    output.Text=snapshot.Text;shown=snapshot.Text;
                    if(follow.Checked){output.Select(output.TextLength,0);output.ScrollToCaret();}
                    else{output.Select(Math.Min(selection,output.TextLength),Math.Min(length,Math.Max(0,output.TextLength-selection)));SendMessage(output.Handle,0x04de,IntPtr.Zero,ref point);}
                }
                info.Text="上次读取 "+DateTime.Now.ToString("HH:mm:ss")+"  ·  文件更新 "+snapshot.ModifiedUtc.ToLocalTime().ToString("MM-dd HH:mm:ss")+"  ·  "+snapshot.Length.ToString("N0")+" 字节";
            }catch(Exception ex){if(!IsDisposed&&request==generation)info.Text="暂时无法读取，稍后重试："+ex.Message;}
            finally{reading=false;}
        }
        protected override void Dispose(bool disposing){if(disposing){generation++;timer.Dispose();}base.Dispose(disposing);}
    }
}
