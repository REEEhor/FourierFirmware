#include <Wire.h>

#define SLAVE_DEVICE_ID 8

const byte cols[] = {
  A2, A1, A0, 15, 14, 16, 10,
};
const byte rows[] = {
  A3, 6, 7, 8
};

#define LEN(arr) (sizeof(arr) / sizeof (*(arr)))

void setup() {
  // join I2C bus (address optional for master)
  Wire.begin(/* address */); 
                             
  for (int col = 0; col < LEN(cols); col++) {
    int col_pin = cols[col];
    pinMode(col_pin, INPUT_PULLUP);
  }
  for (int row = 0; row < LEN(rows); row++) {
    int row_pin = rows[row];
    pinMode(row_pin, OUTPUT);
  }
}

bool states[2][LEN(cols)][LEN(rows)];
bool current_state_id = 0;

void loop() {
  current_state_id = !current_state_id;
  auto current_state = states[current_state_id];
  auto previous_state = states[!current_state_id];

  memset(current_state, 0, sizeof(current_state));

  bool opened_transmition = false;
  for (int row_id = 0; row_id < LEN(rows); row_id++) {
    int row_pin = rows[row_id];
    digitalWrite(row_pin, LOW);

    for (int col_id = 0; col_id < LEN(cols); col_id++) {
      int col_pin = cols[col_id];
      bool button_is_pressed = !digitalRead(col_pin);
      current_state[col_id][row_id] = button_is_pressed;
      
      bool state_changed = current_state[col_id][row_id] != current_state[col_id][row_id];
      if (!state_changed) continue;

      if (!opened_transmition) {
        Wire.beginTransmission(SLAVE_DEVICE_ID);
      }
      // Send info about the pressed button
      Wire.write((byte)button_is_pressed);
      Wire.write((byte)col_id);
      Wire.write((byte)row_id);
    }

    digitalWrite(row_pin, HIGH);
  }

  if (opened_transmition) {
      Wire.endTransmission();
  }


  delay(10);
}

