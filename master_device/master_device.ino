#include <Wire.h>

#define ENABLE_LOG
#define ENABLE_DEBUG_LOG
#define STR_BUFFER_SIZE 128
char str_buffer[STR_BUFFER_SIZE];

#ifdef ENABLE_LOG
  #define LOG_ERROR(fmt, ...) \
    do { \
      snprintf(str_buffer, STR_BUFFER_SIZE, "[ERROR] " fmt, ##__VA_ARGS__); \
      Serial.println(str_buffer); \
    } while (0)
#else
  #define LOG_ERROR(fmt, ...) do {} while (0)
#endif 

#ifdef ENABLE_DEBUG_LOG
  #define PRINT(fmt, ...) \
    do { \
      snprintf(str_buffer, STR_BUFFER_SIZE, fmt, ##__VA_ARGS__); \
      Serial.print(str_buffer); \
    } while (0)
#else
  #define PRINT(fmt, ...) do {} while (0)
#endif 

// Workaround around not being able to do `42'000` or `100'000'000'000` since my
// version of the compiler is really dumb
#define _HELPER_CONCAT(a,b,c,d,e,f,...) a##b##c##d##e##f
#define CONCAT(...) _HELPER_CONCAT(__VA_ARGS__,,,,,,) // works up to 6 arguments
#define BINARY(...) CONCAT(0b, __VA_ARGS__) // works up to 5 arguments

// Usage:
//  `BINARY(11, 001, 10)` -> `0b1100110`

#define DEVICE_COUNT 2
byte device_ids[DEVICE_COUNT] = {0, 1};

const byte MAX_COLUMN_ID = BINARY(111);
const byte MAX_ROW_ID = BINARY(111);

enum struct DeviceId: byte {
  LEFT = 0,
  RIGHT = 1,
};

struct Message {
  byte bits;

  static Message from_parts(DeviceId device_id, bool pressed, byte column_id, byte row_id) {
    byte bits = 0;
    bits |= ((byte)device_id) << 7;
    bits |= ((byte)pressed) << 6;
    bits |= column_id << 3;
    bits |= row_id;

    return Message { .bits = bits };
  }

  static Message create_empty_message() {
    return Message::from_parts(DeviceId::LEFT, false, MAX_COLUMN_ID, MAX_ROW_ID);
  };

  bool is_empty() const {
    return this->bits == Message::create_empty_message().bits;
  }

  DeviceId device_id() const {
    byte mask = BINARY(1000, 0000);
    bool is_right = (this->bits & mask) != 0;
    return (DeviceId)is_right;
  }

  bool key_got_pressed() const {
    byte mask = BINARY(0100, 0000);
    bool got_pressed = (this->bits & mask) != 0;
    return got_pressed;
  }

  byte column_id() const {
    return (this->bits >> 3) & BINARY(0000, 0111);
  }

  byte row_id() const {
    return (this->bits) & BINARY(0000, 0111);
  }

};


void setup() {
  Wire.begin();        // join I2C bus (address optional for master)
  Serial.begin(9600);  // start serial for output
}

byte which_device_idx = 1;
uint32_t last_left_warning_timestamp = 0;
uint32_t last_right_warning_timestamp = 0;

void loop() {
  which_device_idx = !which_device_idx;
  byte current_device_id = device_ids[which_device_idx];

  byte received_bytes = Wire.requestFrom(current_device_id, 1);

  uint32_t *last_warning_timestamp = current_device_id == 0 ? &last_left_warning_timestamp : &last_right_warning_timestamp;
  if (received_bytes == 0) {
    uint32_t millis_from_last_warning = millis() - *last_warning_timestamp;
    if (millis_from_last_warning > 5000) {
      PRINT("[WARN] Device [id=%d] is disconnected (received 0 bytes on request)\n", current_device_id);
      *last_warning_timestamp = millis();
    }
    return;
  }
  *last_warning_timestamp = 0;

  if (received_bytes != 1) {
    PRINT("[WARN] Device [id=%d] returned %d bytes (expected one byte)\n", current_device_id, received_bytes);
    return;
  }

  int read_byte = Wire.read();
  if (!(0 <= read_byte && read_byte <= 0xff)) {
    LOG_ERROR("Invalid byte from I2C: %d (0x%x)", read_byte, read_byte);
    return;
  }

  Message message = Message { .bits = (byte)read_byte };
  if (message.is_empty()) {
    // PRINT("[DEBUG] EMPTY_MESSAGE from device [id=%d]\n", current_device_id);
    return;
  }

  PRINT(
      "[DEBUG] Got message [%c|%s|col=%d|row=%d]\n",
      (message.device_id() == DeviceId::LEFT) ? 'L' : 'R',
      message.key_got_pressed() ? "PRESSED" : "release",
      message.column_id(),
      message.row_id()
  );

}

