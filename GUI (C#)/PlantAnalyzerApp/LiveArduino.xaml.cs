using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices.WindowsRuntime;
using Windows.Foundation;
using Windows.Foundation.Collections;
using Windows.UI.Xaml;
using Windows.UI.Xaml.Controls;
using Windows.UI.Xaml.Controls.Primitives;
using Windows.UI.Xaml.Data;
using Windows.UI.Xaml.Input;
using Windows.UI.Xaml.Media;
using Windows.UI.Xaml.Navigation;
using Microsoft.Maker.Serial;
using Microsoft.Maker.RemoteWiring;
using Microsoft.Maker.Firmata;

// The Blank Page item template is documented at http://go.microsoft.com/fwlink/?LinkId=234238

namespace PlantAnalyzerApp
{
    /// <summary>
    /// An empty page that can be used on its own or navigated to within a Frame.
    /// </summary>
    public sealed partial class LiveArduino : Page
    {

        public LiveArduino()
        {
            this.InitializeComponent();

            this.InitArduino();
        }

        public void InitArduino()
        {
           
        }

        RemoteDevice mkr1000;
        NetworkSerial wificonnection;
        UwpFirmata mkr1000_firmata;

        private void button_Click(object sender, RoutedEventArgs e)
        {
            // Create Firmata
            this.mkr1000_firmata = new UwpFirmata();

            //Create MKR1000 Device
            mkr1000 = new Microsoft.Maker.RemoteWiring.RemoteDevice(mkr1000_firmata);

            //Establish a network serial connection. change it to the right IP address and port
            wificonnection = new Microsoft.Maker.Serial.NetworkSerial(new Windows.Networking.HostName("192.168.1.42"), 3030);

            //Attach event handlers
            wificonnection.ConnectionEstablished += NetWorkSerial_ConnectionEstablished;
            wificonnection.ConnectionFailed += NetWorkSerial_ConnectionFailed;

            //Begin Firmata
            mkr1000_firmata.begin(wificonnection);
            
            //Begin connection
            wificonnection.begin(115200, Microsoft.Maker.Serial.SerialConfig.SERIAL_8N1);
        }


        private void NetWorkSerial_ConnectionEstablished()
        {
            System.Diagnostics.Debug.WriteLine("Arduino Connection Succesfull!");
        }

        private void NetWorkSerial_ConnectionFailed(string message)
        {
            System.Diagnostics.Debug.WriteLine("Arduino Connection Failed: " + message);
        }

        private void button1_Click(object sender, RoutedEventArgs e)
        {
            mkr1000_firmata.sendString("take_temp_picture");
        }
    }
}
