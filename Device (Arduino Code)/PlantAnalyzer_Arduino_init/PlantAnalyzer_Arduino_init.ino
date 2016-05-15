/*////////////////////////////////////////////////////////////////////////////////////////////////////////



  ////////////////////////////////////////////////////////////////////////////////////////////////////////*/
#include <Wire.h>                    // I2C library, used to 
#include <SPI.h>                     // SPI library, used to control camera
#include <ArduCAM.h>                 // Camera library, creates a camera object and define image configuration
#include "memorysaver.h"             // File with OV5642's reg value
#include <SD.h>                      // SD card library, to store JPG images from cameras
#include <WiFi101.h>                 // Wifi library, used to connect to wifi and HTTP requests
#include <WiFiUdp.h>                 // Udp library, used to handle UDP messages
#include <RTCZero.h>                 // Real Time Clock library, used for alarm and name image files
#include <SparkFunHTU21D.h>          //
#include <SparkFunMPL3115A2.h>       //
#include <Firmata.h>                 //
#include "utility/WiFi101Stream.h"   //
////////////////////////////////////////////////////////////////////////////////////////////////////////
#define CS_normalCAM 4          // Normal camera chip select for SPI
#define CS_infrablueCAM 3       // Infrablue camera chip select for SPI
#define CS_SD 2                 // SD chip select for SPI

///*** Test Var ***///
bool useCloud = true;
bool useSQL = true;
bool useCAM = true;
bool useCloud_loop = false;
bool useSQL_loop = false;
bool useCAM_loop = false;
bool use_app = true;

//*** Environment Var  ***////////
float soilmoisture = 0;         //
float airtemp = 0;              // degF
float humidity = 0;             // %
//float soiltemp = 0;           //
//float sunlight = 0;           //

//*** Other Var ***//
int table_index = 0;
String normalimageurl;
String infrablueimageurl;
String ndviimageurl;

///*** Initialize the sensors ***///
MPL3115A2 temp_sensor;            //
HTU21D  humidity_sensor;          //
#define soil_moisture_sensor A0   //
//#define soil_temp_sensor A4
//#define soil_temp_type
//#define LIGHT A1
//#define REFERENCE_3V3 A3

///*** Initialize the cameras ***///
ArduCAM normalCAM(OV5642, CS_normalCAM);
ArduCAM infrablueCAM(OV5642, CS_infrablueCAM);
static uint8_t read_fifo_burst(ArduCAM myCAM, String CAM_NAME);

///*** Initialize the RTC ***///
RTCZero rtc;

// Arduino network info
//byte mac[] = { 0xF8, 0xF0, 0x05, 0xF4, 0xD7, 0xF0 };
IPAddress ip( 192, 168, 1, 42);
IPAddress gateway( 192, 168, 1, 1 );
IPAddress subnet( 255, 255, 255, 0 );

// Email Alerts Config
char server[] = "mail.smtp2go.com";
int port = 2525;

///*** Initialize the WiFi client library ***///
WiFiClient client;
char buffer[64];
char ssid[] = "xxxx";             // your network SSID (name)
char pass[] = "xxxx";          // your network password
WiFi101Stream stream;              //
int wifiStatus = WL_IDLE_STATUS;

// Used for NTP
unsigned int localPort = 2390;      // WAS:2390 local port to listen for UDP packets
IPAddress timeServer(129, 6, 15, 28); // time.nist.gov NTP server
const int NTP_PACKET_SIZE = 48; // NTP time stamp is in the first 48 bytes of the message
byte packetBuffer[ NTP_PACKET_SIZE]; //buffer to hold incoming and outgoing packets
WiFiUDP Udp; // A UDP instance to let us send and receive packets over UDP

///*** Azure Flask WebApp Config ***///
char flask_hostname[] = "xxxxxflask.azurewebsites.net";

//***  Azure Mobile Service  ***///
// You can find this in your service dashboard
const char *mobile_server = "xxxxxmobile.azure-mobile.net";

// Azure Table Name
const char *table_name = "planttable_1";

// Azure Mobile App Key
const char *mobile_app_key = "xxxxxxxxxxxxx";

