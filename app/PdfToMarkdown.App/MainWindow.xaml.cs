using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Media;
using Windows.Storage.Pickers;
using WinRT.Interop;

namespace PdfToMarkdown.App;

public partial class MainWindow : Window
{
    private readonly TextBox _pdfBox = new() { PlaceholderText = "选择或拖入 PDF 文件" };
    private readonly TextBox _mdBox = new() { PlaceholderText = "输出 Markdown 路径" };
    private readonly TextBox _modelsBox = new() { PlaceholderText = "PP-OCRv6 模型目录" };
    private readonly NumberBox _dpiBox = new()
    {
        Value = 200,
        Minimum = 150,
        Maximum = 300,
        SpinButtonPlacementMode = NumberBoxSpinButtonPlacementMode.Inline,
        Header = "DPI"
    };
    private readonly ProgressBar _progress = new() { Minimum = 0, Maximum = 100, Height = 12 };
    private readonly TextBlock _status = new() { Text = "就绪" };
    private readonly TextBox _preview = new()
    {
        IsReadOnly = true,
        AcceptsReturn = true,
        TextWrapping = TextWrapping.Wrap,
        Height = 320,
        FontFamily = new FontFamily("Consolas")
    };
    private readonly Button _startBtn = new() { Content = "开始转换", Width = 120 };
    private readonly Button _cancelBtn = new() { Content = "取消", Width = 80, IsEnabled = false };

    private PdfToMdConverter? _converter;
    private CancellationTokenSource? _cts;

    public MainWindow()
    {
        Title = "PDF OCR → Markdown";
        ExtendsContentIntoTitleBar = false;

        _modelsBox.Text = GuessModelsDir();

        var root = new Grid { Padding = new Thickness(20), RowSpacing = 10, ColumnSpacing = 8 };
        root.RowDefinitions.Add(new RowDefinition { Height = GridLength.Auto });
        root.RowDefinitions.Add(new RowDefinition { Height = GridLength.Auto });
        root.RowDefinitions.Add(new RowDefinition { Height = GridLength.Auto });
        root.RowDefinitions.Add(new RowDefinition { Height = GridLength.Auto });
        root.RowDefinitions.Add(new RowDefinition { Height = GridLength.Auto });
        root.RowDefinitions.Add(new RowDefinition { Height = GridLength.Auto });
        root.RowDefinitions.Add(new RowDefinition { Height = new GridLength(1, GridUnitType.Star) });

        root.Children.Add(MakePathRow("PDF 文件", _pdfBox, async () => await PickPdfAsync(), 0));
        root.Children.Add(MakePathRow("输出 MD", _mdBox, async () => await PickMdAsync(), 1));
        root.Children.Add(MakePathRow("模型目录", _modelsBox, async () => await PickFolderAsync(), 2));

        var tools = new StackPanel { Orientation = Orientation.Horizontal, Spacing = 8 };
        tools.Children.Add(_dpiBox);
        tools.Children.Add(_startBtn);
        tools.Children.Add(_cancelBtn);
        var openMd = new Button { Content = "打开 MD" };
        openMd.Click += async (_, _) =>
        {
            if (File.Exists(_mdBox.Text))
            {
                await Windows.System.Launcher.LaunchUriAsync(new Uri(_mdBox.Text));
            }
        };
        tools.Children.Add(openMd);
        Grid.SetRow(tools, 3);
        root.Children.Add(tools);

        Grid.SetRow(_progress, 4);
        root.Children.Add(_progress);
        Grid.SetRow(_status, 5);
        root.Children.Add(_status);

        var previewPanel = new StackPanel { Spacing = 6 };
        previewPanel.Children.Add(new TextBlock { Text = "Markdown 预览" });
        previewPanel.Children.Add(_preview);
        Grid.SetRow(previewPanel, 6);
        root.Children.Add(previewPanel);

        _startBtn.Click += async (_, _) => await StartAsync();
        _cancelBtn.Click += (_, _) => _converter?.Cancel();

        Content = root;
    }

