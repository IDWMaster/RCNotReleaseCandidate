﻿using System;
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

namespace RCNotReleaseCandidate
{
    /// <summary>
    /// Interaction logic for Startup.xaml
    /// </summary>
    public partial class Startup : Window
    {
        public Startup()
        {
            InitializeComponent();
        }

        private void Button_Click(object sender, RoutedEventArgs e)
        {
            ClientWindow cw = new ClientWindow();
            cw.Show();
            Close();
        }

        private void Button_Click_1(object sender, RoutedEventArgs e)
        {
            MainWindow window = new MainWindow();
            window.Show();
            Close();
        }
    }
}
