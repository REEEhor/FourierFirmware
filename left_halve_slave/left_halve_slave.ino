// 
// LEFT KEYBOARD
//
#define SEND_KEYS

#include <Keyboard.h>
#include <Wire.h>


// ========================================================================================

// Workaround around not being able to do `42'000` or `100'000'000'000` since my
// version of the compiler is really dumb
#define _HELPER_CONCAT(a,b,c,d,e,f,...) a##b##c##d##e##f
#define CONCAT(...) _HELPER_CONCAT(__VA_ARGS__,,,,,,) // works up to 6 arguments
#define BINARY(...) CONCAT(0b, __VA_ARGS__) // works up to 5 arguments

// Usage:
//  `BINARY(11, 001, 10)` -> `0b1100110`


// ========================================================================================

// ================================= LOGGING =================================

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
  #define DBG(fmt, ...) \
    do { \
      snprintf(str_buffer, STR_BUFFER_SIZE, "[debug] " fmt, ##__VA_ARGS__); \
      Serial.println(str_buffer); \
    } while (0)
#else
  #define DBG(fmt, ...) do {} while (0)
#endif 

// ============================== END OF LOGGING ==============================
#define BIN_CONCAT(a,b) 0b##a##b

#define SLAVE_DEVICE_ID 8

const int cols[] = {
  A2, A1, A0, 15, 14, 16, 10,
};
const int rows[] = {
  A3, 6, 7, 8
};

#define MAX_SPECIAL_SHIFT_KEYS 20
#define MAX_MACROS 20

enum class Descriptor: byte {
  SIMPLE_KEY = 0,           // Bottom byte is the byte to be sent via `Keyboard.press(key_code)`.

  HARDWARE_INVALID_KEY = 1, // The key on the keyboard cannot be pressed.

  NO_ACTION = 2,            // The action for the key is not defined.

  THROUGH_KEY = 3,          // Use the action from the previous layer.

  MACRO_KEY = 4,            // Bottom byte is index into the macro array.

  MOMENTARY_LAYER = 5,      // Bottom byte is the layer id.

  SPECIAL_SHIFT = 6,        // Overides what should happen when a shift is pressed.

  CZECH_KEY = 7,            // Bottom byte is the character that should be pressed,
                            //  for example 'n' produces 'ň', shift+'n' produces 'Ň'.
                            // Since all the keys which we want to their "Czech version" to are ASCII (e, s, c, n, d, t, o, u, ...),
                            //  we are going to use the most significant bit of the bottom byte to mark, whether to make the key with ´ or ˇ,
                            //  since that bit would otherwise always be 0.
};

enum class Diacritic: byte {
  ACUTE_ACCENT = 0, // ´
  CARON = 1,        // ˇ
};

struct KeyCode {
  uint16_t bits;

  Descriptor descriptor() const {
    const uint16_t mask = BINARY(0000, 0111, 0000, 0000);
    return (Descriptor)((this->bits & mask) >> 8);
  }

  bool has_shift_indicator() const {
    const uint16_t mask = BINARY(1000, 0000, 0000, 0000);
    return (this->bits & mask);
  }

  void set_shift_indicator(bool has_shift) {
    uint16_t mask = BINARY(1000, 0000, 0000, 0000);
    if (has_shift) {
      this->bits |= mask;
    } else {
      this->bits &= ~mask;
    }
  }

  byte data_bits() const {
    const uint16_t mask = BINARY(0000, 0000, 1111, 1111);
    return (this->bits & mask);
  }

  void set_descriptor(Descriptor descriptor) {
    uint16_t mask = BINARY(0000, 0111, 0000, 0000);
    uint16_t descriptor_bytes = ((uint16_t)descriptor) << 8;
    this->bits &= ~mask;
    this->bits |= descriptor_bytes;
  }

  void set_data_bits(byte data_bits) {
    this->bits &= 0xff00;
    this->bits |= data_bits;
  }

  Diacritic get_czech_diacritic() const {
    bool is_caron = this->data_bits() & BINARY(1000, 0000);
    return (Diacritic)is_caron;
  }

  byte get_czech_unmodified_char() const {
    return this->data_bits() & (0111, 1111);
  }

  // ============ "Constructors" ============

  static KeyCode from_parts(Descriptor descriptor, byte data_bits = 0, bool has_shift = false) {
     KeyCode result = KeyCode { .bits = data_bits };
     result.set_descriptor(descriptor);
     result.set_shift_indicator(has_shift);

     return result;
  }

  static KeyCode create_simple(char bits) {
     return KeyCode::from_parts(Descriptor::SIMPLE_KEY, bits);
  }

  static KeyCode create_special_shift(KeyCode special_shift_array[2][MAX_SPECIAL_SHIFT_KEYS], byte* special_shift_array_len, KeyCode no_shift_key_code, KeyCode shift_key_code) {
    byte new_entry_idx = *special_shift_array_len;
    (*special_shift_array_len)++;

    KeyCode* entry = special_shift_array[new_entry_idx];
    entry[0] = no_shift_key_code;
    entry[1] = shift_key_code;

    return KeyCode::from_parts(Descriptor::SPECIAL_SHIFT, new_entry_idx, /* is special shift: */ true);
  }

  static KeyCode create_momentary_layer(byte layer_id) {
    return KeyCode::from_parts(Descriptor::MOMENTARY_LAYER, layer_id);
  }

  static KeyCode create_czech_key(char unmodified_char, Diacritic modifier) {
    byte bits = unmodified_char | ((byte)modifier << 7);
    return KeyCode::from_parts(Descriptor::CZECH_KEY, bits);
  }

  // TODO: static KeyCode create_macro(... macro arr ...) { ... }

};

struct SpecialShiftArray {
  KeyCode data[2][MAX_SPECIAL_SHIFT_KEYS];
  byte len = 0;
};
SpecialShiftArray special_shift_array;

struct MacroArray {
  char* data[MAX_MACROS];
  byte len = 0;
};
MacroArray macro_array;

// Requirements:
//  - do something different when shift 
//    - implement via `Keyboard.release('SHIFT')` which returns the number of keys released
//
//  - be able to print czech symbols
//    - maybe via macros?
//
//
//   XXXXXXXX XXXXXXXX
//   |    | |
//   |    | |
//   |    enum Descriptor: byte {
//   |       /* Describes, which type of KeyCode this is */
//   |       /* See the definition above for more info :) */
//   |    }
//   |
//   |
//   in the main layout definition:
//     - "if shift is pressed, go to the shift alternate keycodes array" indicator bit
//     - bottom byte is index into the array of shifted keys
//       - the element of the array is two field array [keycode-without-shift, keycode-with-shift]
//   in the shifted keys array:
//     - whether the shift key should be pressed or not for that key
//     - mostly useful for SIMPLE_KEY
//
//


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
       LOG_ERROR("tried to pop layer stack with one layer");
     }
  }

};
LayerStack layer_stack{};

