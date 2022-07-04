#include <Wire.h>
#define DS3231_ADDRESS 0x68

#define ENCODER_TYPE 0      // тип энкодера (0 или 1). Если энкодер работает некорректно (пропуск шагов), смените тип
#define ENC_REVERSE 0       // 1 - инвертировать энкодер, 0 - нет
#define DRIVER_VERSION 1    // 0 - маркировка драйвера дисплея кончается на 4АТ, 1 - на 4Т

///--------------------------------------------------------------------------------------------ВЕРСИЯ ПРОШИВКИ
#define VER "DOSER ver_02_09_2021"

///----названия на главном экране
#define P1 "Г1"
#define P2 "Т"
#define P3 "Г2"
#define P4 "Г3"
///----названия на главном экране

///----названия вменю
#define P1_ "ГАЗОН"
#define P2_ "ТУИ"
#define P3_ "ГАЗОН2"
#define P4_ "ГАЗОН3"
///----названия вменю

#define PERIOD 1200  // максимальный период срабатывания помпы

///----Энкодер
#define CLK 3
#define DT 2
#define SW 0
///----Энкодер

//////----------------Распиновка помп
#define P1_Pin 7
#define P2_Pin 8
#define P3_Pin 9
#define P4_Pin 10

//////----------------Распиновка помп
#define DELAY_MENU 200
#include "GyverEncoder.h"
Encoder enc1(CLK, DT, SW);

#include <EEPROM.h>
#include "LCD_1602_RUS.h"

// -------- АВТОВЫБОР ОПРЕДЕЛЕНИЯ ДИСПЛЕЯ-------------
// Если кончается на 4Т - это 0х27. Если на 4АТ - 0х3f
#if (DRIVER_VERSION)
LCD_1602_RUS lcd(0x27, 20, 4);
#else
LCD_1602_RUS lcd(0x3f, 20, 4);
#endif

// -------- АВТОВЫБОР ОПРЕДЕЛЕНИЯ ДИСПЛЕЯ-------------
byte lcd_off[2]={8,21};
int8_t menu_val = 1;
int8_t menu_val_2 = 0;
int8_t pump_1[10] = {7, 45, 30, 0, 0, 0, 1, 1, 1, 1};
int8_t pump_2[10] = {21, 22, 23};
int8_t pump_3[10] = {13, 32, 33};
int8_t pump_4[10] = {14, 42, 23};
int8_t h, m, s, day, date, month, year;
int8_t pump_test[4] = {0, 0, 0, 0};
int lcd_on = 0;
byte flag = 0;
double counter, lcd_count = 0;

void setup() {
    enc1.setType(TYPE2);
    // -------- ЭНКОДЕР-------------
#if (ENC_REVERSE)
    enc1.setDirection(NORM);
#else
    enc1.setDirection(REVERSE);
#endif
    // -------- ЭНКОДЕР-------------

    attachInterrupt(0, isrCLK, CHANGE);    // прерывание на 2 пине! CLK у энка
    attachInterrupt(1, isrDT, CHANGE);    // прерывание на 3 пине! DT у энка

    pinMode(P1_Pin, OUTPUT);
    pinMode(P2_Pin, OUTPUT);
    pinMode(P3_Pin, OUTPUT);
    pinMode(P4_Pin, OUTPUT);

    digitalWrite(P1_Pin, LOW);
    digitalWrite(P2_Pin, LOW);
    digitalWrite(P3_Pin, LOW);
    digitalWrite(P4_Pin, LOW);

    Wire.begin();
    lcd.init();
    lcd.backlight();
    lcd.clear();

    // --------------------- СБРОС НАСТРОЕК ---------------------
    if (!digitalRead(SW)) {          // если нажат энкодер, сбросить настройки до 1
        ////помпа1
        EEPROM.write(10, 7);
        EEPROM.write(12, 10);
        EEPROM.write(14, 5);
        for (int i = 3; i < 10; i++) {
            EEPROM.write(10 + i * 2, 0);
        }

        ////помпа2
        EEPROM.write(30, 8);
        EEPROM.write(32, 30);
        EEPROM.write(34, 10);
        for (int i = 3; i < 10; i++) {
            EEPROM.write(30 + i * 2, 0);
        }

        ////помпа3
        EEPROM.write(50, 9);
        EEPROM.write(52, 45);
        EEPROM.write(54, 15);
        for (int i = 3; i < 10; i++) {
            EEPROM.write(50 + i * 2, 0);
        }

        ////помпа4
        EEPROM.write(70, 10);
        EEPROM.write(72, 55);
        EEPROM.write(74, 20);
        for (int i = 3; i < 10; i++) {
            EEPROM.write(70 + i * 2, 0);
        }

        EEPROM.write(200, 1);

        lcd.setCursor(0, 0);
        lcd.print("Reset settings");
    }

    while (!digitalRead(SW));        // ждём отпускания кнопки
    lcd.clear();
    // очищаем дисплей, продолжаем работу

    ////помпа1
    for (int i = 0; i < 10; i++) {
        pump_1[i] = EEPROM.read(10 + i * 2);
    }

    ////помпа2
    for (int i = 0; i < 10; i++) {
        pump_2[i] = EEPROM.read(30 + i * 2);
    }

    ////помпа3
    for (int i = 0; i < 10; i++) {
        pump_3[i] = EEPROM.read(50 + i * 2);
    }

    ////помпа4
    for (int i = 0; i < 10; i++) {
        pump_4[i] = EEPROM.read(70 + i * 2);
    }

    lcd_on = EEPROM.read(200);
}

