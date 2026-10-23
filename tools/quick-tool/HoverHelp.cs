// SPDX-License-Identifier: MIT
using System;
using System.Collections.Generic;
using System.Drawing;
using System.Windows.Forms;

namespace BridgeQuickTool {
    internal static class HelpText {
        static readonly Dictionary<string,string> Texts=new Dictionary<string,string> {
            {"实时日志","实时只读查看已有日志。每秒读取所选文件末尾最多 256 KiB / 2000 行，切走此页或关闭工具后停止。不会开启诊断、改写日志或上传。"},
            {"监视日志文件","选择要查看的已有日志，包括 Bridge、DXVK、L4N、Tab 和工具日志。列表按文件更新时间排序。新日志生成后点击刷新日志列表。"},
            {"刷新日志列表","重新寻找固定游戏日志目录中的 .log 文件，尽量保留当前选择。不会开启日志或删除文件。"},
            {"暂停监视","暂停本页读取与显示，游戏仍可以继续写日志。点击继续监视可查看最新末尾内容。"},
            {"继续监视","恢复每秒读取所选日志的最新末尾内容。暂停期间的内容仍在原文件，显示最多 256 KiB / 2000 行。"},
            {"定位日志文件","在资源管理器中选中当前日志文件，方便打开或复制。不会删除、清空或更改日志。"},
            {"自动滚到底部","开启后，新内容出现时滚到日志末尾。关闭后尽量保持阅读位置；要稳定查看旧内容，可暂停监视。"},
            {"日志内容","只读显示已有日志末尾最多 256 KiB / 2000 行，可选中文字并按 Ctrl+C 复制。常见口令和当前用户路径会尽力遮盖；不自动上传。日志被重建或截断后，下一次读取会显示新内容。"},
            {"监视状态","显示本次读取时间、文件最后更新时间和文件总大小。读取时间变化而文件更新时间不变，表示暂无新写入。读取失败时每秒重试。"},
            {"状态栏","显示已记录的加载入口是否就位，以及游戏 / Host 进程是否运行。切换 Bridge 后需重新启动游戏才生效。默认 D3D9 与 -vulkan 兼容入口分开管理，工具不修改 Steam 启动项。"},
            {"游戏目录","当前工具管理的 L4D2 安装目录。开关只管理已记录的 Bridge 入口和官方菜单插件；L4N 本体、自定义 Tab、Skeeto 和画质设置保持原样。"},
            {"启用 Bridge","先正常退出游戏和 Host。校验并恢复备份中的 Bridge 入口与官方菜单插件，下次启动生效。若停用期间 Host、后端或 L4N 已更新，工具会拒绝混用旧入口。"},
            {"停用 Bridge","先正常退出游戏和 Host。备份并移走已记录的 Bridge 入口及官方菜单插件，保留配置和其他插件。另一加载方式的遗留文件保留不动。"},
            {"启动游戏","通过 Steam 启动 L4D2（App 550），沿用当前 Steam 启动项。不会改参数，也不会自动保存窗口中的设置。请先保存需要的修改；游戏或 Host 已在运行、或有未完成操作时不会重复启动。"},
            {"刷新状态","重新检查入口文件和游戏 / Host 进程，不改变 Bridge 或配置。要放弃未保存的选项并重新读取文件，请用“重新读取配置”。"},
            {"恢复未完成操作","仅在上次切换意外中断时出现。退出游戏和 Host 后，按操作记录恢复到之前完整的状态；文件校验不符时不会覆盖外部新文件。"},
            {"常用设置","Host、资源策略和日志级别。选择仅保留在窗口中，点击保存后写入并自动备份；下次完整启动生效。"},
            {"日志与诊断","先按需要保存诊断选项、重启并复现，再打包已有日志。打包可以在游戏运行时执行，不自动上传。"},
            {"备份与记录","恢复完整 Bridge 配置，查看工具数据和日志包。恢复配置前需退出游戏与 Host，并会先备份当前配置。"},
            {"Host 架构","选择 Bridge 后台进程架构，游戏本身的位数不变。\n\nx64：当前默认，日常保持即可。\nx86：主要用于兼容性对照。\n保持原配置：不改这项。\n\n保存后完整重启游戏和 Host。"},
            {"内存保留策略","控制 Bridge 对 backing 数据的保留方式。\n\nLearned-aggressive：使用学习式资源管理策略，当前配置。\nKeep：偏向保留 backing 数据。\nDrop：实验性丢弃策略，不能简单理解为越激进越快。\n\n选择后需保存并完整重启。"},
            {"IPC 批次","游戏与 Host 之间每批合并的 IPC 命令数量，可选 1 / 2 / 4 / 8 / 16 / 32。当前值为 8；更大不保证帧率更高。选择后需保存并完整重启。"},
            {"静态 VB/IB 映射","改变静态顶点 / 索引缓冲影子内存的映射方式，主要用于客户端地址空间紧张时的对照排查，也可能增加映射、缺页等成本。当前关闭即可。\n\n横线表示未显式配置或混合状态；保持不动不会改写。"},
            {"ReShade / Overlay 呈现窗口","保持当前配置：不改这组设置。\n启用：保存呈现窗口及配套输入钩子设置。\n停用：关闭呈现窗口及配套输入钩子，其他 Steam 适配键保留。\n\n整个 Bridge 的启停请用顶部按钮。保存后完整重启。"},
            {"普通日志级别","Info：常规信息、警告和错误，适合日常使用。\nDebug：更多调试信息。\nWarn：主要保留警告和错误。\nError：主要保留错误。\n\n此级别与下方详细诊断开关分开。保存后重启生效。"},
            {"保存设置","只写入你改变的选项，自动备份原文件，保留其他配置项、注释、换行和编码。需先退出游戏与 Host，保存后完整重启。配置被其他程序修改时会拒绝覆盖，请先重新读取。"},
            {"重新读取配置","放弃窗口里尚未保存的选择，重新读取 bridge.conf。不会改写配置文件；执行前会让你确认。"},
            {"打开配置文件","用系统默认程序打开当前 bridge.conf，方便手工查看或编辑。手工保存后请在工具中重新读取配置。"},
            {"当前配置说明","复选框横线表示多个配置值不一致，或某个键没有显式配置。保持不动就不会改写；选择勾选或取消后，仍需点击保存。"},
            {"日志范围","按日志文件的最后修改时间选择最近 1 / 3 / 7 / 30 天，默认 7 天。不是逐行筛选这些日期内的内容；较大文件只取末尾。"},
            {"一键打包日志","打包已有日志、Bridge / DXVK 配置及版本校验信息，可在游戏运行时使用。每份日志最多取末尾 4 MiB，总计最多 64 MiB / 120 份；manifest.json 标记截断和遗漏。\n\nZIP 仅保存在本地，不自动启用诊断或上传。"},
            {"定位最近的 ZIP","在资源管理器中选中本次打开工具后生成的最新日志包。尚未打包时不可用。以前的包可从“打开日志包目录”查看。"},
            {"日志包位置","新生成 ZIP 的完整路径。会尽力遮盖常见口令和本机用户路径，但玩家名、服务器信息等可能保留；分享前请检查。包中不含转储、数据库、缓存、可执行文件正文或 Steam 用户数据。"},
            {"内存诊断（Client + Host）","采集 Client 和 Host 的内存信息，帮助排查内存增长和地址空间问题，会增加采样开销。保存并重启后开始记录，不补录之前的数据。横线表示两端不同或未显式配置。"},
            {"崩溃历史（Client + Host）","保留崩溃前相关调用 / 命令历史，辅助定位问题。它不等于完整内存转储。保存并重启后启用。横线表示两端不同或未显式配置。"},
            {"详细异常信息","开启 Bridge 的扩展异常信息，供异常和崩溃排查使用。不会防止崩溃。保存并重启后生效，日常可以关闭。"},
            {"API 等待诊断","记录调用等待情况，帮助定位 IPC / API 等待造成的卡顿，有诊断开销。保存并重启后启用，建议用于短时间复现。"},
            {"PageBlock 诊断","观察内存块的保留、回收和恢复情况。启用诊断本身不会执行一次 GC，也不会直接改变资源回收策略。保存并重启后生效。"},
            {"逐调用详细日志（开销较高）","同时开启工具支持的逐 API / 命令详细记录，日志可能增长很快，运行开销也较明显。适合短时间复现问题，日常保持关闭。保存并重启后生效。"},
            {"选择内存 / 崩溃排查模式","选中内存诊断和崩溃历史，取消其他可见诊断，普通日志选为 Info。这里只改变窗口里的选择，仍需点击保存，再重启、复现并打包。"},
            {"选择关闭全部详细诊断","把工具支持的详细诊断一起选为关闭，包括没有单独展示的数据、颜色、输入等诊断。仍需保存并重启；不会删除旧日志，也不会改变普通日志级别。"},
            {"配置备份","选择工具自动保存的完整 bridge.conf 备份。文件名使用 UTC 时间，可能与本地时间相差数小时。恢复的是整个 Bridge 配置文件。"},
            {"恢复选中的配置备份","退出游戏和 Host 后，将所选备份恢复为完整 Bridge 配置，并先备份当前文件。执行前会确认；不影响 Skeeto、Tab、画质或 Steam 启动项。"},
            {"打开工具数据目录","打开 tools\\BridgeQuickTool。state\\snapshots 保存 Bridge 入口备份，state\\config-backups 保存配置历史，state\\tool.log 保存操作记录。更新工具时应保留 state 目录。"},
            {"打开日志包目录","打开 tools\\BridgeQuickTool\\reports，查看所有已生成 ZIP。重开工具后也可在此查找以前的包。不会上传或删除日志。"},
            {"操作结果","显示读取、保存、切换和打包操作的结果。窗口内的设置选择不会自动保存；切换或保存后需完整重启游戏才应用。"}
        };
        static readonly Dictionary<string,string> Choices=new Dictionary<string,string> {
            {"Host 架构|x64","x64 Host：让 Bridge 后台使用 64 位进程，游戏本身位数不变。当前默认，日常保持即可。选择后仍需保存并完整重启。"},
            {"Host 架构|x86","x86 Host：主要用于兼容性对照，需要配套 x86 Host 和后端。选择后仍需保存并完整重启。"},
            {"内存保留策略|learned-aggressive","Learned-aggressive：使用 Bridge 的学习式资源管理策略，是当前配置。保存并完整重启后应用。"},
            {"内存保留策略|keep","Keep：偏向保留 backing 数据，是较保守的保留方式。它与显存占用或帧率不是简单的一一对应关系。保存并完整重启后应用。"},
            {"内存保留策略|drop","Drop：实验性资源丢弃策略，仍受 Bridge 的恢复和安全检查约束。更激进不保证更快；用于有目的的对照排查。保存并完整重启后应用。"},
            {"ReShade / Overlay 呈现窗口|on","启用呈现窗口：同时保存配套的窗口与输入钩子设置。不会安装 ReShade 本体，也不会更换 Host。保存并完整重启后生效。"},
            {"ReShade / Overlay 呈现窗口|off","停用呈现窗口：关闭对应窗口与输入钩子，保留其他 Steam 配置键。整个 Bridge 的启停由顶部按钮控制。保存并完整重启后生效。"},
            {"普通日志级别|Info","Info：记录常规信息、警告和错误，适合日常使用。保存并重启后应用。"},
            {"普通日志级别|Debug","Debug：输出更多调试信息，可能增加日志量和开销。详细诊断仍由各自的复选框单独控制。保存并重启后应用。"},
            {"普通日志级别|Warn","Warn：主要保留警告和错误，普通信息会减少。详细诊断由各自开关控制。保存并重启后应用。"},
            {"普通日志级别|Error","Error：主要保留错误，常规信息会减少，排查材料也可能变少。详细诊断由各自开关控制。保存并重启后应用。"}
        };
        internal static string For(string key){string text;return Texts.TryGetValue(key,out text)?text:"";}
        internal static string ForChoice(string key,Choice choice){
            string text;if(Choices.TryGetValue(key+"|"+choice.Value,out text))return text;
            if(choice.Value=="")return "保持原配置：不为这项生成修改。其他选项仍按各自的保存规则处理。";
            if(key=="IPC 批次")return "IPC 批次 "+choice.Value+"：每批合并的命令数量。当前常用值是 8，更大不保证更快。选择后仍需保存并完整重启。";
            if(key=="日志范围")return "最近 "+choice.Value+" 天：按日志文件最后修改时间选择。大文件仍只取末尾，不会逐行筛选日期。仅影响这次打包。";
            return For(key);
        }
    }
    internal sealed class HoverHelp : IDisposable {
        readonly Form owner;
        readonly ToolTip tips=new ToolTip {InitialDelay=450,ReshowDelay=100,AutoPopDelay=30000,ShowAlways=true,OwnerDraw=true};
        readonly Timer delay=new Timer {Interval=450};
        string manualText;
        Control manualControl;
        ComboBox combo;
        int item=-1;
        bool disabled;
        Point position;
        public HoverHelp(Form owner){
            this.owner=owner;
            tips.Popup+=(s,e)=>{string text=e.AssociatedControl==owner?manualText:tips.GetToolTip(e.AssociatedControl);e.ToolTipSize=Measure(text??"");};
            tips.Draw+=(s,e)=>{e.DrawBackground();e.DrawBorder();var box=Rectangle.Inflate(e.Bounds,-12,-10);TextRenderer.DrawText(e.Graphics,e.ToolTipText,owner.Font,box,Color.FromArgb(35,45,55),TextFormatFlags.WordBreak|TextFormatFlags.NoPrefix);};
            delay.Tick+=(s,e)=>{delay.Stop();if(manualControl==null||manualControl.IsDisposed||(!disabled&&(combo==null||!combo.DroppedDown)))return;tips.Show(manualText,owner,position,30000);};
            owner.Disposed+=(s,e)=>Dispose();
        }
        Size Measure(string text){var size=TextRenderer.MeasureText(text,owner.Font,new Size(430,Int32.MaxValue),TextFormatFlags.WordBreak|TextFormatFlags.NoPrefix);return new Size(Math.Min(454,size.Width+24),size.Height+20);}
        internal string TextFor(Control control){return tips.GetToolTip(control);}
        internal void Add(Control control,string key){AddText(control,HelpText.For(key));}
        internal void AddText(Control control,string text){if(String.IsNullOrEmpty(text))return;tips.SetToolTip(control,text);if(String.IsNullOrEmpty(control.AccessibleName))control.AccessibleName=control.Text;}
        void Cancel(){delay.Stop();tips.Hide(owner);manualControl=null;combo=null;item=-1;disabled=false;manualText=null;}
        void Queue(Control control,string text,int index,bool isDisabled){
            if(manualControl==control&&item==index&&disabled==isDisabled)return;
            Cancel();manualControl=control;combo=control as ComboBox;item=index;disabled=isDisabled;manualText=text;
            tips.Hide(control);var size=Measure(text);Rectangle screen=Screen.FromControl(owner).WorkingArea;
            Point location=control.PointToScreen(new Point(isDisabled?10:control.Width+10,isDisabled?control.Height+8:0));
            if(location.X+size.Width>screen.Right)location.X=Math.Max(screen.Left,control.PointToScreen(Point.Empty).X-size.Width-10);
            location.Y=Math.Max(screen.Top,Math.Min(location.Y,screen.Bottom-size.Height));position=owner.PointToClient(location);delay.Start();
        }
        internal void Combo(ComboBox box,string key){
            Add(box,key);box.DrawMode=DrawMode.OwnerDrawFixed;box.ItemHeight=Math.Max(24,box.Font.Height+6);
            box.DrawItem+=(s,e)=>{
                e.DrawBackground();if(e.Index<0)return;var choice=box.Items[e.Index] as Choice;
                string text=choice==null?box.Items[e.Index].ToString():choice.Text;
                var bounds=e.Bounds;bounds.X+=3;bounds.Width-=6;
                TextRenderer.DrawText(e.Graphics,text,box.Font,bounds,e.ForeColor,TextFormatFlags.VerticalCenter|TextFormatFlags.EndEllipsis|TextFormatFlags.NoPrefix);
                e.DrawFocusRectangle();
                if(box.DroppedDown&&(e.State&DrawItemState.Selected)!=0&&choice!=null)Queue(box,HelpText.ForChoice(key,choice),e.Index,false);
            };
            box.DropDownClosed+=(s,e)=>{if(manualControl==box)Cancel();};
        }
        internal void DisabledControls(Control root){
            // Disabled WinForms controls do not receive mouse events. Their
            // container supplies the same help without enabling the action.
            if(root.HasChildren){
                root.MouseMove+=(s,e)=>{Control child=root.GetChildAtPoint(e.Location);while(child!=null&&child.HasChildren){var next=child.GetChildAtPoint(child.PointToClient(root.PointToScreen(e.Location)));if(next==null)break;child=next;}
                    if(child!=null&&!child.Enabled&&!String.IsNullOrEmpty(tips.GetToolTip(child)))Queue(child,tips.GetToolTip(child),-1,true);
                    else if(disabled)Cancel();};
                root.MouseLeave+=(s,e)=>{if(disabled)Cancel();};
                foreach(Control child in root.Controls)DisabledControls(child);
            }
        }
        public void Dispose(){delay.Dispose();tips.Dispose();}
    }
}
