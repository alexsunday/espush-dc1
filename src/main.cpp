#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <Wire.h>

#include "espush.h"
#include "web.h"
#include "board.h"
#include "led.h"
#include "netconfig.h"
#include "esp_fs.h"


// 当wifi 由断开转为已连接到AP，则指示灯状态变更为 常亮
void on_wifi_sta_connected_to_ap(const WiFiEventStationModeConnected& evt)
{
  Serial.println("");
  Serial.print("Connected to ");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // 选取一种连接路由器的方式
#if USE_LED  
  set_led_on();
#endif  
}

void setup(void)
{
#ifndef BOARD_TYPE_DC1
  Serial.begin(9600);
  Serial.println("\r\n\r\nwelcome to espush v1.1.0");
#endif

  esp_fs_init();
  netconfig_init();

#if USE_LED
  led_init();
  set_led_off();
#endif

  board_setup();

  WiFi.mode(WIFI_STA);
  WiFi.setAutoConnect(true);
  WiFi.setAutoReconnect(true);
  WiFi.onStationModeConnected(&on_wifi_sta_connected_to_ap);

  web_setup();
  // espush action
  espush.init();
}

void loop(void)
{
  espush.loop();
  yield();
  web_loop();
  yield();
  board_loop();
  yield();
  netconfig_loop();
}
