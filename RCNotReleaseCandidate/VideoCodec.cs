using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace RCNotReleaseCandidate
{
    abstract class VideoEncoder
    {
        /// <summary>
        /// Encodes a frame of RGBA pixel data
        /// </summary>
        /// <param name="pixels">Pixels to encode (each pixel is 32 bits wide, in RGBA format)</param>
        /// <param name="width">Width (in pixels) of the frame</param>
        /// <param name="height">Height (in pixels) of the frame</param>
        /// <returns></returns>
        public abstract Task Encode(IntPtr pixels, int width, int height);
        public event Action<byte[]> onPacketAvailable;
        protected void notifyPacketAvailable(byte[] packet)
        {
            onPacketAvailable?.Invoke(packet);
        }


    }


    /// <summary>
    /// An RGBA (32 bits per pixel) video frame
    /// </summary>
    struct VideoFrame
    {
        public int Width;
        public int Height;
        public IntPtr pixels;
    }
    abstract class VideoDecoder
    {
        public abstract Task Decode(byte[] packet);
        public event Action<VideoFrame> onFrameAvailable;

        protected void notifyFrameAvailable(VideoFrame frame)
        {
            onFrameAvailable?.Invoke(frame);
        }
    }
}
