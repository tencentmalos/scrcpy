using System;
using System.Diagnostics;
using System.Windows.Forms;
using System.Threading;
using System.Threading.Tasks;
using System.Collections.Generic;
using System.IO;

namespace ScrcpyDotNet
{
    public partial class ScrcpyForm : Form
    {
        private List<string> deviceSerials = new List<string>();
        private CancellationTokenSource cancellationTokenSource;
        private TextBox scrcpyPathTextBox;

        public ScrcpyForm()
        {
            InitializeComponent();
            LoadDevices();
        }

        private void InitializeComponent()
        {
            this.Text = "Scrcpy .NET 示例";
            this.Width = 500;
            this.Height = 450; // 增加高度以容纳路径输入框
            this.FormBorderStyle = FormBorderStyle.FixedDialog;
            this.MaximizeBox = false;
            this.StartPosition = FormStartPosition.CenterScreen;

            // scrcpy库路径输入
            Label scrcpyPathLabel = new Label();
            scrcpyPathLabel.Text = "scrcpy库路径:";
            scrcpyPathLabel.Location = new System.Drawing.Point(10, 10);
            scrcpyPathLabel.AutoSize = true;
            this.Controls.Add(scrcpyPathLabel);

            scrcpyPathTextBox = new TextBox();
            scrcpyPathTextBox.Location = new System.Drawing.Point(120, 10);
            scrcpyPathTextBox.Width = 280;
            this.Controls.Add(scrcpyPathTextBox);

            Button browseButton = new Button();
            browseButton.Text = "浏览...";
            browseButton.Location = new System.Drawing.Point(410, 8);
            browseButton.Click += (sender, e) =>
            {
                OpenFileDialog openFileDialog = new OpenFileDialog();
                if (RuntimeInformation.IsOSPlatform(OSPlatform.Windows))
                {
                    openFileDialog.Filter = "DLL 文件 (*.dll)|*.dll|所有文件 (*.*)|*.*";
                }
                else if (RuntimeInformation.IsOSPlatform(OSPlatform.OSX))
                {
                    openFileDialog.Filter = "dylib 文件 (*.dylib)|*.dylib|所有文件 (*.*)|*.*";
                }
                else
                {
                    openFileDialog.Filter = "SO 文件 (*.so)|*.so|所有文件 (*.*)|*.*";
                }

                if (openFileDialog.ShowDialog() == DialogResult.OK)
                {
                    scrcpyPathTextBox.Text = openFileDialog.FileName;
                }
            };
            this.Controls.Add(browseButton);

            // 设备列表
            ListBox deviceListBox = new ListBox();
            deviceListBox.Location = new System.Drawing.Point(10, 40);
            deviceListBox.Width = 460;
            deviceListBox.Height = 150;
            this.Controls.Add(deviceListBox);

            // 刷新设备按钮
            Button refreshButton = new Button();
            refreshButton.Text = "刷新设备列表";
            refreshButton.Location = new System.Drawing.Point(10, 200);
            refreshButton.Width = 460;
            refreshButton.Click += (sender, e) => LoadDevices();
            this.Controls.Add(refreshButton);

            // 参数面板
            Panel paramPanel = new Panel();
            paramPanel.Location = new System.Drawing.Point(10, 230);
            paramPanel.Width = 460;
            paramPanel.Height = 120;
            this.Controls.Add(paramPanel);

            // 显示触摸点选项
            CheckBox showTouchesCheckBox = new CheckBox();
            showTouchesCheckBox.Text = "显示触摸点";
            showTouchesCheckBox.Location = new System.Drawing.Point(10, 10);
            paramPanel.Controls.Add(showTouchesCheckBox);

            // 全屏选项
            CheckBox fullscreenCheckBox = new CheckBox();
            fullscreenCheckBox.Text = "全屏显示";
            fullscreenCheckBox.Location = new System.Drawing.Point(10, 40);
            paramPanel.Controls.Add(fullscreenCheckBox);

            // 关闭设备屏幕选项
            CheckBox turnScreenOffCheckBox = new CheckBox();
            turnScreenOffCheckBox.Text = "关闭设备屏幕";
            turnScreenOffCheckBox.Location = new System.Drawing.Point(10, 70);
            paramPanel.Controls.Add(turnScreenOffCheckBox);

            // 最大尺寸输入
            Label maxSizeLabel = new Label();
            maxSizeLabel.Text = "最大尺寸:";
            maxSizeLabel.Location = new System.Drawing.Point(190, 10);
            paramPanel.Controls.Add(maxSizeLabel);

            TextBox maxSizeTextBox = new TextBox();
            maxSizeTextBox.Text = "0";
            maxSizeTextBox.Location = new System.Drawing.Point(270, 10);
            maxSizeTextBox.Width = 50;
            paramPanel.Controls.Add(maxSizeTextBox);

            // 比特率输入
            Label bitRateLabel = new Label();
            bitRateLabel.Text = "比特率:";
            bitRateLabel.Location = new System.Drawing.Point(190, 40);
            paramPanel.Controls.Add(bitRateLabel);

            TextBox bitRateTextBox = new TextBox();
            bitRateTextBox.Text = "8000000";
            bitRateTextBox.Location = new System.Drawing.Point(270, 40);
            bitRateTextBox.Width = 80;
            paramPanel.Controls.Add(bitRateTextBox);

            // 启动按钮
            Button startButton = new Button();
            startButton.Text = "启动镜像";
            startButton.Location = new System.Drawing.Point(10, 360);
            startButton.Width = 225;
            startButton.Height = 40;
            startButton.Click += async (sender, e) => 
            {
                if (deviceListBox.SelectedIndex < 0)
                {
                    MessageBox.Show("请选择一个设备");
                    return;
                }

                string scrcpyPath = scrcpyPathTextBox.Text;
                if (!string.IsNullOrEmpty(scrcpyPath) && !File.Exists(scrcpyPath))
                {
                    MessageBox.Show("指定的scrcpy库路径无效");
                    return;
                }

                try
                {
                    Scrcpy.Initialize(string.IsNullOrEmpty(scrcpyPath) ? null : scrcpyPath);
                }
                catch (Exception ex)
                {
                    MessageBox.Show($"初始化scrcpy库失败: {ex.Message}");
                    return;
                }

                string serial = deviceSerials[deviceListBox.SelectedIndex];
                bool showTouches = showTouchesCheckBox.Checked;
                bool fullscreen = fullscreenCheckBox.Checked;
                bool turnScreenOff = turnScreenOffCheckBox.Checked;
                
                ushort maxSize = 0;
                if (ushort.TryParse(maxSizeTextBox.Text, out ushort parsedMaxSize))
                {
                    maxSize = parsedMaxSize;
                }

                uint bitRate = 8000000;
                if (uint.TryParse(bitRateTextBox.Text, out uint parsedBitRate))
                {
                    bitRate = parsedBitRate;
                }

                await StartScrcpy(serial, showTouches, fullscreen, turnScreenOff, maxSize, bitRate);
            };
            this.Controls.Add(startButton);

            // 停止按钮
            Button stopButton = new Button();
            stopButton.Text = "停止镜像";
            stopButton.Location = new System.Drawing.Point(245, 360);
            stopButton.Width = 225;
            stopButton.Height = 40;
            stopButton.Click += (sender, e) => 
            {
                StopScrcpy();
            };
            this.Controls.Add(stopButton);
        }

