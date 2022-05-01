#include "DHTesp.h" // Click here to get the library: http://librarymanager/All#DHTesp
#include <Ticker.h>
#include <TFT_eSPI.h> // Graphics and font library for ST7735 driver chip
#include <SPI.h>
#include "ws2.h"

DHTesp dht;
TFT_eSPI tft = TFT_eSPI();  // Invoke library, pins defined in User_Setup.h

bool getTemperature();

/** Comfort profile */
ComfortState cf;
/** Pin number for DHT11 data pin */
int dhtPin = 33;

/**
 * getTemperature
 * Reads temperature from DHT11 sensor
 * @return bool
 *    true if temperature could be aquired
 *    false if aquisition failed
*/
bool getTemperature() {
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
      comfortStatus = "Unknown:";
      break;
  };

  Serial.println(" T:" + String(newValues.temperature) + " H:" + String(newValues.humidity) + " I:" + String(heatIndex) + " D:" + String(dewPoint) + " " + comfortStatus);
  
  tft.fillScreen(TFT_BLACK);
  tft.setCursor(0, 0, 1);
  tft.setTextColor(TFT_WHITE,TFT_BLACK);  
  tft.setTextSize(2);
  tft.println("Temperatura: " + String(newValues.temperature) + " C");
  tft.println("Humidade:     " + String(newValues.humidity) + "%");
  tft.println("In. de calor:  " + String(heatIndex));
  tft.println("Orvalho:     " + String(dewPoint) + " C");
  tft.println();
  tft.println(comfortStatus);

  return true;
}

void setup()
{
  Serial.begin(115200);

  dht.setup(dhtPin, DHTesp::DHT11);

  tft.init();
  tft.setRotation(1);
  tft.pushImage(0, 0, 240, 135, ws2);
  delay(2000);
}

void loop() {
  getTemperature();
  delay(5000);
}