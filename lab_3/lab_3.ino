#include <M5StickC.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Arduino_JSON.h>

//Callback declarations
typedef void (*buttonCB)();
void onPressButton1();
void onPressButton2();

struct City
{
  const char* city;
  const char* countryCode;
};

//Constants
enum Mode {QR, Feels};
const char *ssid = "One+";
const char *password = "987654321";
const char* openWeatherMapApiKey = "ae9720ff9402ace198f29988938682db";
const unsigned long timerDelay = 5000;
const int days = 5;
const City cities[] = {{"Ankara", "TR"}, {"Delhi", "IN"}, {"Hof", "DE"}};
const unsigned int citiesCount = sizeof(cities) / sizeof(City);

//buttons
const int btnPins[] = {37, 39};
const int btnCount = sizeof(btnPins)/sizeof(int);
int btnCurVals[btnCount] = {0};
int btnLastVals[btnCount] = {0};
buttonCB buttonPressedCallbacks[] = {onPressButton1, onPressButton2};

//book keeping
char query[256];
String jsonBuffer;
String strIp;
String feels;
int curCityId = -1;
Mode curMode =  Mode::QR;
int dataAvailable = 0;
unsigned long requestTime = 0;//when was the last request made?

void updateLCD()
{
  M5.Lcd.fillScreen(BLACK);

  //print status
  if (WiFi.status() == WL_CONNECTED)
  {
    M5.Lcd.setCursor(0, 0);
    M5.Lcd.printf("IP : %s", strIp.c_str());
  }

  //print city and country
  M5.Lcd.setCursor(0, 10);
  if (curCityId == -1)
  {
    M5.Lcd.println("Press BTN A to begin.");
  }
  else
  {
    M5.Lcd.printf("%s, %s", cities[curCityId].city, cities[curCityId].countryCode );
  }

  //print data
  M5.Lcd.setCursor(0, 20);
  if (dataAvailable)
  {
    if (curMode == QR)
    {
      M5.Lcd.qrcode(query, 0, 20);
    }
    else
    {
      M5.Lcd.print(feels);
    }
  }
}

void setup()
{
  M5.begin();
  M5.Lcd.setRotation(3);
  pinMode(32, INPUT);
  pinMode(33, INPUT);
  Serial.begin(115200);

  WiFi.begin(ssid, password);
  Serial.println("Connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  strIp = WiFi.localIP().toString();

  Serial.println("");
  Serial.print("Connected to WiFi network with IP Address: ");
  Serial.println(strIp);

  Serial.printf("Total cities : %u\n", citiesCount);
  updateLCD();
}

void onPressButton1()
{
  Serial.println("inside button 1 CB");

  if (WiFi.status() != WL_CONNECTED) return;

  curCityId++;
  if (curCityId >= citiesCount ) curCityId = 0;
  dataAvailable = 0;
  requestTime = millis();

  updateLCD();
}

void onPressButton2()
{
  Serial.println("inside button 2 CB");

  curMode = curMode == QR ? Feels : QR;
  updateLCD();
}

//if a button is "pressed" call the corresponding callback func
void updateButtonStatus()
{
  for (int i = 0; i < btnCount; ++i )
  {
    if (btnCurVals[i] != btnLastVals[i])
    {
      if (btnCurVals[i] == 0)
      {
        Serial.printf("Button %d pressed.\n", i);
        buttonPressedCallbacks[i]();
      }
      else
      {
        Serial.printf("Button %d released.\n", i);
      }
      btnLastVals[i] = btnCurVals[i];
    }
  }
}

//read the pins
void readButtonVals()
{
  for ( int i = 0; i < btnCount; ++i  )
  {
    btnCurVals[i] = digitalRead(btnPins[i]);
  }
}

void generateFeelsStr(JSONVar& root)
{
  feels = "";
  int len = root["list"].length();
  for (int i = 0; i < len; ++i)
  {
    feels += JSON.stringify(root["list"][i]["main"]["feels_like"]);
    feels += "\n";
  }
}

void updateQuery()
{
  const char* city = cities[curCityId].city;
  const char* countryCode = cities[curCityId].countryCode;
  sprintf(query, "http://api.openweathermap.org/data/2.5/forecast?q=%s,%s&APPID=%s&cnt=%d&units=metric", city, countryCode, openWeatherMapApiKey, days );
}

void loop()
{
  readButtonVals();
  updateButtonStatus();

  if ( !dataAvailable && curCityId >= 0 && (millis() - requestTime) > timerDelay )
  {
    if (WiFi.status() == WL_CONNECTED)
    {
      updateQuery();
      jsonBuffer = httpGETRequest(query);
      Serial.println(jsonBuffer);
      JSONVar myObject = JSON.parse(jsonBuffer);

      if (JSON.typeof(myObject) == "undefined") {
        Serial.println("Parsing input failed!");
        return;
      }
      
      dataAvailable = 1;
      Serial.print("JSON object = ");
      Serial.println(myObject);

      generateFeelsStr(myObject);
    }
    else
    {
      Serial.println("WiFi Disconnected");
    }
    updateLCD();
  }
}

String httpGETRequest(const char* serverName) {
  HTTPClient http;

  // Your IP address with path or Domain name with URL path
  http.begin(serverName);

  // Send HTTP POST request
  int httpResponseCode = http.GET();

  String payload = "{}";

  if (httpResponseCode > 0) {
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
    payload = http.getString();
  }
  else {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
  }
  // Free resources
  http.end();

  return payload;
}
