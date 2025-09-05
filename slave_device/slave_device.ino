#include <Wire.h>

enum struct DeviceId: byte {
  LEFT = 0,
  RIGHT = 1,
};
// Change this to change which keyboard half this code is for
#define THIS_DEVICE (DeviceId::LEFT)

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

#define LEN(arr) (sizeof(arr) / sizeof (*(arr)))

const byte MAX_COLUMN_ID = BINARY(111);
const byte MAX_ROW_ID = BINARY(111);

struct Message {
  byte bits;

  static Message from_all_parts(DeviceId device_id, bool pressed, byte column_id, byte row_id) {
    byte bits = 0;
    bits |= ((byte)device_id) << 7;
    bits |= ((byte)pressed) << 6;
    bits |= column_id << 3;
    bits |= row_id;

    return Message { .bits = bits };
  }

#ifdef THIS_DEVICE
  static Message from_parts(bool pressed, byte column_id, byte row_id) {
    return from_all_parts(THIS_DEVICE, pressed, column_id, row_id);
  }
#endif

  static Message create_empty_message() {
    return Message::from_all_parts(DeviceId::LEFT, false, MAX_COLUMN_ID, MAX_ROW_ID);
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

#define MESSAGE_QUEUE_SIZE 128
Message message_queue[MESSAGE_QUEUE_SIZE];
volatile byte idx_to_push_to = 0;
volatile byte idx_to_pop_from = MESSAGE_QUEUE_SIZE - 1;
//
void push_message_to_queue(Message message) {
  byte next_idx = (idx_to_push_to + 1) % MESSAGE_QUEUE_SIZE;
  bool queue_is_full = next_idx == idx_to_pop_from;
  if (!queue_is_full) {
    message_queue[idx_to_push_to] = message;
    idx_to_push_to = next_idx;
  }
}
//
Message pop_message_from_queue() {
  bool queue_is_empty = idx_to_push_to == idx_to_pop_from;
  if (queue_is_empty) {
    return Message::create_empty_message();
  }
  Message message = message_queue[idx_to_pop_from];
  idx_to_pop_from = (idx_to_pop_from + 1) % MESSAGE_QUEUE_SIZE;
  return message;
}


// Suprisingly, the pins are the same on both boards
const byte column_pins[] = {
  A2, A1, A0, 15, 14, 16, 10,
};
const byte row_pins[] = {
  A3, 6, 7, 8
};

void setup() {
  Serial.begin(9600);
  Wire.begin((byte)THIS_DEVICE);
  Wire.onRequest(request_data_callback);

  for (int col = 0; col < LEN(column_pins); col++) {
    int col_pin = column_pins[col];
    pinMode(col_pin, INPUT_PULLUP);
  }
  for (int row = 0; row < LEN(row_pins); row++) {
    int row_pin = row_pins[row];
    pinMode(row_pin, OUTPUT);
  }
}

bool states[2][LEN(column_pins)][LEN(row_pins)];
bool current_state_id = 0;

void loop() {
  current_state_id = !current_state_id;
  auto current_state = states[current_state_id];
  auto previous_state = states[!current_state_id];

  memset(current_state, 0, sizeof(current_state));

  for (int row_id = 0; row_id < LEN(row_pins); row_id++) {
    int row_pin = row_pins[row_id];
    digitalWrite(row_pin, LOW);

    for (int col_id = 0; col_id < LEN(column_pins); col_id++) {
      int col_pin = column_pins[col_id];
      bool button_is_pressed = !digitalRead(col_pin);
      current_state[col_id][row_id] = button_is_pressed;

      bool state_changed = current_state[col_id][row_id] != previous_state[col_id][row_id];
      if (!state_changed) continue;

      Message message = Message::from_parts(button_is_pressed, col_id, row_id);

      // Send info about the pressed button
      PRINT(
          "Pushing message to queue: [col=%d, row=%d]: %s\n",
          col_id,
          row_id,
          button_is_pressed ? "PRESS" : "release"
      );
      push_message_to_queue(message);
    }

    digitalWrite(row_pin, HIGH);
  }

  delay(10);
}

void request_data_callback() {
  Message message = pop_message_from_queue();
  Wire.write(message.bits);
}


