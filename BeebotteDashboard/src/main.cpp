//#include <Arduino.h>
#include <LovyanGFX.hpp>
#include "LGFX_Config_TTGO_TMusic.hpp"
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>

const char* url_background = "【背景画像ファイルのURL】/background.png"; // PNG
const char* url_layout = "【レイアウトファイルのURL】/layout.json";  // Json

const char* wifi_ssid = "【WiFiアクセスポイントのSSID】";
const char* wifi_password = "【WiFiアクセスポイントのパスワード】";
const char* mqtt_server = "mqtt.beebotte.com";
const uint16_t mqtt_port = 1883; // MQTTサーバのポート番号(TCP接続)

#define MQTT_CLIENT_NAME  "TMusic" // MQTTサーバ接続時のクライアント名
#define MQTT_USER "【IAMトークン】"
#define MQTT_PASSWORD ""

#define FONT_NAME   lgfxJapanGothicP_24
#define FONT_SIZE   24.0f

static LGFX lcd;
WiFiClient espClient;
PubSubClient client(espClient);

#define BACKGROUND_BUFFER_SIZE  70000
unsigned long background_buffer_length;
unsigned char background_buffer[BACKGROUND_BUFFER_SIZE];

#define NUM_OF_SIZE   32
#define LINE_BUFFER_SIZE  32
char lines[NUM_OF_SIZE][LINE_BUFFER_SIZE] = { 0 };
char str_date[11];
char str_time[6];

#define MQTT_BUFFER_SIZE  255 // MQTT送受信のバッファサイズ
const int subscribe_capacity = JSON_OBJECT_SIZE(5);
StaticJsonDocument<subscribe_capacity> json_subscribe;
char message_buffer[MQTT_BUFFER_SIZE];

#define LAYOUT_BUFFER_SIZE  2048
const int layout_capacity = NUM_OF_SIZE * JSON_OBJECT_SIZE(8);
StaticJsonDocument<layout_capacity> json_layout;
char layout_buffer[LAYOUT_BUFFER_SIZE];
unsigned long layout_buffer_length;

void wifi_connect(const char *ssid, const char *password);
long doHttpGet(String url, uint8_t *p_buffer, unsigned long *p_len);
void draw_lines(void);
void updateTime(void);

void mqtt_callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Mqtt Received: ");
  Serial.println(topic);
  DeserializationError err = deserializeJson(json_subscribe, payload, length);
  if( err ){
    Serial.println("Deserialize error");
    Serial.println(err.c_str());
    return;
  }

  JsonArray array = json_layout.as<JsonArray>();
  for( int i = 0 ; i < array.size() ; i++ ){
    if( !array[i]["topic"])
      continue;

    const char* _topic = array[i]["topic"];
    if( strcmp( _topic, topic) != 0 )
      continue;

    const char* type = array[i]["type"];
    if( strcmp(type, "bbt_string") == 0 ){
      const char* value = json_subscribe["data"];
      strcpy(lines[i], value);
    }else
    if( strcmp(type, "bbt_number") == 0 ){
      long value = json_subscribe["data"];
      sprintf(lines[i], "%ld", value);
    }else
    if( strcmp(type, "bbt_float") == 0 ){
      double value = json_subscribe["data"];
      char format[9] = "%lf";
      if( array[i]["digit"] ){
        int digit = array[i]["digit"];
        sprintf(format, "%%.%dlf", digit);
      }
      sprintf(lines[i], format, value);
    }
  }

  draw_lines();
}

void setup() {
  lcd.init();
  lcd.setRotation(1);
  lcd.setBrightness(128);
  lcd.setColorDepth(24);

  lcd.setFont(&FONT_NAME);
  Serial.begin(9600);
  
  wifi_connect(wifi_ssid, wifi_password);
  configTzTime("JST-9", "ntp.nict.jp", "ntp.jst.mfeed.ad.jp");

  long ret;
  
  background_buffer_length = sizeof(background_buffer);
  ret = doHttpGet(url_background, background_buffer, &background_buffer_length);
  if( ret != 0 )
    Serial.println("doHttpGet Error");

  layout_buffer_length = sizeof(layout_buffer);
  ret = doHttpGet(url_layout, (uint8_t*)layout_buffer, &layout_buffer_length);
  if( ret != 0 )
    Serial.println("doHttpGet Error");

  DeserializationError err = deserializeJson(json_layout, layout_buffer, layout_buffer_length);
  if( err ){
    Serial.println("Deserialize error");
    Serial.println(err.c_str());
    return;
  }

  JsonArray array = json_layout.as<JsonArray>();
  for( int i = 0 ; i < array.size() ; i++ ){
    const char* type = array[i]["type"];

    if( strcmp(type, "string") == 0 ){
      const char* value = array[i]["value"];
      strcpy(lines[i], value);
    }else
    if( strcmp(type, "number") == 0 ){
      long value = array[i]["value"];
      sprintf(lines[i], "%ld", value);
    }else
    if( strcmp(type, "float") == 0 ){
      double value = array[i]["value"];
      char format[9] = "%lf";
      if( array[i]["digit"] ){
        int digit = array[i]["digit"];
        sprintf(format, "%%.%dlf", digit);
      }
      sprintf(lines[i], format, value);
    }
  }

  updateTime();
  draw_lines();

  // バッファサイズの変更
  client.setBufferSize(MQTT_BUFFER_SIZE);
  // MQTTコールバック関数の設定
  client.setCallback(mqtt_callback);
  // MQTTブローカに接続
  client.setServer(mqtt_server, mqtt_port);
}

