#include <TimerOne.h>
#include <LiquidCrystal.h>
#include <Adafruit_MAX31855.h>
#include <SPI.h>

#define OVERHEAT 150
#define NEEDED_T 130
#define HYST_GAP 2   //start heating if actual T is less than (NEEDED_T - HYST_GAP)
#define WARMING_PERIOD 600000 // 10 minutes = 600 seconds = 600000 millis
#define FINAL_WARMING_PERIOD 30000 // 30 seconds 

enum state_t
{
  HEATING,
  WARMING,
  FINISHED,
  EMERGENCY
};
      
volatile state_t state           = HEATING;
volatile bool    heating_enabled = false;
volatile bool    heaters_on      = false;
volatile double  celsius_1; // thermocouple 1 data
volatile double  celsius_2; // thermocouple 2 data

#define LCD_D4 8
#define LCD_D5 7
#define LCD_D6 6
#define LCD_D7 5
#define LCD_E  12
#define LCD_RS 13

#define t1_CLK 3 // SPI clock line
#define t1_CS  2 // thermocouple1 selection line
#define t1_DO  4 // data output line

#define t2_CLK 3 // SPI clock line
#define t2_CS  2 // thermocouple2 selection line (!!! check and fix)
#define t2_DO  4 // data output line

#define BUZZER    11 // buzzer line (!!! check and fix)
#define RED_LED   A2 // LED2 meant 
#define GREEN_LED 10 // LED1 meant
#define HEATER    9  // power relay switch

// Initialize the Thermocouple
Adafruit_MAX31855 thermocouple1( t1_CLK, t1_CS, t1_DO );
Adafruit_MAX31855 thermocouple2( t2_CLK, t2_CS, t2_DO );

// initialize the library with the numbers of the interface pins
LiquidCrystal lcd( LCD_RS, LCD_E, LCD_D4, LCD_D5, LCD_D6, LCD_D7 );

void manage_heaters()
{
  celsius_1 = thermocouple1.readCelsius();  
  celsius_2 = thermocouple1.readCelsius();

  if (( celsius_1 > OVERHEAT )
      || (celsius_2 > OVERHEAT))
  {
    digitalWrite( HEATER, LOW ); // heaters OFF firstly
    heaters_on      = false;
    heating_enabled = false;
    state           = EMERGENCY;
    return;
  }
  
  if ( !heating_enabled
      || ((( celsius_1 > NEEDED_T )
         || (celsius_2 > NEEDED_T))
         && heaters_on ))
  {
    digitalWrite( HEATER, LOW ); // heaters OFF 
    heaters_on = false;    
    return;
  }
  
  if ( heating_enabled
     && !heaters_on
     && ( celsius_1 < ( NEEDED_T - HYST_GAP ))
     && ( celsius_2 < ( NEEDED_T - HYST_GAP )))
  {
    digitalWrite( HEATER, HIGH ); // heaters ON
    heaters_on = true;
  }
}
  
void setup()
{
  Serial.begin( 9600 );
  // set up the LCD's number of columns and rows: 
  lcd.begin( 16, 2 );
  lcd.clear();
  lcd.setCursor( 0, 0 );
  lcd.print( "Please wait." );
  
  pinMode(RED_LED,   OUTPUT);
  pinMode(GREEN_LED, OUTPUT);
  pinMode(HEATER,    OUTPUT);
  
  state = HEATING;
  heating_enabled = true;
  heaters_on = true;
  digitalWrite( GREEN_LED, HIGH ); // green led ON
  
  Timer1.initialize(100000); // 100000 microseconds = 0.1 seconds
  Timer1.attachInterrupt(manage_heaters); // check heaters every 0.1 seconds
  
  digitalWrite( BUZZER, HIGH ); // make a beep
  // wait for MAX chip to stabilize
  delay( 500 );
  digitalWrite( BUZZER, LOW );
}

int i;
int points = 0;
double tmp1, tmp2;
unsigned long started, time;
int minutes, seconds;

void show_temperatures()
{
    lcd.setCursor( 0, 1 );
    if ( isnan( tmp1 ) ) 
    {
      lcd.print( "fault" );
    } 
    else 
    {
      lcd.print( tmp1 );
    }
    
    lcd.setCursor( 8, 1 );
    if ( isnan( tmp2 ) ) 
    {
      lcd.print( "fault" );
    } 
    else 
    {
      lcd.print( tmp2 );
    }
}

void loop() {

  switch (state)
  {
  case HEATING:
    lcd.clear();
    lcd.setCursor( 0, 0 );
    lcd.print( "Heating" );
    for ( i = 0; i <= points; i++)
    {
      lcd.print( "." );
    }
    points > 1 ? points = 0 : points++; // switch points counter 0 -> 1 -> 2 -> 0
    
    noInterrupts();
    tmp1 = celsius_1;
    tmp2 = celsius_2;
    interrupts();

    show_temperatures();
    
    if (( tmp1 >= ( NEEDED_T - HYST_GAP ))
       || (tmp2 >= ( NEEDED_T - HYST_GAP )))
    {
      noInterrupts();
      state = WARMING;
      interrupts();
      digitalWrite( BUZZER, HIGH ); // make a beep
      started = millis();
    }
    break;
    
  case WARMING:
    time = millis() - started;
    
    minutes = (time / 1000) / 60;
    seconds = (time / 1000) % 60;
  
    lcd.clear();
    lcd.setCursor( 0, 0 );
    lcd.print( minutes );
    lcd.print( ":" );
    lcd.print( seconds );
    lcd.print( " Warming" );
    for ( i = 0; i <= points; i++)
    {
      lcd.print( "." );
    }
    points > 1 ? points = 0 : points++; // switch points counter 0 -> 1 -> 2 -> 0
    
    noInterrupts();
    tmp1 = celsius_1;
    tmp2 = celsius_2;
    interrupts();
    
    show_temperatures();
    
    if (time > WARMING_PERIOD)
    {
      noInterrupts();
      state = FINISHED;
      interrupts();
      digitalWrite( BUZZER, HIGH ); // make a beep
      started = millis();
    }
    break;
    
  case FINISHED:
    time = millis() - started;
    points > 0 ? points = 0 : points = 1; // switch points counter 0 <-> 1
    lcd.clear();
    lcd.setCursor( 0, 0 );
    if (points == 0)
      lcd.print( "Finished" );
    else
      lcd.print( "Switch OFF" );
  
    show_temperatures();
    
    if (time > FINAL_WARMING_PERIOD)
    {
      noInterrupts();
      heating_enabled = false;
      interrupts();
    }    
    break;
    
  case EMERGENCY:
    digitalWrite( RED_LED, HIGH ); // alarm light
    digitalWrite( GREEN_LED, LOW ); // alarm light
    points > 0 ? points = 0 : points = 1; // switch points counter 0 <-> 1
    lcd.clear();
    lcd.setCursor( 0, 0 );
    if (points == 0)
    {
      lcd.print( "OVERHEATED" );
      digitalWrite( BUZZER, HIGH ); // make a beep      
    }
    else
      lcd.print( "Switch OFF" );
    break;
  }
  
  delay( 500 ); //interface reaction period 0.5 sec
  digitalWrite( BUZZER, LOW ); // silence buzzer
}
