#define BLYNK_TEMPLATE_ID "TMPL5PpLz-Bs"
#define BLYNK_DEVICE_NAME "Quickstart Device"
#define BLYNK_AUTH_TOKEN  "Dgy3A9NYrPp2hby2qMYel-eQ-Te1U4Oy"

#define BLYNK_PRINT Serial

#include "DHTesp.h" // https://github.com/beegee-tokyo/DHTesp
#include <TFT_eSPI.h> // https://github.com/Bodmer/TFT_eSPI
#include <NTPClient.h> // https://github.com/arduino-libraries/NTPClient
#include <SPI.h>
#include <ArduinoJson.h>  // https://github.com/bblanchon/ArduinoJson.git
#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager
#include <WiFiUdp.h>
#include <HTTPClient.h>

#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>

#include "ws2.h"
#include "myLoading.h"

#define LEFT_BUTTON 0
#define RIGHT_BUTTON 35
int leftPressed=0;
int rightPressed=0;

int count = 0;

char auth[] = BLYNK_AUTH_TOKEN;

#define RESPONSE_ARRAY_SIZE 3

typedef struct {
  String date;
  String weekday;
  String description;
  int max;
  int min;
} forecast;
forecast forecasts[RESPONSE_ARRAY_SIZE];

ComfortState cf;
DHTesp dht;
TempAndHumidity newValues;
int dhtPin = 33;

TFT_eSPI tft = TFT_eSPI();

TaskHandle_t loadingTaskHandle = NULL;
TaskHandle_t callNTPClientTaskHandle = NULL;
TaskHandle_t updateHomeScreenHandle = NULL;
void loadingTask(void *pvParameters);
void callNTPClientTask(void *pvParameters);
void updateHomeScreenTask(void *pvParameters);
bool homeScreen();
void setForecastValues(forecast* f, int i);
void forecastScreen(forecast* f);
void checkButtons();
void generateScreen();
void connectWiFi();
void callHGWeather();

HTTPClient http;
String endpoint="https://api.hgbrasil.com/weather?array_limit="+String(RESPONSE_ARRAY_SIZE)+"&fields=only_results,temp,date,time,description,currently,city,humidity,wind_speedy,sunrise,sunset,forecast,date,weekday,max,min,description,&key=a9be0bbb&woeid=455821";
String payload="";
DynamicJsonDocument doc(1000);

int screen = 0;
int lastScreen = 2;

String date;
String weekday;
String description;
String hourAndMinutes;

// Configurações do Servidor NTP
const char* servidorNTP = "a.st1.ntp.br"; // Servidor NTP para pesquisar a hora
const int fusoHorario = -10800; // Fuso horário em segundos (-03h = -10800 seg)
const int taxaDeAtualizacao = 1800000; // Taxa de atualização do servidor NTP em milisegundos
String formattedDate;
WiFiUDP ntpUDP; // Declaração do Protocolo UDP
NTPClient timeClient(ntpUDP, servidorNTP, fusoHorario, 60000);

BlynkTimer timer;

void myTimerEvent()
{
  Blynk.virtualWrite(V2, millis() / 1000);
  double temp = newValues.temperature;
  Blynk.virtualWrite(V4, temp);
  Blynk.virtualWrite(V5, newValues.humidity);

  Blynk.virtualWrite(V6, String(forecasts[1].date));
  Blynk.virtualWrite(V7, String(forecasts[2].date));
  
  String tempAmanha = "Mín: " + String(forecasts[1].min) + " C,  Máx: " + String(forecasts[1].max) + " C";
  String tempDepoisAmanha = "Mín: " + String(forecasts[2].min) + " C,  Máx: " + String(forecasts[2].max) + " C";
  Blynk.virtualWrite(V9, tempAmanha);
  Blynk.virtualWrite(V10, tempDepoisAmanha);

  Blynk.virtualWrite(V11, forecasts[1].description);
  Blynk.virtualWrite(V12, forecasts[2].description);
}

void setup()
{
  // Debug console
  Serial.begin(115200);

  //Config do sensor
  dht.setup(dhtPin, DHTesp::DHT11);

  //Config dos botoes
  pinMode(LEFT_BUTTON,INPUT_PULLUP);
  pinMode(RIGHT_BUTTON,INPUT_PULLUP);

  //Config da tela
  tft.init();
  tft.setRotation(1);
  tft.pushImage(0, 0, 240, 135, ws2);

  //Config do wifi
  connectWiFi();

  Blynk.config(auth);
  Blynk.connect();
  timer.setInterval(1000L, myTimerEvent);

  //Criacao das tasks
  xTaskCreate(loadingTask, "loadingTask", 4000, NULL, 1, &loadingTaskHandle);
  xTaskCreate(callNTPClientTask, "callNTPClientTask", 4000, NULL, 1, &callNTPClientTaskHandle);

  //Requisicao http
  callHGWeather();

  vTaskDelete(loadingTaskHandle);

  xTaskCreate(updateHomeScreenTask, "updateHomeScreenTask", 4000, NULL, 1, &updateHomeScreenHandle);
}

void loop() {
  checkButtons();
  count++;
  if (count >= 60000) {
    count = 0;
    callHGWeather();
  }
  Blynk.run();
  timer.run();
  delay(50);
}