void setup() {

  //
  Wire.begin();
  delay(500);
  Serial.begin(115200);
  delay(500);

  // Init environmental sensors
  temp_sensor.begin();
  humidity_sensor.begin();

  // attempt to connect to WiFi network:
  while ( wifiStatus != WL_CONNECTED) {
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(ssid);
    // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
    //status = WiFi.begin(ssid, pass);
    //stream.config( ip );
    wifiStatus = stream.begin(ssid, pass, 3030);

      // status = WiFi.begin(ssid);    
      // wait 10 seconds for connection:
      delay(10000);
  }  
  printWifiStatus();
  /////////////////////////////////////

  // FIRMATA SETUP
  Firmata.setFirmwareVersion(FIRMATA_FIRMWARE_MAJOR_VERSION, FIRMATA_FIRMWARE_MINOR_VERSION);
  Firmata.attach(STRING_DATA, stringCallback);

  // start up Network Firmata:
  Firmata.begin(stream);
  
  //Set time from NTP
  rtc.begin();

  unsigned long epoch;
  int numberOfTries = 0, maxTries = 6;
  do {
    epoch = readLinuxEpochUsingNTP();
    numberOfTries++;
  }

  while ((epoch == 0) || (numberOfTries > maxTries));

  if (numberOfTries > maxTries) {
    Serial.print("NTP unreachable!!");
    while (1);
  }
  else {
    Serial.print("Epoch received: ");
    Serial.println(epoch);
    rtc.setEpoch(epoch);
  }

  if (useCAM == true) {

    pinMode(CS_normalCAM, OUTPUT);
    pinMode(CS_infrablueCAM, OUTPUT);

    // initialize SPI:
    SPI.begin();
    ////////////////////////////////////
    //
    uint8_t vid, pid;
    uint8_t temp;
    delay(2000);

    //Check if the ArduCAM SPI bus is OK
    normalCAM.write_reg(ARDUCHIP_TEST1, 0x55);
    delay(1000);
    temp = normalCAM.read_reg(ARDUCHIP_TEST1);

    normalCAM.rdSensorReg16_8(OV5642_CHIPID_HIGH, &vid);
    normalCAM.rdSensorReg16_8(OV5642_CHIPID_LOW, &pid);
    delay(1000);
    while ((vid != 0x56) || (pid != 0x42) || (temp != 0x55)) {
      normalCAM.write_reg(ARDUCHIP_TEST1, 0x55);
      delay(3000);
      temp = normalCAM.read_reg(ARDUCHIP_TEST1);
      if (temp != 0x55)
        Serial.println("SPI1 interface Error!");

      // Check if Normal CAM is connected
      normalCAM.rdSensorReg16_8(OV5642_CHIPID_HIGH, &vid);
      normalCAM.rdSensorReg16_8(OV5642_CHIPID_LOW, &pid);

      if ((vid != 0x56) || (pid != 0x42))
        Serial.println("Can't find OV5642 module!");
      else
        delay(1000);
    }
    // Serial.println("CAM1 detected.");
    // Configure JPEG format
    normalCAM.set_format(JPEG);
    normalCAM.InitCAM();
    normalCAM.OV5642_set_JPEG_size(OV5642_2592x1944);
    normalCAM.set_bit(ARDUCHIP_TIM, VSYNC_LEVEL_MASK);
    normalCAM.set_bit(ARDUCHIP_GPIO, GPIO_PWDN_MASK); //enable low power
    normalCAM.clear_fifo_flag();
    normalCAM.write_reg(ARDUCHIP_FRAMES, 0x00);
    /////////////////////////////////////

    vid  = 0;
    temp = 0;
    pid  = 0;

    //Check if the ArduCAM SPI bus is OK
    infrablueCAM.write_reg(ARDUCHIP_TEST1, 0x55);
    delay(1000);
    temp = infrablueCAM.read_reg(ARDUCHIP_TEST1);

    infrablueCAM.rdSensorReg16_8(OV5642_CHIPID_HIGH, &vid);
    infrablueCAM.rdSensorReg16_8(OV5642_CHIPID_LOW, &pid);
    delay(1000);

    //Check if the ArduCAM SPI bus is OK
    while ((vid != 0x56) || (pid != 0x42) || (temp != 0x55)) {
      infrablueCAM.write_reg(ARDUCHIP_TEST1, 0x55);
      delay(1000);
      temp = infrablueCAM.read_reg(ARDUCHIP_TEST1);

      if (temp != 0x55)
        Serial.println("SPI2 interface Error!");
      // Check if Infracam is connected
      infrablueCAM.rdSensorReg16_8(OV5642_CHIPID_HIGH, &vid);
      infrablueCAM.rdSensorReg16_8(OV5642_CHIPID_LOW, &pid);

      if ((vid != 0x56) || (pid != 0x42))
        Serial.println("Can't find OV5642 module!");
      else
        Serial.println("CAM2 detected.");
      /////////////////////////////////////
    }
    Serial.println("CAM2 detected.");
    // Configure JPEG format
    infrablueCAM.set_format(JPEG);
    infrablueCAM.InitCAM();
    infrablueCAM.OV5642_set_JPEG_size(OV5642_2592x1944);
    infrablueCAM.set_bit(ARDUCHIP_TIM, VSYNC_LEVEL_MASK);
    infrablueCAM.set_bit(ARDUCHIP_GPIO, GPIO_PWDN_MASK); //enable low power
    infrablueCAM.clear_fifo_flag();
    infrablueCAM.write_reg(ARDUCHIP_FRAMES, 0x00);
    /////////////////////////////////////

    //
    while (!SD.begin(CS_SD)) {
      Serial.println("SD Card Error");
      delay(500);
    }
    Serial.println("SD Card detected!");
    ////////////////////////////////////////////////////
  }

  rtc.setAlarmTime(12, 0, 0);
  rtc.enableAlarm(rtc.MATCH_HHMMSS);
  rtc.attachInterrupt(second_loop);
}

