
#include <ArduinoJson.h>
#include <ESP8266WiFi.h> //ESP8266 Core WiFi Library
#include <ESP8266WebServer.h>
#include "src/wifi_manager/WiFiManager.h"
#include <ESP8266HTTPClient.h>
#include <Time.h>
#include <EEPROM.h>

time_t _now;
struct tm * timeinfo;
WiFiClient client;


int day, month, year;
int summerTime = 0;
String date;

#define NUM_MAX 4

// for NodeMCU    Vcc Red   Gnd Brown
#define CS_PIN  2  // D4 Yellow
#define CLK_PIN 14  // D5 Green
#define DIN_PIN 13  // D7 Orange

#define LIGHT_PIN  A0

#include "max7219.h"
#include "fonts.h"
#define HOSTNAME "ESP8266-OTA"


String ip; // external IP
String Latitude;
String Longitude;

String Country_name;
String City="";
String Country_code="";
String Fact ="";
boolean TimeZoneUpdated = false; 
int TimeFormat; // to save the EEPROM settings of time format (12 or 24 hrs)

int EEPROMaddress = 200; // the EEPROM address to store / read Time format settings 



// =======================================================================
// CHANGE YOUR CONFIG HERE:
// =======================================================================


String APIKEY = "*******************************";       // Open Weather API KEY                           


char WeatherServername[]="api.openweathermap.org";              // remote server we will connect to


String weatherDescription ="";
String weatherLocation = "";
String Country;
float Temperature;
float Humidity;
float Pressure;



float utcOffset = 0;    // UTC Offset in hours 
int updCnt = 0;  // this counter used in the main loop to count minuts (it will be subtracted one every minute)


void setup() 
{

  delay(1000);

  Serial.begin(115200);
  EEPROM.begin(512);
  
  
  delay(1000);
  initMAX7219();
  sendCmdAll(CMD_SHUTDOWN,1);
  sendCmdAll(CMD_INTENSITY,2);
  Serial.print("Connecting WiFi ");

  printStringWithShiftNoDelay("Connecting to Wifi..           ",17);
  //printStringWithShiftNoDelay(".....Connecting",20);
 
  // Timer.attach(0.1,adjustBrightness);
  WiFi.mode(WIFI_STA);
  WiFiManager wifiManager;
  wifiManager.setAPCallback(configModeCallback);
  //wifiManager.resetSettings();
  
  wifiManager.autoConnect("Wifi Clock");

  
  
  Serial.println("connected...yeey :)");

    
  //time formate settings (24 or 12 hr)
   if(wifiManager._TimeFormat == 0){ // reading the time format from EEPROM
    TimeFormat = EEPROM.read(EEPROMaddress);
   }else{ // reading the time format from wifi manager and saving it to EEPROM
    TimeFormat = wifiManager._TimeFormat;
    EEPROM.write(EEPROMaddress, TimeFormat);
    EEPROM.commit();
   }
  
  Serial.println(TimeFormat);
  Serial.println("");
  Serial.print("IP: "); Serial.println(WiFi.localIP());
  
  


  Serial.println("getting ip address");

  // getting external IP address(this is used for resolving the city and country code)
  int retry = 0;
  while ( !GetExternalIP() && (retry < 2)) {
    Serial.print(".");
    retry++;
  }
  if (retry == 2) {
    Serial.println("Connection failed to get external IP");
    while(!GetExternalIP()){
      printStringWithShiftNoDelay("unable to get external IP          ",15);
    }
  }

  
  Serial.println("getting geographical location");

   // getting location data.. there is 2 sources of location if the first one failed it will try the second one.
   if (  getLocationData2() == false) {    
      printStringWithShiftNoDelay("Unable to get location data 1           ",15);
      if(getLocationData() == false){  
        while(true){
          printStringWithShiftNoDelay("Location Error           ",15);
        }   
    }
  }

  //getting fact data for the first time
  getFact();


  //display the city name that you are connected from
  String temp = "Connected from  "+City+"           ";
  printStringWithShiftNoDelay(temp.c_str(), 18);
  
    
}


