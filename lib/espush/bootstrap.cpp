#include <Arduino.h>
#include <assert.h>

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <ArduinoJson.h>
#include "os_impl.h"

// 返回 -1 连接失败，返回0成功，返回-2不允许重连
int _get_bootstrap_result(HTTPClient& http, String* address, int* port, bool* tls)
{
  http.setReuse(false);
  int httpCode = http.GET();
  if(httpCode <= 0) {
    Serial.println("http code failed.");
    return -1;
  }

  if(httpCode == 404) {
    Serial.println("device not regist on cloud.");
    return -2;
  }

  if(httpCode != 200) {
    Serial.println("bootstrap failed.");
    return -1;
  }

  String result = http.getString();
  Serial.printf("result length %d\r\n", result.length());

  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, result.c_str());
  if(error) {
    Serial.println("json deserialize failed.");
    return -1;
  }

  const char* addr = doc["address"];
  *address = addr;
  *port = doc["port"].as<long>();
  *tls = doc["use_tls"].as<bool>();
  Serial.printf("addr: %s, port: %d, tls: %d\r\n", address->c_str(), *port, *tls);
  return 0;
}

int get_bootstrap_result(String* address, int* port, bool* tls)
{
  assert(address);
  assert(port);

  WiFiClient client;
  HTTPClient http;

  // String url = "http://192.168.2.108:8001/api/portal/bootstrap?tls=0&chip_id=";
#ifdef ASYNC_TCP_SSL_ENABLED
  String url = "http://light.espush.cn/api/portal/bootstrap?tls=1&chip_id=";
#else
  String url = "http://light.espush.cn/api/portal/bootstrap?tls=0&chip_id=";
#endif  
  url += get_imei();

  bool f = http.begin(client, url);
  if(!f) {
    Serial.println("http begin failed.");
    return -1;
  }

  int rc = _get_bootstrap_result(http, address, port, tls);
  http.end();

  return rc;
}
