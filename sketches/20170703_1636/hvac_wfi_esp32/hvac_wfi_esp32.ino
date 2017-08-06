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
//#define DBGXMLGAUGE1//shows gauge data output string
//#define DBGWEBPG1
//#define DBGWEBPG2
//#define DBGWEBPG3
//#define DBGWEBPG4
//#define DBGACSTATE1


//Application Version
const char* sAppVer     = "v0.1 20170702_1716_VK HVAC_Controller_ESP32_Wifi";
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
const char pinAUXFAN   = 27;  //dig out
const char pinACMAIN   = 26;  //dig out
int value = 0;
File html1;
File logo;
File tmp_file;
File Datlog_file;

//HVAC Vars
String  strTemp;
String strDmd;
String strFrost;
uint32_t TmrCntFrost;

struct s_timetype {
	uint32_t time; //in seconds
	uint32_t tm_secs;
	uint32_t tm_mins;
	uint32_t tm_hrs;
	uint32_t tm_days;
    uint32_t tm_yrs;
	char str[64];
};



enum e_ctlmode{
  ECTLMD_OFF,
  ECTLMD_TSTAT,
  ECTLMD_REMOTE,
  ECTLMD_MANUAL,
};
enum e_dmdmode{
  EDMDMD_NONE,
  EDMDMD_COOL,
  EDMDMD_HEAT,
};

struct s_manual{
  bool acpump;
  bool auxfan;
};

//in deg F
struct s_setpt{
  float dmd;
  float rdb;
  float rdbC;
  uint16_t rdbraw;
  uint8_t rnghi;
  uint8_t rnglo;
};

struct s_control{
  e_ctlmode ctlmode_e;
  e_dmdmode dmdmode_e;
  e_dmdmode tstatmode_e;
  e_dmdmode rmtmode_e;
  s_manual manstate_s;
  bool bAcCooling;
  bool bHtHeating;//not implemented yet
  bool bAuxFan;
  bool bEvappump;
  bool bFrostErr;
  bool bTstatCoolDmd;
  s_setpt set1_s;
  s_setpt set2_s;
  s_setpt cond_s;
};