void configModeCallback (WiFiManager *myWiFiManager) {// this function should return true if it was connected to wifi. otherwise will dispay error message and start wifi access point
      Serial.println("unable to connect..");
      printStringWithShiftNoDelay("         Unable to connect..  switching to access point mode. Please Connect     ",18);
      printStringWithShift("APMode",18);
     // printStringWithShiftNoDelay("    Connected           ", 18);
}



// =============================DEFINE VARS==============================
#define MAX_DIGITS 20
byte dig[MAX_DIGITS]={0};
byte digold[MAX_DIGITS]={0};
byte digtrans[MAX_DIGITS]={0};

int dots = 0;
long dotTime = 0;
long clkTime = 0;
long weatherTime = millis();
long factTime = millis() - 150000;
int factCounter = 7;
int dx=0;
int dy=0;
byte del=0;
int h,m,s,h24;


long localEpoc = 0;
long localMillisAtUpdate = 0;
// =======================================================================
void loop()
{
  
  if(updCnt<=0) { // every 10 scrolls, ~450s=7.5m
    
    Serial.println("Getting data ...");
    //printStringWithShiftNoDelay(" updating weather data...           ",20);
    if(!getWeatherData()&&!TimeZoneUpdated){ // this check is to make sure that the timezone is grabed from the weather api... if not will display an error and try again 
      printStringWithShiftNoDelay(String("Error in getting time zone            ").c_str(),17);
    }else{
      updCnt = 10;
    }
    getTime();
    
    Serial.println("weather&time data loaded");
    clkTime = millis();
  }

  
  if(day != timeinfo->tm_mday){ // to update date string when it is midnight 
    Serial.println("midnight date update....");
    update_date();
  }

  
 
  if(millis()-clkTime > 60000 && !del && dots) { // clock for 60s, then scrolls for about 30s
    printStringWithShiftNoDelay((date+"            ").c_str(),25);
    
    updCnt--;
    clkTime = millis();

  }
  if(millis()-dotTime > 500) {
    dotTime = millis();
    dots = !dots;
  }
   if(millis()-weatherTime > 20000) {
    weatherTime = millis();
    displayWeather();
   }
   
  if(millis()-factTime > 200000) {
    if(factCounter<=0){
      getFact();
      factCounter = 7;
    }
    factTime = millis();
    displayFact();
    factCounter--;
    
  }

  
  
  updateTime();
  showAnimClock();
}


int lightCounter = 1;
int ligtTotal = 0;



void displayFact()
{
  if(Fact.length()>0){
    printStringWithShiftNoDelay((String("        Facts           ")).c_str(),13);
    printStringWithShiftNoDelay((String("Facts           ")).c_str(),13);
    printStringWithShiftNoDelay((String("Facts           ")).c_str(),13);
    printStringWithShiftNoDelay((String(""+Fact+"           ")).c_str(),25);
  }
    
}


void displayWeather()
{

  if(weatherDescription.length() > 0){
      if(weatherDescription.indexOf(" ")> 0){
        printStringWithShiftNoDelay((String("  "+weatherDescription+"   ")).c_str(),25);
      }else{
        printStringWithShift((String("  "+weatherDescription+"   ")).c_str(),25);
      }
      
      
      String TempStr = String(Temperature);
      int _length = strlen(TempStr.c_str());
      TempStr[_length-1] = ' ';
    
      String _Humidity = String(Humidity);
       _length = strlen(_Humidity.c_str());
      _Humidity[_length-1] = ' ';
      _Humidity[_length-2] = '%';
       _Humidity[_length-3] = ' ';
    
     //  printStringWithShift("  Humidity.. ",30);
    
        printStringWithShift(String(" "+_Humidity).c_str(),25);
        
    
      //printStringWithShift("  Temperature.. ",30);
      
      printStringWithShift((String(" "+TempStr+"C ")).c_str(),25);
    
      delay(150);

  }
  
}

