#include <Esp.h>
#include <assert.h>

#include "frame.h"
#include "drv.h"
#include "lwip/def.h"

const int MAX_FRAME_LEFT = 2048;

int getRSSI(void);
const char* get_imei(void);

Frame* get_empty_frame(uint8 method)
{
	static Frame f;
	Frame* rsp = &f;
	memset(rsp, 0, sizeof(Frame));
	
	rsp->data_flag = FRAME_STATIC_DATA;
	rsp->method = method;
	rsp->data = NULL;
	
	return rsp;
}

#define CHIP_NAME "CSM64F02"
#define FIRMWARE_VERSION "0.1.0"

Frame* new_devinfo_frame(void)
{
	Frame* out = get_empty_frame(method_devinfo);
	if(!out) {
		return NULL;
	}

  int32_t rssi = getRSSI();
	static char buf[100];	
	sprintf(buf, "{\"method\":\"INFO\",\"chip\":\"%s\",\"version\":\"%s\",\"rssi\":%d}", CHIP_NAME, FIRMWARE_VERSION, rssi);

	int body_length = strlen(buf);
	set_static_data(out, (uint8*)buf, body_length);

	return out;
}

Frame* new_login_frame()
{
	Frame* out = get_empty_frame(method_login);
	if(!out) {
		return NULL;
	}

	static char body_buf[64];
	sprintf(body_buf, "{\"imei\":\"%s\",\"method\":\"LOGIN\"}", get_imei());
	int body_length = strlen(body_buf);
	set_static_data(out, (uint8*)body_buf, body_length);
	
	return out;
}

Frame* new_electric_frame(const String &body)
{
	Frame* out = get_empty_frame(method_uart);
	if(!out) {
		return NULL;
	}

	static char body_buf[200];
	memset(body_buf, 0, sizeof(body_buf));
	body_buf[2] = 0x80;
	body_buf[3] = 0x02;
	memcpy(body_buf + 4, body.c_str(), body.length());
	set_static_data(out, (uint8*)body_buf, body.length() + 4);
	return out;
}

/*
如何处理状态上送？
状态上送，由终端直接请求，最高位为1则为主动上送
FE FE 80 01 00 00 00 01 主动上送回路状态
*/
Frame* new_states_report_frame()
{
	// state report frame is a normal uart frame.
	Frame* out = get_empty_frame(method_uart);
	if(!out) {
		return NULL;
	}

	static char body_buf[8];
	memset(body_buf, 0, sizeof(body_buf));
	body_buf[2] = 0x80;
	body_buf[3] = 0x01;

	uint32_t states = device_get_lines_state();
	union uint32_writer writer;
	writer.val = htonl(states);
	memcpy(body_buf + 4, writer.data, sizeof(uint32_writer));
	set_static_data(out, (uint8*)body_buf, sizeof(body_buf));
	return out;
}

int frame_length(Frame *f)
{
  assert(f);
  return 2 + 1 + 1 + f->length;
}

void set_dyn_data(Frame *f, uint8* data, size_t d_length)
{
    assert(f);
    assert(data);
    assert(d_length);
	assert(!f->data);

	f->data = data;
	f->length = d_length;
	f->data_flag = FRAME_DYN_DATA;
}

void set_static_data(Frame *f, uint8* data, size_t d_length)
{
	assert(f);
	assert(data);
	assert(d_length);
	assert(!f->data);
	
	f->data = data;
	f->length = d_length;
	f->data_flag = FRAME_STATIC_DATA;
}

void free_frame(Frame *f)
{
	assert(f);
}

int serialize_frame(Frame *f, uint8* out, size_t out_max_length)
{
	assert(f);
	assert(out);
	size_t length = 1 + 1 + f->length;

	assert(out_max_length >= (length + 2));
	union uint16_writer w;
	
	w.val = htons(length);
	
	memcpy(out, w.data, sizeof(union uint16_writer));
	out[2] = f->method;
	out[3] = f->txid;
	memcpy(out + 4, f->data, f->length);

	return length + 2;
}