void checkButtons() {
  if (digitalRead(LEFT_BUTTON)==0){
    if (leftPressed == 0) {
      leftPressed = 1;
      if (screen > 0) {
        screen--;
      } else {
        screen = lastScreen;
      }
      generateScreen();
    }
  } else {
    leftPressed=0;
  }

  if (digitalRead(RIGHT_BUTTON)==0){
    if (rightPressed == 0) {
      rightPressed = 1;
      if (screen < lastScreen) {
        screen++;
      } else {
        screen = 0;
      }
      generateScreen();
    }
  } else {
    rightPressed=0;
  }
}

void generateScreen() {
  switch(screen) {
    case 0:
      homeScreen();
      break;
    case 1:
      forecastScreen(&forecasts[1]);
      break;
    case 2:
      forecastScreen(&forecasts[2]);
      break;
    default:
      homeScreen();
      break;
  };
}

void forecastScreen(forecast* f) {
  tft.fillScreen(TFT_BLACK);
  tft.setCursor(0, 0, 1);
  tft.setTextColor(TFT_WHITE,TFT_BLACK);  
  tft.setTextSize(2);
  tft.println(f->weekday + ", " + f->date);
  tft.println();
  tft.println(f->description);
  tft.println();
  tft.println("Min: " + String(f->min) + " C");
  tft.println("Max: " + String(f->max) + " C");
}

bool homeScreen() {
  newValues = dht.getTempAndHumidity();
  
  if (dht.getStatus() != 0) {
    Serial.println("DHT11 error status: " + String(dht.getStatusString()));
    return false;
  }

  float heatIndex = dht.computeHeatIndex(newValues.temperature, newValues.humidity);
  float dewPoint = dht.computeDewPoint(newValues.temperature, newValues.humidity);
  float cr = dht.getComfortRatio(cf, newValues.temperature, newValues.humidity);

  String comfortStatus;
  switch(cf) {
    case Comfort_OK:
      comfortStatus = "Confortavel";
      break;
    case Comfort_TooHot:
      comfortStatus = "Muito quente";
      break;
    case Comfort_TooCold:
      comfortStatus = "Muito frio";
      break;
    case Comfort_TooDry:
      comfortStatus = "Muito seco";
      break;
    case Comfort_TooHumid:
      comfortStatus = "Muito umido";
      break;
    case Comfort_HotAndHumid:
      comfortStatus = "Quente e umido";
      break;
    case Comfort_HotAndDry:
      comfortStatus = "Quente e seco";
      break;
    case Comfort_ColdAndHumid:
      comfortStatus = "Frio e umido";
      break;
    case Comfort_ColdAndDry:
      comfortStatus = "Frio e seco";
      break;
    default:
      comfortStatus = "Desconhecido";
      break;
  };

  Serial.println(" T:" + String(newValues.temperature) + " H:" + String(newValues.humidity) + " I:" + String(heatIndex) + " D:" + String(dewPoint) + " " + comfortStatus);
  
  tft.fillScreen(TFT_BLACK);
  tft.setCursor(0, 0, 1);
  tft.setTextColor(TFT_WHITE,TFT_BLACK);  
  tft.setTextSize(2);
  tft.println("Temp: " + String(newValues.temperature) + " C");
  tft.println("Umid: " + String(newValues.humidity) + "%");
  tft.println(comfortStatus);
  tft.println();
  tft.println(weekday + ", " + date + " - " + hourAndMinutes);
  tft.println(description);

  return true;
}

void callNTPClientTask(void *pvParameters) {
  for (;;) {
    timeClient.update();
    hourAndMinutes = timeClient.getFormattedTime().substring(0, 5);
    vTaskDelay(pdMS_TO_TICKS(60000));
  }
}

void updateHomeScreenTask(void *pvParameters) {
  for (;;) {
    if (screen == 0) homeScreen();
    vTaskDelay(pdMS_TO_TICKS(20000));
  }
}

void loadingTask(void *pvParameters) {
  int frame = 0;
  tft.fillScreen(TFT_BLACK);
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(100));
    tft.pushImage(95, 43, 50, 50, myLoading[frame]);
    frame++;
    if (frame >= 8) frame = 0;
  }
}

void setForecastValues(forecast* f, int i) {
  f->date = doc["forecast"][i]["date"].as<String>();
  f->weekday = doc["forecast"][i]["weekday"].as<String>();
  f->description = doc["forecast"][i]["description"].as<String>();
  f->max = doc["forecast"][i]["max"].as<int>();
  f->min = doc["forecast"][i]["min"].as<int>();
}

void connectWiFi() {
  WiFi.mode(WIFI_STA);

  WiFiManager wm;
  //wm.resetSettings();

  bool res = wm.autoConnect("WeatherStation");

  if(!res) {
    Serial.println("Failed to connect");
    ESP.restart();
  } else { 
    Serial.println("connected");
  }
}

void callHGWeather() {
  http.begin(endpoint);
  int httpCode = http.GET();
  if (httpCode > 0) {
    payload = http.getString();
    Serial.println(httpCode);
    Serial.println(payload);

    char inp[1000];
    payload.toCharArray(inp,1000);
    deserializeJson(doc,inp);

    date = doc["forecast"][0]["date"].as<String>();
    weekday = doc["forecast"][0]["weekday"].as<String>();
    description = doc["description"].as<String>();

    int i;
    for (i = 0; i < RESPONSE_ARRAY_SIZE; ++i) {
      setForecastValues(&forecasts[i], i);
    }
  } else {
    Serial.println("Error on HTTP request");
    ESP.restart();
  }
  http.end();
}