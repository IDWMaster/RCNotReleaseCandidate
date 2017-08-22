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

using Android.App;
using Android.Content;
using Android.OS;
using Android.Runtime;
using Android.Views;
using Android.Widget;
using System.IO;
using System.Threading;
using System.Runtime.InteropServices;
using Android.Graphics;

namespace DroidView
{
    class SurfsUp : Java.Lang.Object, ISurfaceHolderCallback
    {
        public void SurfaceChanged(ISurfaceHolder holder, [GeneratedEnum] Format format, int width, int height)
        {
        }
        public Action created;
        public void SurfaceCreated(ISurfaceHolder holder)
        {
            created();
        }

        public void SurfaceDestroyed(ISurfaceHolder holder)
        {
        }
    }

    class PendingSurface
    {
        public long presentationTimestamp;
        public int ID;
    }

    class PendingPacket
    {
        public byte[] data;
        public long timestamp;
    }

    [Activity(Label = "RaceCar",ScreenOrientation = Android.Content.PM.ScreenOrientation.Landscape, Immersive = true)]
    public class RaceCar : Activity
    {
        public static Stream connection;
        Android.Media.MediaCodec codec;
        SurfaceView sview;
        
        public RaceCar()
        {
            
        }
        bool running = true;
        AutoResetEvent evt = new AutoResetEvent(false);
        protected override void OnDestroy()
        {
            running = false;
            evt.WaitOne();
            base.OnDestroy();
            codec.Dispose();
            sview.Dispose();
        }
        protected override void OnCreate(Bundle savedInstanceState)
        {
            base.OnCreate(savedInstanceState);
            codec = Android.Media.MediaCodec.CreateDecoderByType("video/avc");
            sview = new SurfaceView(this);
            SetContentView(sview);
            sview.Touch += Sview_Touch;
            sview.Holder.AddCallback(new SurfsUp() {  created = ()=> {
                codec.Configure(Android.Media.MediaFormat.CreateVideoFormat("video/avc", 1920, 1080), sview.Holder.Surface, null, Android.Media.MediaCodecConfigFlags.None);
                codec.SetOutputSurface(sview.Holder.Surface);
                codec.Start();
                Queue<PendingPacket> pendingPackets = new Queue<PendingPacket>();
                Queue<PendingSurface> pendingFrames = new Queue<PendingSurface>();
                AutoResetEvent evt = new AutoResetEvent(false);
                System.Threading.Thread renderThread = new Thread(delegate () {
                    long currentTimestamp = 0;
                    //Time since last frame
                    System.Diagnostics.Stopwatch mwatch = new System.Diagnostics.Stopwatch();

                    while (running) {
                        PendingSurface surface = null;
                        lock (pendingFrames)
                        {
                            if(pendingFrames.Count>0)
                            {
                                surface = pendingFrames.Dequeue();
                                
                            }
                        }
                        if (surface == null)
                        {
                            evt.WaitOne();
                        } else
                        {
                            if (currentTimestamp != 0)
                            {
                                /*int sleeptime = (int)(surface.presentationTimestamp-currentTimestamp);
                                sleeptime -= (int)mwatch.ElapsedMilliseconds;
                                mwatch.Reset();
                                mwatch.Start();
                                if (sleeptime>0)
                                {
                                    System.Threading.Thread.Sleep(sleeptime);
                                }*/
                            }
                            currentTimestamp = surface.presentationTimestamp;
                            codec.ReleaseOutputBuffer(surface.ID, true);
                        }

                    }
                });
                renderThread.Start();


                System.Threading.Thread decodeThread = new Thread(delegate () {
                    while(running)
                    {
                        PendingPacket _packet = null;
                        lock(pendingPackets)
                        {
                            if(pendingPackets.Any())
                            {
                                _packet = pendingPackets.Dequeue();
                            }
                        }
                        if(_packet == null)
                        {
                            evt.WaitOne();
                            continue;
                        }
                        byte[] packet = _packet.data;
                        long timestamp = _packet.timestamp;
                        int id = codec.DequeueInputBuffer(-1);
                        using (var buffy = codec.GetInputBuffer(id))
                        {
                            if (buffy.Capacity() >= packet.Length)
                            {
                                Marshal.Copy(packet, 0, buffy.GetDirectBufferAddress(), packet.Length);
                                codec.QueueInputBuffer(id, 0, packet.Length, timestamp, Android.Media.MediaCodecBufferFlags.None);
                                using (var info = new Android.Media.MediaCodec.BufferInfo())
                                {
                                    while (true)
                                    {
                                        int idx = codec.DequeueOutputBuffer(info, 0);
                                        if (idx >= 0)
                                        {
                                            var sval = new PendingSurface() { ID = idx, presentationTimestamp = info.PresentationTimeUs / 1000 };
                                            lock (pendingFrames)
                                            {
                                                pendingFrames.Enqueue(sval);
                                                evt.Set();
                                            }
                                        }
                                        else
                                        {
                                            break;
                                        }
                                    }
                                }
                            }
                        }
                    }
                });
                decodeThread.Start();


                BinaryReader mreader = new BinaryReader(connection);
                System.Threading.Thread netThread = new Thread(delegate () {
                    while (running)
                    {
                        try
                        {
                            switch (mreader.ReadByte())
                            {
                                case 0:
                                    {
                                        long timestamp = mreader.ReadInt64();
                                        byte[] packet = mreader.ReadBytes(mreader.ReadInt32());
                                        PendingPacket mpacket = new PendingPacket() { data = packet, timestamp = timestamp };
                                        lock (pendingPackets)
                                        {
                                            pendingPackets.Enqueue(mpacket);
                                        }
                                        

                                    }
                                    break;
                            }
                        }
                        catch (Exception er)
                        {

                        }

                        evt.Set();
                    }
                });
                netThread.Start();

            }
            });
            
        }

        private void Sview_Touch(object sender, View.TouchEventArgs e)
        {
            var view = sender as View;
            for (int i = 0; i < e.Event.PointerCount; i++)
            {
                int id = e.Event.GetPointerId(i);
                double x = e.Event.GetX();
                double y = e.Event.GetY();
                x /= view.Width;
                y /= view.Height;

                x *= 1920;
                y *= 1080;
                BinaryWriter mwriter = new BinaryWriter(connection);
                byte opcode = 0;
                switch (e.Event.Action)
                {
                    case MotionEventActions.Down:
                        opcode = 1;
                        break;
                    case MotionEventActions.Up:
                        opcode = 2;
                        break;
                    case MotionEventActions.Move:
                        opcode = 3;
                        break;
                }
                lock (connection)
                {
                    mwriter.Write(opcode);
                    mwriter.Write((int)x);
                    mwriter.Write((int)y);
                    mwriter.Write(id);
                }
            }
            lock(connection)
            {
                connection.Flush();
            }
        }

        protected override void Dispose(bool disposing)
        {
            if(disposing)
            {
                connection.Dispose();
                evt.Dispose();
            }
            base.Dispose(disposing);
        }
    }
}