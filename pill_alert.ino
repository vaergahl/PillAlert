//#include <EEPROM.h> // for storage persistence
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <assert.h>

LiquidCrystal_I2C lcd(0x27, 20, 4);

void setup() {
    Serial.begin(9600);
    lcd.begin(20, 4);
    lcd.backlight();
    Serial.println("Hello from the Serial");
    lcd.setCursor(0, 0);
    lcd.print("Hello, World");
}

void loop() {
    int val = analogRead(A0);
    if (val>(32-10)&&val<(32+10)){
        Serial.println("1");
    } else if (val>(63-10)&&val<(63+10)){
        Serial.println("2");
    } else if (val>(93-10)&&val<(93+10)){
        Serial.println("3");
    } else if (val>(123-10)&&val<(123+10)){
        Serial.println("4");
    } else if (val>(154-10)&&val<(154+10)){
        Serial.println("5");
    } else if (val>(184-10)&&val<(184+10)){
        Serial.println("6");
    } else if (val>(216-10)&&val<(216+10)){
        Serial.println("7");
    } else if (val>(247-10)&&val<(247+10)){
        Serial.println("8");
    } else if (val>(277-10)&&val<(277+10)){
        Serial.println("9");
    } else if (val>(308-10)&&val<(308+10)){
        Serial.println("*");
    } else if (val>(339-10)&&val<(339+10)){
        Serial.println("0");
    } else if (val>(370-10)&&val<(370+10)){
        Serial.println("#");
    } else if (val>(401-10)&&val<(401+10)){
        Serial.println("A");
    } else if (val>(432-10)&&val<(432+10)){
        Serial.println("B");
    } else if (val>(463-10)&&val<(463+10)){
        Serial.println("C");
    } else if (val>(492-10)&&val<(492+10)){
        Serial.println("D");
    }
    // delay(1000);
}