unsigned long start_tim = 0;

void loop() {
  client.loop();
  // MQTT未接続の場合、再接続
  while(!client.connected() ){
    Serial.println("Mqtt Reconnecting");
    if( client.connect(MQTT_CLIENT_NAME, MQTT_USER, MQTT_PASSWORD) ){
      // MQTT Subscribe
      JsonArray array = json_layout.as<JsonArray>();
      for( int i = 0 ; i < array.size() ; i++ ){
        if( array[i]["topic"] ){
          const char* topic = array[i]["topic"];
          client.subscribe(topic);
        }
      }
      Serial.println("Mqtt Connected and Subscribing");
      break;
    }
    delay(1000);
  }

  unsigned long end_tim = millis();
  if( (end_tim - start_tim) > 60000 ){
    start_tim = end_tim;
    updateTime();
    draw_lines();
  }
}

void updateTime(void){
  struct tm timeInfo;
  getLocalTime(&timeInfo);
  sprintf(str_date, "%04d/%02d/%02d", timeInfo.tm_year + 1900, timeInfo.tm_mon + 1, timeInfo.tm_mday );
  sprintf(str_time, "%02d:%02d", timeInfo.tm_hour, timeInfo.tm_min );
}

void wifi_connect(const char *ssid, const char *password){
  Serial.println("");
  Serial.print("WiFi Connenting");

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(1000);
  }
  Serial.println("");
  Serial.print("Connected : ");
  Serial.println(WiFi.localIP());
}

void draw_lines(void){
  lcd.drawPng(background_buffer, background_buffer_length, 0, 0);

  JsonArray array = json_layout.as<JsonArray>();
  for( int i = 0 ; i < array.size() ; i++ ){
    const char* type = array[i]["type"];
    int x = array[i]["x"] | 0;
    int y = array[i]["y"] | 0;
    int size = array[i]["size"] | 8;
    long color = array[i]["color"] | 0xffffff;
    const char* align = array[i]["align"] | "left";

    char* str;
    if( strcmp(type, "date") == 0 ){
      str = str_date;
    }else
    if( strcmp(type, "time") == 0 ){
      str = str_time;
    }else{
      str = lines[i];
    }

    lcd.setTextColor(lcd.color565((color >> 16) & 0xff, (color >> 8) & 0xff, color & 0xff));
    lcd.setTextSize(size / FONT_SIZE);
    int tsize = lcd.textWidth(str);
    if( strcmp(align, "right") == 0 ){
      lcd.setCursor(x - tsize, y);
    }else
    if( strcmp(align, "center") == 0 ){
      lcd.setCursor(x - tsize / 2, y);
    }else{
      lcd.setCursor(x, y);
    }
    lcd.print(str);
  }
}

long doHttpGet(String url, uint8_t *p_buffer, unsigned long *p_len){
  HTTPClient http;

  Serial.print("[HTTP] GET begin...\n");
  // configure traged server and url
  http.begin(url);

  Serial.print("[HTTP] GET...\n");
  // start connection and send HTTP header
  int httpCode = http.GET();
  unsigned long index = 0;

  // httpCode will be negative on error
  if(httpCode > 0) {
      // HTTP header has been send and Server response header has been handled
      Serial.printf("[HTTP] GET... code: %d\n", httpCode);

      // file found at server
      if(httpCode == HTTP_CODE_OK) {
        // get tcp stream
        WiFiClient * stream = http.getStreamPtr();

        // get lenght of document (is -1 when Server sends no Content-Length header)
        int len = http.getSize();
        Serial.printf("[HTTP] Content-Length=%d\n", len);
        if( len != -1 && len > *p_len ){
          Serial.printf("[HTTP] buffer size over\n");
          http.end();
          return -1;
        }

        // read all data from server
        while(http.connected() && (len > 0 || len == -1)) {
            // get available data size
            size_t size = stream->available();

            if(size > 0) {
                // read up to 128 byte
                if( (index + size ) > *p_len){
                  Serial.printf("[HTTP] buffer size over\n");
                  http.end();
                  return -1;
                }
                int c = stream->readBytes(&p_buffer[index], size);

                index += c;
                if(len > 0) {
                    len -= c;
                }
            }
            delay(1);
        }
      }
  } else {
    http.end();
    Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
    return -1;
  }

  http.end();
  *p_len = index;

  return 0;
}
