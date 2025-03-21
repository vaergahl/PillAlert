#include <EEPROM.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <assert.h>

#define PAD_RIGHT 1
#define COL_HOUR 5
#define COL_MIN 8
#define COL_SEC 11
#define PILL_MIN 6
#define PILL_HOUR 3
#define EDITOR_START 7

// Addresses
#define START_ADDR 0

LiquidCrystal_I2C lcd(0x27, 20, 4);
bool edit_mode = false;

typedef struct {
    int secs;
    int mins;
    int hours;
} Time;

typedef struct {
    Time time;
    int x, y;
} Pill;

typedef struct {
    unsigned long prev_m;
    const long interval;
} Task;

struct Editor {
    int cursor;
    Time time;
    char values[4];
    int mode; // [0] = clock , [1-4] = pills
} editor;

bool should_exec(Task &t, unsigned long &cm) {
    return (cm - t.prev_m) >= t.interval;
}

void time_render(int value, int col, int row) {
    if (edit_mode) return;
    char buf[3] = {0};
    lcd.setCursor(get_col(col), row);
    sprintf(buf, "%02d", value);
    lcd.print(buf);
}

int get_memloc(int start, int stride) {
    return start + stride;
}

void time_set(int &value, int new_value, int col, int row) {
    value = new_value;
    time_render(value, col, row);
}

void time_inc(int &value, int col, int row) {
    value++;
    time_render(value, col, row);
}

Time clock;
Pill pills[4] = {0};

bool not_init(int &val) {
    return val == NULL || val < 0;
}

void init_clock() {
    EEPROM.get(START_ADDR, clock);
    if (not_init(clock.secs)) clock.secs = 0;
    if (not_init(clock.mins)) clock.mins = 0;
    if (not_init(clock.hours)) clock.hours = 0;
}

void init_editor(int m) {
    edit_mode = true;
    assert(m < 4 || m >= 0);
    editor.cursor =  EDITOR_START;
    editor.mode = m;
    if (editor.mode == 0) editor.time = clock;
    else editor.time = pills[m-1].time;
}

void draw_ui() {
    lcd.begin(20, 4);
    lcd.backlight();
    lcd.setCursor(get_col(0), 0);
    char buf[14];
    sprintf(buf, "TIME %02d:%02d:%02d",
            clock.hours,
            clock.mins,
            clock.hours);
    lcd.print(buf);

    for (int i = 0; i < 4; i++) {
        Pill &pill = pills[i];
        int &row = pill.x;
        int &col = pill.y;
        char pbuf[9];
        sprintf(pbuf, "P%d %02d:%02d",
                i + 1,
                pill.time.hours,
                pill.time.mins);
        lcd.setCursor(get_col(col), row);
        lcd.print(pbuf);
    }
}

void init_pill(int index, int col, int row) {

    Pill &pill = pills[index];
    Time &time = pill.time;
    const int pill_addr = START_ADDR + (sizeof(Time) * (index + 1));
    EEPROM.get(pill_addr, time);
    if (not_init(time.secs)) time.secs = 0;
    if (not_init(time.mins)) time.mins = 0;
    if (not_init(time.hours)) time.hours = 0;
    pill.x = row;
    pill.y = col;
}

int get_col(int column) {
    assert(PAD_RIGHT + column < 20);
    return  PAD_RIGHT + column;
}

void clock_update() {
    time_inc(clock.secs, COL_SEC, 0);
    if (clock.secs >= 60) {
        time_set(clock.secs, 0, COL_SEC, 0);
        time_inc(clock.mins, COL_MIN, 0);
        if (clock.mins >= 60) {
            time_set(clock.mins, 0, COL_MIN, 0);
            time_inc(clock.hours, COL_HOUR, 0);
            if (clock.hours >= 24) time_set(clock.hours, 0, COL_HOUR, 0);
        }
    }
    EEPROM.put(START_ADDR, clock);
}

void draw_editor(const char *n) {
    lcd.clear();
    lcd.setCursor(get_col(0), 0);
    lcd.print("EDITING: ");
    lcd.print(n);
    char buf[6] = {0};
    sprintf(buf, "%02d:%02d", editor.time.hours, editor.time.mins);
    lcd.setCursor(EDITOR_START, 2);
    lcd.print(buf);
    lcd.setCursor(editor.cursor, 3);
    lcd.print("^");
}

