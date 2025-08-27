// 
// LEFT KEYBOARD
//
#define SEND_KEYS

#include <Keyboard.h>
#include <Wire.h>

#define SLAVE_DEVICE_ID 8

const int cols[] = {
  A2, A1, A0, 15, 14, 16, 10,
};
const int rows[] = {
  A3, 6, 7, 8
};

typedef int KeyCode; 

#define MAX_LAYERS 5
#define LEN(arr) (sizeof(arr) / sizeof (*(arr)))
#define KEYBOARD_ROWS_COUNT ( LEN(rows) )
#define KEYBOARD_COLS_COUNT ( 2 * (LEN(cols)) )

struct KeyPosition {
  byte column_id;
  byte row_id;
  bool operator==(const KeyPosition& other) const {
    return this->column_id == other.column_id && this->row_id == other.row_id;
  }
};

struct LayerInfo {
  byte layer_id; 
  KeyPosition layer_leave_key;
};

struct LayerStack {
public:
  LayerInfo stack[MAX_LAYERS]{};
  byte stack_top_idx = 0;

  byte current_layer_id() const {
    return this->stack[this->stack_top_idx].layer_id;
  }

  KeyPosition current_leave_key() const {
    return this->stack[this->stack_top_idx].layer_leave_key;
  }

  LayerInfo& top() {
    return this->stack[this->stack_top_idx];
  }

  void push_layer(LayerInfo layer_info) {
    this->stack_top_idx++;
    this->stack[this->stack_top_idx] = layer_info;
  }

  void pop_layer() {
    this->stack_top_idx--;
    if (this->stack_top_idx < 0) {
      this->stack_top_idx = 0;
      Serial.println("Error: tried to pop stack with one layer");
    }
  }

};
LayerStack layer_stack{};

struct PressedKeys {
  bool key_is_pressed_mapping[KEYBOARD_COLS_COUNT][KEYBOARD_ROWS_COUNT]{};
  bool is_key_currently_down(KeyPosition key) const {
    return this->key_is_pressed_mapping[key.column_id][key.row_id];
  }
  void set_key_state(KeyPosition key, bool state) {
    this->key_is_pressed_mapping[key.column_id][key.row_id] = state;
  }
};
PressedKeys key_state{};

const KeyCode MOMENTARY_LAYER_BASE = 1000;
const KeyCode MOMENTARY_LAYER_MAX = MOMENTARY_LAYER_BASE + MAX_LAYERS;

struct GetMomentaryLayerIdResult {
  KeyCode id;
  bool valid;
};
GetMomentaryLayerIdResult get_momentary_layer_id(KeyCode key_code) {
  bool valid = (MOMENTARY_LAYER_BASE <= key_code) && (key_code <= MOMENTARY_LAYER_MAX);
  if (!valid) {
    return GetMomentaryLayerIdResult{};
  }

  return GetMomentaryLayerIdResult {
    .id = key_code - MOMENTARY_LAYER_BASE,
    .valid = true,
  };
}

const KeyCode _ = -1;
const KeyCode UNDEFINED_KEY = _;
const KeyCode X = 1;
const KeyCode THROUGH_KEY = X;
KeyCode momentary_layer(byte layer_id) {
  return MOMENTARY_LAYER_BASE + layer_id;
}
#define MO momentary_layer

/*
   LEFT          RIGHT

  0123456       0123456
0|oooooo.     0|ooooooo
1|oooooo.     1|ooooo.o
2|oooooo.     2|o.ooooo
3|ooo.o..     3|o...ooo

*/
const KeyCode layers_keymap [MAX_LAYERS][KEYBOARD_ROWS_COUNT][KEYBOARD_COLS_COUNT] = {
  {
    {KEY_ESC, 'q', 'w', 'e', 'r', 't', _,                /*###*/   'y', 'u', 'i', 'o', 'p', KEY_BACKSPACE, KEY_DELETE},
    {KEY_TAB, 'a', 's', 'd', 'f', 'g', _,                /*###*/   'h', 'j', 'k', 'l', ';', _,   KEY_KP_ENTER},
    {KEY_LEFT_SHIFT, 'z', 'x', 'c', 'v', 'b', _,         /*###*/   'n',   _, 'm', ',', '.', '/', KEY_RIGHT_SHIFT},
    {KEY_LEFT_CTRL, KEY_LEFT_GUI, KEY_LEFT_ALT, _, ' ', _, _, /*###*/ ' ',   _,   _,   _, 'x', 'y', 'z'},
    //{KEY_LEFT_CTRL, KEY_LEFT_GUI, KEY_LEFT_ALT, _, ' ', _, _, /*###*/ MO(1),   _,   _,   _, 'x', 'y', 'z'},
  },
  {
    {  X,   X,   X,   X,   X,   X, _, /*###*/    X,   X,   X,   X,  X,  X,  X},
    { '0', '1', '2', '3', '4', '5', _, /*###*/ '6', '7', '8', '9',  X,  _,  X},
    {  X,   X,   X,   X,   X,   X, _, /*###*/    X,   _,   X,   X,  X,  X,  X},
    {  X,   X,   X,   _,   X,   _, _, /*###*/    X,   _,   _,   _,  X,  X,  X},
  },
  {
    {X, X, X, X, X, X, _, /*###*/ X, X, X, X, X, X, X},
    {X, X, X, X, X, X, _, /*###*/ X, X, X, X, X, _, X},
    {X, X, X, X, X, X, _, /*###*/ X, _, X, X, X, X, X},
    {X, X, X, _, X, _, _, /*###*/ X, _, _, _, X, X, X},
  },
  {
    {X, X, X, X, X, X, _, /*###*/ X, X, X, X, X, X, X},
    {X, X, X, X, X, X, _, /*###*/ X, X, X, X, X, _, X},
    {X, X, X, X, X, X, _, /*###*/ X, _, X, X, X, X, X},
    {X, X, X, _, X, _, _, /*###*/ X, _, _, _, X, X, X},
  },
};

