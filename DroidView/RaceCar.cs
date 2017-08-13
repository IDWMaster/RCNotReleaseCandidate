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
                                        byte[] packet = mreader.ReadBytes(mreader.ReadInt32());
                                        int id = codec.DequeueInputBuffer(-1);
                                        using (var buffy = codec.GetInputBuffer(id))
                                        {
                                            if (buffy.Capacity() >= packet.Length)
                                            {
                                                Marshal.Copy(packet, 0, buffy.GetDirectBufferAddress(), packet.Length);
                                                codec.QueueInputBuffer(id, 0, packet.Length, 0, Android.Media.MediaCodecBufferFlags.None);
                                                using (var info = new Android.Media.MediaCodec.BufferInfo())
                                                {
                                                    int idx = codec.DequeueOutputBuffer(info, 0);
                                                    if(idx>=0)
                                                    {
                                                        codec.ReleaseOutputBuffer(idx, true);
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