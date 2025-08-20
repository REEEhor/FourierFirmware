#include <Keyboard.h>
#include <Wire.h>

#define SLAVE_DEVICE_ID 8

const int cols[] = {
  A2, A1, A0, 15, 14, 16, 10,
};
const int rows[] = {
  A3, 6, 7, 8
};


#define LEN(arr) (sizeof(arr) / sizeof (*(arr)))
bool state[2][LEN(cols)][LEN(rows)];

const byte _ = 0;
const byte UNDEFINED_KEY = _;

const byte keymap[LEN(rows)][LEN(cols)] = {
  {KEY_ESC, 'q', 'w', 'e', 'r', 't', _},
  {KEY_TAB, 'a', 's', 'd', 'f', 'g', _},
  {KEY_LEFT_SHIFT, 'z', 'x', 'c', 'v', 'b'},
  {KEY_LEFT_CTRL, KEY_LEFT_GUI, KEY_LEFT_ALT, _, ' '},
};

void setup() {
  Wire.begin(SLAVE_DEVICE_ID);
  Wire.onReceive(receive_callback);
  Serial.begin(9600);
  for (int col = 0; col < LEN(cols); col++) {
    int col_pin = cols[col];
    pinMode(col_pin, INPUT_PULLUP);
  }
  for (int row = 0; row < LEN(rows); row++) {
    int row_pin = rows[row];
    pinMode(row_pin, OUTPUT);
  }
  Serial.println("I'm allive :D");
}

char str_buffer[128];
byte current_state_idx = 0;
const byte BYTES_PER_MESSAGE = 3;

void receive_callback(int byte_count) {
  if (byte_count == 0) {
    Serial.println("Warning: received a zero byte message");  
    return;
  }
  if (byte_count % BYTES_PER_MESSAGE != 0) {
    sprintf(str_buffer, "Error: received %d bytes, but multiple of %d are expected", byte_count, BYTES_PER_MESSAGE);
    Serial.println(str_buffer);
    return;
  }

  Serial.println("============");
  Serial.println("Received:");
  for (byte i = 0; i < byte_count; i += BYTES_PER_MESSAGE) {
    bool is_button_pressed = (bool)Wire.read();
    byte col_id = Wire.read();
    byte row_id = Wire.read();

    sprintf(str_buffer, "[col=%d row=%d]: %s", col_id, row_id, is_button_pressed ? "PRESS" : "RELEASE");
    Serial.println(str_buffer);
  }
  Serial.println("============\n");
}

void loop() {
    Serial.println("Waiting for press...");
    delay(3000);
}

void _old() {
  current_state_idx = !current_state_idx;
  auto current_state = state[current_state_idx];
  auto prev_state = state[!current_state_idx];

  for (int row = 0; row < LEN(rows); row++) {
    int row_pin = rows[row];
    digitalWrite(row_pin, LOW);

    for (int col = 0; col < LEN(cols); col++) {
      int col_pin = cols[col];
      current_state[col][row] = !digitalRead(col_pin);
      bool change = current_state[col][row] != prev_state[col][row];
      if (change) {
        byte key = keymap[row][col];
        if (key == UNDEFINED_KEY) continue;
        bool key_is_pressed = current_state[col][row];
        if (key_is_pressed) {
          Keyboard.press(key);
        } else {
          Keyboard.release(key);
        }
      }

    }

    digitalWrite(row_pin, HIGH);
  }


  delay(10);
}
