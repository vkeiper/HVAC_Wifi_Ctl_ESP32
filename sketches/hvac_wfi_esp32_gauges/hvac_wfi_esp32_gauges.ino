/*
  WiFi Web Server HVAC controller with frost control 

 A web server that utilizes Ajax to update web page dynamically.

 This example is written for a network using WPA encryption. For
 WEP or WPA, change the Wifi.begin() call accordingly.

For esp32 
20170612 by Vince Keiper
 
 */

#include <WiFi.h>
#include "FS.h"
#include "SD.h"
#include "SPI.h"

//#define DBGWEB1 //shows char by char that http req
//#define DBGWEB2
//#define DBGWEB3
//#define DBGWEB4 // show com[lete http req
//#define DBGFROST1
//#define DBGFROST2
#define DBGFROST3  //shows when frost flt is asserted
//#define DMGFROST4
//#define DBGTSTAT1
//#define DBGTSTAT2
//#define DBGTSTAT3
//#define DBGXMLVARS1
//#define DBGXMLGAUGE1
//#define DBGWEBPG1
//#define DBGWEBPG2
//#define DBGWEBPG3
//#define DBGWEBPG4

//Application Version
const char* sAppVer     = "v0.1 20170611_1641_VK HVAC_Controller_ESP32_Wifi";
#define HOME
#ifdef HOME
//Home Wifi
const char* ssid         = "NETGEAR26";
const char* password     = "fluffyvalley904";
#else
//--ATDI Wifi
const char* ssid         = "Astrodyne";
const char* password     = "Astro$01";
#endif

const char* httphdrstart = "HTTP/1.1 200 OK";
const char* httphdrhtml  = "Content-Type: text/html";
const char* httphdrxml   = "Content-Type: text/xml";
const char* httphdrimg   = "Content-Type:  image/png";
const char* httphdrjs    = "Content-Type: application/x-javascript";
const char* httphdrkeep  = "Connection: keep-alive";
const char* httphdr404   = "HTTP/1.1 404 Not Found";
char dbgstr[128];

WiFiServer server(80);
WiFiClient client;

String HTTP_req;// buffered HTTP request stored as null terminated string

//HVAC application variables
const char pinCNDSRMON = 36;  //analog in (frost monitor, condensor)
const char achCNDSRMON =  7;
const char pinAMBMON   = 39;  //analog in (ambient room temp)
const char achAMBMON   =  6;
const char pinTSTAT    = 2;  //dig in
const char pinAUXFAN   = 25;  //dig out
const char pinACMAIN   = 26;  //dig out
int value = 0;
File html1;
File logo;
File tmp_file;
//HVAC Vars
  float an_condtemp,an_ambtemp,an_cnddegf,an_ambdegf;
  int an_ch7raw,an_ch6raw;
  bool tstat_dmd,frost_flt,mn_auxfan,mn_acmain;
  bool acmain_on = false;  
  String  strTemp;
  String strDmd;
  String strFrost;
