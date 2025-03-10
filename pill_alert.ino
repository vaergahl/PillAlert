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
#define CLOCK_ADDR 0

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

/// modes: [0] = clock , [1-4] = pills
struct Editor {
    int cursor;
    Time time;
    int mode;
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

void init_clock() {
    EEPROM.get(CLOCK_ADDR, clock);
    if (clock.secs == NULL || clock.secs < 0) clock.secs = 0;
    if (clock.mins == NULL|| clock.mins < 0) clock.mins = 0;
    if (clock.hours == NULL|| clock.hours < 0) clock.hours = 0;
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
    pill.time.secs = 0;
    pill.time.mins = 0;
    pill.time.hours = 0;
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
    EEPROM.put(CLOCK_ADDR, clock);
}

void draw_editor(const char *n) {
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

void exit_editor() {
    edit_mode = false;
    lcd.clear();
    draw_ui();
}

void edit_time(const char c) {
    lcd.setCursor(editor.cursor, 2);
    lcd.print(c);

    lcd.setCursor(editor.cursor, 3);
    lcd.print(" ");

    if (editor.cursor == EDITOR_START+1) editor.cursor += 2;
    else editor.cursor++;

    if (editor.cursor > EDITOR_START+4) {
        exit_editor();
        return;
    }
    lcd.setCursor(editor.cursor, 3);
    lcd.print("^");
}

void handle_input(char &c) {
    switch(c) {
        case 'A': {} break;
        case 'B': {} break;
        case 'C': {} break;
        case 'D': {} break;
        case '*': {
            if (edit_mode) exit_editor();
        } break;
        case '#': {
            if (edit_mode) return;
            edit_mode = true;
            editor.cursor =  EDITOR_START;
            editor.mode = 0;
            editor.time = clock;
            lcd.clear();
            draw_editor("Clock");
        } break;
        default: {
            if (edit_mode) edit_time(c);
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
