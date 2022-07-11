#include <Wire.h>
#define DS3231_ADDRESS 0x68

#define ENCODER_TYPE 0      // тип энкодера (0 или 1). Если энкодер работает некорректно (пропуск шагов), смените тип
#define ENC_REVERSE 0       // 1 - инвертировать энкодер, 0 - нет
#define DRIVER_VERSION 1    // 0 - маркировка драйвера дисплея кончается на 4АТ, 1 - на 4Т

///--------------------------------------------------------------------------------------------ВЕРСИЯ ПРОШИВКИ
#define VER "DNK ver_11_07_2022"

#define PERIOD 120 // максимальный период срабатывания помпы (min)

///----Энкодер
#define CLK 3
#define DT 2
#define SW 0
///----Энкодер

#define LCD_DELAY_SEC 10
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

struct pumps_schedule {
    byte hour;
    byte minute;
    byte wday[7];
    byte duration;
};

struct pumps_struct {
    byte pin;
    char name[8];
    char prefix[2];
    pumps_schedule schedule;
    byte minleft;
};

//PIN, NAME, PREFIX, {HOUR_START, MIN_START, {ПН,ВТ,СР,ЧТ,ПТ,СБ,ВС}, DURING_MIN,
pumps_struct pumps[] = {
    7, "ГАЗОН",  "Г1", {7,10, {1,1,1,1,1,1,1}, 10}, 0,
    8, "ТУИ",    "Т",  {7,20, {1,1,1,1,1,1,1}, 10}, 0,
    9, "ГАЗОН2", "Г2", {7,30, {1,1,1,1,1,1,1}, 10}, 0,
    10,"ГАЗОН3", "Г3", {7,40, {1,1,1,1,1,1,1}, 10}, 0,
};

byte menu_idx = 1;
byte submenu_idx = 0;
byte h, m, s, wday, day, month, year;
bool lcd_backlight_mode = 0;
bool circle_mode = 0;

void setup() {
    enc1.setType(TYPE2);
#if (ENC_REVERSE)
    enc1.setDirection(NORM);
#else
    enc1.setDirection(REVERSE);
#endif

    attachInterrupt(0, ENC_EVENT(), CHANGE);    // прерывание на 2 пине! CLK у энка
    attachInterrupt(1, ENC_EVENT(), CHANGE);    // прерывание на 3 пине! DT у энка

    for (int i=0; i < sizeof(pumps); i++ ) {
        pinMode(pumps[i].pin, OUTPUT);
        PUMP_OFF(i);
    }

    Wire.begin();
    lcd.init();
    lcd.backlight();
    lcd.clear();

    // --------------------- СБРОС НАСТРОЕК ---------------------
    if (!digitalRead(SW)) {          // если нажат энкодер, сбросить настройки до 1
        EEPROM_WR();
        lcd.setCursor(0, 0); lcd.print("Reset settings");
    }
    while (!digitalRead(SW));        // ждём отпускания кнопки
    lcd.clear();

    EEPROM_RD();
}