void serialize_f(Frame *f, uint8* out)
{
	size_t l = 2 + 1 + 1 + f->length;
	serialize_frame(f, out, l);
}

int deserialize_frame(uint8* in_data, size_t in_length, Frame *out)
{
	assert(in_data);
	assert(out);
	assert(in_length > 2);
	
	union uint16_writer w;
	memcpy(w.data, in_data, 2);
	size_t left = ntohs(w.val);
	
	// 2 + left == frame length, +1 for null;
	if(in_length < (2 + left + 1)) {
		Serial.printf("in_length not enough, %d, %d.", in_length, left);
		return -1;
	}
	if(left > MAX_FRAME_LEFT) {
		Serial.printf("too big frame left %d.", left);
		return -1;
	}
	
	out->method = in_data[2];
	out->txid = in_data[3];
	
	static uint8_t dyn_data[128];
	out->length = left - 2;
	if(out->length > sizeof(dyn_data)) {
		Serial.println("static data to small to copy data.");
		return -1;
	}

	if(!out->length) {
		// 如果数据包无 body，则无需 free，也无需 分配内存
		out->data_flag = FRAME_STATIC_DATA;
		out->data = NULL;
	} else {
		out->data_flag = FRAME_STATIC_DATA;
		out->data = &(dyn_data[0]);
		memcpy(out->data, in_data + 4, left - 2);
		out->data[left-2] = 0;
	}
	
	return 0;
}


Buffer* new_buffer(Buffer* out)
{
	assert(out);
	
	static uint8_t buf[512];
	out->buffer = &(buf[0]);

	out->len = 0;
	out->cap = 512;
	return out;
}

void free_buffer(Buffer* buf)
{
	assert(buf && buf->buffer && buf->cap);
}

int buffer_add_data(Buffer* buf, uint8* ptr, size_t len)
{
	assert(buf && buf->buffer && buf->cap);
	assert(ptr);
	assert(len);
	
	if((buf->len + len) > buf->cap) {
		Serial.printf("buffer full, len: [%d], cap: [%d], cur: [%d]", buf->len, buf->cap, len);
		return -1;
	}
	
	memcpy(buf->buffer + buf->len, ptr, len);
	buf->len += len;
	return buf->len;
}

int buffer_try_extract(Buffer* buf)
{
	assert(buf && buf->buffer && buf->cap);
	if(buf->len < 4) {
		return 0;
	}
	
	union uint16_writer w;
	memcpy(w.data, buf->buffer, 2);
	size_t left = ntohs(w.val);
	if(buf->len >= (left + 2)) {
		return left + 2;
	}
	
	return 0;
}

void buffer_shrink(Buffer* buf, size_t len)
{
	assert(buf && buf->buffer && buf->cap);
	assert(len <= buf->len);
	
	size_t w = buf->len - len;
	if(w > 0) {
		memmove(buf->buffer, buf->buffer + len, w);
	}

	buf->len = w;
	buf->buffer[w] = 0;
}

void buffer_reset(Buffer* buf)
{
	assert(buf);
	buf->len = 0;
}


// 1 => full, 0 => not full.
int buffer_is_full(Buffer* buf)
{
	if((buf->len + 4) >= buf->cap) {
		return 1;
	}
	
	return 0;
}


// light proto frame

/*
精简协议，只取 8 字节
*/
void deserialize_lightproto_frame(uint8_t* data, struct lightproto* out)
{
  assert(data);
  assert(out);

  union uint16_writer w;
  memcpy(w.data, data, 2);
  out->txid = ntohs(w.val);

  memcpy(w.data, data + 2, 2);
  out->cmd = ntohs(w.val);

  memcpy(out->data, data + 4, 4);
}

void serialize_lightproto_frame(struct lightproto* in, uint8_t* out)
{
  assert(in);
  assert(out);

  union uint16_writer w;
  // txid
  w.val = htons(in->txid);
  memcpy(out, w.data, 2);

  // cmd
  w.val = htons(in->cmd);
  memcpy(out + 2, w.data, 2);

  // data
  memcpy(out + 4, in->data, 4);
}
