#include "xc_config_settings.h"
#include "plib/delays.h"
#include "plib/timers.h"
#include "plib/adc.h"

unsigned int timer[4] = {0, 0, 0, 0};
unsigned int alarm[6] = {0, 0, 0, 0, 0, 0};
unsigned int time[12] = {0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0};
int enable = 1;
char U1_value;
char U2_value;
char counter = 0;
int select = 0;
int check_alarm;
int brightness = 0;
int counter3 = 0;
char alarm_set = 0;
char set_sec = 0;
char set_min = 0;
char display_milsecond = 0;
char display_milsecond_stopwatch = 0;
#define NOTE_C  523
#define NOTE_D  587
#define NOTE_E  659
#define NOTE_F  698
#define NOTE_G  784
#define NOTE_A  880
#define NOTE_B  988
int melody[] = {
  NOTE_G, NOTE_E, NOTE_D, NOTE_C, 
  NOTE_D, NOTE_E, NOTE_G, NOTE_E, 
  NOTE_D, NOTE_C, NOTE_D, NOTE_E, 
  NOTE_D, NOTE_E, NOTE_G, NOTE_E, 
  NOTE_G, NOTE_A, NOTE_E, NOTE_A, 
  NOTE_G, NOTE_E, NOTE_D, NOTE_C
};
int tempo[] = {
  12, 12, 12, 12,
  12, 12, 12, 12,
  12, 12, 24, 24,
  24, 24, 12, 12,
  12, 12, 12, 12,
  12, 12, 12, 12
};

////////////////////////////////////////////////////////////////////////
/////////////////////////////CONFIGURATION//////////////////////////////
////////////////////////////////////////////////////////////////////////

void config_IO(void){
    ADCON1 = 0x0F;
    TRISF = 0x00;
    TRISHbits.RH0 = 0;
    TRISHbits.RH1 = 0;
    TRISBbits.RB0 = 1;
    TRISJbits.RJ6 = 0;
    TRISAbits.RA4 = 0;
    LATAbits.LA4 = 0;
}
void configure_ADC(void){
    OpenADC(ADC_FOSC_32 & ADC_RIGHT_JUST & ADC_12_TAD, ADC_CH0 & ADC_INT_OFF & ADC_VREFPLUS_VDD & ADC_VREFMINUS_VSS, 9);
}
unsigned int get_ADC_value(void){
    unsigned int tenbit_sample, eightbit_sample;
    ConvertADC();
    while(BusyADC()){};
    tenbit_sample = ReadADC();
    eightbit_sample = (tenbit_sample/4);
    return eightbit_sample;
}
void configure_PB2_interrupt(void){
    INTCONbits.INT0E = 1;
    INTCON2bits.INTEDG0 = 0;
    INTCONbits.INT0F = 0; 
}
void enable_global_interrupts(void){
    INTCONbits.PEIE = 1;
    INTCONbits.GIE = 1;
}
void StartTimer0(void){
    OpenTimer0(TIMER_INT_ON & T0_16BIT & T0_SOURCE_INT & T0_PS_1_64);
    WriteTimer0(60653);   // t = pre_scaler * Tcy * (2^n - write_value)
}
void StartTimer1(void){
    OpenTimer1(TIMER_INT_ON & T1_16BIT_RW & T1_SOURCE_INT & T1_PS_1_1);
    WriteTimer1(53036);
}
void StartTimer3(void){
    OpenTimer3(TIMER_INT_ON & T3_16BIT_RW & T3_SOURCE_INT & T3_PS_1_8 & T3_SOURCE_INT);
    WriteTimer3(65000);
}

////////////////////////////////////////////////////////////////////////
///////////////////////////////INPUT////////////////////////////////////
////////////////////////////////////////////////////////////////////////

