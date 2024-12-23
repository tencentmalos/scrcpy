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
        public int Width;
        public int Height;
        public int Format;
        public int FrameSize;
        public uint Sequence;
        public ulong Timestamp;
        public uint Reserved1;
        public uint Reserved2;
    }

    public class FrameData
    {
        public FrameHeader Header { get; set; }
        public byte[] ImageData { get; set; } = Array.Empty<byte>();
    }

    public class SharedMemoryReader : IDisposable
    {
        private readonly string _sharedMemoryName;
        private readonly int _maxFrameSize;
        private MemoryMappedFile? _mmf;
        private MemoryMappedViewAccessor? _accessor;
        private bool _disposed;

        public event Action<FrameData>? FrameReceived;

        public SharedMemoryReader(string sharedMemoryName, int maxFrameSize)
        {
            _sharedMemoryName = sharedMemoryName;
            _maxFrameSize = maxFrameSize;
        }

        public bool Initialize()
        {
            try
            {
                // 在 macOS 上，共享内存名称需要以 "/" 开头
                string mappingName = _sharedMemoryName.StartsWith("/") ? _sharedMemoryName[1..] : _sharedMemoryName;
                
                // 计算总大小：头部 + 最大帧数据
                long totalSize = Marshal.SizeOf<FrameHeader>() + _maxFrameSize;
                
                _mmf = MemoryMappedFile.OpenExisting(mappingName);
                _accessor = _mmf.CreateViewAccessor(0, totalSize);
                
                Console.WriteLine($"成功打开共享内存: {mappingName}, 大小: {totalSize}");
                return true;
            }
            catch (Exception ex)
            {
                Console.WriteLine($"初始化共享内存失败: {ex.Message}");
                return false;
            }
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