    private static UIElement MakePathRow(string label, TextBox box, Func<Task> browse, int row)
    {
        var grid = new Grid { ColumnSpacing = 8 };
        grid.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(80) });
        grid.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(1, GridUnitType.Star) });
        grid.ColumnDefinitions.Add(new ColumnDefinition { Width = GridLength.Auto });
        var lbl = new TextBlock { Text = label, VerticalAlignment = VerticalAlignment.Center };
        var btn = new Button { Content = "浏览…" };
        btn.Click += async (_, _) => await browse();
        Grid.SetColumn(box, 1);
        Grid.SetColumn(btn, 2);
        grid.Children.Add(lbl);
        grid.Children.Add(box);
        grid.Children.Add(btn);
        Grid.SetRow(grid, row);
        return grid;
    }

    private async Task PickPdfAsync()
    {
        var picker = new FileOpenPicker();
        InitializePicker(picker);
        picker.FileTypeFilter.Add(".pdf");
        var file = await picker.PickSingleFileAsync();
        if (file != null)
        {
            _pdfBox.Text = file.Path;
            _mdBox.Text = Path.ChangeExtension(file.Path, ".md");
        }
    }

    private async Task PickMdAsync()
    {
        var picker = new FileSavePicker();
        InitializePicker(picker);
        picker.FileTypeChoices.Add("Markdown", new List<string> { ".md" });
        picker.SuggestedFileName = "output";
        var file = await picker.PickSaveFileAsync();
        if (file != null) _mdBox.Text = file.Path;
    }

    private async Task PickFolderAsync()
    {
        var picker = new FolderPicker();
        InitializePicker(picker);
        picker.FileTypeFilter.Add("*");
        var folder = await picker.PickSingleFolderAsync();
        if (folder != null) _modelsBox.Text = folder.Path;
    }

    private void InitializePicker(object picker)
    {
        var hwnd = WindowNative.GetWindowHandle(this);
        InitializeWithWindow.Initialize(picker, hwnd);
    }

    private async Task StartAsync()
    {
        if (string.IsNullOrWhiteSpace(_pdfBox.Text) ||
            string.IsNullOrWhiteSpace(_mdBox.Text) ||
            string.IsNullOrWhiteSpace(_modelsBox.Text))
        {
            _status.Text = "请填写 PDF、输出路径和模型目录";
            return;
        }

        _startBtn.IsEnabled = false;
        _cancelBtn.IsEnabled = true;
        _progress.Value = 0;
        _preview.Text = string.Empty;
        _status.Text = "正在转换…";

        var dpi = (int)_dpiBox.Value;
        var config = $"{{\"dpi\":{dpi},\"cpu_threads\":8,\"enable_mkldnn\":false}}";
        var dispatcher = DispatcherQueue;

        await Task.Run(() =>
        {
            try
            {
                _converter?.Dispose();
                _converter = new PdfToMdConverter();
                _converter.Create(_modelsBox.Text, config);
                var rc = _converter.Convert(_pdfBox.Text, _mdBox.Text, (cur, total, msg) =>
                {
                    dispatcher.TryEnqueue(() =>
                    {
                        _progress.Value = total > 0 ? cur * 100.0 / total : 0;
                        _status.Text = $"[{cur}/{total}] {msg}";
                    });
                });

                dispatcher.TryEnqueue(() =>
                {
                    if (rc == 0)
                    {
                        _status.Text = "转换完成";
                        _progress.Value = 100;
                        try { _preview.Text = File.ReadAllText(_mdBox.Text); }
                        catch { _preview.Text = "（预览读取失败）"; }
                    }
                    else
                    {
                        _status.Text = $"失败 ({rc}): {_converter?.LastError}";
                    }
                    _startBtn.IsEnabled = true;
                    _cancelBtn.IsEnabled = false;
                });
            }
            catch (Exception ex)
            {
                dispatcher.TryEnqueue(() =>
                {
                    _status.Text = ex.Message;
                    _startBtn.IsEnabled = true;
                    _cancelBtn.IsEnabled = false;
                });
            }
        });
    }

    private static string GuessModelsDir()
    {
        var candidates = new[]
        {
            Path.Combine(AppContext.BaseDirectory, "models"),
            @"D:\Environment\PaddleOCR-models\PP-OCRv6",
            @"D:\work\AAA_21ic_Project\MedicalOCR\models",
            Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", "..", "MedicalOCR", "models")),
        };
        foreach (var c in candidates)
        {
            if (Directory.Exists(Path.Combine(c, "PP-OCRv6_small_det"))) return c;
        }
        return candidates[0];
    }
}