float tempSet1 =65.00;//if tempSet2 =0 then in use 24/7 else 4PM to 8AM
float tempSet2 =0;//8:01AM to 3:59PM

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
    setupPtcs();
    setupSSRs();
    setupTmrFrost();
    
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
  bool bLoadIndex = false;
  int iSpLoc1=0,iSpLoc2=0;
  String strRxdTemp ="";
  
  client = server.available();   // listen for incoming clients
  if (client) {                             // if you get a client,
    boolean currentLineIsBlank = true;
    //Serial.println("Initialize new http client");           // print a message out the serial port
    String currentLine = "";                // make a String to hold incoming data from the client
    while (client.connected()) {            // loop while the client's connected
      if (client.available()) {             // if there's bytes to read from the client,
        char c = client.read();             // read a byte, then
        #ifdef DBGWEB1
            Serial.write(c);                    // print it out the serial monitor
        #endif
        HTTP_req += c;  // save the HTTP request 1 char at a time
        
        // last line of client request is blank and ends with \n
        // respond to client only after last line received
        if (c == '\n' && currentLineIsBlank) {
            // example GET /ajax_vars&nocache=299105.2747379479 HTTP/1.1
            if (HTTP_req.indexOf("GET /ajax_vars") > -1) {
                client.println(httphdrstart);
                client.println(httphdrxml);
                client.println(httphdrkeep);
                client.println();
                // send XML file containing input states
                XML_response(client);
            }
            
            // example.. GET /ajax_temp&nocache=299105.2747379479 HTTP/1.1
            else if (HTTP_req.indexOf("GET /ajax_temp") > -1) {
                client.println(httphdrstart);
                client.println(httphdrxml);
                client.println(httphdrkeep);
                client.println();
                // send XML file containing input states
                XML_GaugeResp(client);
            }

            // example.. GET /ajax_temp&nocache=299105.2747379479 HTTP/1.1
            else if (HTTP_req.indexOf("GET /ajax_chgsetpoint") > -1) {
                client.println(httphdrstart);
                client.println(httphdrxml);
                client.println(httphdrkeep);
                client.println();
                // Look for set point xml start tag <SetPoint> "0x3C = '<' "0x3E = '>'
                iSpLoc1 = HTTP_req.indexOf("%3CSetPoint%3E")+14;
                // Look for set point xml end tag </SetPoint> "0x3C = '<' "0x3E = '>'
                iSpLoc2 = HTTP_req.indexOf("%3C/SetPoint%3E");
                strRxdTemp = HTTP_req.substring(iSpLoc1,iSpLoc2);
                Serial.print("[WEBPG] GET ajax_chgsetpoint Set Point: ");
                Serial.print(strRxdTemp);
                Serial.println(" degF");
                Serial.print("IdxStartTag: ");
                Serial.print(iSpLoc1);
                Serial.println();
                Serial.print("IdxEndTag: ");
                Serial.print(iSpLoc2 );
                Serial.println(" END \n");
                if(strRxdTemp != "")
                {
                    tempSet1 = strRxdTemp.toFloat();
                }
            }
            
            /* GET / with a space following is a when just the IP is request comes in*/
            else if (HTTP_req.indexOf("GET / ") > -1){
                bLoadIndex = true;
            }
            else if (HTTP_req.indexOf("GET /index.html") > -1){
                bLoadIndex = true;
            }
            else if (HTTP_req.indexOf("GET /_AUXSSR=1") > -1){
                bLoadIndex = true;
                mn_auxfan = true;
                Serial.println("[WEBPG] GET /AUXSSR=1\n\n\n\n\n\n\n\n\n\\n\n\n");
            }else if (HTTP_req.indexOf("GET /_AUXSSR=0") > -1){
                bLoadIndex = true;
                mn_auxfan = false;
                Serial.println("[WEBPG] GET /AUXSSR=0\n\n\n\n\n\n\n\n\n\n\n\n");
            }
            else if (HTTP_req.indexOf("GET /_ACSSR=1") > -1){
                bLoadIndex = true;
                mn_acmain = true;
                Serial.println("[WEBPG] GET /ACSSR=1\n\n\n\n\n\n\n\n\n\\n\n\n");
            }else if (HTTP_req.indexOf("GET /_ACSSR=0") > -1){
                bLoadIndex = true;
                mn_acmain = false;
                Serial.println("[WEBPG] GET /ACSSR=0\n\n\n\n\n\n\n\n\n\n\n\n");
            }
              // logo requested
            else if (HTTP_req.indexOf("atdilogo.bmp")> -1) {
                logo = SD.open("/atdilogo.bmp"); // open bmp file
                if (logo) {
                    client.println(httphdrstart);
                    client.println(httphdrimg);
                    client.println(httphdrkeep);
                    client.println();
                    Serial.println("[WEBPG] GET Logo, sending it");
                    while(logo.available()) {
                        client.write(logo.read()); // send logo to client
                    }
                    logo.close();
                    Serial.println("[WEBPG] GET Logo, DONE");
                    
                }else{
                  Serial.println("[WEBPG] Logo Not Found!!!");
                }
            }
            //Respond to favicon.ico for chrome
            else if (HTTP_req.indexOf("GET /favicon.ico") > -1){
                client.println("HTTP/1.0 404 \r\n\n\n");
            }
            else if (HTTP_req.indexOf("GET /gauge.min.js") > -1)
            {
                // send file
                tmp_file = SD.open("/gauge.min.js");
                Serial.println("[WEB PAGE] Rxd gauge.min.js\r\n");  
                if (tmp_file) {
                   client.println(httphdrstart);
                   client.println(httphdrjs);
                   client.println(httphdrkeep);
                   client.println();
                   while(tmp_file.available()) {
                        client.write(tmp_file.read()); // send web page to client
                    }
                    tmp_file.close();
                }else{
                  Serial.println("[FILE] ERROR DID NOT OPEN gauge.min.js");
                  
                }
            }
            else if (HTTP_req.indexOf("GET /gauge_qry.js") > -1)
            {
                Serial.println("[WEB PAGE JS] Rxd gauge_qry.js\r\n");
                tmp_file = SD.open("/gauge_qry.js");  
                if (tmp_file) {
                   Serial.println("[WEB PAGE JS OPENED] gauge_qry.js\r\n");  
                   client.println(httphdrstart);
                   client.println(httphdrjs);
                   client.println(httphdrkeep);
                   client.println();
                   while(tmp_file.available()) {
                        client.write(tmp_file.read()); // send web page to client
                    }
                    tmp_file.close();
                }else{
                  Serial.println("[FILE] ERROR DID NOT OPEN gauge.min.js");
                }
            }else{
                  Serial.println("[404 ERROR] No webpage found");
                 client.println(httphdr404);
                 client.println();
            }
            
            if(bLoadIndex == true){
                bLoadIndex = false;
            
                Serial.println("[WEB PAGE] Rxd index.html\r\n");
                // send web page
                html1 = SD.open("/index.html");        // open web page file
                if (html1) {
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
            #ifdef DBGWEBPG4
              // display received HTTP request on serial port
              Serial.println("\r\n[WEB PAGE START] Received buffer");
              Serial.print(HTTP_req);
              Serial.println("[WEB PAGE END] Received buffer \r\n");
            #endif
            
            /*Check if request was AUXFAN ON or OFF
            if (HTTP_req.indexOf("GET /ON_AUXFAN")> -1) {
              mn_auxfan = true;
            }
            else if (HTTP_req.indexOf("GET /OFF_AUXFAN")> -1) {
              mn_auxfan = false;
              Serial.println("TURN OFF AUXFAN\n\n\n\n\\n\n\n");
            }
            // Check if request was ACMAIN ON or OFF
            else if (HTTP_req.indexOf("GET /ON_ACMAIN")> -1) {
              mn_acmain = true;
            }
            else if (HTTP_req.indexOf("GET /OFF_ACMAIN")> -1) {
              mn_acmain = false;
              Serial.println("TURN OFF AUXFAN\n\n\n\n\\n\n\n");
            }
            */
            // clear buffer
            HTTP_req = "";
            
            // break out of the while loop:
            break;        
      }
      // every line of text received from the client ends with \r\n
      if (c == '\n') {
          // last character on line of received text
          // starting new line with next character read
          currentLineIsBlank = true;
      } 
      else if (c != '\r') {
          // a text character was received from client
          currentLineIsBlank = false;
      }
      delay(1);      // give the web browser time to receive the data
    
    }//end if client connected
  } //end while loop 
    // close the connection:
    client.stop();
    //Serial.println("client disonnected");
  }//end if client
  
  chkTmrFrostSemi();
}//end loop

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
      //readFile(SD, "/index.html");   
    }

    // check for index.htm file
    if (!SD.exists("/gauge.min.js")) {
        Serial.println("ERROR - Can't find gauge.min.js file!");
        return;  // can't find index file
    }else{
      //readFile(SD, "/gauge.min.js");   
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
    //Serial.println("[PTC] ADCbits" +String(adc));
    for (i=0;i<159;i++)
    {
        if(scale == 1023){
          scBits = ptcbits[i];
        }else{
          scBits = ptcbits[i]*4;
        }
        if( scBits < adc)
        { 
            //Serial.println("[PTC] Exit Ival" +String(i));
            return ((i-40)+cal);// origi-40;, added more offset to correct ADC issue on ESP32
        }else{
            if( i >159){
                return 120;//return MAX 
            }
        }
    }
}