s_control ctldata_s;
s_timetype time_s;
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
	//start up in remote control tstat (web page) mode
    ctldata_s.ctlmode_e = ECTLMD_REMOTE;
    ctldata_s.dmdmode_e = EDMDMD_NONE;
    ctldata_s.rmtmode_e = EDMDMD_NONE;
    ctldata_s.tstatmode_e = EDMDMD_NONE;
    
    ctldata_s.set1_s.dmd = 66.00;
    ctldata_s.set1_s.rdb = 0.0;
    ctldata_s.set1_s.rnghi = 1;//dont turn on until its +1 degrees over demand
    ctldata_s.set1_s.rnglo = 3;// turnoff when 3 degrees under demand 
    ctldata_s.set2_s.dmd = 0.0;//indicate set point 2 is not enabled

    ctldata_s.cond_s.dmd = 32.00;
    ctldata_s.cond_s.rdb = 0.0;
    ctldata_s.cond_s.rnghi = 5;//dont turn on until its +15 degrees over demand
    ctldata_s.cond_s.rnglo = 0;// turnoff when 3 degrees under demand 
    ctldata_s.cond_s.dmd = 0.0;//indicate set point 2 is not enabled

    ctldata_s.manstate_s.acpump = false;
    ctldata_s.manstate_s.auxfan = false;
    
    Serial.begin(921600);
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
            else if ((HTTP_req.indexOf("GET /ajax_chgsetpoint") > -1) && HTTP_req.indexOf("_end")) {
                client.println(httphdrstart);
                client.println(httphdrxml);
                client.println(httphdrkeep);
                client.println();
                // Look for set point xml start tag '_' after chgsetpoint'
                iSpLoc1 = HTTP_req.indexOf("chgsetpoint_") + strlen("chgsetpoint_");
                // Look for set point xml end tag "_end"
                iSpLoc2 = HTTP_req.indexOf("_end");
                // pull out the string between start and end, ths is the number
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
                    ctldata_s.set1_s.dmd = strRxdTemp.toFloat();
                }
            }

            // example.. GET /ajax_chgctlmode_REMOTE_end&nocache=299105.2747379479 HTTP/1.1
            else if ((HTTP_req.indexOf("GET /ajax_chgctlmode") > -1) && HTTP_req.indexOf("_end") >-1 ){
                client.println(httphdrstart);
                client.println(httphdrxml);
                client.println(httphdrkeep);
                client.println();
                // Look for set point xml start tag '_' after chgsetpoint'
                iSpLoc1 = HTTP_req.indexOf("chgctlmode_") + strlen("chgctlmode_");
                // Look for set point xml end tag "_end"
                iSpLoc2 = HTTP_req.indexOf("_end");
                // pull out the string between start and end, ths is the number
                strRxdTemp = HTTP_req.substring(iSpLoc1,iSpLoc2);
                Serial.print("[WEBPG] GET ajax_chgctlmode Control Mode: ");
                Serial.print(strRxdTemp);
                Serial.println(" Mode");
                Serial.print("IdxStartTag: ");
                Serial.print(iSpLoc1);
                Serial.println();
                Serial.print("IdxEndTag: ");
                Serial.print(iSpLoc2 );
                Serial.println(" END \n");
                if(strRxdTemp != "")
                {
                    if(strRxdTemp.indexOf("TSTAT")>-1){
                      ctldata_s.ctlmode_e = ECTLMD_TSTAT;
                    }else if(strRxdTemp.indexOf("REMOTE")>-1){
                      ctldata_s.ctlmode_e = ECTLMD_REMOTE;
                    }else if(strRxdTemp.indexOf("MANUAL")>-1){
                      ctldata_s.ctlmode_e = ECTLMD_MANUAL;
                    }else{
                      ctldata_s.ctlmode_e = ECTLMD_OFF;
                    }
                    Serial.print("[WEB PG CTLMODE]Control Mode: " +ctldata_s.ctlmode_e );
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
                ctldata_s.manstate_s.auxfan = true;
                Serial.println("[WEBPG] GET /AUXSSR=1\n\n\n\n\n\n\n\n\n\\n\n\n");
            }else if (HTTP_req.indexOf("GET /_AUXSSR=0") > -1){
                bLoadIndex = true;
                ctldata_s.manstate_s.auxfan = false;
                Serial.println("[WEBPG] GET /AUXSSR=0\n\n\n\n\n\n\n\n\n\n\n\n");
            }
            else if (HTTP_req.indexOf("GET /_ACSSR=1") > -1){
                bLoadIndex = true;
                ctldata_s.manstate_s.acpump = true;
                Serial.println("[WEBPG] GET /ACSSR=1\n\n\n\n\n\n\n\n\n\\n\n\n");
            }else if (HTTP_req.indexOf("GET /_ACSSR=0") > -1){
                bLoadIndex = true;
                ctldata_s.manstate_s.acpump = false;
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
                    Serial.println("[WEBPG] GET Logo UPLOADED TO BROWSER");
                    
                }else{
                  Serial.println("[WEBPG] Logo Not Found!!!");
                }
            }
            //Respond to favicon.ico for chrome
            else if (HTTP_req.indexOf("GET /favicon.ico") > -1){
                client.println("HTTP/1.0 404 \r\n\n\n");
            }
            else if (HTTP_req.indexOf("GET /ajax.js") > -1)
            {
                // send file
                tmp_file = SD.open("/ajax.js");
                Serial.println("[WEB PAGE] Rxd ajax.js\r\n");  
                if (tmp_file) {
                   client.println(httphdrstart);
                   client.println(httphdrjs);
                   client.println(httphdrkeep);
                   client.println();
                   while(tmp_file.available()) {
                        client.write(tmp_file.read()); // send web page to client
                    }
                    tmp_file.close();
					Serial.println("[WEBPG] ajax.js UPLOADED TO BROWSER");
                }else{
                  Serial.println("[FILE] ERROR DID NOT OPEN ajax.js");
                  
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
            }
            
            //send css file
            else if (HTTP_req.indexOf("GET /wifihvac.css") > -1)
            {
                Serial.println("[WEB PAGE CSS] Rxd wifihavac.css\r\n");
                tmp_file = SD.open("/wifihvac.css");  
                if (tmp_file) {
                   Serial.println("[WEB PAGE CSS OPENED] wifihvac.css\r\n");  
                   client.println(httphdrstart);
                   client.println(httphdrjs);
                   client.println(httphdrkeep);
                   client.println();
                   while(tmp_file.available()) {
                        client.write(tmp_file.read()); // send web page to client
                    }
                    tmp_file.close();
					Serial.println("[WEBPG] wifihvac.css UPLOADED TO BROWSER");
                }else{
                  Serial.println("[FILE] ERROR DID NOT OPEN wifihvac.css");
                }
            }

            //else no match to request respond with 404
            else{
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
      //delay(1);      // give the web browser time to receive the data
    
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
  //pinMode(pinTSTAT,INPUT);
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
      readFile(SD, "/index.html");   
    }

    // check for ajax.js file, web page cannot run w/o it
    if (!SD.exists("/ajax.js")) {
        Serial.println("ERROR - Can't find ajax.js file!");
        return;  // can't find index file
    }else{
		readFile(SD, "/ajax.js");   
    }
   
	// check for ajax.js file, web page cannot run w/o it
	if (!SD.exists("/datalog/aclog.txt")) {
		createDir(SD, "/datalog");
		writeFile(SD, "/datalog/aclog.txt", "[CREATED] Created the Data Log for The HVAC Controller...\n");
		listDir(SD, "/", 3);

		Serial.println("ERROR - Can't find /datalog/aclog.txt file!");
		return;  // can't find file
	}
	else {
		readFile(SD, "/datalog/aclog.txt");
		appendFile(SD, "/datalog/aclog.txt", "[REBOOTED] \n");
	}
   // removeDir(SD, "/mydir");
   // listDir(SD, "/", 2);
   
   

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

void calc_uptime(uint32_t time)
{
	time_s.tm_secs = time % 60UL;
	time /= 60UL;
	time_s.tm_mins = time % 60UL;
	time /= 60UL;
	time_s.tm_hrs = time % 24UL;
	time /= 24UL;
	time_s.tm_days = time;

	sprintf(time_s.str, "%2d DAYS %2d HRS %2d MINS %2d SECS ", time_s.tm_days, time_s.tm_hrs, time_s.tm_mins, time_s.tm_secs);
	//Serial.println(time_s.str);
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
    //Update Analog Vars
    // Check for frozen condensor 
    FrostCheck();
    // Determine if AC fan & pump should be on
    SetAcState();
	// increment timer execution count
	TmrCntFrost++;
	if (TmrCntFrost % 2 == 0) {
		time_s.time++;
		calc_uptime(time_s.time);
	}
	


  }
}