////////////////////////////////////////////////////////////////////////////////////
void loop() {

  if (useCAM_loop == true) {
    get_table_index();
    capture_normalImage();
    capture_infrablueImage();
  }

  if (useCloud_loop == true) {
    send_normalImage_azure();
    send_infrablueImage_azure();
  }

  if (useSQL_loop == true) {
    get_data();
    //check_alerts();
    send_data_to_azure();
    //sendEmail();
  }

  if (use_app == true) {
    while(1){
      while (Firmata.available()) {
        Serial.println("hi!!!");
        Firmata.processInput();
      }
    }
  }

  Serial.println("END OF LOOP");

  while(1);
  //rtc.standbyMode();
}

void second_loop() {

  // useCAM_loop 
    get_table_index();
    capture_normalImage();
    capture_infrablueImage();
  

  // use Azure Cloud_loop
    send_normalImage_azure();
    send_infrablueImage_azure();
  

  //use SQL
    get_data();
    //check_alerts();
    send_data_to_azure();
    //sendEmail();
  
}

void capture_normalImage() {

  uint64_t temp = 0;
  uint64_t temp_last = 0;
  uint64_t start_capture = 0;

  delay(2000);
  normalCAM.clear_bit(ARDUCHIP_GPIO, GPIO_PWDN_MASK); //Power up Camera
  delay(1000);
  normalCAM.flush_fifo();
  normalCAM.clear_fifo_flag();
  normalCAM.start_capture();
  while (!normalCAM.get_bit(ARDUCHIP_TRIG, CAP_DONE_MASK));
  normalCAM.set_bit(ARDUCHIP_GPIO, GPIO_PWDN_MASK); //enable low power

  Serial.println("Normal Capture Done!");

  read_fifo_burst(normalCAM, "NORM");
  //Clear the capture done flag
  normalCAM.clear_fifo_flag();
  delay(3000);
}