unsigned char read_switches(void){
    unsigned char nibble1 = PORTC;
    nibble1 = nibble1 >> 2;
    nibble1 = nibble1 & 0x0F;
    unsigned char nibble2 = PORTH;
    nibble2 = nibble2 & 0xF0;
    unsigned char byte = nibble1 | nibble2;
    return byte;
}
unsigned char PB1_pressed(void){
    if(!PORTJbits.RJ5){
        while(!PORTJbits.RJ5){}
        Delay10KTCYx(30);
        return 1;
    }else{
        return 0;
    }
}
unsigned char PB2_pressed(void){
    if(!PORTBbits.RB0){
        while(!PORTBbits.RB0);
        Delay10KTCYx(30);
        return 1;
    }else{
        return 0;
    }
}

////////////////////////////////////////////////////////////////////////
///////////////////////////////DELAY////////////////////////////////////
////////////////////////////////////////////////////////////////////////

void milsecond(int multiple){
    for(multiple; multiple > 0; multiple--){
        Delay100TCYx(25);
    }
}
void tenmicrosecond(int multiple){
    for(multiple; multiple > 0; multiple--){
        Delay1TCYx(25);
    }
}

////////////////////////////////////////////////////////////////////////
///////////////////////////////DISPLAY//////////////////////////////////
////////////////////////////////////////////////////////////////////////

unsigned char bin2hex(int val){
    unsigned char patterns[19] = {0x84, 0xF5, 0x4C, 0x64, 0x35, 0x26, 0x06, 0xF4, 0x04, 0x24, 0x14, 0x07, 0x8E, 0x45, 0x0E, 0x1E, 0xFF, 0x00, 0x0F};
    return patterns[val];
}
void display_on_U (unsigned char display_val){
    LATF = display_val;
}
void switch_off_U2 (void){
    LATHbits.LH1 = 1;
}
void switch_on_U2 (void){
    LATHbits.LH1 = 0;
}
void switch_off_U1 (void){
    LATHbits.LH0 = 1;
}
void switch_on_U1 (void){
    LATHbits.LH0 = 0;
}
void illuminate_LEDs(unsigned char pattern){
    LATAbits.LA4 = 1;
    LATF = pattern;
}
void display_driver(void){
    LATAbits.LA4=0;
    static char LED = 0;
    char LED_value = 1;
    static char U1_on = 0;
    LED ^= 0x01;
    if(LED){
        tenmicrosecond(brightness);
        if(U1_on){
            display_on_U(bin2hex(U2_value));
            switch_off_U1();
            switch_on_U2();
            U1_on = 0;
        }else{
            display_on_U(bin2hex(U1_value));
            switch_off_U2();
            switch_on_U1();
            U1_on = 1;
        }
    }else{
        if(display_milsecond == 1){
            for(int i=0; i<(counter+1); i++){
                LED_value *= 2;
            }
            LED_value -= 1;
            switch_off_U2();
            switch_off_U1();
            illuminate_LEDs(LED_value);
        }else if(display_milsecond_stopwatch){
            for(float i=0; i<(counter3); i+=6.25){
                LED_value *= 2;
            }
            LED_value -= 1;
            switch_off_U2();
            switch_off_U1();
            illuminate_LEDs(LED_value);
        }else{
            switch_off_U2();
            switch_off_U1();
            illuminate_LEDs(0);
        }
    }
}

////////////////////////////////////////////////////////////////////////
///////////////////////////////TIMER////////////////////////////////////
////////////////////////////////////////////////////////////////////////

