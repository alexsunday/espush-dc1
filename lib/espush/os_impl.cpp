#include <Arduino.h>
#include <Esp.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>

#include "frame.h"
#include "board.h"


static char chip_imei[32];

/*
  signal to espush-rssi ,signal 的值，为 0~5 共 6 个等级，返回 0~31 的 平台值
  平台rssi值与信号等级对应关系
  0 => 0
  1 => 1
  [2, 15] => 2
  [16, 25] => 3
  [26, 30] => 4
  31 => 5
*/
int signal_to_esp_rssi(int signal)
{
    switch (signal)
    {
    case 0:
        return 0;
    case 1:
        return 1;
    case 2:
        return 2;
    case 3:
        return 16;
    case 4:
        return 26;
    case 5:
        return 31;
    default:
        return 0;
    }
}

/*
    rssi to signal 返回 0~5 共6个等级
    [-126, -88) 或者 [156, 168) 为 0 格
    [-88, -78) 或者 [168, 178) 为 1 格
    [-78, -67) 或者 [178, 189) 为 2 格
    [-67, -55) 或者 [189, 200) 为 3 格
    [-55, -45) 或者 为 4 格
    [-45, 0] 为 第 5 格
*/
int rssi_to_signal(int rssi)
{
    if(rssi > 0) {
        return 5;
    } else if(rssi >= -45) {
        return 5;
    } else if(rssi >= -55) {
        return 4;
    } else if(rssi >= -67) {
        return 3;
    } else if(rssi >= -78) {
        return 2;
    } else if(rssi >= -88) {
        return 1;
    } else {
        return 0;
    }
    
    return 0;
}

// 返回的 rssi，必须格式化为 类似 GPRS 形式
int getRSSI(void)
{
    int32_t rssi = WiFi.RSSI();
    return signal_to_esp_rssi(rssi_to_signal(rssi));
}

const char* get_imei(void)
{
    uint32_t chip = ESP.getChipId();
#ifdef BOARD_TYPE_DC1    
    sprintf(chip_imei, "WED%X", chip);
#else
    sprintf(chip_imei, "WE1%X", chip);
#endif    
    return chip_imei;
}

void show_imei(void)
{
    const char* imei = get_imei();
    Serial.printf("IMEI:[%s]\r\n", imei);
}