// функции с часами
byte decToBcd(byte val) {
    // Convert normal decimal numbers to binary coded decimal
    return ( (val / 10 * 16) + (val % 10) );
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

void setWDay(byte day1) {
    Wire.beginTransmission(DS3231_ADDRESS);
    Wire.write(0x03); //stop Oscillator
    Wire.write(decToBcd(day1));
    Wire.endTransmission();
}

void setDay(byte date1) {
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
    Wire.beginTransmission(DS3231_ADDRESS);
    Wire.write(0x00);
    Wire.endTransmission();
    Wire.requestFrom(DS3231_ADDRESS, 7);
    if (Wire.available()) {
        s     = Wire.read();
        m     = Wire.read();
        h     = Wire.read();
        wday  = Wire.read();
        day   = Wire.read();
        month = Wire.read();
        year  = Wire.read();

        s     = (((s & B11110000) >> 4) * 10 + (s & B00001111));         //Convert BCD to decimal
        m     = (((m & B11110000) >> 4) * 10 + (m & B00001111));
        h     = (((h & B00110000) >> 4) * 10 + (h & B00001111));
        wday  = (wday & B00000111) - 1;                                  // 0-6
        day   = (((day & B00110000) >> 4) * 10 + (day & B00001111));     //Convert BCD to decimal  1-31
        month = (((month & B00010000) >> 4) * 10 + (month & B00001111));
        year  = (((year & B11110000) >> 4) * 10 + (year & B00001111));
    }
}

void LCD_Backlight() {
    if (lcd_backlight_mode == 0) {
        lcd.backlight();
    } else if (lcd_backlight_mode == 1) {
        if (lcd_secleft) {
            lcd.backlight();
            lcd_secleft--;
        } else {
            lcd.noBacklight();
        }
    } else if (lcd_backlight_mode == 2) {
        lcd.backlight();
    }
}

void EEPROM_RD() {
    int addr = 0;
    for (int i = 0; i < sizeof(pumps); i++) {
        EEPROM.get(addr, pumps[i].schedule);
        addr += sizeof(pumps_schedule);
    }
    EEPROM.get(addr, lcd_backlight_mode);
}

void EEPROM_WR() {
    int addr = 0;
    pumps_schedule tmp;

    for (int i = 0; i < sizeof(pumps); i++) {
        EEPROM.put(addr, pumps[i].schedule);
        addr += sizeof(pumps_schedule);
    }
    EEPROM.put(addr, lcd_backlight_mode);
}

void ENC_EVENT() {
    lcd_secleft = LCD_DELAY_SEC;
    lcd.backlight();

    READ_TIME_RTC();
    MENU();
    enc1.tick();
}

void loop() {
    READ_TIME_RTC();
    SCHEDULE();
    PUMPS_UPDATE();
    MENU();
    LCD_Backlight();
    delay(1000);
}

void SCHEDULE() {
    static int8_t curmin = 0;

    if (curmin != m ) {
        // Minute changed
        curmin = m;
        for(int8_t i=0; i<sizeof(pumps); i++) {
            if (pumps[i].minleft) {
                pumps[i].minleft--;
                if ( circle_mode ) break;
            }
            else {
                if ( pumps[i].schedule.hour == h &&
                     pumps[i].schedule.minute == m &&
                     pumps[i].schedule.wday[wday] )
                {
                    pumps[i].minleft = pumps[i].schedule.duration;
                }
            }
        }
    }
}

void MENU() {
    if (enc1.isClick()) {
        submenu_idx = 0;
        EEPROM_WR();
        if (menu_idx < 8) menu_idx++ else menu_idx = 1;
    }

    lcd.clear();

    //меню
    switch (menu_idx) {
        //основной экран
        case 1:
            //часы
            lcd.setCursor(0, 0);
            lcd_print_2d(h); lcd.print(":"); lcd_print_2d(m);
            lcd.setCursor(1, 1);
            lcd_print_2d(s);
            lcd.setCursor(1, 3);
            lcd_print_day(day);

            //вывод на дисплей активности помп
            for(int=0; i<sizeof(pumps); i++) {
                lcd.setCursor(6, i);
                if (pumps[i].minleft) {
                    lcd.write(126);
                } else {
                    lcd.print(" ");
                }

                lcd.setCursor(7, i);
                lcd.print(pumps[i].prefix);
                lcd.setCursor(10, i);
                lcd_print_2d(pumps[i].hour);
                lcd.print(":");
                lcd_print_2d(pumps[i].minute);
                lcd.print(" * ");
                lcd.print(pumps[i].duration);
            }
            break;
        case 2:
            lcd.setCursor(7, 0);
            lcd.print("ТЕСТ ПОМП");

            //часы
            lcd.setCursor(0, 0);
            lcd_print_2d(h); lcd.print(":"); lcd_print_2d(m);
            lcd.setCursor(1, 1);
            lcd_print_2d(s);

            lcd.setCursor(1, 3);
            lcd.print("ПОМПЫ:");
            lcd.setCursor(8, 3);

            if (enc1.isRight()) {
                if (++submenu_idx > 3) submenu_idx = 0;  //обработка поворота энкодера без нажатия - проход по меню
            } else if (enc1.isLeft()) {
                if (--submenu_idx < 0) submenu_idx = 3; //обработка поворота энкодера без нажатия - проход по меню
            }

            for(int i=0; i<sizeof(pumps); i++) {
                if (i==submenu_idx) {
                    lcd.write(126);
                } else {
                    lcd.print(" ");
                }
                lcd.print(pumps[i].minleft);
            }

            if (enc1.isRightH()) {
                if (++pumps[submenu_idx].minleft > 60) pumps[submenu_idx].minleft = 0;
            } else if (enc1.isLeftH()) {
                if (--pumps[submenu_idx].minleft < 0) pumps[submenu_idx].minleft = 1;
            }
            break;
        case 3:
            Pump_screen(0);
            break;
        case 4:
            Pump_screen(1);
            break;
        case 5:
            Pump_screen(2);
            break;
        case 6:
            Pump_screen(3);
            break;
        case 7:
            lcd.setCursor(0, 0);
            lcd.print("Time set ");
            lcd.setCursor(10, 0);
            lcd_print_2d(h);
            lcd.setCursor(13, 0);
            lcd_print_2d(m);

            lcd.setCursor(0, 1);
            lcd.print("Date: ");
            lcd.setCursor(6, 1);
            lcd_print_2d(day);
            lcd.setCursor(9, 1);
            lcd_print_2d(month);
            lcd.setCursor(12, 1);
            lcd.print(year);

            lcd.setCursor(15, 1);
            lcd_print_day(wday);
            lcd.setCursor(0, 2);
            lcd.print("LCD ");
            lcd.setCursor(5, 2);

            switch (lcd_backlight_mode) {
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
                if (++submenu_idx > 6) submenu_idx = 0;  //обработка поворота энкодера без нажатия - проход по меню
            } else if (enc1.isLeft()) {
                if (--submenu_idx < 0) submenu_idx = 6; //обработка поворота энкодера без нажатия - проход по меню
            }

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
            lcd.setCursor(4, 2);
            lcd.print(" ");

            switch (submenu_idx) {
                case 0:
                    lcd.setCursor(9, 0);
                    lcd.write(126);
                    if (enc1.isRightH()) {
                        if (++h > 23) h = 0;
                        setHour(h);
                    } else if (enc1.isLeftH()) {
                        if (--h < 0) h = 23;
                        setHour(h);
                    }
                    break;
                case 1:
                    lcd.setCursor(12, 0);
                    lcd.write(126);
                    if (enc1.isRightH()) {
                        if (++m > 59) m = 0;
                        setMinute(m);
                    } else if (enc1.isLeftH()) {
                        if (--m < 0) m = 59;
                        setMinute(m);
                    }
                    break;
                case 2:
                    lcd.setCursor(5, 1);
                    lcd.write(126);
                    if (enc1.isRightH()) {
                        if (++day > 31) day = 0;
                        setDay(day);
                    } else if (enc1.isLeftH()) {
                        if (--day < 0) day = 31;
                        setDay(day);
                    }
                    break;
                case 3:
                    lcd.setCursor(8, 1);
                    lcd.write(126);
                    if (enc1.isRightH()) {
                        if (++month > 12) month = 0;
                        setMonth(month);
                    } else if (enc1.isLeftH()) {
                        if (--month < 0) month = 12;
                        setMonth(month);
                    }
                    break;
                case 4:
                    lcd.setCursor(11, 1);
                    lcd.write(126);
                    if (enc1.isRightH()) {
                        if (++year > 50) year = 0;
                        setYear(year);
                    } else if (enc1.isLeftH()) {
                        if (--year < 0) year = 20;
                        setYear(year);
                    }
                    break;
                case 5:
                    lcd.setCursor(14, 1);
                    lcd.write(126);
                    if (enc1.isRightH()) {
                        if (++wday > 6) wday = 0;
                        setWDay(wday);
                    } else if (enc1.isLeftH()) {
                        if (--wday < 0) wday = 6;
                        setWDay(wday);
                    }
                    break;
                case 6:
                    lcd.setCursor(4, 2);
                    lcd.write(126);
                    if (enc1.isRightH()) {
                        if (++lcd_backlight_mode > 2) lcd_backlight_mode = 0;  //обработка поворота энкодера без нажатия - проход по меню
                    } else if (enc1.isLeftH()) {
                        if (--lcd_backlight_mode < 0) lcd_backlight_mode = 2; //обработка поворота энкодера без нажатия - проход по меню
                    }
                    break;
            }
            break;
        case 8:
            lcd.setCursor(3, 0);
            lcd.print("КРУГОВОЙ ПОЛИВ");

            lcd.setCursor(1, 2);
            lcd.print("ПОМПЫ:");

            if (enc1.isRight()) {
                for (int i=0; i < 4; i++ ) {
                    pumps[i].minleft = 9;
                }
                circle_mode = 1;
            } else if (enc1.isLeft()) {
                for (int i=0; i < 4; i++ ) {
                    pumps[i].minleft = 0;
                }
                circle_mode = 0;
            }

            lcd.setCursor(8, 2);

            for (int i = 0; i < 4; i++) {
                lcd.write(" ");
                lcd.print(pumps[i].minleft);
            }
            break;
    }
}

void lcd_print_day(byte _day ) {
    switch (_day) {
        case 0:
            lcd.print("ПН");
            break;
        case 1:
            lcd.print("ВТ");
            break;
        case 2:
            lcd.print("СР");
            break;
        case 3:
            lcd.print("ЧТ");
            break;
        case 4:
            lcd.print("ПТ");
            break;
        case 5:
            lcd.print("СБ");
            break;
        case 6:
            lcd.print("ВС");
            break;
    }
}

void Pump_screen (int p) {
    lcd.setCursor(7, 0);
    lcd.print(pumps[p].name);

    lcd.setCursor(0, 1); lcd.print("ВРЕМЯ");
    lcd.setCursor(5, 1); lcd.print(" ");
    lcd_print_2d(pumps[p].schedule.hour); lcd.print(":"); lcd_print_2d(pumps[p].schedule.minute);
    lcd.setCursor(11, 1); lcd.print(">>"); lcd.print(" ");
    lcd.print(pumps[p].schedule.duration); lcd.print(" MIN");

    lcd.setCursor(0, 2);
    lcd.print("ДЕНЬ П В С Ч П С В");

    lcd.setCursor(4, 3);
    for(int i=0; i<7; i++) {
        lcd.print(" ");
        lcd.print(pumps[p].schedule.wday[i]);
    }

    if (enc1.isRight()) {
        if (++submenu_idx > 9) submenu_idx = 0;  //обработка поворота энкодера без нажатия - проход по меню
    } else if (enc1.isLeft()) {
        if (--submenu_idx < 0) submenu_idx = 9; //обработка поворота энкодера без нажатия - проход по меню
    }

    switch (submenu_idx) {
        case 0:
            lcd.setCursor(5, 1);
            lcd.write(126);
            if (enc1.isRightH()) {
                if (++pumps[p].schedule.hour > 23) pumps[p].schedule.hour = 0;
            } else if (enc1.isLeftH()) {
                if (--pumps[p].schedule.hour < 0) pumps[p].schedule.hour = 23;
            }
            break;
        case 1:
            lcd.setCursor(8, 1);
            lcd.write(126);
            if (enc1.isRightH()) {
                if (++pumps[p].schedule.minute > 59) pumps[p].schedule.minute = 0;
            } else if (enc1.isLeftH()) {
                if (--pumps[p].schedule.minute < 0) pumps[p].schedule.minute = 59;
            }
            break;
        case 2:
            lcd.setCursor(13, 1);
            lcd.write(126);
            if (enc1.isRightH()) {
                if (++pumps[p].schedule.duration > PERIOD) pumps[p].schedule.duration = 0;
            } else if (enc1.isLeftH()) {
                if (--pumps[p].schedule.duration < 0) pumps[p].schedule.duration = PERIOD;
            }
            break;
    }

    if (submenu_idx > 2) {
        int wday = submenu_idx - 3;
        lcd.setCursor(4 + wday*2, 3);
        lcd.write(126);
        if (enc1.isRightH()) {
            if (++pumps[p].schedule.wday[wday] > 1) pumps[p].schedule.wday[wday] = 0;
        } else if (enc1.isLeftH()) {
            if (--pumps[p].schedule.wday[wday] < 0) pumps[p].schedule.wday[wday] = 1;
        }
    }
}

void PUMPS_UPDATE() {
    int pumps_affected = 0;
    for (int i=0; i<sizeof(pumps); i++) {
        if (pumps[i].minleft) {
            if (circle_mode) {
                if (!pumps_affected) PUMP_ON(i);
            } else {
                PUMP_ON(i);
            }
            pumps_affected++;
        }
        else {
            PUMP_OFF(i);
        }
    }
    if (!pumps_affected) { circle_mode = 0 };
}

void PUMP_ON(byte p) { digitalWrite(p.pin, LOW) };
void PUMP_OFF(byte p) { digitalWrite(p.pin, HIGH) };

void lcd_print_2d( int n ) { if (n < 10) lcd.print("0"); lcd.print(n); }

