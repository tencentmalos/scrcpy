using System;
using System.Runtime.InteropServices;

namespace ScrcpyDotNet
{
    /// <summary>
    /// scrcpy退出代码
    /// </summary>
    public enum ScrcpyExitCode
    {
        /// <summary>
        /// 正常程序终止
        /// </summary>
        Success = 0,

        /// <summary>
        /// 无法建立连接
        /// </summary>
        Failure = 1,

        /// <summary>
        /// 运行时设备断开连接
        /// </summary>
        Disconnected = 2
    }

    /// <summary>
    /// scrcpy选项结构体，与C结构体布局匹配
    /// </summary>
    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
    public struct ScrcpyOptions
    {
        // 这里只列出了部分字段，完整的结构体定义应该与C结构体完全匹配
        // 实际使用时，建议使用简化的接口而不是直接操作此结构体
        public IntPtr serial;
        public IntPtr crop;
        public IntPtr crop_region2;
        public IntPtr record_filename;
        public IntPtr window_title;
        public IntPtr push_target;
        public IntPtr render_driver;
        public IntPtr video_codec_options;
        public IntPtr audio_codec_options;
        public IntPtr video_encoder;
        public IntPtr audio_encoder;
        public IntPtr camera_id;
        public IntPtr camera_size;
        public IntPtr camera_ar;
        public ushort camera_fps;
        // ... 其他字段省略
    }

    /// <summary>
    /// scrcpy .NET 包装类
    /// </summary>
    public static class Scrcpy
    {
        // 函数委托定义
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private delegate ScrcpyExitCode RunWithParamsDelegate(
            [MarshalAs(UnmanagedType.LPStr)] string serial,
            [MarshalAs(UnmanagedType.I1)] bool window,
            [MarshalAs(UnmanagedType.I1)] bool control,
            ushort max_size,
            uint bit_rate,
            ushort max_fps,
            ushort window_width,
            ushort window_height,
            [MarshalAs(UnmanagedType.I1)] bool fullscreen,
            [MarshalAs(UnmanagedType.I1)] bool show_touches,
            [MarshalAs(UnmanagedType.I1)] bool stay_awake,
            [MarshalAs(UnmanagedType.I1)] bool turn_screen_off,
            [MarshalAs(UnmanagedType.I1)] bool record_screen,
            [MarshalAs(UnmanagedType.LPStr)] string record_filename);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private delegate ScrcpyExitCode RunWithOptionsDelegate(ref ScrcpyOptions options);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private delegate void RequestExitDelegate();

        // 函数指针
        private static RunWithParamsDelegate _runWithParams;
        private static RunWithOptionsDelegate _runWithOptions;
        private static RequestExitDelegate _requestExit;

        // 动态库句柄
        private static IntPtr _libraryHandle = IntPtr.Zero;

        // 获取平台特定的动态库名称
        private static string GetPlatformSpecificLibraryName()
        {
            if (RuntimeInformation.IsOSPlatform(OSPlatform.Windows))
            {
                return "scrcpy.dll";
            }
            else if (RuntimeInformation.IsOSPlatform(OSPlatform.OSX))
            {
                return "libscrcpy.dylib";
            }
            else if (RuntimeInformation.IsOSPlatform(OSPlatform.Linux))
            {
                return "libscrcpy.so";
            }
            else
            {
                throw new PlatformNotSupportedException("不支持的操作系统平台");
            }
        }

        /// <summary>
        /// 初始化scrcpy库
        /// </summary>
        /// <param name="libraryPath">scrcpy库的路径，如果为null则使用默认路径</param>
        /// <returns>是否成功初始化</returns>
        public static bool Initialize(string libraryPath = null)
        {
            // 如果已经初始化，先释放
            if (_libraryHandle != IntPtr.Zero)
            {
                NativeLibrary.Free(_libraryHandle);
                _libraryHandle = IntPtr.Zero;
                _runWithParams = null;
                _runWithOptions = null;
                _requestExit = null;
            }

            // 如果未指定路径，使用默认路径
            string libPath = libraryPath ?? GetPlatformSpecificLibraryName();

            try
            {
                // 加载库
                _libraryHandle = NativeLibrary.Load(libPath);

                // 获取函数指针
                IntPtr procAddress = NativeLibrary.GetExport(_libraryHandle, "sc_run_with_params");
                _runWithParams = Marshal.GetDelegateForFunctionPointer<RunWithParamsDelegate>(procAddress);

                procAddress = NativeLibrary.GetExport(_libraryHandle, "sc_run_with_options");
                _runWithOptions = Marshal.GetDelegateForFunctionPointer<RunWithOptionsDelegate>(procAddress);

                procAddress = NativeLibrary.GetExport(_libraryHandle, "sc_request_exit");
                _requestExit = Marshal.GetDelegateForFunctionPointer<RequestExitDelegate>(procAddress);

                return true;
            }
            catch (Exception ex)
            {
                if (_libraryHandle != IntPtr.Zero)
                {
                    NativeLibrary.Free(_libraryHandle);
                    _libraryHandle = IntPtr.Zero;
                }
                
                throw new DllNotFoundException($"无法加载或初始化scrcpy库: {libPath}", ex);
            }
        }