struct PressedKeys {
  byte key_is_pressed_mapping[KEYBOARD_COLS_COUNT][KEYBOARD_ROWS_COUNT]{};
  byte release_byte_for(KeyPosition key) const {
    return this->key_is_pressed_mapping[key.column_id][key.row_id];
  }
  void set_key_state(KeyPosition key, byte state) {
    this->key_is_pressed_mapping[key.column_id][key.row_id] = state;
  }
};
PressedKeys key_state{};

#define MO(layer_id) KeyCode::create_momentary_layer(layer_id)
#define S(chr)       KeyCode::create_simple(chr)
#define CZ(chr)      KeyCode::create_czech_key(chr)
#define __           KeyCode::from_parts(Descriptor::HARDWARE_INVALID_KEY)
#define NA           KeyCode::from_parts(Descriptor::NO_ACTION)
#define XX           KeyCode::from_parts(Descriptor::THROUGH_KEY)

/*
   LEFT          RIGHT

  0123456       0123456
0|oooooo.     0|ooooooo
1|oooooo.     1|ooooo.o
2|oooooo.     2|o.ooooo
3|ooo.o..     3|o...ooo
*/

// MAIN LAYOUT DEFINITION
const KeyCode layers_keymap [MAX_LAYERS][KEYBOARD_ROWS_COUNT][KEYBOARD_COLS_COUNT] = {
  {
    {S(KEY_ESC), S('q'), S('w'), S('e'), S('r'), S('t'), __,                /*###*/   S('y'), S('u'), S('i'), S('o'), S('p'), S(KEY_BACKSPACE), S(KEY_BACKSPACE)},
    {S(KEY_TAB), S('a'), S('s'), S('d'), S('f'), S('g'), __,                /*###*/   S('h'), S('j'), S('k'), S('l'), S(';'), __, S(KEY_KP_ENTER)},
    {S(KEY_LEFT_SHIFT), S('z'), S('x'), S('c'), S('v'), S('b'), __,         /*###*/   S('n'),  __, S('m'), S(','), S('.'), S('/'), S(KEY_RIGHT_SHIFT)},
    {S(KEY_LEFT_CTRL), S(KEY_LEFT_GUI), S(KEY_LEFT_ALT), __, S(' '), __, __, /*###*/ MO(1),   __,   __,   __, S('x'), S('y'), S('z')},
  },

  // TODO the ( " ) could have a better meaning with shift held down
  {
    { S('`'),      NA,     NA,     NA,      NA,     __, __, /*###*/ NA, NA, NA, NA, NA, S(KEY_DELETE), },
    { S('0'),  S('1'), S('2'), S('3'),  S('4'), S('5'), __, /*###*/ S('6'), S('7'), S('8'), S('9'), XX, __, XX },
    {     XX, S('\\'), S('9'), S('\''), S('0'),     XX, __, /*###*/ S('-'), __, S('='), S('['), S(']'), NA, XX },
    { XX, S(KEY_MENU),     XX,      __,  MO(2),     __, __, /*###*/ NA, __, __, __, XX, XX, XX },
  },

  // TODO make z,x,c,v use ctrl in front of them
  {
    {XX, XX, S(KEY_LEFT_SHIFT), XX, XX, XX, __,                          /*###*/ XX, S(KEY_HOME),         S(KEY_UP_ARROW), S(KEY_END),         XX, XX, XX},
    {XX, S(KEY_LEFT_CTRL), S(KEY_LEFT_GUI), S(KEY_LEFT_ALT), XX, XX, __, /*###*/ XX, S(KEY_LEFT_ARROW), S(KEY_DOWN_ARROW), S(KEY_RIGHT_ARROW), XX, __, XX},
    {XX, 'z', 'x', 'c', 'v', 'b', __,                                    /*###*/ XX, __, XX, XX, XX, XX, XX}, 
    {XX, XX, XX, __, NA, __, __,                                         /*###*/ XX, __, __, __, XX, XX, XX},
  },
  {
    { XX, XX, XX, XX, XX, XX, __, /*###*/ XX, XX, XX, XX, XX, XX, XX },
    { XX, XX, XX, XX, XX, XX, __, /*###*/ XX, XX, XX, XX, XX, __, XX },
    { XX, XX, XX, XX, XX, XX, __, /*###*/ XX, __, XX, XX, XX, XX, XX },
    { XX, XX, XX, __, XX, __, __, /*###*/ XX, __, __, __, XX, XX, XX },
  },
};