hw_timer_t * tmrFrost = NULL;
volatile SemaphoreHandle_t timerSemaphore;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

volatile uint32_t isrCounter = 0;
volatile uint32_t lastIsrAt = 0;


void IRAM_ATTR onTimer(){
  // Increment the counter and set the time of ISR
  portENTER_CRITICAL_ISR(&timerMux);
  isrCounter++;
  lastIsrAt = millis();
  portEXIT_CRITICAL_ISR(&timerMux);
  // Give a semaphore that we can check in the loop
  xSemaphoreGiveFromISR(timerSemaphore, NULL);
  // It is safe to use digitalRead/Write here if you want to toggle an output
}

void setupTmrFrost() {
  // Create semaphore to inform us when the timer has fired
  timerSemaphore = xSemaphoreCreateBinary();

  // Use 1st timer of 4 (counted from zero).
  // Set 80 divider for prescaler (see ESP32 Technical Reference Manual for more
  // info).
  tmrFrost = timerBegin(0, 80, true);

  // Attach onTimer function to our timer.
  timerAttachInterrupt(tmrFrost, &onTimer, true);

  // Set alarm to call onTimer function every half second (value in microseconds).
  // Repeat the alarm (third parameter)
  timerAlarmWrite(tmrFrost, 500000, true);

  // Start an alarm
  timerAlarmEnable(tmrFrost);

  timerStart(tmrFrost);
}

