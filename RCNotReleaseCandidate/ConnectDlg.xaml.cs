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
using System.Windows.Shapes;
using System.Net;
using System.Net.Sockets;
using System.IO;
namespace RCNotReleaseCandidate
{
    /// <summary>
    /// Interaction logic for ConnectDlg.xaml
    /// </summary>
    public partial class ConnectDlg : Window
    {
        public ConnectDlg()
        {
            InitializeComponent();
        }
        public Stream networkConnection;
        async void Connect()
        {
            ipOrHostname.IsEnabled = false;
            connectBtn.IsEnabled = false;
            try
            {
                using (TcpClient mclient = new TcpClient())
                {
                    await mclient.ConnectAsync(ipOrHostname.Text, 3870);
                    Close();
                    networkConnection = mclient.GetStream();
                }

            }catch(Exception er)
            {
                MessageBox.Show(er.Message);
            }

             ipOrHostname.IsEnabled = true;
            connectBtn.IsEnabled = true;

        }
        private void Button_Click(object sender, RoutedEventArgs e)
        {
            Connect();
        }

        private void TextBlock_KeyDown(object sender, KeyEventArgs e)
        {
            Connect();
        }
    }
}
