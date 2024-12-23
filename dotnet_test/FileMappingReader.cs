using System;
using System.IO;
using System.IO.MemoryMappedFiles;
using System.Runtime.InteropServices;
using System.Threading;
using System.Threading.Tasks;

namespace ScrcpyImageViewer
{
    // 对应 C 结构体的帧头信息
    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    public struct FrameHeader
    {
        public uint Width;
        public uint Height;
        public uint Format;
        public uint FrameSize;
        public ulong Timestamp;
        public uint Sequence;
        public uint Reserved1;
        public uint Reserved2;
    }

    public class FrameData
    {
        public FrameHeader Header { get; set; }
        public byte[] ImageData { get; set; } = Array.Empty<byte>();
    }

    public class FileMappingReader : IDisposable
    {
        private readonly string _mappingName;
        private readonly int _maxFrameSize;
        private MemoryMappedFile? _mmf;
        private MemoryMappedViewAccessor? _accessor;
        private bool _disposed;
        private string? _filePath;

        public event Action<FrameData>? FrameReceived;

        public FileMappingReader(string mappingName, int maxFrameSize)
        {
            _mappingName = mappingName;
            _maxFrameSize = maxFrameSize;
        }

        public bool Initialize()
        {
            try
            {
                // 构建文件路径，与 C 端保持一致
                string tempDir = GetTempDirectory();
                _filePath = Path.Combine(tempDir, $"scrcpy_{_mappingName}.map");
                
                Console.WriteLine($"尝试打开文件映射: {_filePath}");
                
                // 检查文件是否存在
                if (!File.Exists(_filePath))
                {
                    Console.WriteLine($"文件映射不存在: {_filePath}");
                    return false;
                }
                
                // 计算总大小：头部 + 最大帧数据
                long totalSize = Marshal.SizeOf<FrameHeader>() + _maxFrameSize;
                
                // 使用文件创建内存映射
                _mmf = MemoryMappedFile.CreateFromFile(_filePath, FileMode.Open, _mappingName, totalSize);
                _accessor = _mmf.CreateViewAccessor(0, totalSize);
                
                Console.WriteLine($"成功打开文件映射: {_filePath}, 大小: {totalSize}");
                return true;
            }
            catch (Exception ex)
            {
                Console.WriteLine($"初始化文件映射失败: {ex.Message}");
                return false;
            }
        }

        private string GetTempDirectory()
        {
            // 获取临时目录，与 C 端逻辑保持一致
            string? tempDir = Environment.GetEnvironmentVariable("TMPDIR");
            if (!string.IsNullOrEmpty(tempDir))
                return tempDir.TrimEnd('/');
                
            tempDir = Environment.GetEnvironmentVariable("TMP");
            if (!string.IsNullOrEmpty(tempDir))
                return tempDir.TrimEnd('/');
                
            tempDir = Environment.GetEnvironmentVariable("TEMP");
            if (!string.IsNullOrEmpty(tempDir))
                return tempDir.TrimEnd('/');
                
            return "/tmp";
        }

        public FrameData? ReadFrame()
        {
            if (_accessor == null || _disposed)
                return null;

            try
            {
                // 读取帧头
                var header = _accessor.ReadStruct<FrameHeader>(0);
                
                // 验证帧数据大小
                if (header.FrameSize <= 0 || header.FrameSize > _maxFrameSize)
                {
                    return null;
                }

                // 读取图像数据
                var imageData = new byte[header.FrameSize];
                int headerSize = Marshal.SizeOf<FrameHeader>();
                
                for (int i = 0; i < header.FrameSize; i++)
                {
                    imageData[i] = _accessor.ReadByte(headerSize + i);
                }

                return new FrameData
                {
                    Header = header,
                    ImageData = imageData
                };
            }
            catch (Exception ex)
            {
                Console.WriteLine($"读取帧数据失败: {ex.Message}");
                return null;
            }
        }

        public async Task StartMonitoring(CancellationToken cancellationToken = default)
        {
            uint lastSequence = 0;
            
            while (!cancellationToken.IsCancellationRequested && !_disposed)
            {
                try
                {
                    var frame = ReadFrame();
                    if (frame != null && frame.Header.Sequence != lastSequence)
                    {
                        lastSequence = frame.Header.Sequence;
                        FrameReceived?.Invoke(frame);
                    }
                    
                    // 等待一小段时间避免过度占用 CPU
                    await Task.Delay(16, cancellationToken); // ~60 FPS
                }
                catch (OperationCanceledException)
                {
                    break;
                }
                catch (Exception ex)
                {
                    Console.WriteLine($"监控帧数据时出错: {ex.Message}");
                    await Task.Delay(100, cancellationToken);
                }
            }
        }

        public void Dispose()
        {
            if (_disposed)
                return;

            _disposed = true;
            _accessor?.Dispose();
            _mmf?.Dispose();
        }
    }

    // 扩展方法用于读取结构体
    public static class MemoryMappedViewAccessorExtensions
    {
        public static T ReadStruct<T>(this MemoryMappedViewAccessor accessor, long position) where T : struct
        {
            int size = Marshal.SizeOf<T>();
            byte[] buffer = new byte[size];
            
            for (int i = 0; i < size; i++)
            {
                buffer[i] = accessor.ReadByte(position + i);
            }
            
            GCHandle handle = GCHandle.Alloc(buffer, GCHandleType.Pinned);
            try
            {
                return Marshal.PtrToStructure<T>(handle.AddrOfPinnedObject());
            }
            finally
            {
                handle.Free();
            }
        }
    }
}