void chkTmrFrostSemi() {
  // If Timer has fired
  if (xSemaphoreTake(timerSemaphore, 0) == pdTRUE){
    uint32_t isrCount = 0, isrTime = 0;
    // Read the interrupt count and time
    portENTER_CRITICAL(&timerMux);
    isrCount = isrCounter;
    isrTime = lastIsrAt;
    portEXIT_CRITICAL(&timerMux);
    // Print it
    #ifdef DNGFROST1
        Serial.print("Frost Timer onTimer no. ");
        Serial.print(isrCount);
        Serial.print(" at ");
        Serial.print(isrTime);
        Serial.println(" ms");
    #endif
    //Update Analog Vars, te
    FrostCheck();
    SetAcMainAuxFan();
  }
}

//--method to toggle pins for SSR control
void FrostCheck(void)
{
  static bool bFrostEmin;
   
  //--last status for th pin passed in
    an_ch7raw = analogRead(pinCNDSRMON);
    an_ch6raw = analogRead(pinAMBMON);
    
    an_condtemp = (float)ProcessPtc(4095,an_ch7raw,-6);
    an_ambtemp = (float)ProcessPtc(4095,an_ch6raw,-6);
    an_ambdegf = an_condtemp* 9/5 + 32;
    an_cnddegf = an_ambtemp* 9/5 + 32;

    #ifdef DBGFROST2
        //sprintf(&dbgstr[0],"FRST  RAW %d bits Scaled %2.2f degC %2.2f degF\r\n",
        //    an_ch7raw,an_condtemp, an_cnddegf);
        //Serial.print(dbgstr);
    
        //sprintf(&dbgstr[0],"[FRST] RAW %d bits Scaled %2.2f degC %2.2f degF",
        //    an_ch6raw,an_ambtemp, an_ambdegf);
        //Serial.print(dbgstr);
    #endif
  
  if( an_condtemp <1 && bFrostEmin == false){
    bFrostEmin = true;
    #ifdef DBGFROST3
        Serial.println("FROST EMINENT, SHUTDOWN ACPUMP & WAIT FOR WARMUP");
    #endif
    frost_flt = true;
  
  }else{
    //If we were in Frost Fault call to test for 
    if(frost_flt == true){
        frost_flt = bRestoreACMain();
    }
    #ifdef DBGFROST4
      Serial.println("NO FROST COND. NOMAL OP MODE");
    #endif
  }
}

//--method to restart the AC after warmup period
bool bRestoreACMain(void)
{
   bool retval = false;
   //build in hysterisis for turn back on
    if( an_condtemp >5){
        retval = true;
        Serial.println("Frost Timer Fired- RESTORE ACMAIN");
    }else{
      retval = false;
        Serial.println("CONDENSER <32 degC Remain Off");
    }

    return retval;
}

//aSSR = {{1,0,0,"AUXFAN"},{2,0,0,"ACMAIN"}, {0,0,0,"TSTAT"}, {3,0,0,"SPARE"}}

