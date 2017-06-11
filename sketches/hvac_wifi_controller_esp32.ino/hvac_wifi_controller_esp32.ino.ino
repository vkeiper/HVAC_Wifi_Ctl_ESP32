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
#include "FS.h"
#include "SD.h"
#include "SPI.h"

// size of buffer used to capture HTTP requests
#define REQ_BUF_SZ   256

//Application Version
const char* sAppVer     = "v0.1 20170609_0311_VK HVAC_Controller_ESP32_Wifi";

const char* ssid         = "NETGEAR26";
const char* password     = "fluffyvalley904";
const char* httphdrstart = "HTTP/1.1 200 OK";
const char* httphdrhtml  = "Content-Type: text/html";
const char* httphdrxml   = "Content-Type: text/xml";
const char* httphdrkeep  = "Connection: keep-alive";
char dbgstr[128];

WiFiServer server(80);
WiFiClient client;

char HTTP_req[REQ_BUF_SZ] = {0}; // buffered HTTP request stored as null terminated string
char req_index = 0;              // index into HTTP_req buffer
//HVAC application variables
const char pinCNDSRMON = 36;  //analog in (frost monitor, condensor)
const char achCNDSRMON =  7;
const char pinAMBMON   = 39;  //analog in (ambient room temp)
const char achAMBMON   =  6;
const char pinTSTAT    = 12;  //dig in
const char pinAUXFAN   = 25;  //dig out
const char pinACMAIN   = 26;  //dig out
int value = 0;
File html1;
File logo;

//HVAC Vars
  float an_condtemp,an_ambtemp;
  int an_ch7raw,an_ch6raw;
  bool tstat_dmd,frost_flt;
  
  String  strTemp;
  String strDmd;
  String strFrost;
/*
 * This table represents the PTC map. Index0
 * indicates -40C and each index is 1 degree 
 * higher than the last.
 */
static const uint16_t ptcbits[] = {993,992,989,987,985,983,980,977,974,971,
    968,965,962,958,954,950,946,942,937,932,
    927,922,917,911,906,900,893,887,880,874,
    866,859,852,844,836,827,819,810,802,792,
    783,774,764,754,744,734,724,713,702,692,
    681,670,659,648,636,625,614,602,591,580,
    568,557,545,534,523,512,500,489,478,467,
    456,446,435,425,414,404,394,384,375,365,
    356,346,337,328,320,311,303,294,286,279,
    271,263,256,249,242,235,229,222,216,210,
    204,198,192,187,182,176,171,166,162,157,
    153,148,144,140,136,132,128,124,121,117,
    114,111,108,105,102,99,96,93,91,88,
    86,83,81,79,77,75,73,71,69,67,
    65,63,62,60,58,57,55,54,52,51,
    50,48,47,46,45,44,42,41,40,39
};

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

    setupSD();
}



