using System.Runtime.InteropServices;
using System.Text;

namespace PdfToMarkdown.App;

internal static class NativeMethods
{
    private const string Dll = "PdfToMarkdown.Native.dll";

    public delegate void ProgressCallback(
        IntPtr userData, int currentPage, int totalPages, IntPtr messageUtf8);

    [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    public static extern int PdfToMd_Create(
        [MarshalAs(UnmanagedType.LPUTF8Str)] string modelsDir,
        [MarshalAs(UnmanagedType.LPUTF8Str)] string? configJson,
        out IntPtr handle);

    [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)]
    public static extern void PdfToMd_Destroy(IntPtr handle);

    [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    public static extern int PdfToMd_Convert(
        IntPtr handle,
        [MarshalAs(UnmanagedType.LPUTF8Str)] string pdfPath,
        [MarshalAs(UnmanagedType.LPUTF8Str)] string mdPath,
        ProgressCallback? progress,
        IntPtr userData);

    [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)]
    public static extern void PdfToMd_Cancel(IntPtr handle);

    [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)]
    public static extern IntPtr PdfToMd_GetLastError(IntPtr handle);

    [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)]
    public static extern IntPtr PdfToMd_GetVersion();

    public static string PtrToUtf8(IntPtr ptr)
    {
        if (ptr == IntPtr.Zero) return string.Empty;
        // Native returns UTF-8 C strings.
        var len = 0;
        while (Marshal.ReadByte(ptr, len) != 0) len++;
        if (len == 0) return string.Empty;
        var bytes = new byte[len];
        Marshal.Copy(ptr, bytes, 0, len);
        return Encoding.UTF8.GetString(bytes);
    }
}

internal sealed class PdfToMdConverter : IDisposable
{
    private IntPtr _handle = IntPtr.Zero;
    private NativeMethods.ProgressCallback? _progressKeepAlive;

    public void Create(string modelsDir, string configJson)
    {
        var rc = NativeMethods.PdfToMd_Create(modelsDir, configJson, out _handle);
        if (rc != 0)
        {
            var err = NativeMethods.PtrToUtf8(NativeMethods.PdfToMd_GetLastError(_handle));
            throw new InvalidOperationException($"初始化失败 ({rc}): {err}");
        }
    }

    public int Convert(string pdf, string md, Action<int, int, string> onProgress)
    {
        _progressKeepAlive = (user, current, total, msgPtr) =>
        {
            var msg = NativeMethods.PtrToUtf8(msgPtr);
            onProgress(current, total, msg);
        };
        return NativeMethods.PdfToMd_Convert(_handle, pdf, md, _progressKeepAlive, IntPtr.Zero);
    }

    public void Cancel()
    {
        if (_handle != IntPtr.Zero) NativeMethods.PdfToMd_Cancel(_handle);
    }

    public string LastError =>
        NativeMethods.PtrToUtf8(NativeMethods.PdfToMd_GetLastError(_handle));

    public void Dispose()
    {
        if (_handle != IntPtr.Zero)
        {
            NativeMethods.PdfToMd_Destroy(_handle);
            _handle = IntPtr.Zero;
        }
        _progressKeepAlive = null;
    }
}
