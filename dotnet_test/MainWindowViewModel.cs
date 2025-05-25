using System;
using System.ComponentModel;
using System.Diagnostics;
using System.Runtime.CompilerServices;
using System.Threading;
using System.Threading.Tasks;
using System.Windows.Input;
using Avalonia.Media;
using Avalonia.Media.Imaging;
using Avalonia.Threading;

namespace ScrcpyImageViewer.ViewModels
{
    public class MainWindowViewModel : INotifyPropertyChanged
    {
        private FileMappingReader? _reader;
        private CancellationTokenSource? _cancellationTokenSource;
        private bool _isConnected;
        private string _mappingName = "frames_8888";
        private string _statusText = "未连接";
        private IBrush _statusColor = Brushes.Red;
        private Bitmap? _currentImage;
        private string _frameInfo = "无帧信息";
        private string _fpsInfo = "FPS: 0";
        
        private readonly Stopwatch _fpsStopwatch = new();
        private int _frameCount;
        private DateTime _lastFpsUpdate = DateTime.Now;

        public event PropertyChangedEventHandler? PropertyChanged;

        public string MappingName
        {
            get => _mappingName;
            set
            {
                if (_mappingName != value)
                {
                    _mappingName = value;
                    OnPropertyChanged();
                }
            }
        }

        public string StatusText
        {
            get => _statusText;
            private set
            {
                if (_statusText != value)
                {
                    _statusText = value;
                    OnPropertyChanged();
                }
            }
        }

        public IBrush StatusColor
        {
            get => _statusColor;
            private set
            {
                if (_statusColor != value)
                {
                    _statusColor = value;
                    OnPropertyChanged();
                }
            }
        }

        public string ConnectButtonText => _isConnected ? "断开连接" : "连接";

        public Bitmap? CurrentImage
        {
            get => _currentImage;
            private set
            {
                if (_currentImage != value)
                {
                    _currentImage = value;
                    OnPropertyChanged();
                }
            }
        }

        public string FrameInfo
        {
            get => _frameInfo;
            private set
            {
                if (_frameInfo != value)
                {
                    _frameInfo = value;
                    OnPropertyChanged();
                }
            }
        }

        public string FpsInfo
        {
            get => _fpsInfo;
            private set
            {
                if (_fpsInfo != value)
                {
                    _fpsInfo = value;
                    OnPropertyChanged();
                }
            }
        }

        public ICommand ToggleConnectionCommand { get; }

        public MainWindowViewModel()
        {
            ToggleConnectionCommand = new RelayCommand(ToggleConnection);
        }

        private async void ToggleConnection()
        {
            if (_isConnected)
            {
                await DisconnectAsync();
            }
            else
            {
                await ConnectAsync();
            }
        }

        private async Task ConnectAsync()
        {
            try
            {
                StatusText = "正在连接...";
                StatusColor = Brushes.Orange;

                _reader = new FileMappingReader(_mappingName, 1920 * 1080 * 4); // RGBA
                
                if (!_reader.Initialize())
                {
                    StatusText = "连接失败 - 文件映射不存在";
                    StatusColor = Brushes.Red;
                    _reader?.Dispose();
                    _reader = null;
                    return;
                }

                _reader.FrameReceived += OnFrameReceived;
                _cancellationTokenSource = new CancellationTokenSource();
                
                // 启动监控任务
                _ = Task.Run(() => _reader.StartMonitoring(_cancellationTokenSource.Token));
                
                _isConnected = true;
                StatusText = "已连接";
                StatusColor = Brushes.Green;
                
                _fpsStopwatch.Start();
                
                OnPropertyChanged(nameof(ConnectButtonText));
            }
            catch (Exception ex)
            {
                StatusText = $"连接错误: {ex.Message}";
                StatusColor = Brushes.Red;
                _reader?.Dispose();
                _reader = null;
            }
        }