        /// <summary>
        /// 释放scrcpy库
        /// </summary>
        public static void Cleanup()
        {
            if (_libraryHandle != IntPtr.Zero)
            {
                NativeLibrary.Free(_libraryHandle);
                _libraryHandle = IntPtr.Zero;
                _runWithParams = null;
                _runWithOptions = null;
                _requestExit = null;
            }
        }

        /// <summary>
        /// 使用简化参数运行scrcpy
        /// </summary>
        /// <param name="serial">设备序列号，null表示使用默认设备</param>
        /// <param name="window">是否显示窗口</param>
        /// <param name="control">是否启用控制</param>
        /// <param name="maxSize">最大尺寸（0表示无限制）</param>
        /// <param name="bitRate">视频比特率（0表示使用默认值）</param>
        /// <param name="maxFps">最大帧率（0表示无限制）</param>
        /// <param name="windowWidth">窗口宽度（0表示使用默认值）</param>
        /// <param name="windowHeight">窗口高度（0表示使用默认值）</param>
        /// <param name="fullscreen">是否全屏</param>
        /// <param name="showTouches">是否显示触摸点</param>
        /// <param name="stayAwake">是否保持设备屏幕常亮</param>
        /// <param name="turnScreenOff">是否关闭设备屏幕</param>
        /// <param name="recordScreen">是否录制屏幕</param>
        /// <param name="recordFilename">录制文件名</param>
        /// <returns>scrcpy退出代码</returns>
        public static ScrcpyExitCode Run(
            string serial = null,
            bool window = true,
            bool control = true,
            ushort maxSize = 0,
            uint bitRate = 8000000,
            ushort maxFps = 0,
            ushort windowWidth = 0,
            ushort windowHeight = 0,
            bool fullscreen = false,
            bool showTouches = false,
            bool stayAwake = false,
            bool turnScreenOff = false,
            bool recordScreen = false,
            string recordFilename = null)
        {
            if (_libraryHandle == IntPtr.Zero)
            {
                Initialize();
            }

            if (_runWithParams == null)
            {
                throw new InvalidOperationException("scrcpy库未正确初始化");
            }

            return _runWithParams(
                serial,
                window,
                control,
                maxSize,
                bitRate,
                maxFps,
                windowWidth,
                windowHeight,
                fullscreen,
                showTouches,
                stayAwake,
                turnScreenOff,
                recordScreen,
                recordFilename);
        }

        /// <summary>
        /// 使用选项结构体运行scrcpy
        /// </summary>
        /// <param name="options">选项结构体</param>
        /// <returns>scrcpy退出代码</returns>
        public static ScrcpyExitCode RunWithOptions(ref ScrcpyOptions options)
        {
            if (_libraryHandle == IntPtr.Zero)
            {
                Initialize();
            }

            if (_runWithOptions == null)
            {
                throw new InvalidOperationException("scrcpy库未正确初始化");
            }

            return _runWithOptions(ref options);
        }

        /// <summary>
        /// 请求退出当前运行的scrcpy实例
        /// </summary>
        public static void RequestExit()
        {
            if (_libraryHandle == IntPtr.Zero || _requestExit == null)
            {
                throw new InvalidOperationException("scrcpy库未正确初始化");
            }

            _requestExit();
        }
    }

    /// <summary>
    /// 使用示例
    /// </summary>
    public class ScrcpyExample
    {
        public static void Main()
        {
            try
            {
                // 初始化scrcpy库，可以指定路径
                // Scrcpy.Initialize(@"/path/to/libscrcpy.dylib"); // macOS
                // Scrcpy.Initialize(@"C:\Path\To\scrcpy.dll"); // Windows
                Scrcpy.Initialize();

                // 简单用法：显示默认设备的屏幕
                ScrcpyExitCode result = Scrcpy.Run();
                Console.WriteLine($"scrcpy退出代码: {result}");

                // 高级用法：指定设备并设置更多参数
                result = Scrcpy.Run(
                    serial: "device_serial_number",
                    window: true,
                    control: true,
                    maxSize: 1080,
                    bitRate: 8000000,
                    maxFps: 60,
                    windowWidth: 1280,
                    windowHeight: 720,
                    fullscreen: false,
                    showTouches: true,
                    stayAwake: true,
                    turnScreenOff: false,
                    recordScreen: true,
                    recordFilename: "recording.mp4");
                Console.WriteLine($"scrcpy退出代码: {result}");

                // 清理资源
                Scrcpy.Cleanup();
            }
            catch (Exception ex)
            {
                Console.WriteLine($"错误: {ex.Message}");
            }
        }
    }
}
