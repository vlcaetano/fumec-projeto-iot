#include "DHTesp.h" // https://github.com/beegee-tokyo/DHTesp
#include <TFT_eSPI.h> // https://github.com/Bodmer/TFT_eSPI
#include <NTPClient.h> // https://github.com/arduino-libraries/NTPClient
#include <SPI.h>
#include <ArduinoJson.h>  // https://github.com/bblanchon/ArduinoJson.git
#include <WiFi.h>
#include <WiFiUdp.h>
#include <HTTPClient.h>

#include "ws2.h"
#include "myLoading.h"

#define LEFT_BUTTON 0
#define RIGHT_BUTTON 35
int leftPressed=0;
int rightPressed=0;

DHTesp dht;
TFT_eSPI tft = TFT_eSPI();

TaskHandle_t loadingTaskHandle = NULL;
TaskHandle_t callHGWeatherTaskHandle = NULL;
TaskHandle_t callNTPClientTaskHandle = NULL;
TaskHandle_t updateHomeScreenHandle = NULL;
void loadingTask(void *pvParameters);
void callHGWeatherTask(void *pvParameters);
void callNTPClientTask(void *pvParameters);
void updateHomeScreenTask(void *pvParameters);
bool homeScreen();
void setForecastValues(String *d, String *w, String *des, int *mx, int *mn, int arrayPosition);
void forecastScreen(String *d, String *w, String *des, int *mx, int *mn);
void checkButtons();
void generateScreen();
void connectWiFi();
void callHGWeather();

ComfortState cf;
int dhtPin = 33;

const char* ssid     = "";
const char* password = "";
HTTPClient http;
String endpoint="https://api.hgbrasil.com/weather?array_limit=3&fields=only_results,temp,date,time,description,currently,city,humidity,wind_speedy,sunrise,sunset,forecast,date,weekday,max,min,description,&key=a9be0bbb&woeid=455821";
String payload="";
DynamicJsonDocument doc(1000);

int screen = 0;
int lastScreen = 2;

String date;
String weekday;
String description;
String hourAndMinutes;

String date1;
String weekday1;
String description1;
int max1;
int min1;
String date2;
String weekday2;
String description2;
int max2;
int min2;

// Configurações do Servidor NTP
const char* servidorNTP = "a.st1.ntp.br"; // Servidor NTP para pesquisar a hora
 
const int fusoHorario = -10800; // Fuso horário em segundos (-03h = -10800 seg)
const int taxaDeAtualizacao = 1800000; // Taxa de atualização do servidor NTP em milisegundos
String formattedDate;
WiFiUDP ntpUDP; // Declaração do Protocolo UDP
NTPClient timeClient(ntpUDP, servidorNTP, fusoHorario, 60000);

void setup() {
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

  xTaskCreate(loadingTask, "loadingTask", 4000, NULL, 1, &loadingTaskHandle);
  xTaskCreate(callHGWeatherTask, "callHGWeatherTask", 4000, NULL, 1, &callHGWeatherTaskHandle);
  xTaskCreate(callNTPClientTask, "callNTPClientTask", 4000, NULL, 1, &callNTPClientTaskHandle);

  //Requisicao http
  callHGWeather();

  vTaskDelete(loadingTaskHandle);

  xTaskCreate(updateHomeScreenTask, "updateHomeScreenTask", 4000, NULL, 1, &updateHomeScreenHandle);
}

void loop() {
  checkButtons();
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

void callHGWeatherTask(void *pvParameters) {
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(300000));
    callHGWeather();
  }
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
      forecastScreen(&date1, &weekday1, &description1, &max1, &min1);
      break;
    case 2:
      forecastScreen(&date2, &weekday2, &description2, &max2, &min2);
      break;
    default:
      homeScreen();
      break;
  };
}

bool homeScreen() {
  TempAndHumidity newValues = dht.getTempAndHumidity();
  
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
  tft.println("Hum:  " + String(newValues.humidity) + "%");
  // tft.println("In. de calor:  " + String(heatIndex));
  // tft.println("Orvalho:     " + String(dewPoint) + " C");
  tft.println(comfortStatus);
  tft.println();
  tft.println(weekday + ", " + date + " - " + hourAndMinutes);
  tft.println(description);

  return true;
}

void forecastScreen(String *d, String *w, String *des, int *mx, int *mn) {
  tft.fillScreen(TFT_BLACK);
  tft.setCursor(0, 0, 1);
  tft.setTextColor(TFT_WHITE,TFT_BLACK);  
  tft.setTextSize(2);
  tft.println(*w + ", " + *d);
  tft.println();
  tft.println(*des);
  tft.println();
  tft.println("Min: " + String(*mn) + " C");
  tft.println("Max: " + String(*mx) + " C");
}

void setForecastValues(String *d, String *w, String *des, int *mx, int *mn, int arrayPosition) {
  *d = doc["forecast"][arrayPosition]["date"].as<String>();
  *w = doc["forecast"][arrayPosition]["weekday"].as<String>();
  *des = doc["forecast"][arrayPosition]["description"].as<String>();
  *mx = doc["forecast"][arrayPosition]["max"].as<int>();
  *mn = doc["forecast"][arrayPosition]["min"].as<int>();
}

void connectWiFi() {
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
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

    setForecastValues(&date1, &weekday1, &description1, &max1, &min1, 1);
    setForecastValues(&date2, &weekday2, &description2, &max2, &min2, 2);
  } else {
    Serial.println("Error on HTTP request");
  }
  http.end();
}