void send_normalImage_azure() {

  // Load Normal Picture
  while (SD.begin(CS_SD));

  String normalFilename = String(table_index) + "NORM.JPG";

  File normPicture = SD.open(normalFilename);

  // Get the size of the image (frame) taken
  while (!normPicture.available());
  long jpglen = normPicture.size();
  Serial.print("Storing ");
  Serial.print(jpglen, DEC);
  Serial.println(" byte image.");

  // Prepare request
  String start_request = "";
  String end_request = "";
  start_request = start_request + "\n" + "--AaB03x" + "\n" + "Content-Type: image/jpeg" + "\n" + "Content-Disposition: form-data; name=\"file\"; filename=\"" + normalFilename + "\"" + "\n" + "Content-Transfer-Encoding: binary" + "\n" + "\n";
  end_request = end_request + "\n" + "--AaB03x--" + "\n";
  long extra_length;
  extra_length = start_request.length() + end_request.length();
  Serial.println("Extra length:");
  Serial.println(extra_length);
  long len = jpglen + extra_length;

  Serial.println(start_request);
  Serial.println("Starting connection to server...");

  // Connect to the server, please change your IP address !
  if (client.connectSSL(flask_hostname, 443)) {
    client.print(F("POST "));
    client.print(F("/"));
    client.println(F(" HTTP/1.1"));
    client.println(F("Host: " + String(flask_hostname)));
    client.println(F("Content-Type: multipart/form-data; boundary=AaB03x"));
    client.print(F("Content-Length: "));
    client.println(len);
    client.print(start_request);

    if (normPicture) {

      int count = 0;
      byte clientBuf[128];
      int clientCount = 0;

      while (normPicture.available()) {
        clientBuf[clientCount] = normPicture.read();
        clientCount++;
        count++;

        if (clientCount > 127) {
          //Serial.println(".");
          client.write(clientBuf, 128);
          clientCount = 0;
        }
      }
      if (clientCount > 0) client.write(clientBuf, clientCount);
      Serial.println(String(count));
      normPicture.close();
    }

    client.print(end_request);
    client.println();

    Serial.println("Transmission over");
  }
  else {
    Serial.println(F("Connection failed"));
  }

  while (client.connected()) {
    while (client.available()) {
      // Read answer
      char c = client.read();
      Serial.print(c);
    }
  }
  delay(3000);
  client.stop();
}

void capture_infrablueImage() {

  uint8_t  temp = 0;
  uint8_t temp_last = 0;
  uint8_t  start_capture = 0;

  delay(2000);
  infrablueCAM.clear_bit(ARDUCHIP_GPIO, GPIO_PWDN_MASK); //Power up Camera
  delay(1000);
  infrablueCAM.flush_fifo();
  infrablueCAM.clear_fifo_flag();
  infrablueCAM.start_capture();
  while (!infrablueCAM.get_bit(ARDUCHIP_TRIG, CAP_DONE_MASK));

  infrablueCAM.set_bit(ARDUCHIP_GPIO, GPIO_PWDN_MASK); //enable low power

  Serial.println("Blue Capture Done!");
  read_fifo_burst(infrablueCAM, "BLUE");
  //Clear the capture done flag
  delay(1000);
  infrablueCAM.clear_fifo_flag();
  delay(3000);
}

