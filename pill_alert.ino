#include <Keypad.h>
#include <EEPROM.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <assert.h>
#include <Servo.h>

#define PAD_RIGHT 1
#define COL_HOUR 5
#define COL_MIN 8
#define COL_SEC 11
#define PILL_MIN 6
#define PILL_HOUR 3
#define EDITOR_START 7
#define SERVO_PAD 10

// Alarm
#define ALARM_PIN A0
#define NOTE_C6 1047
#define NOTE_E6 1319
#define NOTE_G6 1568
#define NOTE_C7 2093

// Addresses
#define START_ADDR 0

// Keypad
#define ROWS 4
#define COLS 4

typedef struct {
    int secs;
    int mins;
    int hours;
} Time;

typedef struct {
    Servo servo;
    bool trigger;
} PillServo;

typedef struct {
    Time time;
    int row, col;
    bool alarm;
    PillServo ps;
} Pill;

typedef struct {
    unsigned long prev_m;
    long interval;
} Task;

struct Editor {
    bool on;
    int cursor;
    Time time;
    char values[4];
    int mode; // [0] = clock , [1-4] = pills
} editor;

struct Alarm {
    bool playing;
    bool on;
} alarm;

struct Note {
    bool playing;
    int freq;
    int half_period;
    long end_time;
    int current;
} note;

Time clock;
Pill pills[4] = {0};

char keys[ROWS][COLS] = {
    {'1', '2', '3', 'A'},
    {'4', '5', '6', 'B'},
    {'7', '8', '9', 'C'},
    {'*', '0', '#', 'D'}
};

byte rowPins[ROWS] = {9, 8, 7, 6};  // Connect to row pins of keypad
byte colPins[COLS] = {5, 4, 3, 2};  // Connect to column pins of keypad

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);
LiquidCrystal_I2C lcd(0x27, 20, 4);

void stop_alarm() {
    alarm.on = false;
    alarm.playing = false;
    note.current = 0;
    analogWrite(ALARM_PIN, 0);
    for (int i = 0; i < 4; i++) {
        alarm_unmark(pills[i]);
    }
}

bool should_exec(Task &t, const unsigned long &cur_ms) {
    return (cur_ms - t.prev_m) >= t.interval;
}

void time_render(int value, int col, int row) {
    if (editor.on) return;
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

bool not_init(int &val) {
    return val == NULL || val < 0;
}

void init_alarm() {
    pinMode(ALARM_PIN, OUTPUT);
    alarm.on = false;
    alarm.playing = false;

    // Note
    note.playing = false;
    note.current = 0;
    note.freq = 0;
    note.half_period = 0;
    note.end_time = 0;
}

void init_clock() {
    EEPROM.get(START_ADDR, clock);
    if (not_init(clock.secs)) clock.secs = 0;
    if (not_init(clock.mins)) clock.mins = 0;
    if (not_init(clock.hours)) clock.hours = 0;
    editor.on = false;
}

void init_editor(int m) {
    editor.on = true;
    assert(m < 4 || m >= 0);
    editor.cursor =  EDITOR_START;
    editor.mode = m;
    if (editor.mode == 0) editor.time = clock;
    else editor.time = pills[m-1].time;
}

void draw_ui() {
    lcd.setCursor(get_col(0), 0);
    char buf[14];
    sprintf(buf, "TIME %02d:%02d:%02d",
            clock.hours,
            clock.mins,
            clock.hours);
    lcd.print(buf);

    for (int i = 0; i < 4; i++) {
        Pill &pill = pills[i];
        int &row = pill.row;
        int &col = pill.col;
        char pbuf[9];
        sprintf(pbuf, "P%d %02d:%02d",
                i + 1,
                pill.time.hours,
                pill.time.mins);
        lcd.setCursor(get_col(col), row);
        lcd.print(pbuf);
        if (pill.alarm) {
            lcd.setCursor(pill.col, pill.row);
            lcd.print(">");
        }
    }
}

void init_servo(PillServo &ps, int index) {
    ps.servo.attach(index + SERVO_PAD);
    ps.servo.write(0);
    ps.trigger = false;
}

void init_pill(int index, int col, int row) {
    Pill &pill = pills[index];
    Time &time = pill.time;
    const int pill_addr = START_ADDR + (sizeof(Time) * (index + 1));
    EEPROM.get(pill_addr, time);
    if (not_init(time.secs)) time.secs = 0;
    if (not_init(time.mins)) time.mins = 0;
    if (not_init(time.hours)) time.hours = 0;
    pill.row = row;
    pill.col = col;
    pill.alarm = false;
    init_servo(pill.ps, index);
}

int get_col(int column) {
    assert(PAD_RIGHT + column < 20);
    return  PAD_RIGHT + column;
}

void alarm_mark(Pill &p) {
    if (p.alarm) return;
    p.alarm = true;
    if (editor.on) return;
    lcd.setCursor(p.col, p.row);
    lcd.print(">");
}

void alarm_unmark(Pill &p) {
    if (!p.alarm) return;
    p.alarm = false;
    if (editor.on) return;
    lcd.setCursor(p.col, p.row);
    lcd.print(" ");
}

void check_pills() {
    bool should_alarm = false;
    for (int i = 0; i < 4; i++) {
        Pill &pill = pills[i];
        Time &pt = pill.time;
        char buf[16] = {0};
        if (pt.mins == clock.mins && pt.hours == clock.hours) {
            should_alarm = true;
            pill.ps.trigger = true;
            alarm_mark(pill);
        } else alarm_unmark(pill);
        Serial.println(buf);
    }
    if (should_alarm && !alarm.on ) alarm.on = true;
    else stop_alarm();
}

void clock_update() {
    time_inc(clock.secs, COL_SEC, 0);
    if (clock.secs >= 60) {
        time_set(clock.secs, 0, COL_SEC, 0);
        time_inc(clock.mins, COL_MIN, 0);
        // we check pill time each time minutes is incremented.
        check_pills();
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
    editor.on = false;
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
            if (editor.on || alarm.on) return;
            init_editor(1);
            draw_editor("Pill 1");
        } break;
        case 'B': {
            if (editor.on || alarm.on) return;
            init_editor(2);
            draw_editor("Pill 2");
        } break;
        case 'C': {
            if (editor.on || alarm.on) return;
            init_editor(3);
            draw_editor("Pill 3");
        } break;
        case 'D': {
            if (editor.on || alarm.on) return;
            init_editor(4);
            draw_editor("Pill 4");
        } break;
        case '#': {
            if (editor.on || alarm.on) return;
            init_editor(0);
            draw_editor("Clock");
        } break;
        case '*': {
            if (editor.on) exit_editor(false);
            else stop_alarm();
        } break;
        default: {
            if (editor.on) set_editor_val(c);
        } break;
    }
}

