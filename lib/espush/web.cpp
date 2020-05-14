#include <Arduino.h>

#include "board.h"

#ifdef BOARD_TYPE_DC1
#include <CSE7766.h>
#include <CAT9554.h>
#endif

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>

#include "drv.h"
#include "espush.h"


#define HTTP_PORT 80
ESP8266WebServer server(HTTP_PORT);
ESP8266HTTPUpdateServer httpUpdater;

// 看你的继电器是连接那个io，默认gpio0
const int logLed = 14;
const int wifiLed = 0;
// 插口
const int plugin4 = 4;
const int plugin5 = 5;
const int plugin6 = 6;
// 总开关
const int plugin7 = 7;

const char AP_SAVE_RSP[] PROGMEM = "{\"msg\":\"ok\", \"code\": 0}";
const char AP_ITEM[] PROGMEM = "{\"s\": \"{v}\", \"r\": {r}, \"e\": {e} },";


template <typename Generic>
void DEBUG(Generic text) {
  if (1) {
    Serial.print("*WM: ");
    Serial.println(text);
  }
}


// web服务器的根目录
void handleRoot()
{
  String msg = "<h1>ZhiYun</h1><p>DC1 firmware from zhiyun.</p><p>Build at ";
  msg += __DATE__;
  msg += " ";
  msg += __TIME__;
  msg += "</p>";
  msg += "<ul>";
  msg += "<li><a href='https://discuss.espush.cn/'>Discuss</a></li>";
  msg += "<li><a href='https://shang.qq.com/wpa/qunwpa?idkey=269750631a47930330c196f4c312aba0997e9d6e6efe617aab1a3a5c14d0e579'>QGroup</a></li>";
  msg += "<li><a href='/update'>OTA</a></li>";
  msg += "</ul>";
  server.send(200, "text/html", msg);
}

#ifdef BOARD_TYPE_DC1
// 电力芯片cse7766
void handleCSE7766()
{
  double value = 0;
  for (uint8_t i=0; i<server.args(); i++){
    if (server.argName(i)=="type")
    {
      if (server.arg(i)=="getVoltage"){
        value = myCSE7766.getVoltage();
      }else if (server.arg(i)=="getCurrent"){
        value = myCSE7766.getCurrent();
      }else if (server.arg(i)=="getActivePower"){
        value = myCSE7766.getActivePower();
      }else if (server.arg(i)=="getApparentPower"){
        value = myCSE7766.getApparentPower();
      }else if (server.arg(i)=="getReactivePower"){
        value = myCSE7766.getReactivePower();
      }else if (server.arg(i)=="getPowerFactor"){
        value = myCSE7766.getPowerFactor();
      }else if (server.arg(i)=="getEnergy"){
        value = myCSE7766.getEnergy();
      }
    }
  }
  String message = "{\"code\":0,\"value\":"+String(value)+",\"message\":\"success\"}";
  server.send(200, "application/json", message);
}

// 当前的LED开关状态API
void handleCurrentLEDStatus()
{
  String message;
  message = "{\"Voltage\":"+String(myCSE7766.getVoltage())+
  ",\"Current\":"+String(myCSE7766.getCurrent())+
  ",\"ActivePower\":"+String(myCSE7766.getActivePower())+
  ",\"ApparentPower\":"+String(myCSE7766.getApparentPower())+
  ",\"ReactivePower\":"+String(myCSE7766.getReactivePower())+
  ",\"PowerFactor\":"+String(myCSE7766.getPowerFactor())+
  ",\"Energy\":"+String(myCSE7766.getEnergy())+
  ",\"code\":0,\"message\":\"success\"}";
  server.send(200, "application/json", message);
}
#endif

const char* get_imei(void);
int device_get_lines(void);

void handleDeviceInfo()
{
  String msg;
  String imei(get_imei());

  msg = "{\"chip_id\":\"" + String(imei) +
  "\",\"type\":\"light\",\"chip\":\"ESP-DC1\",\"lines\":" + String(device_get_lines()) +
  ",\"bright\":0}";

  server.send(200, "application/json", msg);
}

void handleReconnect()
{
  espush.set_reconnect(true);
  server.send(200, "application/json", "{\"msg\": \"OK\"}");
}

// 页面或者api没有找到
void handleNotFound()
{
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET)?"GET":"POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i=0; i<server.args(); i++){
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}

/*
局域网模式
发送 POST 请求，无缓存，到设备 IP

/lmode/status 查询所有回路状态，组装 JSON 返回
*/
void handle_lmode_status()
{
  static char buf[8];
  memset(buf, 0, sizeof(buf));

  uint32_t result = device_get_lines_state();
  sprintf(buf, "%d", result);
  server.send(200, "application/json", buf);
}

