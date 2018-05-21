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
    class Overlay:UIElement
    {
        protected override bool IsEnabledCore
        {
            get
            {
                return true;
            }
        }
        public Overlay()
        {
            Focusable = true;
        }


    }
    /// <summary>
    /// Interaction logic for ClientWindow.xaml
    /// </summary>
    public partial class ClientWindow : Window
    {
        ENGINE_CONTEXT context;
        MainWindow.CB cb;
        D3DImage img = new D3DImage();
        int width = 1920;
        int height = 1080;
        void onPacket(long timestamp, IntPtr data, int len)
        {
            Dispatcher.Invoke(delegate () {
                img.Lock();
                img.AddDirtyRect(new Int32Rect(0, 0, img.PixelWidth, img.PixelHeight));
                img.Unlock();
            });
        }
        UIElement overlaycontrol;
        public ClientWindow()
        {
            InitializeComponent();
            cb = onPacket;
            hostname.Focus();
            overlaycontrol = new Overlay();
            overlayGrid.Children.Add(overlaycontrol);
        }
        Dictionary<ulong, Cursor> ulongforcursors = new Dictionary<ulong, Cursor>();
        Stream str;
        void LoadCursor(ulong forthisparticularcursor)
        {
            cimg.Cursor = ulongforcursors[forthisparticularcursor];
        }
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
                            case 12:
                                Dispatcher.Invoke(() => {
                                    if(trustedComputer.IsChecked.Value)
                                    {
                                        System.Windows.Forms.Cursor.Hide();
                                    }
                                });
                                break;
                            case 13:
                                Dispatcher.Invoke(() => {
                                    if (trustedComputer.IsChecked.Value)
                                    {
                                        System.Windows.Forms.Cursor.Show();
                                    }
                                });
                                break;
                            case 14:
                                ulong forme = mreader.ReadUInt64();
                                Dispatcher.Invoke(() => {
                                    if (trustedComputer.IsChecked.Value)
                                    {
                                        if(ulongforcursors.ContainsKey(forme))
                                        {
                                            LoadCursor(forme);
                                        }else
                                        {
                                            BinaryWriter mwriter = new BinaryWriter(str);
                                            mwriter.Write((byte)16);
                                            mwriter.Write(forme);
                                        }
                                    }
                                });
                                break;
                            case 15:
                                {
                                    ulong foranothercursor = mreader.ReadUInt64();
                                    MemoryStream cstr = new MemoryStream(mreader.ReadBytes(mreader.ReadInt32()));
                                    Dispatcher.Invoke(() => {
                                        Cursor curses = new Cursor(cstr);
                                        ulongforcursors[foranothercursor] = curses;
                                        LoadCursor(foranothercursor);
                                    });
                                }
                                break;
                            case 16:
                                break; //RESERVED. DO NOT USE.
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
            overlaycontrol.KeyDown += cimg_KeyDown;
            overlaycontrol.KeyUp += cimg_KeyUp;
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

        private void cimg_MouseMove(object sender, MouseEventArgs e)
        {
            BinaryWriter mwriter = new BinaryWriter(str);
            mwriter.Write((byte)4);
            Point er = e.GetPosition(cimg);
            er.X /= cimg.ActualWidth;
            er.Y /= cimg.ActualHeight;
            er.X *= width;
            er.Y *= height;
            mwriter.Write((int)er.X);
            mwriter.Write((int)er.Y);
            mwriter.Flush();
        }

        private void cimg_MouseDown(object sender, MouseButtonEventArgs e)
        {
            overlaycontrol.Focus();
            BinaryWriter mwriter = new BinaryWriter(str);
            byte op = 5;
            switch (e.ChangedButton)
            {
                case MouseButton.Right:
                    {
                        op = 7;
                    }
                    break;
            }
            mwriter.Write(op);
            Point er = e.GetPosition(cimg);
            er.X /= cimg.ActualWidth;
            er.Y /= cimg.ActualHeight;
            er.X *= width;
            er.Y *= height;
            mwriter.Write((int)er.X);
            mwriter.Write((int)er.Y);
            mwriter.Flush();
        }

        private void cimg_MouseUp(object sender, MouseButtonEventArgs e)
        {
            
            BinaryWriter mwriter = new BinaryWriter(str);
            byte op = 6;
            switch (e.ChangedButton)
            {
                case MouseButton.Right:
                    {
                        op = 8;
                    }
                    break;
            }
            mwriter.Write(op);
            Point er = e.GetPosition(cimg);
            er.X /= cimg.ActualWidth;
            er.Y /= cimg.ActualHeight;
            er.X *= width;
            er.Y *= height;
            mwriter.Write((int)er.X);
            mwriter.Write((int)er.Y);
            mwriter.Flush();
        }

        private void cimg_MouseWheel(object sender, MouseWheelEventArgs e)
        {
            BinaryWriter mwriter = new BinaryWriter(str);
            byte op = 9;
            
            mwriter.Write(op);
            Point er = e.GetPosition(cimg);
            er.X /= cimg.ActualWidth;
            er.Y /= cimg.ActualHeight;
            er.X *= width;
            er.Y *= height;
            mwriter.Write((int)er.X);
            mwriter.Write((int)er.Y);
            mwriter.Write(e.Delta); //Are they a good airline? Is there such a thing as a good airline?
            mwriter.Flush();
        }

        private void cimg_KeyDown(object sender, KeyEventArgs e)
        {
            if(str == null)
            {
                return;
            }
            e.Handled = true;
            int keycode = KeyInterop.VirtualKeyFromKey(e.Key);
            BinaryWriter mwriter = new BinaryWriter(str);
            mwriter.Write((byte)10);
            mwriter.Write(keycode);
        }

        private void cimg_KeyUp(object sender, KeyEventArgs e)
        {
            if(str == null)
            {
                return;
            }
            e.Handled = true;
            int keycode = KeyInterop.VirtualKeyFromKey(e.Key);
            BinaryWriter mwriter = new BinaryWriter(str);
            mwriter.Write((byte)11);
            mwriter.Write(keycode);
        }
    }
}