void time_counter(void){
    counter++;
    if(counter == 8){
        counter = 0;
        time[0] += 1;
        if(time[0] == 10){
            time[0] = 0;
            time[1] += 1;
            if (time[1] == 6){
                time[1] = 0;
                time[2] += 1;
                if(time[2] == 10){
                    time[2] = 0;
                    time[3] += 1;
                    if(time[3] == 6){
                        time[3] = 0;
                        time[4] += 1;
                        if(time[4] == 10){
                            time[4] = 0;
                            time[5] += 1;
                        }
                        if(time[5] == 2 & time[4] ==4){
                            time[4] = 0;
                            time[5] = 0;
                            time[6] += 1;
                            if(time[6] == 10){
                                time[6] = 0;
                                time[7] += 1;
                                int mo =(time[9]*10)+time[8];
                                if((((mo == 1)||(mo == 3)||(mo == 5)||(mo == 7)||(mo == 8)||(mo == 10)||(mo == 12)) && ((time[7] == 3) && (time[6] == 1))) || 
                                        (((mo == 4)||(mo == 6)||(mo == 9)||(mo == 11)) && ((time[7] == 3) && (time[6] == 0))) || 
                                        ((mo == 2) && ((time[7] == 2) && (time[6] == 8)) && !((((time[11]*10)+time[10]) % 4) == 0)) || 
                                        ((mo == 2) && ((time[7] == 2) && (time[6] == 9)) && ((((time[11]*10)+time[10]) % 4) == 0))){
                                    time[6] = 1;
                                    time[7] = 0;
                                    time[8] += 1;
                                    if(time[8] == 10){
                                        time[8] = 0;
                                        time[9] += 1;
                                    }
                                    if(time[9] == 1 & time[8] ==2){
                                        time[8] = 1;
                                        time[9] = 0;
                                        time[10] += 1;
                                        if(time[10] == 10){
                                            time[10] = 0;
                                            time[11] += 1;
                                            if(time[11] == 10){
                                                time[11] == 0;
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
void set_second(void){
    int pot = get_ADC_value();
    int pot = pot/4.25;
    int lower_digit = pot % 10;
    int upper_digit = (pot/10) % 10;
    U1_value = lower_digit;
    U2_value = upper_digit;
    if(PB1_pressed()){
        time[0] = lower_digit;
        time[1] = upper_digit;
    };
    
}
void set_minute(){
    int pot = get_ADC_value();
    int pot = pot/4.25;
    int lower_digit = pot % 10;
    int upper_digit = (pot/10) % 10;
    U1_value = lower_digit;
    U2_value = upper_digit;
    if(PB1_pressed()){
        time[2] = lower_digit;
        time[3] = upper_digit;
    };
    
}
void set_hour(){
    int pot = get_ADC_value();
    int pot = pot/10.6;
    int lower_digit = pot % 10;
    int upper_digit = (pot/10) % 10;
    U1_value = lower_digit;
    U2_value = upper_digit;
    if(PB1_pressed()){
        time[4] = lower_digit;
        time[5] = upper_digit;
    };
    
}
void set_day(void){
    int pot = get_ADC_value();
    int mo =(time[9]*10)+time[8];
    if((mo == 1)||(mo == 3)||(mo == 5)||(mo == 8)||(mo == 8)||(mo == 10)||(mo == 12)){
        pot = (pot/8.5)+1;
    }else if((mo == 4)||(mo == 6)||(mo == 9)||(mo == 11)){
        pot = (pot/8.79)+1;
    }else if((mo == 2) && ((((time[11]*10)+time[10]) % 4) == 0)){
        pot = (pot/9.1)+1;
    }else{
        pot = (pot/9.44)+1;
    }
    int lower_digit = pot % 10;
    int upper_digit = (pot/10) % 10;
    U1_value = lower_digit;
    U2_value = upper_digit;
    if(PB1_pressed()){
        time[6] = lower_digit;
        time[7] = upper_digit;
    };
    
}void set_month(void){
    int pot = get_ADC_value();
    int pot = (pot/23.18)+1;
    int lower_digit = pot % 10;
    int upper_digit = (pot/10) % 10;
    U1_value = lower_digit;
    U2_value = upper_digit;
    if(PB1_pressed()){
        time[8] = lower_digit;
        time[9] = upper_digit;
        int mo =(time[9]*10)+time[8];
        if(((mo == 4)||(mo == 6)||(mo == 9)||(mo == 11)) && (time[7] == 3 && time[6] == 1)){
            time[6] = 0;
        }else if((mo == 2) && ((((time[11]*10)+time[10]) % 4) == 0) && (time[7] > 3)){
            time[6] = 9;
            time[7] = 2;
        }else if((mo == 2) && !((((time[11]*10)+time[10]) % 4) == 0) && (time[7] == 2 && time[6] > 8)){
            time[6] = 8;
            time[7] = 2;
        }
    };
    
}void set_year(void){
    int pot = get_ADC_value();
    int pot = pot/2.57;
    int lower_digit = pot % 10;
    int upper_digit = (pot/10) % 10;
    U1_value = lower_digit;
    U2_value = upper_digit;
    if(PB1_pressed()){
        time[10] = lower_digit;
        time[11] = upper_digit;
    };
    
}

////////////////////////////////////////////////////////////////////////
///////////////////////////////ALARM////////////////////////////////////
////////////////////////////////////////////////////////////////////////
void buzz(long frequency, long length) {
  long delayValue = 100000 / frequency / 3 ; 
  long numCycles = frequency * length / 1000; 
  for (long i = 0; i < numCycles; i++) {
    LATJbits.LATJ6 = 1;
    tenmicrosecond(delayValue);
    LATJbits.LATJ6 = 0;
    tenmicrosecond(delayValue);
  }
}
void alarm_check(void){
    check_alarm = 1;
    for(int i = 0; i < 6;i++){
        if (time[i] != alarm[i]){
            check_alarm = 0;
            break;
        }
    }
}
void alarm_active(void) {
    CloseTimer1();
    LATF = 0x00;
    switch_on_U1();
    switch_on_U2();
    LATAbits.LA4 = 1;
    display_milsecond_stopwatch = 0;
    display_milsecond = 0;
    enable = 0;
    select = 0;
    while(!enable){
        for (int thisNote = 0; thisNote < 24; thisNote++){
            if(!enable){
                int noteDuration = 1000 / tempo[thisNote];
                buzz(melody[thisNote], noteDuration);
                int pauseBetweenNotes = noteDuration * 1.30;
                LATF = 0xFF;
                milsecond(pauseBetweenNotes);
                LATF = 0x00;
                buzz(0, noteDuration);
            }else{
                break;
            }
        }
        milsecond(1000);
    }
    StartTimer1();
    LATAbits.LA4 = 0;
}
void set_alarm_second(){
    int pot = get_ADC_value();
    int pot = pot/4.25;
    int lower_digit = pot % 10;
    int upper_digit = (pot/10) % 10;
    U1_value = lower_digit;
    U2_value = upper_digit;
    if(PB1_pressed()){
        alarm_set = 1;
        alarm[0] = lower_digit;
        alarm[1] = upper_digit;
    };
    
}
void set_alarm_minute(){
    int pot = get_ADC_value();
    int pot = pot/4.25;
    int lower_digit = pot % 10;
    int upper_digit = (pot/10) % 10;
    U1_value = lower_digit;
    U2_value = upper_digit;
    if(PB1_pressed()){
        alarm_set = 1;
        alarm[2] = lower_digit;
        alarm[3] = upper_digit;
    };
    
}
void set_alarm_hour(){
    int pot = get_ADC_value();
    int pot = pot/10.6;
    int lower_digit = pot % 10;
    int upper_digit = (pot/10) % 10;
    U1_value = lower_digit;
    U2_value = upper_digit;
    if(PB1_pressed()){
        alarm_set = 1;
        alarm[4] = lower_digit;
        alarm[5] = upper_digit;
    };
    
}

////////////////////////////////////////////////////////////////////////
///////////////////////////////TIMER1///////////////////////////////////
////////////////////////////////////////////////////////////////////////

void set_timer_second(void){
    int pot = get_ADC_value();
    int pot = pot/4.25;
    int lower_digit = pot % 10;
    int upper_digit = (pot/10) % 10;
    U1_value = lower_digit;
    U2_value = upper_digit;
    if(PB1_pressed()){
        timer[0] = lower_digit;
        timer[1] = upper_digit;
        set_sec = 1;
    }
}

void set_timer_minute(void){
    int pot = get_ADC_value();
    int pot = pot/4.25;
    int lower_digit = pot % 10;
    int upper_digit = (pot/10) % 10;
    U1_value = lower_digit;
    U2_value = upper_digit;
    if(PB1_pressed()){
        timer[2] = lower_digit;
        timer[3] = upper_digit;
        set_min = 1;
    }
}

////////////////////////////////////////////////////////////////////////
/////////////////////////////////ISR////////////////////////////////////
////////////////////////////////////////////////////////////////////////

void interrupt isr(void){
    if(INTCONbits.TMR0IF){
        WriteTimer0(60653);
        time_counter();
        if(alarm_set){
            alarm_check();
        }
        INTCONbits.TMR0IF = 0; 
    }
    if(INTCONbits.INT0IF){
        switch_off_U1();
        switch_off_U2();
        while (PB2_pressed());
        enable = !enable;
        INTCONbits.INT0IF = 0;
    }
    if(PIR2bits.TMR3IF){
        WriteTimer3(59286);
        counter3++;
        if(counter3 == 50){
            counter3 = 0;
            if(display_milsecond_stopwatch){
                U1_value += 1;
                if(U1_value == 10){
                    U1_value = 0;
                    U2_value += 1;
                    if (U2_value == 10){
                        U2_value = 0;
                    }
                }
            }else{
                if(timer[0] == 0){
                    if(timer[1] == 0){
                        if(timer[2] == 0){
                            if(!(timer[3] == 0)){
                                timer[2] = 9;
                                timer[3] -= 1;
                            }
                        }else{
                            timer[1] = 9;
                            timer[2] -= 1;
                        }
                    }else{
                        timer[0] = 9;
                        timer[1] -= 1;
                    }
                }else{
                    timer[0] -= 1;
                }
            }
        }
        PIR2bits.TMR3IF = 0;
    }
    if(PIR1bits.TMR1IF){
        WriteTimer1(53036);
        display_driver();
        PIR1bits.TMR1IF = 0;
    }
}

////////////////////////////////////////////////////////////////////////
/////////////////////////////DISPLAY MODES//////////////////////////////
////////////////////////////////////////////////////////////////////////

void set_brightness(void){
    int brightness_select = brightness;
    while(enable){
        if(check_alarm){
            alarm_active();
        }
        int pot = 255 - get_ADC_value();
        pot = pot/2.55;
        brightness = pot;
        U1_value = 17;
        U2_value = 17;
        if(PB1_pressed()){
            brightness_select = brightness;
        }
    }
    brightness = brightness_select;
}

void timer_function(void){
    while(enable){
        if(check_alarm){
            alarm_active();
        }
        for(int i = 0; i < 3;i++){
            timer[i] = 0;
        }
        int display_value = 0;
        char display_value_ten = 0;
        int start_timer = 0;
        while(!start_timer && enable){
            switch (read_switches()){
                case 1:
                    set_timer_second();
                    break;
                case 2:
                    set_timer_minute();
                    break;
                default:
                    U1_value = 16;
                    U2_value = 16;
                    if(PB1_pressed() && (set_sec || set_min)){
                        start_timer = 1;
                    }
                    break;
            }
        }
        StartTimer3();
        while(enable){
            char finished = 1;
            for(int i = 0; i < 3;i++){
                if (!(timer[i] == 0)){
                    finished = 0;
                    break;
                }
            }
            if(read_switches()  == 1){
                display_value = timer[0];
                display_value_ten = timer[1];
            }else if(read_switches()  == 2){
                display_value = timer[2];
                display_value_ten = timer[3];
            }else{
                display_value = 16;
                display_value_ten = 16;
            }
            U1_value = display_value;
            U2_value = display_value_ten;
            if(finished){
                CloseTimer3();
                alarm_active();
                enable = 0;
            }
        }
        CloseTimer3();
    }
    CloseTimer3();
}

void stopwatch(void){
    while(enable){
        if(check_alarm){
            alarm_active();
        }
        char i = 1;
        U1_value = 0;
        U2_value = 0;
        if(PB1_pressed()){
            display_milsecond_stopwatch = 1;
            counter3 = 0;
            StartTimer3();
            while(i && enable){
                if(PB1_pressed()){
                    CloseTimer3();
                    while(i && enable){
                        if(PB1_pressed()){
                            display_milsecond_stopwatch = 0;
                            i=0;
                        }
                    }
                }
            }
        }
    }
    CloseTimer3();
    display_milsecond_stopwatch = 0;
}

void set_alarm(void){
    while(enable){
        if(check_alarm){
            alarm_active();
        }
        switch (read_switches()){
            case 1:
                set_alarm_second();
                break;
            case 2:
                set_alarm_minute();
                break;
            case 4:
                set_alarm_hour();
                break;
            default:
                U1_value = 16;
                U2_value = 16;
                break;  
        }
      
    }
}
void set_time(){
    while(enable){
        if(check_alarm){
            alarm_active();
        }
        switch (read_switches()){
            case 1:
                set_second();
                break;
            case 2:
                set_minute();
                break;
            case 4:
                set_hour();
                break;
            case 8:
                set_day();
                break;
            case 16:
                set_month();
                break;
            case 32:
                set_year();
                break;
            default:
                U1_value = 16;
                U2_value = 16;
                break;
        }
    }
}
void display_mode(void){
    int display_value;
    int display_value_ten;
    while(enable){
        if(check_alarm){
            alarm_active();
        }
        switch (read_switches()){
            case 1:
                display_milsecond = 1;
                display_value = time[0];
                display_value_ten = time[1];
                break;
            case 2:
                display_milsecond = 1;
                display_value = time[2];
                display_value_ten = time[3];
                break;
            case 4:
                display_milsecond = 1;
                display_value = time[4];
                display_value_ten = time[5];
                break;
            case 8:
                display_milsecond = 1;
                display_value = time[6];
                display_value_ten = time[7];
                break;
            case 16:
                display_milsecond = 1;
                display_value = time[8];
                display_value_ten = time[9];
                break;
            case 32:
                display_milsecond = 1;
                display_value = time[10];
                display_value_ten = time[11];
                break;
            default:
                display_milsecond = 0;
                display_value = 16;
                display_value_ten = 16;
                break; 
        }
        U1_value = display_value;
        U2_value = display_value_ten;
    }
    display_milsecond = 0;
}
void main() {
    config_IO();
    StartTimer0();
    StartTimer1();
    configure_ADC();
    configure_PB2_interrupt();
    enable_global_interrupts();
    while (1){
        if(check_alarm){
            alarm_active();
        }
        if(PB1_pressed()){
            if(select == 5){
                select = 0;
            }else{
                select += 1;
            }
        }
        switch (select){
            case 0:
                U1_value = 18;
                U2_value = 13;
                if(enable == 1){
                    display_mode();
                }
                break;
            case 1:
                U1_value = 18;
                U2_value = 5;
                if(enable == 1){
                    set_time();
                }
                break;
            case 2:
                U1_value = 11;
                U2_value = 5;
                if(enable == 1){
                    set_brightness();
                }
                break;
            case 3:
                U1_value = 1;
                U2_value = 18;
                if(enable == 1){
                    timer_function();
                }
                break;
            case 4:
                U1_value = 12;
                U2_value = 5;
                if(enable == 1){
                    stopwatch();
                }
                break;
            case 5:
                U1_value = 10;
                U2_value = 5;
                if(enable == 1){
                    set_alarm();
                }
                break;
        }
    }
}