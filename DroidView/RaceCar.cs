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

    [Activity(Label = "RaceCar")]
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
            sview.Holder.AddCallback(new SurfsUp() {  created = ()=> {
                codec.Configure(Android.Media.MediaFormat.CreateVideoFormat("video/avc", 1920, 1080), sview.Holder.Surface, null, Android.Media.MediaCodecConfigFlags.None);
                codec.SetOutputSurface(sview.Holder.Surface);
                codec.Start();

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
                            if(pendingFrames.Any())
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
                                int sleeptime = (int)(surface.presentationTimestamp-currentTimestamp);
                                sleeptime -= (int)mwatch.ElapsedMilliseconds;
                                mwatch.Reset();
                                mwatch.Start();
                                if (sleeptime>0)
                                {
                                    System.Threading.Thread.Sleep(sleeptime);
                                }
                            }
                            currentTimestamp = surface.presentationTimestamp;
                            codec.ReleaseOutputBuffer(surface.ID, true);
                        }

                    }
                });
                renderThread.Start();


                BinaryReader mreader = new BinaryReader(connection);
                System.Threading.Thread mthread = new Thread(delegate () {
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
                                                        }else
                                                        {
                                                            break;
                                                        }
                                                    }
                                                }
                                            }
                                            else
                                            {
                                                throw new IndexOutOfRangeException("Illegal packet size.");
                                            }
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
                mthread.Start();

            }
            });
            
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