void loop(){
  client = server.available();   // listen for incoming clients
  if (client) {                             // if you get a client,
    Serial.println("Initialize new http client");           // print a message out the serial port
    String currentLine = "";                // make a String to hold incoming data from the client
    while (client.connected()) {            // loop while the client's connected
      if (client.available()) {             // if there's bytes to read from the client,
        char c = client.read();             // read a byte, then
        Serial.write(c);                    // print it out the serial monitor
        // buffer first part of HTTP request in HTTP_req array (string)
        // leave last element in array as 0 to null terminate string (REQ_BUF_SZ - 1)
        if (req_index < (REQ_BUF_SZ - 1)) {
            HTTP_req[req_index] = c;          // save HTTP request character
            req_index++;
        }
        if (c == '\n') {                    // if the byte is a newline character
          
          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
          if (currentLine.length() == 0) {
            
            // remainder of header follows below, depending on if
            // web page or XML page is requested
            // Ajax request - send XML file
            // GET /ajax_vars&nocache=299105.2747379479 HTTP/1.1
            if (StrContains(HTTP_req, "GET /ajax_vars")) {
                // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
                // and a content-type so the client knows what's coming, then a blank line
                // send a standard http response header
                client.println(httphdrstart);
                // send rest of HTTP header
                client.println(httphdrxml);
                client.println(httphdrkeep);
                client.println();
                // send XML file containing input states
                XML_response(client);
            }
            
            else if (StrContains(HTTP_req, "GET / ")
                                 || StrContains(HTTP_req, "GET /index.html")
                                 || StrContains(HTTP_req, "GET /OFF_")
                                 || StrContains(HTTP_req, "GET /ON_"))
                                 {
              // send web page
              html1 = SD.open("/index.html");        // open web page file
              if (html1) {
                 // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
                 // and a content-type so the client knows what's coming, then a blank line
                 // send a standard http response header
                 client.println(httphdrstart);
                 client.println(httphdrhtml);
                 client.println(httphdrkeep);
                 client.println();
                 while(html1.available()) {
                      client.write(html1.read()); // send web page to client
                  }
                  html1.close();
              }
            }
              // remainder of header follows below, depending on if
              // web page or XML page is requested
              // Ajax request - send XML file
            else if (StrContains(HTTP_req, "atdilogo.bmp")) {
                // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
                 // and a content-type so the client knows what's coming, then a blank line
                 // send a standard http response header
                 client.println(httphdrstart);
                 Serial.println("[WEBPG] GET Logo, sending it");
                // send logo .bmp file
                logo = SD.open("/atdilogo.bmp"); // open bmp file
                if (logo) {
                    while(logo.available()) {
                        client.write(logo.read()); // send logo to client
                    }
                    logo.close();
                }
            }
            //Respond to favico for chrome
            else if (StrContains(HTTP_req, "GET /favicon.ico")){
                client.println("HTTP/1.0 404 \r\n\n\n");
            }
            
            // display received HTTP request on serial port
            Serial.print(HTTP_req);
            // reset buffer index and all buffer elements to 0
            req_index = 0;
            StrClear(HTTP_req, REQ_BUF_SZ);

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
      delay(1);      // give the web browser time to receive the data
    }
    // close the connection:
    client.stop();
    Serial.println("client disonnected");
  }
}

void setupPtcs()
{
   /* 
    *  Attach pin to ADC (will also clear any other analog mode that could be on)
     */
    if( adcAttachPin(pinCNDSRMON)){
      Serial.println("\r\npinA7 Attached to ADC");
    }else{
      Serial.println("\r\nFAILED pinA7 Attached to ADC");
    }

    if( adcAttachPin(pinAMBMON)){
      Serial.println("\r\npinA6 Attached to ADC");
    }else{
      Serial.println("\r\nFAILED pinA6 Attached to ADC");
    }


    if( adcStart(pinCNDSRMON)){
      Serial.println("\r\npinA7 Started ADC");
    }else{
      Serial.println("\r\nFAILED pinA7 Started ADC");
    }
    
    if( adcStart(pinAMBMON)){
      Serial.println("\r\npinA6 Started ADC");
    }else{
      Serial.println("\r\nFAILED pinA6 Started ADC");
    }

}

void setupSSRs()
{
  pinMode(pinTSTAT,INPUT);
  pinMode(pinAUXFAN,OUTPUT);
  pinMode(pinACMAIN,OUTPUT);

  digitalWrite(pinAUXFAN,HIGH);
  digitalWrite(pinACMAIN,HIGH);
  

}


//Start SD Card funcs
void listDir(fs::FS &fs, const char * dirname, uint8_t levels){
    Serial.printf("Listing directory: %s\n", dirname);

    File root = fs.open(dirname);
    if(!root){
        Serial.println("Failed to open directory");
        return;
    }
    if(!root.isDirectory()){
        Serial.println("Not a directory");
        return;
    }

    File file = root.openNextFile();
    while(file){
        if(file.isDirectory()){
            Serial.print("  DIR : ");
            Serial.println(file.name());
            if(levels){
                listDir(fs, file.name(), levels -1);
            }
        } else {
            Serial.print("  FILE: ");
            Serial.print(file.name());
            Serial.print("  SIZE: ");
            Serial.println(file.size());
        }
        file = root.openNextFile();
    }
}

void createDir(fs::FS &fs, const char * path){
    Serial.printf("Creating Dir: %s\n", path);
    if(fs.mkdir(path)){
        Serial.println("Dir created");
    } else {
        Serial.println("mkdir failed");
    }
}

void removeDir(fs::FS &fs, const char * path){
    Serial.printf("Removing Dir: %s\n", path);
    if(fs.rmdir(path)){
        Serial.println("Dir removed");
    } else {
        Serial.println("rmdir failed");
    }
}

void readFile(fs::FS &fs, const char * path){
    Serial.printf("Reading file: %s\n", path);

    File file = fs.open(path);
    if(!file){
        Serial.println("Failed to open file for reading");
        return;
    }

    Serial.print("Read from file: ");
    while(file.available()){
        Serial.write(file.read());
    }
}

void writeFile(fs::FS &fs, const char * path, const char * message){
    Serial.printf("Writing file: %s\n", path);

    File file = fs.open(path, FILE_WRITE);
    if(!file){
        Serial.println("Failed to open file for writing");
        return;
    }
    if(file.print(message)){
        Serial.println("File written");
    } else {
        Serial.println("Write failed");
    }
}

void appendFile(fs::FS &fs, const char * path, const char * message){
    Serial.printf("Appending to file: %s\n", path);

    File file = fs.open(path, FILE_APPEND);
    if(!file){
        Serial.println("Failed to open file for appending");
        return;
    }
    if(file.print(message)){
        Serial.println("Message appended");
    } else {
        Serial.println("Append failed");
    }
}

void renameFile(fs::FS &fs, const char * path1, const char * path2){
    Serial.printf("Renaming file %s to %s\n", path1, path2);
    if (fs.rename(path1, path2)) {
        Serial.println("File renamed");
    } else {
        Serial.println("Rename failed");
    }
}

void deleteFile(fs::FS &fs, const char * path){
    Serial.printf("Deleting file: %s\n", path);
    if(fs.remove(path)){
        Serial.println("File deleted");
    } else {
        Serial.println("Delete failed");
    }
}

void testFileIO(fs::FS &fs, const char * path){
    File file = fs.open(path);
    static uint8_t buf[512];
    size_t len = 0;
    uint32_t start = millis();
    uint32_t end = start;
    if(file){
        len = file.size();
        size_t flen = len;
        start = millis();
        while(len){
            size_t toRead = len;
            if(toRead > 512){
                toRead = 512;
            }
            file.read(buf, toRead);
            len -= toRead;
        }
        end = millis() - start;
        Serial.printf("%u bytes read for %u ms\n", flen, end);
        file.close();
    } else {
        Serial.println("Failed to open file for reading");
    }


    file = fs.open(path, FILE_WRITE);
    if(!file){
        Serial.println("Failed to open file for writing");
        return;
    }

    size_t i;
    start = millis();
    for(i=0; i<2048; i++){
        file.write(buf, 512);
    }
    end = millis() - start;
    Serial.printf("%u bytes written for %u ms\n", 2048 * 512, end);
    file.close();
}

 void setupSD(){
    Serial.begin(115200);
    if(!SD.begin()){
        Serial.println("Card Mount Failed");
        return;
    }else{
        Serial.println("Card Mounted!!");
    }
    uint8_t cardType = SD.cardType();

    if(cardType == CARD_NONE){
        Serial.println("No SD card attached");
        return;
    }else{
        Serial.println("SD card attached  ");
    }

    Serial.print("SD Card Type: ");
    if(cardType == CARD_MMC){
        Serial.println("MMC");
    } else if(cardType == CARD_SD){
        Serial.println("SDSC");
    } else if(cardType == CARD_SDHC){
        Serial.println("SDHC");
    } else {
        Serial.println("UNKNOWN");
    }

    uint64_t cardSize = SD.cardSize() / (1024 * 1024);
    Serial.printf("SD Card Size: %lluMB\n", cardSize);

    listDir(SD, "/", 0);
    
    // check for index.htm file
    if (!SD.exists("/index.html")) {
        Serial.println("ERROR - Can't find index.htm file!");
        return;  // can't find index file
    }else{
      readFile(SD, "/index.html");
      //readFile(SD, "/atdilogo.bmp");
      
    }
   // createDir(SD, "/mydir");
   //listDir(SD, "/", 0);
   // removeDir(SD, "/mydir");
   // listDir(SD, "/", 2);
   //writeFile(SD, "/hello.txt", "Hello ");
   //appendFile(SD, "/hello.txt", "World!\n");
   
    //deleteFile(SD, "/foo.txt");
    //renameFile(SD, "/hello.txt", "/foo.txt");
    //readFile(SD, "/foo.txt");
    //testFileIO(SD, "/test.txt");
}
//end SD Card func



//Start String funcs
// sets every element of str to 0 (clears array)
void StrClear(char *str, char length)
{
    for (int i = 0; i < length; i++) {
        str[i] = 0;
    }
}

// searches for the string sfind in the string str
// returns 1 if string found
// returns 0 if string not found
char StrContains(char *str, char *sfind)
{
    char found = 0;
    char index = 0;
    char len;

    len = strlen(str);
    
    if (strlen(sfind) > len) {
        return 0;
    }
    while (index < len) {
        if (str[index] == sfind[found]) {
            found++;
            if (strlen(sfind) == found) {
                return 1;
            }
        }
        else {
            found = 0;
        }
        index++;
    }

    return 0;
}

//End String funcs



/*
 * PAss in ADC results scaled at 1023 and it will return
 * a temp in degC from -40 to 120 based on ADC reading.
 */
int8_t ProcessPtc(uint16_t scale,uint16_t adc, int8_t cal)
{
    uint16_t scBits=0;
    int i;
    Serial.println("[PTC] ADCbits" +String(adc));
    for (i=0;i<159;i++)
    {
        if(scale == 1023){
          scBits = ptcbits[i];
        }else{
          scBits = ptcbits[i]*4;
        }
        if( scBits < adc)
        { 
            Serial.println("[PTC] Exit Ival" +String(i));
            return ((i-40)+cal);// origi-40;, added more offset to correct ADC issue on ESP32
        }else{
            if( i >159){
                return 120;//return MAX 
            }
        }
    }
}

// send the XML file with switch statuses and analog value
void XML_response(WiFiClient cl)
{
    String strbuff;
    
    int analog_val;
   
    an_ch7raw = analogRead(pinCNDSRMON);
    an_ch6raw = analogRead(pinAMBMON);
    
    an_condtemp = (float)ProcessPtc(4095,an_ch7raw,-6);
    an_ambtemp = (float)ProcessPtc(4095,an_ch6raw,-6);
    sprintf(&dbgstr[0],"FRST  RAW %d bits Scaled %2.2f degC %2.2f degF\r\n",
        an_ch7raw,an_condtemp, an_condtemp* 9/5 + 32);
    Serial.print(dbgstr);

    sprintf(&dbgstr[0],"[XML] FRST RAW %d bits Scaled %2.2f degC %2.2f degF",
        an_ch6raw,an_ambtemp, an_ambtemp* 9/5 + 32);
    Serial.print(dbgstr);

    strbuff = String("<?xml version = \"1.0\" ?>");
    strbuff = String(strbuff + "<ajax_vars>");
    strbuff = String(strbuff + "<digCh_1>");
    if(digitalRead(pinTSTAT) == false){
        strbuff = String(strbuff + "COOL");
    }
    else {
        strbuff = String(strbuff + "OFF");
    }
    strbuff = String(strbuff + "</digCh_1>");
    
    strbuff = String(strbuff + "<digCh_2>");
    if( frost_flt == true){
      strbuff = String(strbuff + "ERR");
    }
    else {
        strbuff = String(strbuff + "---");
    }
    strbuff = String(strbuff + "</digCh_2>");
    
    // read analog pin 7 (AC Condensor temp)
    //analog_val = analogRead(7);
    strbuff = String(strbuff + "<anCh_1>");
    strbuff = String(strbuff +"RAW " +String(an_ch7raw) +" " + String(an_condtemp));
    strbuff = String(strbuff +"degC " + String(an_condtemp* 9/5 + 32) +"degF");
    strbuff = String(strbuff + "</anCh_1>");
    
    // read analog pin 6 (Ambient Room temp)
    strbuff = String(strbuff + "<anCh_2>");
    strbuff = String(strbuff +"RAW " +String(an_ch6raw) +" " + String(an_ambtemp));
    strbuff = String(strbuff +"degC " + String(an_ambtemp* 9/5 + 32) +"degF");
    strbuff = String(strbuff + "</anCh_2>");
    
    strbuff = String(strbuff + "</ajax_vars>");
    Serial.println("[\r\nXML SEND] START\r\n");
    Serial.print(strbuff);
    Serial.println("\r\n[XML SEND] END\r\n");
    client.print(strbuff);
}