// =======================================================================

void showSimpleClock()
{
  dx=dy=0;
  clr();
  showDigit(h/10,  0, dig6x8);
  showDigit(h%10,  8, dig6x8);
  showDigit(m/10, 17, dig6x8);
  showDigit(m%10, 25, dig6x8);
  showDigit(s/10, 34, dig6x8);
  showDigit(s%10, 42, dig6x8);
  setCol(15,dots ? B00100100 : 0);
  setCol(32,dots ? B00100100 : 0);
  refreshAll();
}

// =======================================================================

void showAnimClock()
{
  byte digPos[6]={0,8,17,25,34,42};
  int digHt = 12;
  int num = 6; 
  int i;
  if(del==0) {
    del = digHt;
    for(i=0; i<num; i++) digold[i] = dig[i];
    dig[0] = h/10 ? h/10 : 10;
    dig[1] = h%10;
    dig[2] = m/10;
    dig[3] = m%10;
    dig[4] = s/10;
    dig[5] = s%10;
    for(i=0; i<num; i++)  digtrans[i] = (dig[i]==digold[i]) ? 0 : digHt;
  } else
    del--;
  
  clr();
  for(i=0; i<num; i++) {
    if(digtrans[i]==0) {
      dy=0;
      showDigit(dig[i], digPos[i], dig6x8);
    } else {
      dy = digHt-digtrans[i];
      showDigit(digold[i], digPos[i], dig6x8);
      dy = -digtrans[i];
      showDigit(dig[i], digPos[i], dig6x8);
      digtrans[i]--;
    }
  }
  dy=0;
  setCol(15,dots ? B00100100 : 0);
  setCol(32,dots ? B00100100 : 0);
  refreshAll();
  delay(30);
}

// =======================================================================

void showDigit(char ch, int col, const uint8_t *data)
{
  if(dy<-8 | dy>8) return;
  int len = pgm_read_byte(data);
  int w = pgm_read_byte(data + 1 + ch * len);
  col += dx;
  for (int i = 0; i < w; i++)
    if(col+i>=0 && col+i<8*NUM_MAX) {
      byte v = pgm_read_byte(data + 1 + ch * len + 1 + i);
      if(!dy) scr[col + i] = v; else scr[col + i] |= dy>0 ? v>>dy : v<<-dy;
    }
}

// =======================================================================

void setCol(int col, byte v)
{
  if(dy<-8 | dy>8) return;
  col += dx;
  if(col>=0 && col<8*NUM_MAX)
    if(!dy) scr[col] = v; else scr[col] |= dy>0 ? v>>dy : v<<-dy;
}

// =======================================================================

int showChar(char ch, const uint8_t *data)
{
  int len = pgm_read_byte(data);
  int i,w = pgm_read_byte(data + 1 + ch * len);
  for (i = 0; i < w; i++)
    scr[NUM_MAX*8 + i] = pgm_read_byte(data + 1 + ch * len + 1 + i);
  scr[NUM_MAX*8 + i] = 0;
  return w;
}

// =======================================================================
int dualChar = 0;

