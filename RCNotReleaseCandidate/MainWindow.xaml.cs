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
using System.Runtime.InteropServices;
using System.Windows.Threading;
using System.Security;

namespace RCNotReleaseCandidate
{

    struct ENGINE_CONTEXT
    {
        public IntPtr surface; //IDirect3DSurface9*
        public IntPtr handle; //Handle to engine
    }

    /// <summary>
    /// Interaction logic for MainWindow.xaml
    /// </summary>
    public partial class MainWindow : Window
    {
        Stream str;
        TcpListener mlist;
        bool running = true;
        
        public MainWindow()
        {
            InitializeComponent();
            mlist = new TcpListener(new IPEndPoint(IPAddress.Any, 3870));
            mlist.Start();
            
            Loaded += windowLoaded;
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
        ENGINE_CONTEXT ctx;
        [SuppressUnmanagedCodeSecurity]
        [DllImport("D3DNatives.dll")]
        static extern ENGINE_CONTEXT CreateEngine(IntPtr window);
        [SuppressUnmanagedCodeSecurity]
        [DllImport("D3DNatives.dll")]
        static extern void DrawBackbuffer(IntPtr context);
        [DllImport("D3DNatives.dll")]
        static extern void RecordFrame(IntPtr context);
        private void windowLoaded(object sender, RoutedEventArgs e)
        {
            IntPtr handle = new WindowInteropHelper(this).Handle;
            System.Threading.Thread updateThread = new System.Threading.Thread(delegate () {
                ctx = CreateEngine(handle);
                Dispatcher.Invoke(() => {
                    renderTarget = new D3DImage();
                    drawTarget.Source = renderTarget;
                    renderTarget.Lock();
                    renderTarget.SetBackBuffer(D3DResourceType.IDirect3DSurface9, ctx.surface);
                    renderTarget.Unlock();
                });
                while (true)
                {
                    DrawBackbuffer(ctx.handle);
                    RecordFrame(ctx.handle);
                    Dispatcher.Invoke(() => {
                        renderTarget.Lock();
                        renderTarget.AddDirtyRect(new Int32Rect(0, 0, renderTarget.PixelWidth, renderTarget.PixelHeight));
                        renderTarget.Unlock();
                    });
                    
                }
            });
            updateThread.Start();
        }

        private void renderFrame(object sender, EventArgs e)
        {
            
        }

        private void RenderTarget_IsFrontBufferAvailableChanged(object sender, DependencyPropertyChangedEventArgs e)
        {
            
        }

        D3DImage renderTarget;
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