void setup() {
    Serial.begin(9600);
    Serial.println("Starting Pill Alert");
    lcd.begin(20, 4);
    lcd.backlight();
    init_alarm();
    init_clock();
    init_pill(0, 0, 2);
    init_pill(1, 0, 3);
    init_pill(2, 10, 2);
    init_pill(3, 10, 3);
    draw_ui();
}

Task t_clock = { 0, 1000 };
Task t_input = { 0, 50   };
Task t_alarm = { 0, 1000 };
Task t_note  = { 0, 0 };
Task t_pwm   = { 0, 0 };
Task t_servos[4] = {
    {0, 1000},
    {0, 1000},
    {0, 1000},
    {0, 1000},
};

int melody[] = { 880, 784, 659, 784, 880, 988 };
int note_durations[] = { 300, 300, 300, 500, 300, 700 };

void start_note(int freq, int duration, unsigned long ms) {
    if (freq == 0) {
        analogWrite(ALARM_PIN, 0);
        t_note.prev_m = ms + duration;
        note.playing = false;
        return;
    }

    Serial.print("Starting Note with freq: ");
    Serial.println(freq);

    note.freq = freq;
    note.half_period = (1000000 / note.freq) / 2;
    note.end_time = micros() + (duration * 1000L);
    note.playing = true;
    t_pwm.prev_m = micros();
    t_pwm.interval = note.half_period;
}

void play_note(unsigned long cur_us) {
    if (cur_us >= note.end_time) {
        analogWrite(ALARM_PIN, 0);
        note.playing = false;
        return;
    }

    if (should_exec(t_pwm, cur_us)) {
        static bool state = false;
        analogWrite(ALARM_PIN, state ? 128 : 0);
        state = !state;
        t_pwm.prev_m = cur_us;
    }
}


void play_alarm(const unsigned long cur_ms, const unsigned long cur_us) {
    if (alarm.playing) {
        if (!note.playing && note.current < 6) {
            start_note(melody[note.current], note_durations[note.current], cur_ms);
            t_note.interval = note_durations[note.current] * 1.3;
            note.current++;
        } else if (note.playing) {
            play_note(cur_us);
        } else {
            alarm.playing = false;
            t_alarm.prev_m = cur_ms;
        }
    } else {
        if (should_exec(t_alarm, cur_ms)) {
            alarm.playing = true;
            note.current = 0;
            t_note.prev_m = cur_ms;
            t_note.interval = 0;
        }
    }
}

void loop() {
    const unsigned long cur_ms = millis();
    const unsigned long cur_us = micros();

    if (should_exec(t_clock, cur_ms)) {
        t_clock.prev_m = cur_ms;
        clock_update();
    }

    if (should_exec(t_input, cur_ms)) {
        t_input.prev_m = cur_ms;
        char key = keypad.getKey();
        if (key) {
            handle_input(key);
        }
    }

    if (alarm.on) play_alarm(cur_ms, cur_us);
}