// функции с часами
byte decToBcd(byte val) {
    // Convert normal decimal numbers to binary coded decimal
    return ( (val / 10 * 16) + (val % 10) );
}

byte bcdToDec(byte val) {
    // Convert binary coded decimal to normal decimal numbers
    return ( (val / 16 * 10) + (val % 16) );
}


void setMinute(byte min1) {
    Wire.beginTransmission(DS3231_ADDRESS);
    Wire.write(0x01); //stop Oscillator
    Wire.write(decToBcd(min1));
    Wire.endTransmission();
}

void setHour(byte hour1) {
    Wire.beginTransmission(DS3231_ADDRESS);
    Wire.write(0x02); //stop Oscillator
    Wire.write(decToBcd(hour1));
    Wire.endTransmission();
}

void setDay(byte day1) {
    Wire.beginTransmission(DS3231_ADDRESS);
    Wire.write(0x03); //stop Oscillator
    Wire.write(decToBcd(day1));
    Wire.endTransmission();
}

void setDate(byte date1) {
    Wire.beginTransmission(DS3231_ADDRESS);
    Wire.write(0x04); //stop Oscillator
    Wire.write(decToBcd(date1));
    Wire.endTransmission();
}

void setMonth(byte month1) {
    Wire.beginTransmission(DS3231_ADDRESS);
    Wire.write(0x05); //stop Oscillator
    Wire.write(decToBcd(month1));
    Wire.endTransmission();
}

void setYear(byte year1) {
    Wire.beginTransmission(DS3231_ADDRESS);
    Wire.write(0x06); //stop Oscillator
    Wire.write(decToBcd(year1));
    Wire.endTransmission();
}

void READ_TIME_RTC() {
    Wire.beginTransmission(DS3231_ADDRESS);        //104 is DS3231 device address
    Wire.write(0x00);                                  //Start at register 0
    Wire.endTransmission();
    Wire.requestFrom(DS3231_ADDRESS, 7);           //Request seven bytes
    if (Wire.available()) {
        s = Wire.read();                           //Get second
        m = Wire.read();                           //Get minute
        h   = Wire.read();                           //Get hour
        day     = Wire.read();
        date    = Wire.read();
        month   = Wire.read();                           //Get month
        year    = Wire.read();

        s = (((s & B11110000) >> 4) * 10 + (s & B00001111)); //Convert BCD to decimal
        m = (((m & B11110000) >> 4) * 10 + (m & B00001111));
        h   = (((h & B00110000) >> 4) * 10 + (h & B00001111));   //Convert BCD to decimal (assume 24 hour mode)
        day     = (day & B00000111); // 1-7
        date    = (((date & B00110000) >> 4) * 10 + (date & B00001111));     //Convert BCD to decimal  1-31
        month   = (((month & B00010000) >> 4) * 10 + (month & B00001111));   //msb7 is century overflow
        year    = (((year & B11110000) >> 4) * 10 + (year & B00001111));
    }
}

void isrCLK() {
    enc1.tick();  // отработка в прерывании
}

void isrDT() {
    enc1.tick();  // отработка в прерывании
}

void LCD_Backlight() {
    if (lcd_on == 0) {
        if ((h>lcd_off[0])&&(h<lcd_off[1])) {
            lcd.backlight();
        }
        else {
            if (flag == 1) {
                lcd_count++;
                lcd.backlight();
            }

            if (lcd_count == DELAY_MENU ) {
                lcd_count = 0;
                lcd.noBacklight();
                flag = 0;
            }
        }
    }
    if (lcd_on == 1) {
        if (flag == 1) {
            lcd_count++;
            lcd.backlight();
        }

        if (lcd_count == DELAY_MENU ) {
            lcd_count = 0;
            lcd.noBacklight();
            flag = 0;
        }
    }
    if (lcd_on == 2) {
        lcd.backlight();
    }
}

void EEPROM_WR() {
    ////помпа1
    for (int i = 0; i < 10; i++) {
        EEPROM.write(10 + i * 2, pump_1[i]);
    }

    ////помпа2
    for (int i = 0; i < 10; i++) {
        EEPROM.write(30 + i * 2, pump_2[i]);
    }

    ////помпа3
    for (int i = 0; i < 10; i++) {
        EEPROM.write(50 + i * 2, pump_3[i]);
    }

    ////помпа4
    for (int i = 0; i < 10; i++) {
        EEPROM.write(70 + i * 2, pump_4[i]);
    }

    EEPROM.write(200, lcd_on);
}