#undef MO(layer_id)
#undef S(chr)
#undef CZ(chr)
#undef __
#undef NA
#undef XX

KeyCode get_keycode_at_layer(KeyPosition key_pos, byte layer_id) {
  const auto layer = layers_keymap[layer_id];
  return layer[key_pos.row_id][key_pos.column_id];
}

#define BASE_LAYER 0
#define PRESSED_BUT_NO_INFO true
#define RELEASED false

const byte BYTES_PER_MESSAGE = 3;

void receive_callback(int byte_count) {
  if (byte_count == 0) {
    LOG_ERROR("Received a zero byte message");
    return;
  }
  if (byte_count % BYTES_PER_MESSAGE != 0) {
    LOG_ERROR("Received %d bytes, but multiple of %d are expected", byte_count, BYTES_PER_MESSAGE);
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

KeyCode get_keycode_by_passing_through_keys(KeyPosition key) {
  KeyCode key_code = get_keycode_at_layer(key, layer_stack.stack[0].layer_id);
  byte result_stack_idx = layer_stack.stack_top_idx;

  for (;result_stack_idx != 0; result_stack_idx--) {
    byte layer_id = layer_stack.stack[result_stack_idx].layer_id;
    KeyCode tmp_key_code = get_keycode_at_layer(key, layer_id);
    if (tmp_key_code.descriptor() != Descriptor::THROUGH_KEY) {
      key_code = tmp_key_code;
      break;
    };
  }
  return key_code;
}

void handle_key_up(KeyPosition key) {
  byte byte_for_release = key_state.release_byte_for(key);
  if (!byte_for_release) {
    // We encountered a key, that was released before
    DBG("Released key[row=%d][col=%d], which was released before", key.row_id, key.column_id);
    return;
  }

  if (layer_stack.current_leave_key() == key) {
    key_state.set_key_state(key, RELEASED);
    handle_layer_exit();
    return;
  }

  if (byte_for_release == PRESSED_BUT_NO_INFO) {
    key_state.set_key_state(key, RELEASED);
    DBG("unknown key was released [row=%d][col=%d]", key.row_id, key.column_id);
    return;
  }

  key_state.set_key_state(key, RELEASED);
  #ifdef SEND_KEYS
    Keyboard.release(byte_for_release);
  #endif
  DBG("released '%c' [%d]", byte_for_release, byte_for_release);
}

void handle_layer_exit() {
  // Check if the key that holds the current layer is held down
  while (!key_state.release_byte_for(layer_stack.current_leave_key())) {
    DBG("Leaving layer [%d] (releasing non-through keys)", layer_stack.current_layer_id());

    const auto current_layer = layers_keymap[layer_stack.current_layer_id()];

    // Check if any keys are currently pressed and if they are not a 'through key', release them
    for (byte row_id = 0; row_id < KEYBOARD_ROWS_COUNT; row_id++) {
      for (byte column_id = 0; column_id < KEYBOARD_COLS_COUNT; column_id++) {
        KeyPosition key_pos = KeyPosition { .column_id = column_id, .row_id = row_id };

        // Check if the key can or should be released
        byte release_byte = key_state.release_byte_for(key_pos);
        if (release_byte == RELEASED || release_byte == PRESSED_BUT_NO_INFO) continue;

        // Skip releasing of through keys, we want to keep them pressed
        // For example SHIFT key should not be released when leaving the layer if it is being held down via a through key
        KeyCode key_code = current_layer[row_id][column_id];
        if (key_code.descriptor() == Descriptor::THROUGH_KEY) continue;

        // Release the key
        key_state.set_key_state(key_pos, RELEASED);
        #ifdef SEND_KEYS
          Keyboard.release(release_byte);
        #endif
        DBG("auto-released key '%c' [%d], because leaving layer %d", release_byte, release_byte, layer_stack.current_layer_id());
      }
    }

    layer_stack.pop_layer();
  }
}

void handle_key_down(KeyPosition key) {
  KeyCode key_code = get_keycode_by_passing_through_keys(key);

  switch(key_code.descriptor()) {
    case Descriptor::SIMPLE_KEY: {
      byte byte_to_press = key_code.data_bits();
      key_state.set_key_state(key, byte_to_press);
      #ifdef SEND_KEYS
        Keyboard.press(byte_to_press);
      #endif
      DBG("PRESS '%c' [%d]", byte_to_press, byte_to_press);
      return;
    }
    case Descriptor::HARDWARE_INVALID_KEY:
      LOG_ERROR("");
      LOG_ERROR("o.O ! A key that should not be possible to use on the keyboard was pressed ! O.o");
      LOG_ERROR("  hint: This is probably because there is a `__` in a bad spot in layout definition ;)");
      LOG_ERROR("");
      return;
    case Descriptor::NO_ACTION:
      DBG("No-action key was pressed");
      return;
    case Descriptor::THROUGH_KEY:
      LOG_ERROR("Probably a through key is in the bottom row at [col=%d][row=%d]", key.column_id, key.row_id);
      return;
    case Descriptor::MACRO_KEY: {
      byte macro_idx = key_code.data_bits();
      DBG("(TODO) Pressed macro key for macro with idx %d", macro_idx);

      //TODO
      return;
    }
    case Descriptor::MOMENTARY_LAYER: {
      byte layer_id = key_code.data_bits();
      if (layer_id) {
        key_state.set_key_state(key, PRESSED_BUT_NO_INFO);
        layer_stack.push_layer(LayerInfo {.layer_id = layer_id, .layer_leave_key = key});
        DBG("going to layer number: %d", layer_id);
        return;
      }

     // TODO
      return;
    }
    case Descriptor::SPECIAL_SHIFT: {
      byte shift_array_idx = key_code.data_bits();
      DBG("(TODO) Pressed shift-alternative key for with idx %d", shift_array_idx);

      // TODO
      // KeyCode non_shift_key_code = shift_array_or_something[shift_array_idx][0];
      // KeyCode shift_key_code = shift_array_or_something[shift_array_idx][1];
      return;
    }
    case Descriptor::CZECH_KEY: {
      char pressed_char = key_code.get_czech_unmodified_char();

      if (key_code.get_czech_diacritic() == Diacritic::CARON) {
        DBG("(TODO) Pressed czech key '%c' with CARON", pressed_char); 
      } else {
        DBG("(TODO) Pressed czech key '%c' with ACUTE_ACCENT", pressed_char); 
      }

      // TODO
      // // Not this since we have to modify that buffer
      // char[] acute_macro_buffer = "AltGr_up_down X_up_down";
      // char[] caron_macro_buffer = "Shift_up AltGr_up_down X_up_down";
      return;
    }
  }
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
  DBG("I'm allive :D");

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

  delay(5);
}
