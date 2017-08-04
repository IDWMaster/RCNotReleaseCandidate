using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Data;
using System.Windows.Documents;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Navigation;
using System.Windows.Shapes;
using System.Net;
using System.Net.Sockets;
using System.IO;
using System.Windows.Interop;
using Microsoft.Wpf.Interop.DirectX;

namespace RCNotReleaseCandidate
{
    /// <summary>
    /// Interaction logic for MainWindow.xaml
    /// </summary>
    public partial class MainWindow : Window
    {
        Stream str;
        TcpListener mlist;
        bool running = true;
        void Render(IntPtr surface, bool isNew)
        {

        }
        public MainWindow()
        {
            InitializeComponent();
            mlist = new TcpListener(new IPEndPoint(IPAddress.Any, 3870));
            mlist.Start();
            renderTarget = new D3D11Image();
            renderTarget.WindowOwner = new WindowInteropHelper(this).Handle;
            renderTarget.OnRender = Render;
            renderTarget.RequestRender();
            drawTarget.Source = renderTarget;
            Action item = async () => {
                while(running)
                {
                    try
                    {
                        //for now; support one bi-directional connection.
                        var client = (await mlist.AcceptTcpClientAsync()).GetStream();
                        str = client;
                        break;
                    }catch(Exception er) {
                    }
                }
            };
            item();
        }
        D3D11Image renderTarget;
        //Preview mode (show local screen mirror)
        async void StartPreview()
        {
        }

        async void StreamOn()
        {
            
        }

        private void MenuItem_Click(object sender, RoutedEventArgs e)
        {
            var cdlg = new ConnectDlg();
            cdlg.ShowDialog();
            if(cdlg.networkConnection != null)
            {
                str = cdlg.networkConnection;
            }
        }
    }
}
