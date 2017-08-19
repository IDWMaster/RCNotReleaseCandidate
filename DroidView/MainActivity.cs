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
            SetContentView (Resource.Layout.Main);
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
                    await mclient.ConnectAsync(FindViewById<EditText>(Resource.Id.editText1).Text, 3870);
                    RaceCar.connection = mclient.GetStream();
                    StartActivity(typeof(RaceCar)); //start your engines!
                }catch(Exception er)
                {
                    btn.Text = "Connect";
                btn.Enabled = true;
                    FindViewById<TextView>(Resource.Id.textView2).Text = er.Message;
                mclient.Dispose();
            }
            
        }
    }
}

