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
        Stream debugStream = File.Create("C:\\cygwin64\\debug");
        TcpListener mlist;
        bool running = true;
        
        public MainWindow()
        {
            InitializeComponent();
            mlist = new TcpListener(new IPEndPoint(IPAddress.Any, 3870));
            mlist.Start();
            
            Loaded += windowLoaded;
            Closed += MainWindow_Closed;
            drawTarget.Source = renderTarget;
            Action item = async () => {
                while(running)
                {
                    try
                    {
                        //for now; support one bi-directional connection.
                        var client = (await mlist.AcceptTcpClientAsync()).GetStream();
                        str = client;
                        StreamOn();
                        break;
                    }catch(Exception er) {
                    }
                }
            };
            item();
        }

        private void MainWindow_Closed(object sender, EventArgs e)
        {
            running = false;
            pevt.Set();
        }

        delegate void CB(long timestamp,IntPtr data, int len);
        ENGINE_CONTEXT ctx;
        [SuppressUnmanagedCodeSecurity]
        [DllImport("D3DNatives.dll")]
        static extern ENGINE_CONTEXT CreateEngine(IntPtr window, CB callback);
        [SuppressUnmanagedCodeSecurity]
        [DllImport("D3DNatives.dll")]
        static extern void DrawBackbuffer(IntPtr context);
        [DllImport("D3DNatives.dll")]
        static extern void RecordFrame(IntPtr context);
        Queue<byte[]> packets = new Queue<byte[]>();
        System.Threading.AutoResetEvent pevt = new System.Threading.AutoResetEvent(false);

        private void windowLoaded(object sender, RoutedEventArgs e)
        {
            IntPtr handle = new WindowInteropHelper(this).Handle;
            System.Threading.Thread updateThread = new System.Threading.Thread(delegate () {

                System.Threading.Thread netthread = new System.Threading.Thread(delegate () {
                    while(running)
                    {
                        try
                        {
                            byte[] packet = null;
                            lock(packets)
                            {
                                if(packets.Any())
                                {
                                    packet = packets.Dequeue();
                                }
                            }
                            if(packet == null)
                            {
                                pevt.WaitOne();
                                continue;
                            }
                            str.Write(packet, 0, packet.Length);
                            str.Flush();
                        }catch(Exception er)
                        {

                        }
                    }
                });
                netthread.Start();
                ctx = CreateEngine(handle,(timestamp,data,len)=> {
                    byte[] buffer = new byte[len];
                    Marshal.Copy(data, buffer, 0, len);
                    try
                    {
                        MemoryStream mstream = new MemoryStream();
                        BinaryWriter mwriter = new BinaryWriter(mstream);
                        mwriter.Write((byte)0);
                        mwriter.Write(timestamp);
                        mwriter.Write(buffer.Length);
                        mwriter.Write(buffer);
                        str.Write(mstream.ToArray(),0,(int)mstream.Length);
                        debugStream.Write(mstream.ToArray(), 0, (int)mstream.Length);
                      /* lock(packets)
                        {
                            packets.Enqueue((mwriter.BaseStream as MemoryStream).ToArray());
                            pevt.Set();
                        }*/
                    }catch(Exception er)
                    {
                        StartPreview(); //return to preview mode.
                    }
                });
                Dispatcher.Invoke(() => {
                    renderTarget = new D3DImage();
                    drawTarget.Source = renderTarget;
                    renderTarget.Lock();
                    renderTarget.SetBackBuffer(D3DResourceType.IDirect3DSurface9, ctx.surface);
                    renderTarget.Unlock();
                });
                while (running)
                {
                    if(recordMode)
                    {
                        RecordFrame(ctx.handle);
                    }else
                    {
                        DrawBackbuffer(ctx.handle);
                    }
                    try
                    {
                        Dispatcher.Invoke(() =>
                        {
                            renderTarget.Lock();
                            renderTarget.AddDirtyRect(new Int32Rect(0, 0, renderTarget.PixelWidth, renderTarget.PixelHeight));
                            renderTarget.Unlock();
                        });
                    }catch(Exception er)
                    {

                    }
                    
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

        bool recordMode = false;

        D3DImage renderTarget;
        //Preview mode (show local screen mirror)
        async void StartPreview()
        {
            recordMode = false;

        }

        async void StreamOn()
        {
            recordMode = true;
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