void SetAcMainAuxFan(void)
{
    //-- read stat demand for cool state
    if( digitalRead(pinTSTAT) == 0)
    {
      tstat_dmd = true;
    }else{
      tstat_dmd = false;
    }
    #ifdef DBGTSTAT1
        //--status dbg print
        Serial.println("MAN AUX " +String(mn_auxfan) +" MAN AC " +String(mn_acmain) +"pinSTAT " +digitalRead(pinTSTAT) +"TSTAT VAR " +String(tstat_dmd) +"FRST " +String(frost_flt));
    #endif
    //--Test if ACMAIN should be on
    if( mn_auxfan == true){ 
        digitalWrite(pinACMAIN,HIGH);
    }else{
        if(tstat_dmd == true && frost_flt == false){
            digitalWrite(pinACMAIN,HIGH);
    }else if(tstat_dmd == false  || frost_flt == true){
            digitalWrite(pinACMAIN,LOW);
        }
    }

    //--Test If AuxFan should be ON
    if( mn_acmain == true){ 
        digitalWrite(pinAUXFAN,HIGH);
    }else{
        if(tstat_dmd == true && frost_flt == false){
            digitalWrite(pinAUXFAN,HIGH);
        }
        else if( tstat_dmd == false  || frost_flt == true){
            digitalWrite(pinAUXFAN,LOW);
        }
    }
}



// send the XML file with switch statuses and analog value
void XML_response(WiFiClient cl)
{
    String strbuff,strtstat,strmnaux,strmnac;
    
    int analog_val;
   
    //sprintf(&dbgstr[0],"[XML] FRST  RAW %d bits Scaled %2.2f degC %2.2f degF\r\n",
    //    an_ch7raw,an_condtemp, an_condtemp* 9/5 + 32);
    //Serial.print(dbgstr);

    //sprintf(&dbgstr[0],"[XML] AMB RAW %d bits Scaled %2.2f degC %2.2f degF",
    //    an_ch6raw,an_ambtemp, an_ambtemp* 9/5 + 32);
    //Serial.print(dbgstr);

    strtstat = digitalRead(pinTSTAT) == false?"COOLING":"NOT COOL";
    //set manual over ride str's
    strmnaux = mn_auxfan ==true?"TRUE":"FALSE";
    strmnac = mn_acmain ==true?"TRUE":"FALSE";
    
    strbuff = String("<?xml version = \"1.0\" ?>");
    strbuff = String(strbuff + "<ajax_vars>");
    strbuff = String(strbuff + "<digCh_1>");
    strbuff = String(strbuff + strtstat);
    strbuff = String(strbuff + " MANUAL OVER-RIDES:");
    strbuff = String(strbuff + " AUXFAN: " +strmnaux);
    strbuff = String(strbuff + " ACMAIN: " + strmnac);
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
    strbuff = String(strbuff +"degC " + String(an_cnddegf) +" degF");
    strbuff = String(strbuff + "</anCh_1>");
    
    // read analog pin 6 (Ambient Room temp)
    strbuff = String(strbuff + "<anCh_2>");
    strbuff = String(strbuff +"RAW " +String(an_ch6raw) +" " + String(an_ambtemp));
    strbuff = String(strbuff +"degC " + String(an_ambdegf) +" degF");
    strbuff = String(strbuff + "</anCh_2>");
    
    strbuff = String(strbuff + "</ajax_vars>");
    #ifdef DBGXMLVARS1
    Serial.println("\r\n[XML SEND] START");
    Serial.print(strbuff);
    Serial.println("\r\n[XML SEND] END\r\n");
    #endif
    client.print(strbuff);
}

// send the XML file with switch statuses and analog value
void XML_GaugeResp(WiFiClient cl)
{
    String strbuff;
   
    strbuff = String("<?xml version = \"1.0\" ?>");
    strbuff = String(strbuff + "<ajax_temp>");
    strbuff = String(strbuff + "<an_CndTemp>");
    strbuff = String(strbuff +String(an_cnddegf));
    strbuff = String(strbuff + "</an_CndTemp>");

    strbuff = String(strbuff + "<an_AmbTemp>");
    strbuff = String(strbuff + String(an_ambdegf));
    strbuff = String(strbuff + "</an_AmbTemp>");

    strbuff = String(strbuff + "<an_SetTemp1>");
    strbuff = String(strbuff + String(tempSet1));
    strbuff = String(strbuff + "</an_SetTemp1>");

    strbuff = String(strbuff + "<an_SetTemp2>");
    strbuff = String(strbuff + String(tempSet2));
    strbuff = String(strbuff + "</an_SetTemp2>");    
    
    strbuff = String(strbuff + "</ajax_temp>");
    #ifdef DBGXMLGAUGE1
      Serial.println("[\r\nXML TEMP SEND] START\r\n");
      Serial.print(strbuff);
      Serial.println("\r\n[XML TEMP SEND] END\r\n");
    #endif
    client.print(strbuff);
}

