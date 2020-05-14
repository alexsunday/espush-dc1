#include <FS.h>
#include "esp_fs.h"

#include "board.h"

// 文件系统初始化
int esp_fs_init(void)
{
  SPIFFSConfig cfg;
  cfg.setAutoFormat(true);
  SPIFFS.setConfig(cfg);

  bool result = SPIFFS.begin();
  if(!result) {
    Serial.println("spiffs begin failed.");
    return -1;
  }

  return 0;
}


// 关闭文件系统
void esp_fs_close(void)
{
  SPIFFS.end();
}
