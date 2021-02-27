#include "Arduino.h"
#include <WiFi.h>

#include <ArduinoJson.h>
#include "freertos/FreeRTOS.h"
#include <WiFi.h>
#include <MQTT.h>

#include "json_file.h"
#include "esp_adc_cal.h"

//create a new file 'wifi_secret.h' with the two following constants
//the reason is that 'wifi_sercert.h' is ignored by git
const char* ssid = "SSID";
const char* password =  "PASSWORD";
//#include "wifi_secret.h"

int analogBattery = A0;
int g_vref = 1100;

DynamicJsonDocument config(1*1024);//5 KB
MQTTClient mqtt(1*1024);// 1KB for small messages
WiFiClient wifi;//needed to stay on global scope

void timelog(String Text){
  Serial.println(String(millis())+" : "+Text);//micros()
}

void adc_vref_init(){
    esp_adc_cal_characteristics_t adc_chars;
    esp_adc_cal_value_t val_type = esp_adc_cal_characterize((adc_unit_t)ADC_UNIT_1, (adc_atten_t)ADC1_CHANNEL_6, (adc_bits_width_t)ADC_WIDTH_BIT_12, 1100, &adc_chars);
    //Check type of calibration value used to characterize ADC
    if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF) {
        Serial.printf("eFuse Vref:%u mV\n", adc_chars.vref);
        g_vref = adc_chars.vref;
    } else if (val_type == ESP_ADC_CAL_VAL_EFUSE_TP) {
        Serial.printf("Two Point --> coeff_a:%umV coeff_b:%umV\n", adc_chars.coeff_a, adc_chars.coeff_b);
    } else {
        Serial.println("Default Vref: 1100mV");
    }
}

float get_battery()
{
  uint16_t v = analogRead(analogBattery);
  float battery_voltage = ((float)v / 4095.0) * 2.0 * 3.3 * (g_vref / 1000.0);
  return battery_voltage;
}


void blink(int duration_ms){
  digitalWrite(LED_BUILTIN,HIGH);
  //delay(duration_ms);
  vTaskDelay(pdMS_TO_TICKS(duration_ms));
  digitalWrite(LED_BUILTIN,LOW);
}

void mqtt_publish_status(){
  String str_topic = config["base_topic"];
  str_topic += "/status";
  float time_f = millis();
  time_f /=1000;
  String str_wakeup = String(time_f);
  float battery_f = get_battery();
  String str_battery = String(battery_f);
  String json_payload = "{\"battery\":"+str_battery+",\"wakeup\":"+str_wakeup+"}";
  mqtt.publish(str_topic,json_payload,true,2);//LWMQTT_QOS2 = 2
  Serial.printf("published (%s)=>(%s)\r\n",str_topic.c_str(),json_payload.c_str());
}

bool connect(){
  timelog("wifi check");
  int max_timeout = 10;
  while ((WiFi.status() != WL_CONNECTED)&&(max_timeout>0)) {
    Serial.print(".");
    delay(500);
    max_timeout--;
  }
  Serial.print("\n");
  if(max_timeout == 0){
    timelog("wifi timeout!");
    blink(10);delay(200);
    blink(10);delay(200);
    return false;
  }
  max_timeout = 10;
  timelog("wifi connected!");
  mqtt.begin(config["mqtt"]["host"],config["mqtt"]["port"], wifi);
  Serial.print("connecting mqtt");
  while ((!mqtt.connect(config["mqtt"]["client_id"]))&&(max_timeout>0)) {
    Serial.print(".");
    blink(50);
    delay(450);
    max_timeout--;
  }
  if(max_timeout == 0){
    timelog("MQTT timeout!\n");
    blink(10);delay(500);
    blink(10);delay(500);
    return false;
  }
  Serial.print("\n");
  timelog("mqtt connected!\n");
  return true;
}

void setup() {

  Serial.begin(115200);
  //Serial.setDebugOutput(true);
  Serial.println();
  adc_vref_init();
  pinMode(LED_BUILTIN, OUTPUT);
  timelog("Boot ready");
  blink(100);
  WiFi.begin(ssid, password);
  load_config(config,true);
  timelog("config loaded");

  if(connect()){
    mqtt.loop();
    delay(100);//allow mqtt and serial transmission
    mqtt_publish_status();    timelog("=>status");
    mqtt.loop();
    delay(100);//allow mqtt and serial transmission
  }

  Serial.println("ESP going to deep sleep");
  Serial.flush();
  blink(100);

  uint32_t deep_sleep_sec = config["deep_sleep_sec"];
  esp_deep_sleep(deep_sleep_sec*1000000);
  esp_deep_sleep_start();

}

void loop() {
  //no loop
}