void loop() {
    READ_TIME_RTC();
    enc1.tick();
    MAIN();
    PUMPS();
    LCD_Backlight();
}

void MAIN() {
    if (enc1.isPress() || enc1.isTurn()) {
        counter = 0;
        flag = 1;
        lcd_count = 0;
    }

    if (menu_val != 1) {
        counter++;
    }

    if (counter == DELAY_MENU ) {
        lcd.clear();
        counter = 0;
        menu_val = 1;
        menu_val_2 = 0;
        EEPROM_WR();
    }

    if (enc1.isClick()) {
        menu_val_2 = 0;
        EEPROM_WR();
        lcd.clear();

        if (menu_val < 9) {
            menu_val++;
        }
        else {
            menu_val = 1;
        }
    }

    //меню
    switch (menu_val) {
        //основной экран
        case 1:
            //часы
            lcd.setCursor(0, 0);

            if (h < 10) {
                lcd.print("0");
            }
            lcd.print(h);
            lcd.print(":");

            if (m < 10) {
                lcd.print("0");
            }
            lcd.print(m);

            lcd.setCursor(1, 1);
            if (s < 10) {
                lcd.print("0");
            }
            lcd.print(s);
            lcd.setCursor(1, 3);
            DAY(day);

            //вывод на дисплей активности помп
            if(pump_1[day+2]==1) {
                if ((h == pump_1[0])&&(m == pump_1[1])&&(s <= pump_1[2])) {
                    lcd.setCursor(6, 0);
                    lcd.write(126);
                }  else {
                    lcd.setCursor(6, 0);
                    lcd.print(" ");
                }
            }

            if (pump_2[day + 2] == 1) {
                if ((h == pump_2[0])&&(m == pump_2[1])&& s <= pump_2[2]) {
                    lcd.setCursor(6, 1);
                    lcd.write(126);
                } else {
                    lcd.setCursor(6, 1);
                    lcd.print(" ");
                }
            }

            if (pump_3[day + 2] == 1) {
                if ((h == pump_3[0])&&(m == pump_3[1])&&s <= pump_3[2]) {
                    lcd.setCursor(6, 2);
                    lcd.write(126);
                } else {
                    lcd.setCursor(6, 2);
                    lcd.print(" ");
                }
            }

            if (pump_4[day + 2] == 1) {
                if ((h == pump_4[0])&&(m == pump_4[1])&&s <= pump_4[2]) {
                    lcd.setCursor(6, 3);
                    lcd.write(126);
                } else {
                    lcd.setCursor(6, 3);
                    lcd.print(" ");
                }
            }

            lcd.setCursor(7, 0);
            lcd.print(P1);
            lcd.setCursor(10, 0);

            if (pump_1[0] < 10) {
                lcd.print("0");
            }

            lcd.print(pump_1[0]);
            lcd.print(":");

            if (pump_1[1] < 10) {
                lcd.print("0");
            }

            lcd.print(pump_1[1]);
            lcd.print(" * ");
            lcd.print(pump_1[2]);

            lcd.setCursor(7, 1);
            lcd.print(P2);
            lcd.setCursor(10, 1);

            if (pump_2[0] < 10) {
                lcd.print("0");
            }

            lcd.print(pump_2[0]);
            lcd.print(":");

            if (pump_2[1] < 10) {
                lcd.print("0");
            }

            lcd.print(pump_2[1]);
            lcd.print(" * ");
            lcd.print(pump_2[2]);

            lcd.setCursor(7, 2);
            lcd.print(P3);
            lcd.setCursor(10, 2);

            if (pump_3[0] < 10) {
                lcd.print("0");
            }

            lcd.print(pump_3[0]);
            lcd.print(":");

            if (pump_3[1] < 10) {
                lcd.print("0");
            }

            lcd.print(pump_3[1]);
            lcd.print(" * ");
            lcd.print(pump_3[2]);

            lcd.setCursor(7, 3);
            lcd.print(P4);
            lcd.setCursor(10, 3);

            if (pump_4[0] < 10) {
                lcd.print("0");
            }

            lcd.print(pump_4[0]);
            lcd.print(":");

            if (pump_4[1] < 10) {
                lcd.print("0");
            }

            lcd.print(pump_4[1]);
            lcd.print(" * ");
            lcd.print(pump_4[2]);

            break;
        case 2:
            lcd.setCursor(7, 0);
            lcd.print("ТЕСТ ПОМП");

            //часы
            lcd.setCursor(0, 0);
            if (h < 10) {
                lcd.print("0");
            }
            lcd.print(h);
            lcd.print(":");
            if (m < 10) {
                lcd.print("0");
            }
            lcd.print(m);

            lcd.setCursor(1, 1);
            if (s < 10) {
                lcd.print("0");
            }
            lcd.print(s);
            lcd.setCursor(1, 3);
            lcd.print("ПОМПЫ:");
            lcd.setCursor(8, 3);

            if (enc1.isRight()) {
                if (++menu_val_2 >= 4) menu_val_2 = 0;  //обработка поворота энкодера без нажатия - проход по меню
            } else if (enc1.isLeft()) {
                if (--menu_val_2 < 0) menu_val_2 = 3; //обработка поворота энкодера без нажатия - проход по меню
            }

            switch (menu_val_2) {
                case 0:
                    lcd.write(126);
                    lcd.print(pump_test[0]);
                    lcd.print(" ");
                    lcd.print(pump_test[1]);
                    lcd.print(" ");
                    lcd.print(pump_test[2]);
                    lcd.print(" ");
                    lcd.print(pump_test[3]);

                    if (enc1.isRightH()) {
                        if (++pump_test[0] >= 2) pump_test[0] = 0;
                    } else if (enc1.isLeftH()) {
                        if (--pump_test[0] < 0) pump_test[0] = 1;
                    }
                    break;
                case 1:
                    lcd.print(" ");
                    lcd.print(pump_test[0]);
                    lcd.write(126);
                    lcd.print(pump_test[1]);
                    lcd.print(" ");
                    lcd.print(pump_test[2]);
                    lcd.print(" ");
                    lcd.print(pump_test[3]);

                    if (enc1.isRightH()) {
                        if (++pump_test[1] >= 2) pump_test[1] = 0;
                    } else if (enc1.isLeftH()) {
                        if (--pump_test[1] < 0) pump_test[1] = 1;
                    }
                    break;
                case 2:
                    lcd.print(" ");
                    lcd.print(pump_test[0]);
                    lcd.print(" ");
                    lcd.print(pump_test[1]);
                    lcd.write(126);
                    lcd.print(pump_test[2]);
                    lcd.print(" ");
                    lcd.print(pump_test[3]);

                    if (enc1.isRightH()) {
                        if (++pump_test[2] >= 2) pump_test[2] = 0;
                    } else if (enc1.isLeftH()) {
                        if (--pump_test[2] < 0) pump_test[2] = 1;
                    }
                    break;
                case 3:
                    lcd.print(" ");
                    lcd.print(pump_test[0]);
                    lcd.print(" ");
                    lcd.print(pump_test[1]);
                    lcd.print(" ");
                    lcd.print(pump_test[2]);
                    lcd.write(126);
                    lcd.print(pump_test[3]);

                    if (enc1.isRightH()) {
                        if (++pump_test[3] >= 2) pump_test[3] = 0;
                    } else if (enc1.isLeftH()) {
                        if (--pump_test[3] < 0) pump_test[3] = 1;
                    }
                    break;
            }
            break;
        case 3:
            Pump_screen ((int8_t*)pump_1, P1_);
            break;
        case 4:
            Pump_screen ((int8_t*)pump_2, P2_);
            break;
        case 5:
            Pump_screen ((int8_t*)pump_3, P3_);
            break;
        case 6:
            Pump_screen ((int8_t*)pump_4, P4_);
            break;
        case 7:
            lcd.setCursor(0, 0);
            lcd.print("Time set ");
            lcd.setCursor(10, 0);
            if (h < 10) {
                lcd.print("0");
            }
            lcd.print(h);
            lcd.setCursor(13, 0);
            if (m < 10) {
                lcd.print("0");
            }
            lcd.print(m);

            lcd.setCursor(0, 1);
            lcd.print("Date: ");
            lcd.setCursor(6, 1);
            if (date < 10) {
                lcd.print("0");
            }
            lcd.print(date);
            lcd.setCursor(9, 1);
            if (month < 10) {
                lcd.print("0");
            }
            lcd.print(month);
            lcd.setCursor(12, 1);
            lcd.print(year);

            lcd.setCursor(15, 1);
            DAY(day);
            lcd.setCursor(0, 2);
            lcd.print("LCD ");
            lcd.setCursor(5, 2);

            switch (lcd_on) {
                case 0:
                    lcd.print("AUTO");
                    break;
                case 1:
                    lcd.print("OFF ");
                    break;
                case 2:
                    lcd.print("ON  ");
                    break;
            }

            if (enc1.isRight()) {
                if (++menu_val_2 >= 7) menu_val_2 = 0;  //обработка поворота энкодера без нажатия - проход по меню
            } else if (enc1.isLeft()) {
                if (--menu_val_2 < 0) menu_val_2 = 6; //обработка поворота энкодера без нажатия - проход по меню
            }

            switch (menu_val_2) {
                case 0:
                    lcd.setCursor(9, 0);
                    lcd.write(126);
                    lcd.setCursor(12, 0);
                    lcd.print(":");
                    lcd.setCursor(5, 1);
                    lcd.print(" ");
                    lcd.setCursor(8, 1);
                    lcd.print("-");
                    lcd.setCursor(11, 1);
                    lcd.print("-");
                    lcd.setCursor(4, 2);
                    lcd.print(" ");
                    if (enc1.isRightH()) {
                        if (++h >= 24) h = 0;
                        setHour(h);
                    } else if (enc1.isLeftH()) {
                        if (--h < 0) h = 23;
                        setHour(h);
                    }
                    break;
                case 1:
                    lcd.setCursor(9, 0);
                    lcd.print(" ");
                    lcd.setCursor(12, 0);
                    lcd.write(126);
                    lcd.setCursor(5, 1);
                    lcd.print(" ");
                    lcd.setCursor(8, 1);
                    lcd.print("-");
                    lcd.setCursor(11, 1);
                    lcd.print("-");
                    lcd.setCursor(4, 2);
                    lcd.print(" ");
                    if (enc1.isRightH()) {
                        if (++m >= 60) m = 0;
                        setMinute(m);
                    } else if (enc1.isLeftH()) {
                        if (--m < 0) m = 59;
                        setMinute(m);
                    }
                    break;
                case 2:
                    lcd.setCursor(9, 0);
                    lcd.print(" ");
                    lcd.setCursor(12, 0);
                    lcd.print(":");
                    lcd.setCursor(5, 1);
                    lcd.write(126);
                    lcd.setCursor(8, 1);
                    lcd.print("-");
                    lcd.setCursor(11, 1);
                    lcd.print("-");
                    lcd.setCursor(4, 2);
                    lcd.print(" ");
                    if (enc1.isRightH()) {
                        if (++date >= 32) date = 0;
                        setDate(date);
                    } else if (enc1.isLeftH()) {
                        if (--date < 0) date = 31;
                        setDate(date);
                    }
                    break;
                case 3:
                    lcd.setCursor(9, 0);
                    lcd.print(" ");
                    lcd.setCursor(12, 0);
                    lcd.print(":");
                    lcd.setCursor(5, 1);
                    lcd.print(" ");
                    lcd.setCursor(8, 1);
                    lcd.write(126);
                    lcd.setCursor(11, 1);
                    lcd.print("-");
                    lcd.setCursor(4, 2);
                    lcd.print(" ");
                    if (enc1.isRightH()) {
                        if (++month >= 13) month = 0;
                        setMonth(month);
                    } else if (enc1.isLeftH()) {
                        if (--month < 0) month = 12;
                        setMonth(month);
                    }
                    break;
                case 4:
                    lcd.setCursor(9, 0);
                    lcd.print(" ");
                    lcd.setCursor(12, 0);
                    lcd.print(":");
                    lcd.setCursor(5, 1);
                    lcd.print(" ");
                    lcd.setCursor(8, 1);
                    lcd.print("-");
                    lcd.setCursor(11, 1);
                    lcd.write(126);
                    lcd.setCursor(14, 1);
                    lcd.print(" ");
                    lcd.setCursor(4, 2);
                    lcd.print(" ");
                    if (enc1.isRightH()) {
                        if (++year >= 51) year = 0;
                        setYear(year);
                    } else if (enc1.isLeftH()) {
                        if (--year < 0) year = 20;
                        setYear(year);
                    }
                    break;
                case 5:
                    lcd.setCursor(9, 0);
                    lcd.print(" ");
                    lcd.setCursor(12, 0);
                    lcd.print(":");
                    lcd.setCursor(5, 1);
                    lcd.print(" ");
                    lcd.setCursor(8, 1);
                    lcd.print("-");
                    lcd.setCursor(11, 1);
                    lcd.print("-");
                    lcd.setCursor(14, 1);
                    lcd.write(126);
                    lcd.setCursor(4, 2);
                    lcd.print(" ");
                    if (enc1.isRightH()) {
                        if (++day >= 8) day = 1;
                        setDay(day);
                    } else if (enc1.isLeftH()) {
                        if (--day < 1) day = 7;
                        setDay(day);
                    }
                    break;
                case 6:
                    lcd.setCursor(9, 0);
                    lcd.print(" ");
                    lcd.setCursor(12, 0);
                    lcd.print(":");
                    lcd.setCursor(5, 1);
                    lcd.print(" ");
                    lcd.setCursor(8, 1);
                    lcd.print("-");
                    lcd.setCursor(11, 1);
                    lcd.print("-");
                    lcd.setCursor(14, 1);
                    lcd.print(" ");
                    lcd.setCursor(4, 2);
                    lcd.write(126);
                    if (enc1.isRightH()) {
                        if (++lcd_on >= 3) lcd_on = 0;  //обработка поворота энкодера без нажатия - проход по меню
                    } else if (enc1.isLeftH()) {
                        if (--lcd_on < 0) lcd_on = 2; //обработка поворота энкодера без нажатия - проход по меню
                    }
                    break;
            }
            break;
        case 8:
            lcd.setCursor(0, 0);
            lcd.setCursor(0, 1);
            break;
    }
}



