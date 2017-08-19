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
        //Stream debugStream = File.Create("C:\\cygwin64\\debug");
        TcpListener mlist;
        bool running = true;
        
        public MainWindow()
        {
            currentCallback = frameHandler;
            InitializeComponent();

            Loaded += windowLoaded;
            Closed += MainWindow_Closed;
            drawTarget.Source = renderTarget;
            try
            {
                mlist = new TcpListener(new IPEndPoint(IPAddress.Any, 3870));
                mlist.Start();

                Action item = async () =>
                {
                    while (running)
                    {
                        try
                        {
                            //for now; support one bi-directional connection.
                            var client = (await mlist.AcceptTcpClientAsync()).GetStream();
                            str = client;
                            StreamOn();
                        }
                        catch (Exception er)
                        {
                        }
                    }
                };
                item();
            }catch(Exception er)
            {
                MessageBox.Show("Unable to initialize remote desktop server. The following error occurred: "+er.Message);
                Close();
            }
        }
        bool clientOnly = false;

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
        [DllImport("D3DNatives.dll")]
        static extern void DispatchInput(int type, int x, int y, int touchpoint);
        [DllImport("D3DNatives.dll")]
        static extern void FreeEngine(IntPtr context);
        Queue<byte[]> packets = new Queue<byte[]>();
        System.Threading.AutoResetEvent pevt = new System.Threading.AutoResetEvent(false);


        void Reset()
        {
            
            lock (ctxlock)
            {
                renderTarget.Lock();
                renderTarget.SetBackBuffer(D3DResourceType.IDirect3DSurface9, IntPtr.Zero);
                renderTarget.Unlock();
                if (ctx.handle != IntPtr.Zero)
                {
                    FreeEngine(ctx.handle);
                    ctx.handle = IntPtr.Zero;
                }
                ctx = CreateEngine(new WindowInteropHelper(this).Handle, currentCallback);
                renderTarget.Lock();
                renderTarget.SetBackBuffer(D3DResourceType.IDirect3DSurface9, ctx.surface);
                renderTarget.Unlock();
            }
        }
        CB currentCallback;

        private void frameHandler(long timestamp, IntPtr data, int len)
        {
            if(str == null)
            {
                return;
            }
            byte[] buffer = new byte[len];
            Marshal.Copy(data, buffer, 0, len);
            try
            {
                //MemoryStream mstream = new MemoryStream();
                BinaryWriter mwriter = new BinaryWriter(str);
                mwriter.Write((byte)0);
                mwriter.Write(timestamp);
                mwriter.Write(buffer.Length);
                mwriter.Write(buffer);
                str.Flush();

            }
            catch (Exception er)
            {
                str = null;
                Dispatcher.InvokeAsync(StartPreview); //return to preview mode
            }
        }
        object ctxlock = new object();
        private void windowLoaded(object sender, RoutedEventArgs e)
        {
            IntPtr handle = new WindowInteropHelper(this).Handle;
            System.Threading.Thread updateThread = new System.Threading.Thread(delegate () {

                ctx = CreateEngine(handle,currentCallback);
                Dispatcher.Invoke(() => {
                    renderTarget = new D3DImage();
                    drawTarget.Source = renderTarget;
                    renderTarget.Lock();
                    renderTarget.SetBackBuffer(D3DResourceType.IDirect3DSurface9, ctx.surface);
                    renderTarget.Unlock();
                });
                while (running)
                {
                    lock (ctxlock)
                    {
                        if (recordMode)
                        {
                            RecordFrame(ctx.handle);
                        }
                        else
                        {
                            if (!clientOnly)
                            {
                                DrawBackbuffer(ctx.handle);
                            }
                        }
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
            Reset();
            recordMode = false;
        }

        async void StreamOn()
        {


            Reset();
            recordMode = true;


            System.Threading.Thread netthread = new System.Threading.Thread(delegate () {
                BinaryReader mreader = new BinaryReader(str);
                while (running && str != null)
                {
                    try
                    {
                        switch(mreader.ReadByte())
                        {
                            case 1:
                                {
                                    //Touchdown!
                                    DispatchInput(0, mreader.ReadInt32(), mreader.ReadInt32(), mreader.ReadInt32());
                                }
                                break;
                            case 2:
                                {
                                    //Touchup!
                                    DispatchInput(1, mreader.ReadInt32(), mreader.ReadInt32(), mreader.ReadInt32());
                                }
                                break;
                            case 3:
                                {
                                    //Touchmove!
                                    DispatchInput(2, mreader.ReadInt32(), mreader.ReadInt32(), mreader.ReadInt32());
                                }
                                break;
                        }
                    }
                    catch (Exception er)
                    {
                        break;
                    }
                }
            });
            netthread.Start();
        }

        
    }
}