//--method to toggle pins for SSR control
void FrostCheck(void)
{  
    ctldata_s.cond_s.rdbraw = analogRead(pinCNDSRMON);
    ctldata_s.set1_s.rdbraw = analogRead(pinAMBMON);
    
    ctldata_s.cond_s.rdbC = (float)ProcessPtc(4095,ctldata_s.cond_s.rdbraw,-6);
    ctldata_s.set1_s.rdbC = (float)ProcessPtc(4095,ctldata_s.set1_s.rdbraw,-6);
    
    ctldata_s.cond_s.rdb = ctldata_s.cond_s.rdbC* 9/5 + 32;
    ctldata_s.set1_s.rdb = ctldata_s.set1_s.rdbC* 9/5 + 32;
    
    #ifdef DBGFROST2
        //sprintf(&dbgstr[0],"FRST  RAW %d bits Scaled %2.2f degC %2.2f degF\r\n",
        //    an_ch7raw,an_condtemp, an_cnddegf);
        //Serial.print(dbgstr);
    
        //sprintf(&dbgstr[0],"[FRST] RAW %d bits Scaled %2.2f degC %2.2f degF",
        //    an_ch6raw,an_ambtemp, an_ambdegf);
        //Serial.print(dbgstr);
    #endif
  
  if( ctldata_s.cond_s.rdb <32 ){
    #ifdef DBGFROST3
        Serial.println("FROST EMINENT, SHUTDOWN ACPUMP & WAIT FOR WARMUP");
    #endif
    ctldata_s.bFrostErr = true;
  }else{
    //If we were in Frost Fault call to test for 
    if(ctldata_s.bFrostErr == true){
        ctldata_s.bFrostErr = bWarmedUp(ctldata_s.cond_s.rdb,ctldata_s.cond_s.rnghi );
    }
    #ifdef DBGFROST4
      Serial.println("NO FROST COND. NOMAL OP MODE");
    #endif
  }
}