void send_infrablueImage_azure() {

  // Load Infrablue Picture
  while (SD.begin(CS_SD));
  String infrablueFilename = String(table_index) + "BLUE.JPG";
  File infrabluePicture = SD.open(infrablueFilename);

  // Get the size of the image (frame) taken
  while (!infrabluePicture.available());
  long jpglen = infrabluePicture.size();
  Serial.print("Storing ");
  Serial.print(jpglen, DEC);
  Serial.println(" byte image.");

  // Prepare request
  String start_request = "";
  String end_request = "";
  start_request = start_request + "\n" + "--AaB03x" + "\n" + "Content-Type: image/jpeg" + "\n" + "Content-Disposition: form-data; name=\"file\"; filename=\"" + infrablueFilename + "\"" + "\n" + "Content-Transfer-Encoding: binary" + "\n" + "\n";
  end_request = end_request + "\n" + "--AaB03x--" + "\n";
  long extra_length;
  extra_length = start_request.length() + end_request.length();
  Serial.println("Extra length:");
  Serial.println(extra_length);
  long len = jpglen + extra_length;

  Serial.println(start_request);
  Serial.println("Starting connection to server...");

  // Connect to the server, please change your IP address !
  if (client.connectSSL(flask_hostname, 443)) {
    client.print(F("POST "));
    client.print(F("/"));
    client.println(F(" HTTP/1.1"));
    client.println(F("Host: " + String(flask_hostname)));
    client.println(F("Content-Type: multipart/form-data; boundary=AaB03x"));
    client.print(F("Content-Length: "));
    client.println(len);
    client.print(start_request);

    if (infrabluePicture) {

      int count = 0;
      byte clientBuf[128];
      int clientCount = 0;

      while (infrabluePicture.available()) {
        clientBuf[clientCount] = infrabluePicture.read();
        clientCount++;
        count++;

        if (clientCount > 127) {
          //Serial.println(".");
          client.write(clientBuf, 128);
          clientCount = 0;
        }
      }
      if (clientCount > 0) client.write(clientBuf, clientCount);
      infrabluePicture.close();
    }

    client.print(end_request);
    client.println();

    Serial.println("Transmission over");
  }
  else {
    Serial.println(F("Connection failed"));
  }

  //Wait for response
  while (!client.available()) {
    if (!client.connected()) {
      return;
    }
  }

  //Read the response and send to serial
  bool print = true;
  while (client.available()) {
    char c = client.read();
    if (c == '\n')
      print = false;
    if (print)
      Serial.print(c);
  }

  //Close the connection
  client.stop();

  /*
    while (client.connected()) {
      while (client.available()) {
        // Read answer
        char c = client.read();
        Serial.print(c);
      }
    }
    delay(1000);
    client.stop();
  */
}

uint8_t read_fifo_burst(ArduCAM myCAM, String CAM_NAME) {

  uint64_t temp = 0;
  uint64_t temp_last = 0;
  uint64_t length = 0;
  static int i = 0;

  //////////////////////////////////////////
  char str[10];
  File outFile;
  byte buf[256];
  String k1 = String(table_index);
  String k2 = CAM_NAME;

  //////////////////////////////////////////
  //Construct a file name
  if(k2 == "BTEMP" || k2 == "NTEMP" ) {
    String k = k2;
    k.toCharArray(str, 10);
    strcat(str, ".jpg");
  }
  else {
    String k = k1 + k2;
    k.toCharArray(str, 10);
    strcat(str, ".jpg");
  }

  //Check if file exists and delete
  if (SD.exists(str)) {
    SD.remove(str);
  }
  delay(2000);
  //Open the new file
  outFile = SD.open(str, O_WRITE | O_CREAT | O_TRUNC);
  if (! outFile)
  {
    Serial.println("open file failed");
    SD.begin(CS_SD);
    //return;
  }

  //////////////////////////////////////////////////
  length = myCAM.read_fifo_length();
  if (length >= 524288) //512 kb
  {
    Serial.println("Not found the end.");
    return 0;
  }
  //Serial.println(length);
  i = 0;
  myCAM.CS_LOW();
  delay(500);
  myCAM.set_fifo_burst();
  SPI.transfer(0x00);//First byte is 0xC0 ,not 0xff

  ////////////////////////////////////////////////////////
  while ( (temp != 0xD9) | (temp_last != 0xFF))
  {
    temp_last = temp;
    temp = SPI.transfer(0x00);

    //Write image data to buffer if not full
    if (i < 256)
      buf[i++] = temp;
    else
    {
      //Write 256 bytes image data to file
      myCAM.CS_HIGH();
      outFile.write(buf, 256);
      i = 0;
      buf[i++] = temp;
      myCAM.CS_LOW();
      myCAM.set_fifo_burst();
      delay(5);
    }
  }

  //Write the remain bytes in the buffer
  if (i > 0)
  {
    myCAM.CS_HIGH();
    outFile.write(buf, i);
  }
  //Close the file
  outFile.close();
  ///////////////////////////////////////////////////////
}


void printTime() {
  print2digits(rtc.getHours());
  Serial.print(":");
  print2digits(rtc.getMinutes());
  Serial.print(":");
  print2digits(rtc.getSeconds());
  Serial.println();
}

void printDate() {
  Serial.print(rtc.getDay());
  Serial.print("/");
  Serial.print(rtc.getMonth());
  Serial.print("/");
  Serial.print(rtc.getYear());
  Serial.print(" ");
}

