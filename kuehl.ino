/*
 * Required libraries
 * - Arduino Keypad
 * - NeoPixelBus
 * - WifiManager, https://github.com/tzapu/WiFiManager/tree/development
 * - ArduinoJson, version 5.x
 */

#include <FS.h>
#include <SPIFFS.h>
//#include <Keypad.h>
//#include <NeoPixelBus.h>
#ifdef __AVR__
  #include <avr/power.h>
#endif

#include <WiFiManager.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

/*****************************************************
 * RGB LED configuration
 ****************************************************/
#define RGB_PIN        4
#define NUMPIXELS      1

/*****************************************************
 * Keypad configuration
 ****************************************************/
const byte KEYPAD_ROWS = 4;
const byte KEYPAD_COLS = 4;

// Keypad pin configuration (pin 1 and 10 are unused)
// Pins 2-5 on keypad
byte colPins[KEYPAD_COLS] = {32, 33, 25, 26};
// Pins 6-9 on keypad
byte rowPins[KEYPAD_ROWS] = {27, 14, 12, 13};

char keys[KEYPAD_ROWS][KEYPAD_COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};

/*****************************************************
 * Declarations in global scope
 ****************************************************/
enum ledColor {
  yellow,
  red,
  blue
};

char token[64];
char projectId[64];

bool shouldSaveConfig = false;

//Keypad keypad = Keypad( makeKeymap(keys), rowPins, colPins, KEYPAD_ROWS, KEYPAD_COLS );
//NeoPixelBus<NeoGrbFeature, Neo800KbpsMethod> pixels(NUMPIXELS, RGB_PIN);
WiFiManager wifiManager;
HTTPClient http;

WiFiManagerParameter custom_todoist_token("token", "Todoist Token", token, 64);
WiFiManagerParameter custom_todoist_projectId("projectId", "Todoist Project ID", projectId, 64);
WiFiManagerParameter custom_text("<p>Configure access to Todoist:</p>");

/*****************************************************
 * Utility functions
 ****************************************************/
String chipId() {
  uint64_t chipid = ESP.getEfuseMac();
  char chipIdString[13];

  sprintf(chipIdString, "%04X%08X",(uint16_t)(chipid>>32), (uint32_t)chipid);

  return String(chipIdString);
}

String uuid() {
  char buffer[33];  // buffer to hold 32 Hex Digit + /0
  int i;
  for(i = 0; i < 4; i++) {
    sprintf (buffer + (i*8), "%08x", esp_random());
  }
  return String(buffer);
}

#if 0
void showLed(ledColor colorName) {
  RgbColor colorValue;
  
  switch(colorName) {
    case yellow:
      colorValue = RgbColor(150, 150, 0);
      break;
    case red:
      colorValue = RgbColor(0, 150, 0);
      break;
    case blue:
      colorValue = RgbColor(0, 0, 150);
      break;
    default:
      colorValue = RgbColor(150, 150, 150);
      break;
  }
  
  pixels.SetPixelColor(0, colorValue);
  pixels.Show();
}

void turnOffLed() {
  pixels.SetPixelColor(0, RgbColor(0));
  pixels.Show();
}
#else
#define showLed(c)
#define turnOffLed()
#endif

String urlencode(String str)
{
    String encodedString="";
    char c;
    char code0;
    char code1;
    char code2;
    for (int i =0; i < str.length(); i++){
      c=str.charAt(i);
      if (c == ' '){
        encodedString+= '+';
      } else if (isalnum(c)){
        encodedString+=c;
      } else{
        code1=(c & 0xf)+'0';
        if ((c & 0xf) >9){
            code1=(c & 0xf) - 10 + 'A';
        }
        c=(c>>4)&0xf;
        code0=c+'0';
        if (c > 9){
            code0=c - 10 + 'A';
        }
        code2='\0';
        encodedString+='%';
        encodedString+=code0;
        encodedString+=code1;
        //encodedString+=code2;
      }
      yield();
    }
    return encodedString;
    
}

/*****************************************************
 * Setup
 ****************************************************/
void setup() {
  Serial.begin(115200);

  //pixels.Begin();
  //pixels.Show();
  
  showLed(yellow);

  if (SPIFFS.begin(true)) {
    Serial.println("mounted file system");
    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading
      Serial.println("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        Serial.println("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success()) {
          Serial.println("\nparsed json");

          strcpy(token, json["todoist_token"]);
          strcpy(projectId, json["todoist_projectId"]);

        } else {
          Serial.println("failed to load json config");
        }
        configFile.close();
      }
    }
  } else {
    Serial.println("failed to mount FS");
  }

  wifiManager.setSaveConfigCallback(saveConfigCallback);
  wifiManager.addParameter(&custom_text);
  wifiManager.addParameter(&custom_todoist_token);
  wifiManager.addParameter(&custom_todoist_projectId);

  wifiManager.setAPCallback(configModeCallback);

  char chipIdString[13];
  chipId().toCharArray(chipIdString, 13);
  wifiManager.autoConnect("Kuehl01", chipIdString);

  if (shouldSaveConfig) {
    strcpy(token, custom_todoist_token.getValue());
    strcpy(projectId, custom_todoist_projectId.getValue());
  
    Serial.println("Saving configuration..");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["todoist_token"] = token;
    json["todoist_projectId"] = projectId;

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("Failed to open config file for writing");
    }

    json.printTo(Serial);
    json.printTo(configFile);
    configFile.close();
    //end save
  }

  turnOffLed();
}

void configModeCallback(WiFiManager *myWiFiManager) {
  Serial.println("***************************************************");
  Serial.println("* No Wifi configuration or not possible to connect.");
  Serial.println("* Entered config mode. Connect to me using:");
  Serial.println("*");
  Serial.print("* Wifi SSID: ");
  Serial.println(myWiFiManager->getConfigPortalSSID());
  Serial.print("* Wifi Password: ");
  Serial.println(chipId());
  Serial.print("* Web IP address: ");
  Serial.println(WiFi.softAPIP());
  Serial.println("***************************************************");

  showLed(red);
}

void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;

  turnOffLed();
}

/*****************************************************
 * Loop
 ****************************************************/
void loop(){
  //char key = keypad.getKey();
  char key = Serial.read();
  
  if (key){
    if(String(key) == "*") {
      wifiManager.resetSettings();
      ESP.restart();
    } else {
      showLed(blue);
    
      Serial.println(key);
  
      String requestUrl = "https://todoist.com/api/v7/sync?token=" + String(token) + "&commands=" + urlencode(
        R"([{"type": "item_add", "temp_id": ")" + 
        uuid() + 
        R"(", "uuid": ")" + 
        uuid() + R"(", "args": {"content": "Task )" + 
        key + 
        R"(", "project_id": )" + String(projectId) + "}}]");
      
      Serial.print("Request URL: ");
      Serial.println(requestUrl);
  
      http.begin(requestUrl);
      int httpCode = http.GET();
  
      String payload = http.getString();
      
      Serial.println(httpCode);
      Serial.println(payload);
  
      turnOffLed();
    }
  }
}