//--method to restart the AC after warmup period
bool bWarmedUp(float val,float rng)
{
   bool retval = false;
   //build in hysterisis for turn back on
    if( val >rng){
        retval = true;
        Serial.println("Frost Timer Fired- RESTORE ACMAIN");
    }else{
      retval = false;
        Serial.println("CONDENSER <32 degC Remain Off");
    }

    return retval;
}

enum E_ACSTATE{
  EAC_INIT,
  EAC_MANUAL,
  EAC_TSTAT,
  EAC_REMOTE,
  EAC_OFF,
};

void SetAcState(void)
{
    static E_ACSTATE state = EAC_INIT;
	String LogEntry = "";

    //-- read tstat demand for tstat demanded cool state
    if( digitalRead(pinTSTAT) == 0)
    {
      ctldata_s.bTstatCoolDmd = true;
    }else{
      ctldata_s.bTstatCoolDmd = false;
    }
    
    #ifdef DBGACSTATE1
        //--status dbg print
        Serial.println("MAN AUX " +String(ctldata_s.manstate_s.auxfan) +" MAN AC " +String(ctldata_s.manstate_s.acpump) +"pinSTAT " +digitalRead(pinTSTAT) +"TSTAT VAR " +String(ctldata_s.bTstatCoolDmd) +"FRST " +String(ctldata_s.bFrostErr));
    #endif

    // Test if mode change is required and do it
    if( ctldata_s.ctlmode_e == ECTLMD_MANUAL ){
      state = EAC_MANUAL;
    }else if( ctldata_s.ctlmode_e == ECTLMD_TSTAT ){
       state = EAC_TSTAT;
       ctldata_s.manstate_s.acpump = false;
       ctldata_s.manstate_s.auxfan = false;
    }else if( ctldata_s.ctlmode_e == ECTLMD_REMOTE ){
       state = EAC_REMOTE;
       ctldata_s.manstate_s.acpump = false;
       ctldata_s.manstate_s.auxfan = false;
    }else{
       state = EAC_OFF;
       ctldata_s.manstate_s.acpump = false;
       ctldata_s.manstate_s.auxfan = false;
    }

    // State MAchine for AC PUMP & AUXFAN for all modes
    switch(state){
        case EAC_INIT:
            state = EAC_TSTAT;
            #ifdef DBGACSTATE1   
              Serial.println("[AC STATE] INIT");
            #endif
        break;

        case EAC_MANUAL:
            // Set ac pump state
            if( ctldata_s.manstate_s.acpump == true){ 
                digitalWrite(pinACMAIN,HIGH);
            }else{
                digitalWrite(pinACMAIN,LOW);
            }
            
            //Set auxfan ON state
            if( ctldata_s.manstate_s.auxfan == true){ 
                digitalWrite(pinAUXFAN,HIGH);
            }else{
                digitalWrite(pinAUXFAN,LOW);
            }

            #ifdef DBGACSTATE1  
                Serial.println("[AC STATE] MANUAL");
            #endif
        break;

        case EAC_TSTAT:
            //Test For ACMAIN ON state
            if(ctldata_s.bTstatCoolDmd == true && ctldata_s.bFrostErr == false){
                digitalWrite(pinACMAIN,HIGH);
                
            }else if(ctldata_s.bTstatCoolDmd == false  || ctldata_s.bFrostErr == true){
                digitalWrite(pinACMAIN,LOW);
            }
            // Test for AUXFAN ON state
            if(ctldata_s.bTstatCoolDmd == true || ctldata_s.bFrostErr == true){
              digitalWrite(pinAUXFAN,HIGH);
            }
            else if( ctldata_s.bTstatCoolDmd == false  && ctldata_s.bFrostErr == false){
                digitalWrite(pinAUXFAN,LOW);
            }
            #ifdef DBGACSTATE1   
                Serial.println("[AC STATE] TSTAT");
            #endif
        break;
        
        case EAC_REMOTE:
            // AUXFAN always on in remote mode
            digitalWrite(pinAUXFAN,HIGH);  
            
            // Test if pump should be turned on
            // If NO frost_flt && AC pump off && temp goes 2deg > demand turn pump on
            if( !ctldata_s.bFrostErr && 
                (ctldata_s.set1_s.rdb >= ctldata_s.set1_s.dmd + ctldata_s.set1_s.rnghi && 
                (digitalRead(pinACMAIN) == LOW))){
              digitalWrite(pinACMAIN,HIGH);
			  LogEntry = String(time_s.str) + " [AC STATE] REMOTE DMD PUMPON " + String(ctldata_s.set1_s.dmd) + " RDB " + String(ctldata_s.set1_s.rdb) +
				  " RNGLO " + String(ctldata_s.set1_s.rnglo) + " RNGH " + String(ctldata_s.set1_s.rnghi) +"\n";
			  LogEntry.toCharArray(dbgstr, sizeof(dbgstr));
			  Serial.println(dbgstr);
			 // LOG pump cycle to SD card
			  if (SD.exists("/datalog/aclog.txt")) {
				  
				  appendFile(SD, "/datalog/aclog.txt", dbgstr);
			  }
            }

            //Test if pump should be turn off
            // If frost_flt OR AC pump on and temp goes 2deg > demand turn pump on
            if( ctldata_s.bFrostErr || 
                (ctldata_s.set1_s.rdb <= ctldata_s.set1_s.dmd - ctldata_s.set1_s.rnglo && 
                (digitalRead(pinACMAIN) == HIGH))){
              digitalWrite(pinACMAIN,LOW);
			  LogEntry = String(time_s.str) + " [AC STATE] REMOTE DMD PUMPOFF " + String(ctldata_s.set1_s.dmd) + " RDB " + String(ctldata_s.set1_s.rdb) +
				  " RNGLO " + String(ctldata_s.set1_s.rnglo) + " RNGH " + String(ctldata_s.set1_s.rnghi) + "\n";
			  LogEntry.toCharArray(dbgstr, sizeof(dbgstr));
			  Serial.println(dbgstr);
			 
			  // LOG pump cycle to SD card
			  if (SD.exists("/datalog/aclog.txt")) {

				  appendFile(SD, "/datalog/aclog.txt", dbgstr);
			  }
            }   
            
        break;

        case EAC_OFF:
            digitalWrite(pinAUXFAN,LOW);  
            digitalWrite(pinACMAIN,LOW);
            #ifdef DBGACSTATE1   
                Serial.println("[AC STATE] OFF");
            #endif
        break;
        
        default:
        break;
    };

    //set vars indicating pin states
    ctldata_s.bAcCooling = digitalRead(pinACMAIN);
    ctldata_s.bAuxFan = digitalRead(pinAUXFAN);
}


