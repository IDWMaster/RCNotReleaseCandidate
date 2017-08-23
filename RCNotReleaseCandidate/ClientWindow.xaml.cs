/*
Copyright 2017 Brian Bosak
Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

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
using System.Windows.Interop;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Shapes;
using System.IO;
using System.Net;
using System.Net.Sockets;
namespace RCNotReleaseCandidate
{
    /// <summary>
    /// Interaction logic for ClientWindow.xaml
    /// </summary>
    public partial class ClientWindow : Window
    {
        ENGINE_CONTEXT context;
        MainWindow.CB cb;
        D3DImage img = new D3DImage();
        void onPacket(long timestamp, IntPtr data, int len)
        {
            Dispatcher.Invoke(delegate () {
                img.Lock();
                img.AddDirtyRect(new Int32Rect(0, 0, img.PixelWidth, img.PixelHeight));
                img.Unlock();
            });
        }
        public ClientWindow()
        {
            InitializeComponent();
            cb = onPacket;
            hostname.Focus();
        }

        Stream str;
        void StartVideo()
        {
            
            context = MainWindow.CreateEngine(new WindowInteropHelper(this).Handle, cb);
            cimg.Source = img;
            img.Lock();
            img.SetBackBuffer(D3DResourceType.IDirect3DSurface9, context.surface);
            img.Unlock();
            System.Threading.Thread mthread = new System.Threading.Thread(delegate () {
                BinaryReader mreader = new BinaryReader(str);
                while (true)
                {
                    try
                    {
                        switch (mreader.ReadByte())
                        {
                            case 0:
                                //Video frame
                                long timestamp = mreader.ReadInt64();
                                byte[] packet = mreader.ReadBytes(mreader.ReadInt32());
                                unsafe
                                {
                                    fixed (byte* pptr = packet)
                                    {
                                        MainWindow.SendPacket(context.handle, new IntPtr(pptr), packet.Length);
                                    }
                                }
                                break;
                            default:
                                throw new Exception("Unsupported operation");
                                break;
                        }
                    }
                    catch (Exception er)
                    {
                        Dispatcher.Invoke(Close);
                        break;
                    }
                }
            });
            mthread.Start();
        }
        void Connect()
        {
            TcpClient mclient = new TcpClient();
            try
            {
                mclient.Connect(hostname.Text, 3870);
                str = mclient.GetStream();
                StartVideo();
                
            }catch(Exception er)
            {
                MessageBox.Show(er.Message);
                mclient.Close();
            }
        }
        private void Button_Click(object sender, RoutedEventArgs e)
        {
            Connect();
        }

        private void hostname_KeyDown(object sender, KeyEventArgs e)
        {
            if(e.Key == Key.Enter)
            {
                Connect();
            }
        }
    }
}