unsigned long readLinuxEpochUsingNTP() {
  Udp.begin(localPort);
  sendNTPpacket(timeServer); // send an NTP packet to a time server

  // wait to see if a reply is available
  delay(1000);

  if (Udp.parsePacket()) {

    Serial.println("NTP time received");

    // We've received a packet, read the data from it
    Udp.read(packetBuffer, NTP_PACKET_SIZE); // read the packet into the buffer
    //the timestamp starts at byte 40 of the received packet and is four bytes,
    // or two words, long. First, esxtract the two words:
    unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
    unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);

    // combine the four bytes (two words) into a long integer
    // this is NTP time (seconds since Jan 1 1900):
    unsigned long secsSince1900 = highWord << 16 | lowWord;

    // now convert NTP time into everyday time:
    // Unix time starts on Jan 1 1970. In seconds, that's 2208988800:
    const unsigned long seventyYears = 2208988800UL;
    Udp.stop();

    // subtract seventy years:
    // For Central Time, -6 HOURS, or 21600 SECONDS
    return (secsSince1900 - seventyYears - 21600);
  }
  else {
    Udp.stop();
    return 0;
  }
}

// send an NTP request to the time server at the given address
unsigned long sendNTPpacket(IPAddress & address) {
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)

  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;            // Stratum, or type of clock
  packetBuffer[2] = 6;            // Polling Interval
  packetBuffer[3] = 0xEC;         // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;

  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  Udp.beginPacket(address, 123); //NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}

void printWifiStatus() {

  // print the SSID of the network you're attached to:
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print your WiFi shield's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  // print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
}

void print2digits(int number) {
  if (number < 10) {
    Serial.print("0");
  }
  Serial.print(number);
}

void get_table_index() {
  table_index = table_index + 1;
  //read from config file
  //write to config file
}

void get_data() {

  // Get Soil Moisture level
  soilmoisture = analogRead(soil_moisture_sensor);

  // Get humidity
  humidity = humidity_sensor.readHumidity();

  // Get temperature from pressure sensor
  airtemp = temp_sensor.readTempF();

  //Set Image URL based off of table index for data
  normalimageurl = "https://xxxxxflask.azurewebsites.net/uploads/" + String(table_index) + "NORM.JPG";
  infrablueimageurl = "https://xxxxxflask.azurewebsites.net/uploads/" + String(table_index) + "BLUE.JPG";
  ndviimageurl = "https://xxxxxflask.azurewebsites.net/uploads/ndvi_" + String(table_index) + ".JPG";
}

void get_data_for_app() {

  // Get Soil Moisture level
  soilmoisture = analogRead(soil_moisture_sensor);

  // Get humidity
  humidity = humidity_sensor.readHumidity();

  // Get temperature from pressure sensor
  airtemp = temp_sensor.readTempF();

  //Set Image URL based off of table index for data
  normalimageurl = "https://xxxxxflask.azurewebsites.net/uploads/TEMP_NORM.JPG";
  infrablueimageurl = "https://xxxxxflask.azurewebsites.net/uploads/TEMP_BLUE.JPG";
  ndviimageurl = "https://xxxxxflask.azurewebsites.net/uploads/ndvi_TEMP.JPG";
}

void stringCallback(char *windows_string)
{
  String windows_command =String(windows_string);

  if (windows_command == "take_temp_picture") {
    //capture_temp_normal_image();
    capture_temp_infrablue_image();
    send_temp_infrablueImage_azure();
    //send_temp_normal_image_azure();
  }
  
}

void capture_temp_normal_image() {

  uint64_t temp = 0;
  uint64_t temp_last = 0;
  uint64_t start_capture = 0;

  delay(2000);
  normalCAM.clear_bit(ARDUCHIP_GPIO, GPIO_PWDN_MASK); //Power up Camera
  delay(1000);
  normalCAM.flush_fifo();
  normalCAM.clear_fifo_flag();
  normalCAM.start_capture();
  while (!normalCAM.get_bit(ARDUCHIP_TRIG, CAP_DONE_MASK));
  normalCAM.set_bit(ARDUCHIP_GPIO, GPIO_PWDN_MASK); //enable low power

  Serial.println("Normal Capture Done!");

  read_fifo_burst(normalCAM, "NTEMP");
  //Clear the capture done flag
  normalCAM.clear_fifo_flag();
  delay(3000);
}

