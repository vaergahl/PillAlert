#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <assert.h>

#define PAD_RIGHT 1
#define COL_HOUR 5
#define COL_MIN 8
#define COL_SEC 11
#define PILL_MIN 6
#define PILL_HOUR 3

LiquidCrystal_I2C lcd(0x27, 20, 4);

typedef struct {
    int seconds;
    int minutes;
    int hours;
} Time;

typedef struct {
    Time time;
    int x, y;
} Pill;

Time clock;
Pill pills[4] = {0};

void time_render(int value, int col, int row) {
    char buf[2] = {0};
    lcd.setCursor(get_col(col), row);
    sprintf(buf, "%02d", value);
    lcd.print(buf);
}

void time_set(int &value, int new_value, int col, int row) {
    value = new_value;
    time_render(value, col, row);
}

void time_inc(int &value, int col, int row) {
    value++;
    time_render(value, col, row);
}

void init_clock() {
    lcd.setCursor(get_col(0), 0);
    lcd.print("TIME --:--:--");
    time_set(clock.seconds, 0, COL_SEC, 0);
    time_set(clock.minutes, 0, COL_MIN, 0);
    time_set(clock.hours, 0, COL_HOUR, 0);
}

void init_pill(int index, int col, int row) {
    Pill &pill = pills[index];
    pill.time.seconds = 0;
    pill.time.minutes = 0;
    pill.time.hours = 0;
    pill.x = row;
    pill.y = col;
    lcd.setCursor(get_col(col), row);
    lcd.print("P");
    lcd.print(index+1);
    lcd.print(" 00:00");
}

int get_col(int column) {
    assert(PAD_RIGHT + column < 20);
    return  PAD_RIGHT + column;
}

void clock_update() {
    time_inc(clock.seconds, COL_SEC, 0);
    if (clock.seconds >= 60) {
        time_set(clock.seconds, 0, COL_SEC, 0);
        time_inc(clock.minutes, COL_MIN, 0);
        if (clock.minutes >= 60) {
            time_set(clock.minutes, 0, COL_MIN, 0);
            time_inc(clock.hours, COL_HOUR, 0);
            if (clock.hours >= 24) time_set(clock.hours, 0, COL_HOUR, 0);
        }
    }
    // Pill &pill = pills[2];
    // time_inc(pill.time.minutes, pill.y + PILL_MIN, pill.x);
    delay(1000);
}

void setup() {
    Serial.begin(9600);
    lcd.begin(20, 4);
    lcd.backlight();
    Serial.println("Hello from the Serial");
    init_clock();
    init_pill(0, 0, 2);
    init_pill(1, 0, 3);
    init_pill(2, 10, 2);
    init_pill(3, 10, 3);
}

void loop() {
    clock_update();
}
