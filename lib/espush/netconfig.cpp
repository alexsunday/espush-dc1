#include <Arduino.h>
#include <os_type.h>
#include <FS.h>
#include <ESP8266WiFi.h>

#include "board.h"
#include "netconfig.h"
#include "led.h"
#include "espush.h"

void setup_ap_config(void)
{
  String ssid = "ZY-" + String(ESP.getChipId());

  if(!WiFi.isConnected()){
    WiFi.persistent(false);
    // disconnect sta, start ap
    WiFi.disconnect(); //  this alone is not enough to stop the autoconnecter
    WiFi.mode(WIFI_AP);
    WiFi.persistent(true);
  } 
  else {
    //setup AP
    WiFi.mode(WIFI_AP_STA);
  }

  WiFi.softAPConfig(IPAddress(10,0,1,1), IPAddress(10,0,1,1), IPAddress(255,255,255,0));
  WiFi.softAP(ssid);
}


#ifdef NETCONFIG_USE_FLASH

#define NETCONFIG_FS_PATH "/netconfig"

static os_timer_t timer;
static char boot_count;

int netconfig_fs_init(void)
{
  // check exists
  bool exists = SPIFFS.exists(NETCONFIG_FS_PATH);
  if(!exists) {
    // open for default
    File f = SPIFFS.open(NETCONFIG_FS_PATH, "w+");
    if(!f) {
      Serial.println("open file netconfig failed.");
      return -1;
    }

    char buf[1];
    buf[0] = 0;

    f.write(buf, sizeof(buf));
    f.close();
    Serial.println("write default file completed.");
  }

  return 0;
}


// 读取标志位，返回 -1 为失败，返回 0 ~ 其他则代表正常读取数据
int read_boot_count(void)
{
  char buf[2];
  File f = SPIFFS.open(NETCONFIG_FS_PATH, "r");
  if(!f) {
    Serial.println("open net file failed.");
    return -1;
  }

  size_t has_read = f.readBytes(buf, 1);
  if(has_read < 0 || has_read != 1) {
    Serial.printf("has read bytes not equal, %d failed.\r\n", has_read);
    f.close();
    return -1;
  }

  f.close();
  return buf[0];
}

// 设定标志位为指定值
int write_boot_count(char v)
{
  File f = SPIFFS.open(NETCONFIG_FS_PATH, "w");
  if(!f) {
    Serial.println("open net file for write failed.");
    return -1;
  }

  char buf[1];
  buf[0] = v;
  f.write(buf, sizeof(buf));
  f.close();
  Serial.println("reset boot flag zero completed.");

  return 0;
}

// 标志位+1 返回增加过后的标志位
int boot_count_add(void)
{
  File f = SPIFFS.open(NETCONFIG_FS_PATH, "r+");
  if(!f) {
    Serial.println("open net file for add failed.");
    return -1;
  }

  char buf[2];
  size_t has_read = f.readBytes(buf, 1);
  if(has_read < 0 || has_read != 1) {
    Serial.println("read byte length not equal, failed.");
    return -1;
  }
  Serial.printf("boot count flag: %d\r\n", buf[0]);

  buf[0] ++;
  boot_count = buf[0];
  f.seek(0);
  f.write(buf, 1);

  f.close();
  Serial.println("boot count add completed.");
  return 0;
}

void timer_callback(void* params)
{
  int res = write_boot_count(0);
  if(res < 0) {
    Serial.println("reset boot count to zero failed");
  }

  if(boot_count >= 5) {
    Serial.println("open smart config.");
    espush.close();
    espush.set_reconnect(false);
#ifdef NETCONFIG_USE_SMARTCONFIG    
    WiFi.beginSmartConfig();
#endif

#ifdef NETCONFIG_USE_WEB_AP
    setup_ap_config();
#endif

#if USE_LED
    set_led_blink_fast();
#endif    
  }
}

void netconfig_flash_init(void)
{
  boot_count = 0;
  int res = netconfig_fs_init();
  if(res < 0) {
    Serial.println("fs init failed.");
    return;
  }

  res = boot_count_add();
  if(res < 0) {
    Serial.println("boot count add failed.");
    return;
  }

  os_timer_disarm(&timer);
  os_timer_setfn(&timer, timer_callback, NULL);
  os_timer_arm(&timer, 3 * 1000, 0);
}

void netconfig_flash_loop()
{
  // empty!
}

#endif

#ifdef NETCONFIG_USE_BUTTON

#include <EasyButton.h>

EasyButton btn(NETCONFIG_BUTTON_GPIO, 35, false, true);


void onPressedForSmartConfig()
{
  Serial.println("open smart config.");
  espush.close();
  espush.set_reconnect(false);
  WiFi.beginSmartConfig();

#if USE_LED
  set_led_blink_fast();
#endif
}

void onPressedForAP()
{
  Serial.println("ap config...???");
  espush.close();
  espush.set_reconnect(false);

#if USE_LED
  set_led_blink_fast();
#endif

  setup_ap_config();
}

#ifdef BOARD_TYPE_DC1
void device_batch_toggle();
#endif

void netconfig_button_clicked()
{
  Serial.println("button clicked...");
#ifdef BOARD_TYPE_DC1
  device_batch_toggle();
#endif  
}

void netconfig_button_init(void)
{
  btn.begin();
#ifdef NETCONFIG_USE_SMARTCONFIG  
  btn.onPressedFor(5000, onPressedForSmartConfig);
#endif
#ifdef NETCONFIG_USE_WEB_AP  
  btn.onPressedFor(4000, onPressedForAP);
#endif  
  btn.onPressed(netconfig_button_clicked);
}

void netconfig_button_loop(void)
{
  btn.read();
}

#endif

/*
1，打开文件系统
2，执行标志位操作 读取标志位并+1
3，打开定时器任务
4，定时器执行完毕后，关闭文件系统
*/
void netconfig_init(void)
{
#ifdef NETCONFIG_USE_FLASH
  netconfig_flash_init();
#endif

#ifdef NETCONFIG_USE_BUTTON
  netconfig_button_init();
#endif
}

void netconfig_loop(void)
{
#ifdef NETCONFIG_USE_FLASH
  netconfig_flash_loop();
#endif

#ifdef NETCONFIG_USE_BUTTON
  netconfig_button_loop();
#endif
}