#define PRESSED true
#define RELEASED false

char str_buffer[128];
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

  //Serial.println("============");
  //Serial.println("Received:");
  for (byte i = 0; i < byte_count; i += BYTES_PER_MESSAGE) {
    bool is_button_pressed = (bool)Wire.read();
    byte col_id = Wire.read();
    byte row_id = Wire.read();

    //sprintf(str_buffer, "[col=%d row=%d]: %s", col_id, row_id, is_button_pressed ? "PRESS" : "RELEASE");
    //Serial.println(str_buffer);
    col_id = col_id + LEN(cols);
    if (is_button_pressed) {
      handle_key_down(KeyPosition {.column_id = col_id, .row_id = row_id});
    } else {
      handle_key_up(KeyPosition {.column_id = col_id, .row_id = row_id});
    }
  }
  //Serial.println("============\n");
}

void handle_key_up(KeyPosition key) {
  key_state.set_key_state(key, RELEASED);
  const auto current_layer = layers_keymap[layer_stack.current_layer_id()];
  KeyCode key_code = current_layer[key.row_id][key.column_id];

  if (key_code == UNDEFINED_KEY) {
    sprintf(str_buffer, "DEBUG: undefined key pressed: %d", key_code);
    Serial.println(str_buffer);
    return;
  }

  if (layer_stack.current_leave_key() == key) {
    sprintf(str_buffer, "DEBUG: leaving layer number: %d (releasing all non-modifier keys)", layer_stack.current_layer_id());
    layer_stack.pop_layer();
    Serial.println(str_buffer);
    // TODO: don't forget to release all non modifier keys here or something
    return;
  }

  if (key_code < 0 || key_code > 0xff) {
    sprintf(str_buffer, "ERROR: The keycode was %d, which is invalid", key_code);
    Serial.println(str_buffer);
    sprintf(str_buffer, "  - Key: col=%d row=%d", key.column_id, key.row_id);
    Serial.println(str_buffer);
    sprintf(str_buffer, "  - layer id=%d", layer_stack.current_layer_id());
    Serial.println(str_buffer);
    return;
  }

#ifdef SEND_KEYS
  Keyboard.release((byte)key_code);
#endif
  sprintf(str_buffer, "DEBUG: release '%c' [%d]", key_code, key_code);
  Serial.println(str_buffer);
}

void handle_key_down(KeyPosition key) {
  key_state.set_key_state(key, PRESSED);
  const auto current_layer = layers_keymap[layer_stack.current_layer_id()];
  KeyCode key_code = current_layer[key.row_id][key.column_id];
  if (key_code == UNDEFINED_KEY) {
    sprintf(str_buffer, "DEBUG: undefined key pressed: %d", key_code);
    Serial.println(str_buffer);
    return;
  }

  auto layer_id = get_momentary_layer_id(key_code);
  if (layer_id.valid) {
    layer_stack.push_layer(LayerInfo {.layer_id = layer_id.id, .layer_leave_key = key});
    sprintf(str_buffer, "DEBUG: going to layer number: %d", layer_id.id);
    Serial.println(str_buffer);
    return;
  }

  if (key_code < 0 || key_code > 0xff) {
    sprintf(str_buffer, "ERROR: The keycode was %d, which could not be printed", key_code);
    Serial.println(str_buffer);
    return;
  }

#ifdef SEND_KEYS
  Keyboard.press((byte)key_code);
#endif
  sprintf(str_buffer, "DEBUG: PRESS '%c' [%d]", key_code, key_code);
  Serial.println(str_buffer);
}


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

  const byte INVALID_KEY_POSITION_COORDINATE = 0xff; // There will never be a key at row 255 or column 255
  layer_stack.top().layer_leave_key.row_id = INVALID_KEY_POSITION_COORDINATE;
  layer_stack.top().layer_leave_key.column_id = INVALID_KEY_POSITION_COORDINATE;
}

bool states[2][LEN(cols)][LEN(rows)];
byte current_state_id = 0;

void loop() {
  current_state_id = !current_state_id;
  auto current_state = states[current_state_id];
  auto previous_state = states[!current_state_id];

  memset(current_state, 0, sizeof(current_state));

  for (int row_id = 0; row_id < LEN(rows); row_id++) {
    int row_pin = rows[row_id];
    digitalWrite(row_pin, LOW);

    for (int col_id = 0; col_id < LEN(cols); col_id++) {
      int col_pin = cols[col_id];
      bool button_is_pressed = !digitalRead(col_pin);
      current_state[col_id][row_id] = button_is_pressed;

      bool state_changed = current_state[col_id][row_id] != previous_state[col_id][row_id];
      if (!state_changed) continue;

      if (button_is_pressed) {
        handle_key_down(KeyPosition{.column_id = col_id, .row_id = row_id});
      } else {
        handle_key_up(KeyPosition{.column_id = col_id, .row_id = row_id});
      }
    }

    digitalWrite(row_pin, HIGH);
  }

  delay(10);
}