unsigned char convertPolish(unsigned char _c)
{
  unsigned char c = _c;
  if(c==196 || c==197 || c==195) {
    dualChar = c;
    return 0;
  }
  if(dualChar) {
    switch(_c) {
      case 133: c = 1+'~'; break; // 'ą'
      case 135: c = 2+'~'; break; // 'ć'
      case 153: c = 3+'~'; break; // 'ę'
      case 130: c = 4+'~'; break; // 'ł'
      case 132: c = dualChar==197 ? 5+'~' : 10+'~'; break; // 'ń' and 'Ą'
      case 179: c = 6+'~'; break; // 'ó'
      case 155: c = 7+'~'; break; // 'ś'
      case 186: c = 8+'~'; break; // 'ź'
      case 188: c = 9+'~'; break; // 'ż'
      //case 132: c = 10+'~'; break; // 'Ą'
      case 134: c = 11+'~'; break; // 'Ć'
      case 152: c = 12+'~'; break; // 'Ę'
      case 129: c = 13+'~'; break; // 'Ł'
      case 131: c = 14+'~'; break; // 'Ń'
      case 147: c = 15+'~'; break; // 'Ó'
      case 154: c = 16+'~'; break; // 'Ś'
      case 185: c = 17+'~'; break; // 'Ź'
      case 187: c = 18+'~'; break; // 'Ż'
      default:  break;
    }
    dualChar = 0;
    return c;
  }    
  switch(_c) {
    case 185: c = 1+'~'; break;
    case 230: c = 2+'~'; break;
    case 234: c = 3+'~'; break;
    case 179: c = 4+'~'; break;
    case 241: c = 5+'~'; break;
    case 243: c = 6+'~'; break;
    case 156: c = 7+'~'; break;
    case 159: c = 8+'~'; break;
    case 191: c = 9+'~'; break;
    case 165: c = 10+'~'; break;
    case 198: c = 11+'~'; break;
    case 202: c = 12+'~'; break;
    case 163: c = 13+'~'; break;
    case 209: c = 14+'~'; break;
    case 211: c = 15+'~'; break;
    case 140: c = 16+'~'; break;
    case 143: c = 17+'~'; break;
    case 175: c = 18+'~'; break;
    default:  break;
  }
  return c;
}

// =======================================================================

void printCharWithShift(unsigned char c, int shiftDelay) {
  c = convertPolish(c);
  if (c < ' ' || c > '~'+25) return;
  c -= 32;
  int w = showChar(c, font);
  for (int i=0; i<w+1; i++) {
    delay(shiftDelay);
    scrollLeft();
    refreshAll();
  }
}

// =======================================================================

void printStringWithShift(const char* s, int shiftDelay){
  sendCmdAll(CMD_SHUTDOWN,1);
  while (*s) {
    printCharWithShift(*s, shiftDelay);
    s++;
  }
  delay(shiftDelay * 20);
}

void printStringWithShiftNoDelay(const char* s, int shiftDelay){
  sendCmdAll(CMD_SHUTDOWN,1);
  while (*s) {
    printCharWithShift(*s, shiftDelay);
    s++;
  }
  //delay(shiftDelay * 20);
}
// =======================================================================

boolean getTime()
{ 
      Serial.println(utcOffset);
      
      configTime(utcOffset * 3600, 0, "pool.ntp.org", "time.nist.gov");
      Serial.println("Waiting for time");
      int retry = 0;
      while (!time(nullptr) && (retry < 5)) {
        Serial.print(".");
        delay(1000);
        retry++;
      }
      
      if (retry == 5) {
        Serial.println("Connection failed to time service");
        return false;
      }else
      {
        Serial.println("Connected to time service");
      }
      
      localMillisAtUpdate = millis();
      _now = time(nullptr);
      localEpoc = _now;
      
      timeinfo = localtime(&_now); 
      Serial.println(ctime(&_now));
      timeinfo = localtime(&_now); 
      
      update_date(); // update date string that will be displayed...
    
      h= timeinfo->tm_hour;
      m = timeinfo->tm_min;
      s = timeinfo->tm_sec;

      return true;

}

void update_date(){
      String temp = ctime(&_now);
      String temp_day_st = temp.substring(0, 3);
      String temp_month = temp.substring(4, 7);
      String temp_year = String(timeinfo->tm_year+1900);
      String temp_day_int = String(timeinfo->tm_mday);

      day = timeinfo->tm_mday ;
      month =  timeinfo->tm_mon;
      year = timeinfo->tm_year+1900;
      
      date = "     "+temp_day_st+", "+temp_day_int+" "+temp_month+" "+temp_year;
      date.toUpperCase();
      Serial.println(date);
}