        private void LoadDevices()
        {
            deviceSerials.Clear();
            ListBox deviceListBox = (ListBox)this.Controls[3]; // 根据控件添加顺序调整索引
            deviceListBox.Items.Clear();

            try
            {
                Process process = new Process();
                process.StartInfo.FileName = "adb";
                process.StartInfo.Arguments = "devices";
                process.StartInfo.UseShellExecute = false;
                process.StartInfo.RedirectStandardOutput = true;
                process.StartInfo.CreateNoWindow = true;
                process.Start();

                string output = process.StandardOutput.ReadToEnd();
                process.WaitForExit();

                string[] lines = output.Split(new[] { '\r', '\n' }, StringSplitOptions.RemoveEmptyEntries);
                
                for (int i = 1; i < lines.Length; i++) // 跳过第一行（标题行）
                {
                    string line = lines[i].Trim();
                    if (string.IsNullOrEmpty(line)) continue;

                    string[] parts = line.Split(new[] { '\t', ' ' }, StringSplitOptions.RemoveEmptyEntries);
                    if (parts.Length >= 2)
                    {
                        string serial = parts[0];
                        string state = parts[1];
                        
                        if (state == "device") // 只添加已连接的设备
                        {
                            deviceSerials.Add(serial);
                            deviceListBox.Items.Add($"{serial}");
                        }
                    }
                }
            }
            catch (Exception ex)
            {
                MessageBox.Show($"获取设备列表失败: {ex.Message}");
            }
        }

        private async Task StartScrcpy(string serial, bool showTouches, bool fullscreen, bool turnScreenOff, ushort maxSize, uint bitRate)
        {
            // 取消之前的任务（如果有）
            if (cancellationTokenSource != null)
            {
                cancellationTokenSource.Cancel();
                cancellationTokenSource.Dispose();
            }

            cancellationTokenSource = new CancellationTokenSource();
            var token = cancellationTokenSource.Token;

            try
            {
                await Task.Run(() =>
                {
                    ScrcpyExitCode result = Scrcpy.Run(
                        serial: serial,
                        window: true,
                        control: true,
                        maxSize: maxSize,
                        bitRate: bitRate,
                        maxFps: 0,
                        windowWidth: 0,
                        windowHeight: 0,
                        fullscreen: fullscreen,
                        showTouches: showTouches,
                        stayAwake: true,
                        turnScreenOff: turnScreenOff,
                        recordScreen: false,
                        recordFilename: null);

                    if (!token.IsCancellationRequested)
                    {
                        this.Invoke(new Action(() =>
                        {
                            MessageBox.Show($"scrcpy已退出，退出代码: {result}");
                        }));
                    }
                }, token);
            }
            catch (OperationCanceledException)
            {
                // 任务被取消，不需要处理
            }
            catch (Exception ex)
            {
                MessageBox.Show($"启动scrcpy失败: {ex.Message}");
            }
        }

        private void StopScrcpy()
        {
            if (cancellationTokenSource != null)
            {
                try
                {
                    Scrcpy.RequestExit();
                }
                catch (Exception ex)
                {
                    // 忽略退出请求时的错误，例如库未初始化
                    Console.WriteLine($"请求退出scrcpy时发生错误: {ex.Message}");
                }
                cancellationTokenSource.Cancel();
            }
        }

        protected override void OnFormClosing(FormClosingEventArgs e)
        {
            StopScrcpy();
            Scrcpy.Cleanup(); // 清理scrcpy库资源
            base.OnFormClosing(e);
        }
    }

    public class Program
    {
        [STAThread]
        public static void Main()
        {
            Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault(false);
            Application.Run(new ScrcpyForm());
        }
    }
}