void capture_temp_infrablue_image() {

  uint8_t  temp = 0;
  uint8_t temp_last = 0;
  uint8_t  start_capture = 0;

  delay(2000);
  infrablueCAM.clear_bit(ARDUCHIP_GPIO, GPIO_PWDN_MASK); //Power up Camera
  delay(1000);
  infrablueCAM.flush_fifo();
  infrablueCAM.clear_fifo_flag();
  infrablueCAM.start_capture();
  while (!infrablueCAM.get_bit(ARDUCHIP_TRIG, CAP_DONE_MASK));

  infrablueCAM.set_bit(ARDUCHIP_GPIO, GPIO_PWDN_MASK); //enable low power

  Serial.println("Blue Capture Done!");
  read_fifo_burst(infrablueCAM, "BTEMP");
  //Clear the capture done flag
  delay(1000);
  infrablueCAM.clear_fifo_flag();
  delay(3000);
}


void send_temp_infrablueImage_azure() {

  // Load Infrablue Picture
  while (SD.begin(CS_SD));
  String infrablueFilename = "BTEMP.JPG";
  File infrabluePicture = SD.open(infrablueFilename);

  // Get the size of the image (frame) taken
  while (!infrabluePicture.available());
  long jpglen = infrabluePicture.size();
  Serial.print("Storing ");
  Serial.print(jpglen, DEC);
  Serial.println(" byte image.");

  // Prepare request
  String start_request = "";
  String end_request = "";
  start_request = start_request + "\n" + "--AaB03x" + "\n" + "Content-Type: image/jpeg" + "\n" + "Content-Disposition: form-data; name=\"file\"; filename=\"" + infrablueFilename + "\"" + "\n" + "Content-Transfer-Encoding: binary" + "\n" + "\n";
  end_request = end_request + "\n" + "--AaB03x--" + "\n";
  long extra_length;
  extra_length = start_request.length() + end_request.length();
  Serial.println("Extra length:");
  Serial.println(extra_length);
  long len = jpglen + extra_length;

  Serial.println(start_request);
  Serial.println("Starting connection to server...");

  // Connect to the server, please change your IP address !
  if (client.connectSSL(flask_hostname, 443)) {
    client.print(F("POST "));
    client.print(F("/"));
    client.println(F(" HTTP/1.1"));
    client.println(F("Host: " + String(flask_hostname)));
    client.println(F("Content-Type: multipart/form-data; boundary=AaB03x"));
    client.print(F("Content-Length: "));
    client.println(len);
    client.print(start_request);

    if (infrabluePicture) {

      int count = 0;
      byte clientBuf[128];
      int clientCount = 0;

      while (infrabluePicture.available()) {
        clientBuf[clientCount] = infrabluePicture.read();
        clientCount++;
        count++;

        if (clientCount > 127) {
          //Serial.println(".");
          client.write(clientBuf, 128);
          clientCount = 0;
        }
      }
      if (clientCount > 0) client.write(clientBuf, clientCount);
      infrabluePicture.close();
    }

    client.print(end_request);
    client.println();

    Serial.println("Transmission over");
  }
  else {
    Serial.println(F("Connection failed"));
  }

  //Wait for response
  while (!client.available()) {
    if (!client.connected()) {
      return;
    }
  }

  //Read the response and send to serial
  bool print = true;
  while (client.available()) {
    char c = client.read();
    if (c == '\n')
      print = false;
    if (print)
      Serial.print(c);
  }

  //Close the connection
  client.stop();

}