bool getFact(){
  String datarx;
  String host = "https://uselessfacts.jsph.pl/random.json?language=en&output=json";
  WiFiClientSecure httpsClient;
  httpsClient.setTimeout(6000);
  delay(1000);
  int retry = 0;
  while ((!httpsClient.connect("uselessfacts.jsph.pl", 443)) && (retry < 5)) {
    delay(1000);
    Serial.print(".");
    retry++;
  }
  if (retry == 5) {
    Serial.println("Connection failed");
    return false;
  }
  else {
    Serial.println("Connected to Server");
  }
//  httpsClient.print(String("GET ") + path + 
//                    "HTTP/1.1\r\n" +
//                    "Host: "+ host +
//                    "\r\n" + "Connection: close\r\n\r\n");

  httpsClient.println("GET "+host+" HTTP/1.1");
  httpsClient.println("Host: uselessfacts.jsph.pl");
  httpsClient.println("Connection: close");
  httpsClient.println();

  while (httpsClient.connected()) {
    
    String line = httpsClient.readStringUntil('\n');
    if (line == "\r") {
      break;
    }
  }
  while (httpsClient.available()) {
    datarx += httpsClient.readStringUntil('\n');
  }
  //Serial.println(datarx);

  datarx = datarx.substring(datarx.indexOf("{"),datarx.length());
  Serial.println(datarx);
   
   DynamicJsonDocument doc(1024);
   
   DeserializationError error = deserializeJson(doc, datarx);
   if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.c_str());
    return false;
  }

    Fact = (const char*) doc["text"];

    
    Serial.println(Fact);
    
    return true;
  
}

// =======================================================================

// =======================================================================

// =======================================================================

void updateTime()
{
  long curEpoch = localEpoc + ((millis() - localMillisAtUpdate) / 1000);
  _now = curEpoch ; 
  long epoch =  (long) round(curEpoch + 3600 * (summerTime) + 86400L) % 86400L;
 
  h = ((epoch  % 86400L) / 3600) % TimeFormat;
  if(h == 0 && TimeFormat == 12){
    h = 12;
    
  }
  m = (epoch % 3600) / 60;
  s = epoch % 60;
}


boolean getWeatherData()                                //client function to send/receive GET request data.
{
  String result;
  if (client.connect(WeatherServername, 80))   
          {                                         //starts client connection, checks for connection
           
          //client.println("GET /data/2.5/weather?q="+City+","+Country_code+"&units=metric&APPID="+APIKEY);
          client.println("GET /data/2.5/weather?lat="+Latitude+"&lon="+Longitude+"&units=metric&appid="+APIKEY);
          client.println("Host: api.openweathermap.org");
          client.println("User-Agent: ArduinoWiFi/1.1");
          client.println("Connection: close");
          client.println();
          } 
  else {
         Serial.println("connection failed");        //error message if no client connect
         Serial.println();
         return false;
       }

  while(client.connected() && !client.available()) 
  delay(1);                                          //waits for data
  while (client.connected() || client.available())    
       {                                             //connected or data available
         char c = client.read();                     //gets byte from ethernet buffer
         result = result+c;
       }

    client.stop();                                      //stop client

    //result.replace('[', ' ');
    //result.replace(']', ' ');
    Serial.println(result);
    char jsonArray [result.length()+1];
    result.toCharArray(jsonArray,sizeof(jsonArray));
    jsonArray[result.length() + 1] = '\0';
    StaticJsonDocument<1024> json_buf;
    deserializeJson(json_buf, jsonArray);
    JsonObject root = json_buf.as<JsonObject>();
    

    if(!TimeZoneUpdated){
      int timezone =root["timezone"];
      utcOffset = (timezone/60)/60;
      Serial.println(utcOffset);
      TimeZoneUpdated = true;
     
    }

    

    
    String location = root["name"];
    String country = root["sys"]["country"];
    float temperature = root["main"]["temp"];
    String t =root["main"]["temp"];
    Serial.println(temperature);
     
    float humidity = root["main"]["humidity"];
     
    String weather = root["weather"]["main"];
    const char* description = root["weather"][0]["description"];

    float pressure = root["main"]["pressure"];
    weatherDescription = description;
    weatherLocation = location;
    Country = country;
    Temperature = temperature;
    Humidity = humidity;
    Pressure = pressure;
    result = "";

    return true;
}