// /lmode/state line=1&state=on/off 单回路操作
void handle_lmode_state()
{
  String line = server.arg("line");
  String state_str = server.arg("state");
  if(line.equals("")) {
    server.send(400, "application/json", "{\"msg\":\"arguments line error!\"}");
    return;
  }
  if(line.equals("")) {
    server.send(400, "application/json", "{\"msg\":\"arguments state error!\"}");
    return;
  }

  int lineNo = atoi(line.c_str());
  bool state = false;
  if(state_str.equals("on")) {
    state = true;
  }

  device_set_line_state(lineNo, state);
  server.send(200, "application/json", "{\"msg\":\"ok!\"}");
}

// /lmode/batch state=on/off 批量操作
void handle_lmode_batch()
{
  String argsName = "";
  String argValue = "";
  for(int i=0; i!=server.args(); ++i) {
    argsName = server.argName(i);
    if(argsName.equals("state")) {
      argValue = server.arg(i);
      if(argValue.equals("on")) {
        device_set_batch_state(true);
      } else {
        device_set_batch_state(false);
      }

      server.send(200, "application/json", "{\"msg\":\"ok\"}");
      return;
    }
  }

  server.send(400, "application/json", "{\"msg\":\"arguments error!\"}");
}

void handleSaveAP()
{
  DEBUG(F("my WiFi save"));

  //SAVE/connect here
  String ssid = server.arg("s").c_str();
  String pass = server.arg("p").c_str();

  String page = FPSTR(AP_SAVE_RSP);
  server.sendHeader("Content-Length", String(page.length()));
  server.send(200, "application/json", page);
  DEBUG(F("Sent wifi save page"));
  delay(50);

  wl_status_t result = WiFi.begin(ssid.c_str(), pass.c_str());
  if(result == WL_CONNECT_FAILED) {
    Serial.println("connect failed.");
    return;
  }
  
  espush.set_reconnect(true);
}

int getRSSIasQuality(int RSSI) {
  int quality = 0;

  if (RSSI <= -100) {
    quality = 0;
  } else if (RSSI >= -50) {
    quality = 100;
  } else {
    quality = 2 * (RSSI + 100);
  }
  return quality;
}

// 把字符串转换为HEX 表示
String hex_convert(String &s)
{
  String out = "";
  for(unsigned int i=0; i!= s.length(); ++i) {
    out += String(s[i], 16);
  }

  return out;
}

void handleGetAPs()
{
  int n = WiFi.scanNetworks();
  int indices[n];
  for (int i = 0; i < n; i++) {
    indices[i] = i;
  }

  // RSSI SORT
  for (int i = 0; i < n; i++) {
    for (int j = i + 1; j < n; j++) {
      if (WiFi.RSSI(indices[j]) > WiFi.RSSI(indices[i])) {
        std::swap(indices[i], indices[j]);
      }
    }
  }

  String cssid;
  for (int i = 0; i < n; i++) {
    if (indices[i] == -1) continue;
    cssid = WiFi.SSID(indices[i]);
    for (int j = i + 1; j < n; j++) {
      if (cssid == WiFi.SSID(indices[j])) {
        DEBUG("DUP AP: " + WiFi.SSID(indices[j]));
        indices[j] = -1; // set dup aps to index -1
      }
    }
  }

  String result("[");
  //display networks in page
  for (int i = 0; i < n; i++) {
    if (indices[i] == -1) continue; // skip dups
    DEBUG(WiFi.SSID(indices[i]));
    DEBUG(WiFi.RSSI(indices[i]));
    int quality = getRSSIasQuality(WiFi.RSSI(indices[i]));

    String item = FPSTR(AP_ITEM);
    String rssiQ;
    rssiQ += quality;

    String cur_ssid = WiFi.SSID(indices[i]);
    item.replace("{v}", hex_convert(cur_ssid));
    item.replace("{r}", rssiQ);

    if (WiFi.encryptionType(indices[i]) != ENC_TYPE_NONE) {
      item.replace("{e}", "1");
    } else {
      item.replace("{e}", "0");
    }

    DEBUG(item);
    result += item;
    delay(0);
  }

  // 去掉最后的逗号
  result.setCharAt(result.length() - 1, ']');
  // result[result.length()] = ']';

  server.sendHeader("Content-Length", String(result.length()));
  server.send(200, "application/json", result);
}

void web_setup()
{
  server.on("/", handleRoot);

#ifdef BOARD_TYPE_DC1
  server.on("/status", handleCurrentLEDStatus);
  server.on("/cse7766", handleCSE7766);
#endif  

  server.on("/espush/reconnect", handleReconnect);
  server.on("/device/info", handleDeviceInfo);
  server.on("/lmode/status", handle_lmode_status);
  server.on("/lmode/state", handle_lmode_state);
  server.on("/lmode/batch", handle_lmode_batch);
  server.on("/device/wifi/save", handleSaveAP);
  server.on("/device/wifi/scan", handleGetAPs);

  httpUpdater.setup(&server);
  server.onNotFound(handleNotFound);

  server.begin();
}

void web_loop(void)
{
  server.handleClient();
}
