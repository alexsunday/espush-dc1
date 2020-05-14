#include <Arduino.h>
#include <FS.h>

#include "smemory.h"
#include "drv.h"
#include "frame.h"

#define STATES_FS_PATH "/states"


uint32_t load_states_via_flash(void)
{
  // check exists
  char buf[4];
  union uint32_writer w;
  uint32_t default_state = 0;

  memset(buf, 0, sizeof(buf));
  bool exists = SPIFFS.exists(STATES_FS_PATH);
  if(!exists) {
    // open for default
    File f = SPIFFS.open(STATES_FS_PATH, "w+");
    if(!f) {
      Serial.println("open states file failed.");
      return default_state;
    }

    f.write(buf, sizeof(buf));  // just only write 1 Bytes.
    f.close();
    Serial.println("write default states completed.");
    return default_state;
  }

  // 文件已存在
  File f = SPIFFS.open(STATES_FS_PATH, "r");
  if(!f) {
    Serial.println("open net file failed.");
    return default_state;
  }

  size_t has_read = f.readBytes(buf, sizeof(buf));
  if(has_read < 0 || has_read != sizeof(buf)) {
    Serial.printf("has read bytes not equal, %d failed.\r\n", has_read);
    f.close();
    return default_state;
  }

  f.close();
  memcpy(w.data, buf, sizeof(buf));
  return w.val;
}

void init_states(uint32_t res)
{
  size_t len = device_get_lines_state();

  for(size_t i=0; i!=len; ++i) {
    device_set_line_state(i + 1, bitRead(res, i));
  }
}

void save_states_to_flash()
{
  uint32_t res = device_get_lines_state();
  File f = SPIFFS.open(STATES_FS_PATH, "w");
  if(!f) {
    Serial.println("open net file for write failed.");
    return;
  }

  union uint32_writer w;
  w.val = res;
  f.write(w.data, sizeof(w));
  f.close();
}