void send_data_to_azure() {
  Serial.println("\nconnecting...");

  if (client.connect(mobile_server, 80)) {
    Serial.print("sending everything");

    // POST URI
    sprintf(buffer, "POST /tables/%s HTTP/1.1", table_name);
    client.println(buffer);

    // Host header
    sprintf(buffer, "Host: %s", mobile_server);
    client.println(buffer);

    // Azure Mobile Services application key
    sprintf(buffer, "X-ZUMO-APPLICATION: %s", mobile_app_key);
    client.println(buffer);

    // JSON content type
    client.println("Content-Type: application/json");

    String data_string = "";
    // Content: Data (index, soiltemp, soilmoisture, sunlight, airtemp, humidity, and URLs)
    data_string = data_string + "{\"index\":\"" + table_index + "\",\"soilmoisture\":\"" + soilmoisture + "\",\"airtemp\":\"" + airtemp + "\",\"humidity\":\"" + humidity + "\",\"normalimageurl\":\"" + normalimageurl + "\",\"infrablueimageurl\":\"" + infrablueimageurl + "\",\"ndviimageurl\":\"" + ndviimageurl + "\"}";
    client.print("Content-Length: ");
    client.println(F(data_string.length()));

    // End of headers
    client.println();

    // Request body
    client.println(data_string);
  }
  else {
    Serial.println("connection failed");
  }

  //Wait for response
  while (!client.available()) {
    if (!client.connected()) {
      return;
    }
  }

  //Read the response and dump to serial
  bool print = true;
  while (client.available()) {
    char c = client.read();
    // Print only until the first carriage return
    if (c == '\n')
      print = false;
    if (print)
      Serial.print(c);
  }

  //Close the connection
  client.stop();
}
/*

void check_alerts() {

}

  void sendEmail() {

  Serial.println(F("Sending hello"));
  Serial.println(F("Sending auth login"));


  if(client.connect(email_server,port) == 1) {
    Serial.println(F("connected"));
  } else {
    Serial.println(F("connection failed"));
    return 0;
  }

  if(!eRcv()) return 0;

  // replace 1.2.3.4 with your Arduino's ip
  client.println("EHLO 1.2.3.4");
  if(!eRcv()) return 0;

  client.println("auth login");
  if(!eRcv()) return 0;

  Serial.println(F("Sending User"));
  // Change to your base64 encoded user
  client.println("xxxxxxxxxxxxxx");

  if(!eRcv()) return 0;

  Serial.println(F("Sending Password"));
  // change to your base64 encoded password
  client.println("xxxxx");

  if(!eRcv()) return 0;

  // change to your email address (sender)
  Serial.println(F("Sending From"));
  client.println("MAIL From: <xxxxxx_SMTP@live.com>");
  if(!eRcv()) return 0;

  // change to recipient address
  Serial.println(F("Sending To"));
  client.println("RCPT To: <xxxxxxx.com>");
  if(!eRcv()) return 0;

  Serial.println(F("Sending DATA"));
  client.println("DATA");
  if(!eRcv()) return 0;

  Serial.println(F("Sending email"));

  // change to recipient address
  client.println("To: You <xxxxxx");

  // change to your address
  client.println("From: Me <xxxxx_SMTP@live.com>");

  client.println("Subject: Freeze Alert");

  client.println("Warning! Freezing Temperatures outside!");

  client.println(".");

  if(!eRcv()) return 0;

  Serial.println(F("Sending QUIT"));
  client.println("QUIT");
  if(!eRcv()) return 0;

  client.stop();

  Serial.println(F("disconnected"));

  return 1;
  }

  byte eRcv()
  {
  byte respCode;
  byte thisByte;
  int loopCount = 0;

  while(!client.available()) {
    delay(1);
    loopCount++;

    // if nothing received for 10 seconds, timeout
    if(loopCount > 10000) {
      client.stop();
      Serial.println(F("\r\nTimeout"));
      return 0;
    }
  }

  respCode = client.peek();

  while(client.available())
  {
    thisByte = client.read();
    Serial.write(thisByte);
  }

  if(respCode >= '4')
  {
    efail();
    return 0;
  }

  return 1;
  }


  void efail()
  {
  byte thisByte = 0;
  int loopCount = 0;

  client.println(F("QUIT"));

  while(!client.available()) {
    delay(1);
    loopCount++;

    // if nothing received for 10 seconds, timeout
    if(loopCount > 10000) {
      client.stop();
      Serial.println(F("\r\nTimeout"));
      return;
    }
  }

  while(client.available())
  {
    thisByte = client.read();
    Serial.write(thisByte);
  }

  client.stop();

  Serial.println(F("disconnected"));
  }
*/