void DAY(int8_t _day ) {
    switch (_day) {
        case 1:
            lcd.print("ВС");
            break;
        case 2:
            lcd.print("ПН");
            break;
        case 3:
            lcd.print("ВТ");
            break;
        case 4:
            lcd.print("СР");
            break;
        case 5:
            lcd.print("ЧТ");
            break;
        case 6:
            lcd.print("ПТ");
            break;
        case 7:
            lcd.print("СБ");
            break;
    }
}

void Pump_screen (int8_t *p, String p_name ) {
    lcd.setCursor(7, 0);
    lcd.print(p_name);
    lcd.setCursor(0, 1);
    lcd.print("ВРЕМЯ");
    lcd.setCursor(0, 2);
    lcd.print("ДЕНЬ П В С Ч П С В");
    lcd.setCursor(11, 1);
    lcd.print(">>");
    if (enc1.isRight()) {
        if (++menu_val_2 >= 10) menu_val_2 = 0;  //обработка поворота энкодера без нажатия - проход по меню
    } else if (enc1.isLeft()) {
        if (--menu_val_2 < 0) menu_val_2 = 9; //обработка поворота энкодера без нажатия - проход по меню
    }

    switch (menu_val_2) {
        case 0:
            lcd.setCursor(5, 1);
            lcd.write(126);
            lcd.setCursor(6, 1);
            if (p[0] < 10) {
                lcd.print("0");
            }
            lcd.print(p[0]);
            lcd.print(":");
            if (p[1] < 10) {
                lcd.print("0");
            }
            lcd.print(p[1]);
            lcd.setCursor(13, 1);
            lcd.print(" ");
            lcd.setCursor(14, 1);
            lcd.print(p[2]);
            lcd.print(" MIN");
            lcd.setCursor(4, 3);
            lcd.print(" ");
            lcd.print(p[4]);
            lcd.print(" ");
            lcd.print(p[5]);
            lcd.print(" ");
            lcd.print(p[6]);
            lcd.print(" ");
            lcd.print(p[7]);
            lcd.print(" ");
            lcd.print(p[8]);
            lcd.print(" ");
            lcd.print(p[9]);
            lcd.print(" ");
            lcd.print(p[3]);
            if (enc1.isRightH()) {
                if (++p[0] >= 24) p[0] = 0;
            } else if (enc1.isLeftH()) {
                if (--p[0] < 0) p[0] = 23;
            }
            break;
        case 1:
            lcd.setCursor(5, 1);
            lcd.print(" ");
            lcd.setCursor(6, 1);
            if (p[0] < 10) {
                lcd.print("0");
            }
            lcd.print(p[0]);
            lcd.write(126);
            if (p[1] < 10) {
                lcd.print("0");
            }
            lcd.print(p[1]);
            lcd.setCursor(13, 1);
            lcd.print(" ");
            lcd.setCursor(14, 1);
            lcd.print(p[2]);
            lcd.print(" MIN");
            lcd.setCursor(4, 3);
            lcd.print(" ");
            lcd.print(p[4]);
            lcd.print(" ");
            lcd.print(p[5]);
            lcd.print(" ");
            lcd.print(p[6]);
            lcd.print(" ");
            lcd.print(p[7]);
            lcd.print(" ");
            lcd.print(p[8]);
            lcd.print(" ");
            lcd.print(p[9]);
            lcd.print(" ");
            lcd.print(p[3]);
            if (enc1.isRightH()) {
                if (++p[1] >= 59) p[1] = 0;
            } else if (enc1.isLeftH()) {
                if (--p[1] < 0) p[1] = 60;
            }
            break;
        case 2:
            lcd.setCursor(5, 1);
            lcd.print(" ");
            lcd.setCursor(6, 1);
            if (p[0] < 10) {
                lcd.print("0");
            }
            lcd.print(p[0]);
            lcd.print(":");
            if (p[1] < 10) {
                lcd.print("0");
            }
            lcd.print(p[1]);
            lcd.setCursor(13, 1);
            lcd.write(126);
            lcd.setCursor(14, 1);
            lcd.print(p[2]);
            lcd.print(" MIN");
            lcd.setCursor(4, 3);
            lcd.print(" ");
            lcd.print(p[4]);
            lcd.print(" ");
            lcd.print(p[5]);
            lcd.print(" ");
            lcd.print(p[6]);
            lcd.print(" ");
            lcd.print(p[7]);
            lcd.print(" ");
            lcd.print(p[8]);
            lcd.print(" ");
            lcd.print(p[9]);
            lcd.print(" ");
            lcd.print(p[3]);
            if (enc1.isRightH()) {
                if (++p[2] >= PERIOD+1) p[2] = 0;
            } else if (enc1.isLeftH()) {
                if (--p[2] < 0) p[2] = PERIOD;
            }
            break;
        case 3:
            lcd.setCursor(5, 1);
            lcd.print(" ");
            lcd.setCursor(6, 1);
            if (p[0] < 10) {
                lcd.print("0");
            }
            lcd.print(p[0]);
            lcd.print(":");
            if (p[1] < 10) {
                lcd.print("0");
            }
            lcd.print(p[1]);
            lcd.setCursor(13, 1);
            lcd.print(" ");
            lcd.setCursor(14, 1);
            lcd.print(p[2]);
            lcd.print(" MIN");
            lcd.setCursor(4, 3);
            lcd.write(126);
            lcd.print(p[4]);
            lcd.print(" ");
            lcd.print(p[5]);
            lcd.print(" ");
            lcd.print(p[6]);
            lcd.print(" ");
            lcd.print(p[7]);
            lcd.print(" ");
            lcd.print(p[8]);
            lcd.print(" ");
            lcd.print(p[9]);
            lcd.print(" ");
            lcd.print(p[3]);
            if (enc1.isRightH()) {
                if (++p[4] >= 2) p[4] = 0;
            } else if (enc1.isLeftH()) {
                if (--p[4] < 0) p[4] = 1;
            }
            break;
        case 4:
            lcd.setCursor(5, 1);
            lcd.print(" ");
            lcd.setCursor(6, 1);
            if (p[0] < 10) {
                lcd.print("0");
            }
            lcd.print(p[0]);
            lcd.print(":");
            if (p[1] < 10) {
                lcd.print("0");
            }
            lcd.print(p[1]);
            lcd.setCursor(13, 1);
            lcd.print(" ");
            lcd.setCursor(14, 1);
            lcd.print(p[2]);
            lcd.print(" MIN");
            lcd.setCursor(4, 3);
            lcd.print(" ");
            lcd.print(p[4]);
            lcd.write(126);
            lcd.print(p[5]);
            lcd.print(" ");
            lcd.print(p[6]);
            lcd.print(" ");
            lcd.print(p[7]);
            lcd.print(" ");
            lcd.print(p[8]);
            lcd.print(" ");
            lcd.print(p[9]);
            lcd.print(" ");
            lcd.print(p[3]);
            if (enc1.isRightH()) {
                if (++p[5] >= 2) p[5] = 0;
            } else if (enc1.isLeftH()) {
                if (--p[5] < 0) p[5] = 1;
            }
            break;
        case 5:
            lcd.setCursor(5, 1);
            lcd.print(" ");
            lcd.setCursor(6, 1);
            if (p[0] < 10) {
                lcd.print("0");
            }
            lcd.print(p[0]);
            lcd.print(":");
            if (p[1] < 10) {
                lcd.print("0");
            }
            lcd.print(p[1]);
            lcd.setCursor(13, 1);
            lcd.print(" ");
            lcd.setCursor(14, 1);
            lcd.print(p[2]);
            lcd.print(" MIN");
            lcd.setCursor(4, 3);
            lcd.print(" ");
            lcd.print(p[4]);
            lcd.print(" ");
            lcd.print(p[5]);
            lcd.write(126);
            lcd.print(p[6]);
            lcd.print(" ");
            lcd.print(p[7]);
            lcd.print(" ");
            lcd.print(p[8]);
            lcd.print(" ");
            lcd.print(p[9]);
            lcd.print(" ");
            lcd.print(p[3]);
            if (enc1.isRightH()) {
                if (++p[6] >= 2) p[6] = 0;
            } else if (enc1.isLeftH()) {
                if (--p[6] < 0) p[6] = 1;
            }
            break;
        case 6:
            lcd.setCursor(5, 1);
            lcd.print(" ");
            lcd.setCursor(6, 1);
            if (p[0] < 10) {
                lcd.print("0");
            }
            lcd.print(p[0]);
            lcd.print(":");
            if (p[1] < 10) {
                lcd.print("0");
            }
            lcd.print(p[1]);
            lcd.setCursor(13, 1);
            lcd.print(" ");
            lcd.setCursor(14, 1);
            lcd.print(p[2]);
            lcd.print(" MIN");
            lcd.setCursor(4, 3);
            lcd.print(" ");
            lcd.print(p[4]);
            lcd.print(" ");
            lcd.print(p[5]);
            lcd.print(" ");
            lcd.print(p[6]);
            lcd.write(126);
            lcd.print(p[7]);
            lcd.print(" ");
            lcd.print(p[8]);
            lcd.print(" ");
            lcd.print(p[9]);
            lcd.print(" ");
            lcd.print(p[3]);
            if (enc1.isRightH()) {
                if (++p[7] >= 2) p[7] = 0;
            } else if (enc1.isLeftH()) {
                if (--p[7] < 0) p[7] = 1;
            }
            break;
        case 7:
            lcd.setCursor(5, 1);
            lcd.print(" ");
            lcd.setCursor(6, 1);
            if (p[0] < 10) {
                lcd.print("0");
            }
            lcd.print(p[0]);
            lcd.print(":");
            if (p[1] < 10) {
                lcd.print("0");
            }
            lcd.print(p[1]);
            lcd.setCursor(13, 1);
            lcd.print(" ");
            lcd.setCursor(14, 1);
            lcd.print(p[2]);
            lcd.print(" MIN");
            lcd.setCursor(4, 3);
            lcd.print(" ");
            lcd.print(p[4]);
            lcd.print(" ");
            lcd.print(p[5]);
            lcd.print(" ");
            lcd.print(p[6]);
            lcd.print(" ");
            lcd.print(p[7]);
            lcd.write(126);
            lcd.print(p[8]);
            lcd.print(" ");
            lcd.print(p[9]);
            lcd.print(" ");
            lcd.print(p[3]);
            if (enc1.isRightH()) {
                if (++p[8] >= 2) p[8] = 0;
            } else if (enc1.isLeftH()) {
                if (--p[8] < 0) p[8] = 1;
            }
            break;
        case 8:
            lcd.setCursor(5, 1);
            lcd.print(" ");
            lcd.setCursor(6, 1);
            if (p[0] < 10) {
                lcd.print("0");
            }
            lcd.print(p[0]);
            lcd.print(":");
            if (p[1] < 10) {
                lcd.print("0");
            }
            lcd.print(p[1]);
            lcd.setCursor(13, 1);
            lcd.print(" ");
            lcd.setCursor(14, 1);
            lcd.print(p[2]);
            lcd.print(" MIN");
            lcd.setCursor(4, 3);
            lcd.print(" ");
            lcd.print(p[4]);
            lcd.print(" ");
            lcd.print(p[5]);
            lcd.print(" ");
            lcd.print(p[6]);
            lcd.print(" ");
            lcd.print(p[7]);
            lcd.print(" ");
            lcd.print(p[8]);
            lcd.write(126);
            lcd.print(p[9]);
            lcd.print(" ");
            lcd.print(p[3]);
            if (enc1.isRightH()) {
                if (++p[9] >= 2) p[9] = 0;
            } else if (enc1.isLeftH()) {
                if (--p[9] < 0) p[9] = 1;
            }
            break;
        case 9:
            lcd.setCursor(5, 1);
            lcd.print(" ");
            lcd.setCursor(6, 1);
            if (p[0] < 10) {
                lcd.print("0");
            }
            lcd.print(p[0]);
            lcd.print(":");
            if (p[1] < 10) {
                lcd.print("0");
            }
            lcd.print(p[1]);
            lcd.setCursor(13, 1);
            lcd.print(" ");
            lcd.setCursor(14, 1);
            lcd.print(p[2]);
            lcd.print(" MIN");
            lcd.setCursor(4, 3);
            lcd.print(" ");
            lcd.print(p[4]);
            lcd.print(" ");
            lcd.print(p[5]);
            lcd.print(" ");
            lcd.print(p[6]);
            lcd.print(" ");
            lcd.print(p[7]);
            lcd.print(" ");
            lcd.print(p[8]);
            lcd.print(" ");
            lcd.print(p[9]);
            lcd.write(126);
            lcd.print(p[3]);
            if (enc1.isRightH()) {
                if (++p[3] >= 2) p[3] = 0;
            } else if (enc1.isLeftH()) {
                if (--p[3] < 0) p[3] = 1;
            }
            break;
    }
}

