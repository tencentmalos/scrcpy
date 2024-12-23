using Avalonia.Controls;
using ScrcpyImageViewer.ViewModels;

namespace ScrcpyImageViewer.Views
{
    public partial class MainWindow : Window
    {
        public MainWindow()
        {
            InitializeComponent();
            DataContext = new MainWindowViewModel();
        }
    }
}