bool GetExternalIP()
{
  WiFiClient client;
  if (!client.connect("api.ipify.org", 80)) {
    Serial.println("Failed to connect with 'api.ipify.org' !");
    return false;
  }
  else {
    int timeout = millis() + 5000;
    client.print("GET /?format=json HTTP/1.1\r\nHost: api.ipify.org\r\n\r\n");
    while (client.available() == 0) {
      if (timeout - millis() < 0) {
        Serial.println(">>> Client Timeout !");
        client.stop();
        return false;
      }
    }
    int size;
    String line;
    while ((size = client.available()) > 0) {
      line = client.readStringUntil('\n');

      if (line.startsWith("{\"ip\":")) {
        Serial.println(line);
        ip = line;
        ip.replace("{","");
        ip.replace("}","");
        ip.replace("\"","");
        ip.replace("ip","");
        ip.replace(":","");
        
      }
     
    }
    
    
  }
  client.stop();
  return true;
}

bool getLocationData()                                //client function to send/receive GET request data.

{
  HTTPClient http; //Object of class HTTPClient
  http.begin("http://api.ipstack.com/"+ip+"?access_key=4c20ee20e404e6301da5595b56a70ea3&output=json");
  int httpCode = http.GET();
                                    //stop client
   if (httpCode > 0) 
    {
      String json  = http.getString();
      
      
      Serial.println(json);
      json.replace("\\","");
      DynamicJsonDocument doc(1024);
   
     DeserializationError error = deserializeJson(doc, json);
     if (error) {
      Serial.print(F("deserializeJson() failed!!: "));
      Serial.println(error.c_str());
      return false;
    }
  
  
      const char* country_name  = doc["country_name"];
      const char* city = doc["city"];
      const char* country_code = doc["country_code"];
      float _latitude = doc["latitude"];
      float _longitude = doc["longitude"];
      Serial.println(country_name);
      Serial.println(city);
      Serial.println(_latitude);
      Serial.println(_longitude);
      
      Country_name = country_name;
      City = city;
      Country_code = country_code;
      Latitude = String(_latitude);
      Longitude =String(_longitude);
      
      if(City.length() == 0){
              return false;
      }


      return true; 

    }
    else{
      Serial.println("connection failed");   
      http.end();
      return false; 
      
    }

}

bool getLocationData2()                                //client function to send/receive GET request data.
{

  HTTPClient http; //Object of class HTTPClient
  http.begin("http://www.iplocate.io/api/lookup/"+ip);
  int httpCode = http.GET();
                                    //stop client
   if (httpCode > 0) 
    {
      String json  = http.getString();
      
      
      Serial.println(json);

     DynamicJsonDocument doc(1024);
     
     DeserializationError error = deserializeJson(doc, json);
         if (error) {
          Serial.print(F("deserializeJson() failed: "));
          Serial.println(error.c_str());
          http.end();
          return false;
          
        }


        const char* country_name  = doc["country"];
        const char* city = doc["city"];
        const char* country_code = doc["country_code"];
        float _latitude = doc["latitude"];
        float _longitude = doc["longitude"];
        
        Serial.println(country_name);
        Serial.println(city);
        Serial.println(_latitude);
        Serial.println(_longitude);
        
        Country_name = country_name;
        City = city;
        Country_code = country_code;
        Latitude = String(_latitude);
        Longitude =String(_longitude);

        if(City.length() == 0){
            http.end();
            return false;
          }
    
        


    }
    http.end(); //Close connection

    return true;
  //json.replace("\\","");

}




// =======================================================================