//опрос помп
void PUMPS() {
    if (pump_test[0] == 1) {
        digitalWrite(P1_Pin, LOW);
    } else {
        if (pump_1[day + 2] == 1) {
            if ((h == pump_1[0]) && (m >= pump_1[1]) && (m < pump_1[1] + pump_1[2])) {
                digitalWrite(P1_Pin, LOW);
            } else {
                digitalWrite(P1_Pin, HIGH);
            }
        } else {
            digitalWrite(P1_Pin, HIGH);
        }
    }

    if (pump_test[1] == 1) {
        digitalWrite(P2_Pin, LOW);
    } else {
        if (pump_2[day + 2] == 1) {
            if ((h == pump_2[0]) && (m >= pump_2[1]) && (m < pump_2[1] + pump_2[2])) {
                digitalWrite(P2_Pin, LOW);
            } else {
                digitalWrite(P2_Pin, HIGH);
            }
        } else {
            digitalWrite(P2_Pin, HIGH);
        }
    }

    if (pump_test[2] == 1) {
        digitalWrite(P3_Pin, LOW);
    } else {
        if (pump_3[day + 2] == 1) {
            if ((h == pump_3[0]) && (m >= pump_3[1]) && (m < pump_3[1] + pump_3[2])) {
                digitalWrite(P3_Pin, LOW);
            } else {
                digitalWrite(P3_Pin, HIGH);
            }
        } else {
            digitalWrite(P3_Pin, HIGH);
        }
    }

    if (pump_test[3] == 1) {
        digitalWrite(P4_Pin, LOW);
    } else {
        if (pump_4[day + 2] == 1) {
            if ((h == pump_4[0]) && (m >= pump_4[1]) && (m < pump_4[1] + pump_4[2]))
            {
                digitalWrite(P4_Pin, LOW);

            } else {
                digitalWrite(P4_Pin, HIGH);
            }
        } else {
            digitalWrite(P4_Pin, HIGH);
        }
    }
}

