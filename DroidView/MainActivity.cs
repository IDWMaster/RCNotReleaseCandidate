/*
Copyright 2017 Brian Bosak
Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/


using Android.App;
using Android.Widget;
using Android.OS;
using System.Net;
using System.Net.Sockets;
using System;

namespace DroidView
{
    [Activity(Label = "DroidView", MainLauncher = true, Icon = "@drawable/icon")]
    public class MainActivity : Activity
    {
        protected override void OnCreate(Bundle bundle)
        {
            base.OnCreate(bundle);

            // Set our view from the "main" layout resource
            SetContentView(Resource.Layout.Main);
            var connectBtn = FindViewById<Button>(Resource.Id.connectBtn);
            connectBtn.Click += doConnect;
        }

        private async void doConnect(object sender, System.EventArgs e)
        {
            var btn = sender as Button;
            btn.Enabled = false;
            btn.Text = "Establishing connection";

            FindViewById<TextView>(Resource.Id.textView2).Text = "";
            TcpClient mclient = new TcpClient();

            try
            {
                await mclient.ConnectAsync("192.168.86.58", 3870);
                RaceCar.connection = mclient.GetStream();
                StartActivity(typeof(RaceCar)); //start your engines!
            }
            catch (Exception er)
            {
                btn.Text = "Connect";
                btn.Enabled = true;
                FindViewById<TextView>(Resource.Id.textView2).Text = er.Message;
                mclient.Dispose();
            }

        }
    }
}