// send the XML file with switch statuses and analog value
void XML_response(WiFiClient cl)
{
    String strbuff,strtstat,strmnaux,strmnac;
    
    int analog_val;
    strtstat = digitalRead(pinTSTAT) == false?"COOLING":"NOT COOL";
    //send AC PUMP and AUXFAN ON state, if 
    if( ctldata_s.ctlmode_e == ECTLMD_MANUAL){
      strmnaux = ctldata_s.manstate_s.auxfan ==true?"TRUE":"FALSE";
      strmnac = ctldata_s.manstate_s.acpump ==true?"TRUE":"FALSE";
    }else{
      strmnaux = ctldata_s.bAuxFan ==true?"TRUE":"FALSE";
      strmnac = ctldata_s.bAcCooling ==true?"TRUE":"FALSE";
    }
    strbuff = String("<?xml version = \"1.0\" ?>");
    strbuff = String(strbuff + "<ajax_vars>");
    strbuff = String(strbuff + "<digCh_1>");
    strbuff = String(strbuff + strtstat);
    strbuff = String(strbuff + " MANUAL OVER-RIDES:");
    strbuff = String(strbuff + " AUXFAN: " +strmnaux);
    strbuff = String(strbuff + " ACMAIN: " + strmnac);
    strbuff = String(strbuff + "</digCh_1>");
    
    strbuff = String(strbuff + "<digCh_2>");
    if( ctldata_s.bFrostErr == true){
      strbuff = String(strbuff + "ERR");
    }
    else {
        strbuff = String(strbuff + "---");
    }
    strbuff = String(strbuff + "</digCh_2>");
    
    // read analog pin 7 (AC Condensor temp)
    //analog_val = analogRead(7);
    strbuff = String(strbuff + "<anCh_1>");
    strbuff = String(strbuff +"RAW " +String(ctldata_s.cond_s.rdbraw) +" " + String(ctldata_s.cond_s.rdbC));
    strbuff = String(strbuff +"degC " + String(ctldata_s.cond_s.rdb) +" degF");
    strbuff = String(strbuff + "</anCh_1>");
    
    // read analog pin 6 (Ambient Room temp)
    strbuff = String(strbuff + "<anCh_2>");
    strbuff = String(strbuff +"RAW " +String(ctldata_s.set1_s.rdbraw) +" " + String(ctldata_s.set1_s.rdbC));
    strbuff = String(strbuff +"degC " + String(ctldata_s.set1_s.rdb) +" degF");
    strbuff = String(strbuff + "</anCh_2>");

    // Send State Machine data
    String state;
    if( ctldata_s.ctlmode_e == ECTLMD_MANUAL ){
      state = "MANUAL";
    }else if( ctldata_s.ctlmode_e == ECTLMD_TSTAT ){
       state = "TSTAT";
    }else if( ctldata_s.ctlmode_e == ECTLMD_REMOTE ){
       state = "REMOTE";
    }else{
       state = "OFF";
    }
    strbuff = String(strbuff + "<dig_ACSTATE>");
    strbuff = String(strbuff + state);
    strbuff = String(strbuff + "</dig_ACSTATE>");
    
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
    strbuff = String(strbuff +String(ctldata_s.cond_s.rdb));
    strbuff = String(strbuff + "</an_CndTemp>");

    strbuff = String(strbuff + "<an_AmbTemp>");
    strbuff = String(strbuff + String(ctldata_s.set1_s.rdb));
    strbuff = String(strbuff + "</an_AmbTemp>");

    strbuff = String(strbuff + "<an_SetTemp1>");
    strbuff = String(strbuff + String(ctldata_s.set1_s.dmd));
    strbuff = String(strbuff + "</an_SetTemp1>");

    strbuff = String(strbuff + "<an_SetTemp2>");
    strbuff = String(strbuff + String(ctldata_s.set2_s.dmd));
    strbuff = String(strbuff + "</an_SetTemp2>");    
    
    strbuff = String(strbuff + "</ajax_temp>");
    #ifdef DBGXMLGAUGE1
      Serial.println("[\r\nXML TEMP SEND] START\r\n");
      Serial.print(strbuff);
      Serial.println("\r\n[XML TEMP SEND] END\r\n");
    #endif
    client.print(strbuff);
}