        private async Task DisconnectAsync()
        {
            try
            {
                _cancellationTokenSource?.Cancel();
                _reader?.Dispose();
                
                _isConnected = false;
                StatusText = "未连接";
                StatusColor = Brushes.Red;
                CurrentImage = null;
                FrameInfo = "无帧信息";
                FpsInfo = "FPS: 0";
                
                _fpsStopwatch.Stop();
                _fpsStopwatch.Reset();
                _frameCount = 0;
                
                OnPropertyChanged(nameof(ConnectButtonText));
            }
            catch (Exception ex)
            {
                StatusText = $"断开连接错误: {ex.Message}";
                StatusColor = Brushes.Red;
            }
        }

        private void OnFrameReceived(FrameData frameData)
        {
            try
            {
                // 在 UI 线程上更新图像
                Dispatcher.UIThread.Post(() =>
                {
                    UpdateImage(frameData);
                    UpdateFrameInfo(frameData);
                    UpdateFpsInfo();
                });
            }
            catch (Exception ex)
            {
                Console.WriteLine($"处理帧数据时出错: {ex.Message}");
            }
        }

        private void UpdateImage(FrameData frameData)
        {
            try
            {
                // 格式 2 = RGBA
                if (frameData.Header.Format == 2)
                {
                    var bitmap = CreateBitmapFromRgba(
                        frameData.ImageData,
                        (int)frameData.Header.Width,
                        (int)frameData.Header.Height);
                    
                    CurrentImage = bitmap;
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine($"更新图像时出错: {ex.Message}");
            }
        }

        private Bitmap CreateBitmapFromRgba(byte[] rgbaData, int width, int height)
        {
            // 在 macOS 上，SDL 读取的是 ABGR 格式，需要转换为 BGRA
            var bgraData = new byte[width * height * 4];
            
            for (int i = 0; i < width * height; i++)
            {
                int srcIndex = i * 4;
                int dstIndex = i * 4;
                
                // ABGR -> BGRA
                bgraData[dstIndex] = rgbaData[srcIndex + 1];     // B
                bgraData[dstIndex + 1] = rgbaData[srcIndex + 2]; // G
                bgraData[dstIndex + 2] = rgbaData[srcIndex + 3]; // R
                bgraData[dstIndex + 3] = rgbaData[srcIndex];     // A
            }
            
            return new Bitmap(
                Avalonia.Platform.PixelFormat.Bgra8888,
                Avalonia.Platform.AlphaFormat.Unpremul,
                IntPtr.Zero,
                new Avalonia.PixelSize(width, height),
                new Avalonia.Vector(96, 96),
                bgraData);
        }

        private void UpdateFrameInfo(FrameData frameData)
        {
            FrameInfo = $"分辨率: {frameData.Header.Width}x{frameData.Header.Height}, " +
                       $"序列: {frameData.Header.Sequence}, " +
                       $"大小: {frameData.Header.FrameSize} bytes";
        }

        private void UpdateFpsInfo()
        {
            _frameCount++;
            
            var now = DateTime.Now;
            if ((now - _lastFpsUpdate).TotalSeconds >= 1.0)
            {
                var fps = _frameCount / (now - _lastFpsUpdate).TotalSeconds;
                FpsInfo = $"FPS: {fps:F1}";
                
                _frameCount = 0;
                _lastFpsUpdate = now;
            }
        }

        protected virtual void OnPropertyChanged([CallerMemberName] string? propertyName = null)
        {
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(propertyName));
        }
    }

    // 简单的命令实现
    public class RelayCommand : ICommand
    {
        private readonly Action _execute;
        private readonly Func<bool>? _canExecute;

        public RelayCommand(Action execute, Func<bool>? canExecute = null)
        {
            _execute = execute ?? throw new ArgumentNullException(nameof(execute));
            _canExecute = canExecute;
        }

        public event EventHandler? CanExecuteChanged;

        public bool CanExecute(object? parameter) => _canExecute?.Invoke() ?? true;

        public void Execute(object? parameter) => _execute();

        public void RaiseCanExecuteChanged() => CanExecuteChanged?.Invoke(this, EventArgs.Empty);
    }
}
