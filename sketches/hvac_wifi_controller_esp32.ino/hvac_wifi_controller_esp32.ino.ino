/*
  WiFi Web Server LED Blink

 A simple web server that lets you blink an LED via the web.
 This sketch will print the IP address of your WiFi Shield (once connected)
 to the Serial monitor. From there, you can open that address in a web browser
 to turn on and off the LED on pin 5.

 If the IP address of your shield is yourAddress:
 http://yourAddress/H turns the LED on
 http://yourAddress/L turns it off

 This example is written for a network using WPA encryption. For
 WEP or WPA, change the Wifi.begin() call accordingly.

 Circuit:
 * WiFi shield attached
 * LED attached to pin 5

 created for arduino 25 Nov 2012
 by Tom Igoe

ported for sparkfun esp32 
31.01.2017 by Jan Hendrik Berlin
 
 */

#include <WiFi.h>

const char* sAppVer  = "v0.1 20170609_0134_VK HVAC_Controller_ESP32_Wifi";
const char* ssid     = "NETGEAR26";
const char* password = "fluffyvalley904";

WiFiServer server(80);

//HVAC application variables
const char pinCNDSRMON = 27;  //analog in (frost monitor, condensor)
const char achCNSRMON  =  7;
const char pinAMBMON   = 14;  //analog in (ambient room temp)
const char achAMBMON   =  6;
const char pinTSTAT    = 12;  //dig in
const char pinAUXFAN   = 25;  //dig out
const char pinACMAIN   = 26;  //dig out

void setup()
{
    Serial.begin(115200);
    //pinMode(25, OUTPUT);      // set the SSR_AUXFAN
    //pinMode(26, OUTPUT);      // set the SSR_ACMAIN
    setupPtcs();
    setupSSRs();
    
    delay(10);

    // We start by connecting to a WiFi network

    Serial.println();
    Serial.println(sAppVer);
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(ssid);
    Serial.print("Using ");
    Serial.print(password);
    Serial.print("\r\n");

    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
    
    server.begin();
}

int value = 0;

void loop(){
  char dbgstr[128];
  float an_condtemp,an_ambtemp;
  int an_ch7raw,an_ch6raw;
  bool tstat_dmd,frost_flt;
  
  String  strTemp;
  String strDmd;
  String strFrost;
 WiFiClient client = server.available();   // listen for incoming clients

  if (client) {                             // if you get a client,
    Serial.println("Initialize new http client");           // print a message out the serial port
    String currentLine = "";                // make a String to hold incoming data from the client
    while (client.connected()) {            // loop while the client's connected
      if (client.available()) {             // if there's bytes to read from the client,
        char c = client.read();             // read a byte, then
        Serial.write(c);                    // print it out the serial monitor
        if (c == '\n') {                    // if the byte is a newline character

          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
          if (currentLine.length() == 0) {
            // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
            // and a content-type so the client knows what's coming, then a blank line:
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println();

            // the content of the HTTP response follows the header:
            client.print( "<title>WiFi Linked HVAC Controller</title>");
            client.print("<meta http-equiv=\"refresh\"content=\"1\">");
            client.print("<h1>MBR HVAC Web Server</h1>");
            client.print("<p>SSR_AUXFAN <a href=\"/ON_AUXFAN\">-ON-</a> <a href=\"/OFF_AUXFAN\">-OFF-</a><br></p>");
            client.print("<p>SSR_ACMAIN <a href=\"/ON_ACMAIN\">-ON-</a> <a href=\"/OFF_ACMAIN\">-OFF-</a><br></p>");

            an_ch7raw = analogRead(achCNSRMON);
            an_ch6raw = analogRead(achAMBMON);
            
            an_condtemp = (float)an_ch7raw * (300.00/4095.00);
            an_ambtemp = (float)an_ch6raw * (300.00/4095.00);
            sprintf(&dbgstr[0],"<p>Temp sensor:  RAW %d bits &nbsp Scaled %2.2f degC %2.2f degF</p>",
                an_ch7raw,an_condtemp, an_condtemp* 9/5 + 32);
            client.print(dbgstr);

            sprintf(&dbgstr[0],"<p>Ambient Temp sensor:  RAW %d bits &nbsp Scaled %2.2f degC %2.2f degF</p>",
                an_ch6raw,an_ambtemp, an_ambtemp* 9/5 + 32);
            client.print(dbgstr);
   
            if(digitalRead(pinTSTAT) == false)
            {
                strDmd = String( "COOL");
            }
            else
            {
                strDmd = String("OFF");
            }
            
            if( frost_flt == true)
            {
                strFrost = String("ERR"); 
            }
            else
            {
                strFrost = String("---");
            }
            
            strTemp = String("<p> TSTAT " +strDmd +"&nbspFROST &nbsp" +strFrost +"</p>");
            client.print(strTemp);
               
            // The HTTP response ends with another blank line:
            client.println();
            // break out of the while loop:
            break;
          } else {    // if you got a newline, then clear currentLine:
            currentLine = "";
          }
        } else if (c != '\r') {  // if you got anything else but a carriage return character,
          currentLine += c;      // add it to the end of the currentLine
        }

        // Check if request was AUXFAN ON or OFF
        if (currentLine.endsWith("GET /ON_AUXFAN")) {
          digitalWrite(pinAUXFAN, HIGH);              
        }
        else if (currentLine.endsWith("GET /OFF_AUXFAN")) {
          digitalWrite(pinAUXFAN, LOW);                
        }

        // Check if request was ACMAIN ON or OFF
        else if (currentLine.endsWith("GET /ON_ACMAIN")) {
          digitalWrite(pinACMAIN, HIGH);              
        }
        else if (currentLine.endsWith("GET /OFF_ACMAIN")) {
          digitalWrite(pinACMAIN, LOW);               
        }
      }
    }
    // close the connection:
    client.stop();
    Serial.println("client disonnected");
  }
}

void setupPtcs()
{
  pinMode(A7, INPUT);      // set PTC1 (FROST DETECT)
  pinMode(A6,INPUT);  //analog in (ambient room temp)
}

void setupSSRs()
{
  pinMode(pinTSTAT,INPUT);
  pinMode(pinAUXFAN,OUTPUT);
  pinMode(pinACMAIN,OUTPUT);

  digitalWrite(pinAUXFAN,HIGH);
  digitalWrite(pinACMAIN,HIGH);
  

}