void exit_editor(bool save) {
    if (save) {
        char buf[3] = { editor.values[0], editor.values[1], '\0' };
        editor.time.hours = atoi(buf);
        assert(editor.time.hours <= 24);
        buf[0] = editor.values[2];
        buf[1] = editor.values[3];
        editor.time.mins = atoi(buf);
        assert(editor.time.mins < 60);
        editor.time.secs = 0;
        if (editor.mode > 0) {
            Pill &pill = pills[editor.mode - 1];
            pill.time = editor.time;
            const int pill_addr = START_ADDR + (sizeof(Time) * editor.mode);
            EEPROM.put(pill_addr, pill.time);
        } else clock = editor.time;
    }
    edit_mode = false;
    lcd.clear();
    draw_ui();
}

int cur_as_index() {
    int i = editor.cursor - EDITOR_START;
    assert(i < 5);
    if (i > 2) return i - 1;
    return i;
}

bool set_editor_value(const char &c) {
    int i = cur_as_index();
    switch (i) {
        case 0: {
            if (c - '0' > 2) return false;
        } break;
        case 1: {
            // check prev value if has value of 2
            if (editor.values[0] == '2') {
                if (c - '0' > 3) return false;
            }
        } break;
        case 2: {
            if (c - '0' > 5) return false;
        } break;
        default: break;
    }
    editor.values[i] = c;
    lcd.setCursor(editor.cursor, 2);
    lcd.print(editor.values[i]);
    return true;

}

void set_editor_val(const char &c) {
    if (!set_editor_value(c)) return;
    lcd.setCursor(editor.cursor, 3);
    lcd.print(" ");
    if (editor.cursor == EDITOR_START+1) editor.cursor += 2;
    else editor.cursor++;

    if (editor.cursor > EDITOR_START+4) {
        exit_editor(true);
        return;
    }
    lcd.setCursor(editor.cursor, 3);
    lcd.print("^");
}

void handle_input(char &c) {
    switch(c) {
        case 'A': {
            if (edit_mode) return;
            init_editor(1);
            draw_editor("Pill 1");
        } break;
        case 'B': {
            if (edit_mode) return;
            init_editor(2);
            draw_editor("Pill 2");
        } break;
        case 'C': {
            if (edit_mode) return;
            init_editor(3);
            draw_editor("Pill 3");
        } break;
        case 'D': {
            if (edit_mode) return;
            init_editor(4);
            draw_editor("Pill 4");
        } break;
        case '#': {
            if (edit_mode) return;
            init_editor(0);
            draw_editor("Clock");
        } break;
        case '*': {
            if (edit_mode) exit_editor(false);
        } break;
        default: {
            if (edit_mode) set_editor_val(c);
        } break;
    }
}

void setup() {
    Serial.begin(9600);
    Serial.println("Starting Pill Alert");
    init_clock();
    init_pill(0, 0, 2);
    init_pill(1, 0, 3);
    init_pill(2, 10, 2);
    init_pill(3, 10, 3);
    draw_ui();
}

Task t_clock = { 0, 1000 };
Task t_input = { 0, 50  };
char prev_key = 'x';

void loop() {
    unsigned long cur_m = millis();

    if (should_exec(t_clock, cur_m)) {
        t_clock.prev_m = cur_m;
        clock_update();
    }

    if (should_exec(t_input, cur_m)) {
        t_input.prev_m = cur_m;
        char key =  get_input();
        if (key != prev_key) {
            prev_key = key;
            if ( key != 'x') {
                handle_input(key);
            }
        }
    }
}

char get_input() {
    // taken from: https://layadcircuits.com/ds/Smart-Keypad-Adapter/LayadCircuits_Product_LC075_SmartKP_UG_v1_0_0..pdf
    int val = analogRead(A0);
    if (val>(32-10)&&val<(32+10)){
        return '1';
    } else if (val>(63-10)&&val<(63+10)){
        return '2';
    } else if (val>(93-10)&&val<(93+10)){
        return '3';
    } else if (val>(123-10)&&val<(123+10)){
        return '4';
    } else if (val>(154-10)&&val<(154+10)){
        return '5';
    } else if (val>(184-10)&&val<(184+10)){
        return '6';
    } else if (val>(216-10)&&val<(216+10)){
        return '7';
    } else if (val>(247-10)&&val<(247+10)){
        return '8';
    } else if (val>(277-10)&&val<(277+10)){
        return '9';
    } else if (val>(308-10)&&val<(308+10)){
        return '*';
    } else if (val>(339-10)&&val<(339+10)){
        return '0';
    } else if (val>(370-10)&&val<(370+10)){
        return '#';
    } else if (val>(401-10)&&val<(401+10)){
        return 'A';
    } else if (val>(432-10)&&val<(432+10)){
        return 'B';
    } else if (val>(463-10)&&val<(463+10)){
        return 'C';
    } else if (val>(492-10)&&val<(492+10)){
        return 'D';
    }
    return 'x';
}
