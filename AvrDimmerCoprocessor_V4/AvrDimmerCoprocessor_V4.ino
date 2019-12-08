// (Voltage controlled) dimmer with ATtiny45 (or ATtiny85)
//
// This code is inspired by ESP forum article:
// https://forum.arduino.cc/index.php?topic=314773.0
// Apr 09, 2015, 09:32 pm , by ArthurRep 
//
// See also AvrDimmerCoprocessor_V2A, V2B, and V3 for the
// iteration steps which got me to this point.
//
// Changes; 
// - Free up pin 7 for I2C usage.
//   Done using info from:  https://thewanderingengineer.com/2014/08/11/pin-change-interrupts-on-attiny85/
// - Remove analog input, and use Wire.h to get I2C input.
//   Configure as slave on I2C address 8.
//   Connect SCK to pin 7 and SDA to pin 5.
// - If brightness 0 or 100, keep triac pulse off or on (no phase cutting)
// - Changed the way the pulse is generated and limited (not using overflow, but comparator B).
// - Keep counter running, to add a zero cross interrupt backoff mechanism (attempt to fix occasional light flashes)
// - Add pull-up resistor (10K) between pin 1 and 8 to keep reset high (to prevent accidental reset)
// - Use higher resolution (divide clock by 512 intstead of 1024).
// - Changed interrupt detection from falling edge to rising edge (my detector has HIGH zc detect value)
//
// Note: if this version still suffers from the occasional light flicker (wrong zero crossing interrupts), then
// perhaps the next version should be an attempt at creating a phase-locked-loop. That will be quite a challenge...
// Possibly the zero detector hardware needs to be looked at (additional pull-up resitor perhaps?).
// Or... maybe we need to look at the pulse width comming from the zero-crossing detector. It might be that
// short spikes on the 240V AC will be detected also. My guess is that a real zero crossing will have a bit wider
// pulse than a random spike.
//
// Thijs Kaper, 3 November 2019.
//
//
// This arduino sketch uses a zero crossing detect circuit and an opto-isolated triac.
// Brightness setting is controlled via I2C data input (I2C master is for example an ESP8266).
//
// AC Phase control is accomplished using the internal hardware timer1 in the ATtiny45.
//
// Timing Sequence
// * timer is set up and keeps running
// * zero crossing detected
// * timer reset to zero (but only if a certain time has elapsed to block accidental trigger pulses)
// * comparator set to "delay to on" value
// * counter reaches comparator value
// * comparator ISR turns on triac gate
// * second comparator set to "delay + pulse width"
// * counter reaches second comparator
// * comparator ISR turns off triac gate
// * triac stops conducting at next zero cross

// The hardware timer runs at 8MHz.
// A half period of a 50Hz AC signal takes 10 ms is 80000 counts.
// Prescaler set to 512 gives 156 (and a quarter) counts per half period (was 78 with prescale 1024)

#include <avr/io.h>
#include <avr/interrupt.h>
#include <Wire.h>

#define DETECT PB1 // 1 //zero cross detect, pcint1, is physical pin 6
#define GATE 3        //triac gate is physical pin 2

#define PULSE 4        //trigger pulse width (counts) (was 2 with 1024 prescaler)
#define BACKOFFCNT 150 // minimum time to next zero crossing, was 75 with 1024 prescaler (expect zc around 156)
#define MAXCNT 145     // max modulated width (bright). was 65 with 1024 prescaler
#define MINCNT 4       // min modulated width (dimmed). was 2 with 1024 prescaler

int brightness = 0;
int valid_zc = 0;

void setup(){
  // set up pins
  pinMode(DETECT, INPUT);      //zero cross detect
  digitalWrite(DETECT, HIGH);  //enable pull-up resistor
  pinMode(GATE, OUTPUT);       //triac gate control

  // set up Timer1
  TCCR1 = 0;                   // stop timer
  OCR1A = MAXCNT;              // initialize the comparator (A) - start of pulse
  OCR1B = OCR1A + PULSE;       // initialize the comparator (B) - end of pulse
  TIMSK = _BV(OCIE1A) | _BV(OCIE1B);  //interrupt on Compare Match A + B
  TCNT1 = 0;                   // reset timer - count from zero
  TCCR1 = 0b00001010;          // prescaler on 512, see table 12.5 of the tiny85 datasheet (starts counter)

  GIMSK = 0b00100000;    // turns on pin change interrupts
  PCMSK = 0b00000010;    // turn on interrupts on PCINT1 = pin 6 = PB1
  
  sei();  // enable interrupts

  Wire.begin(8);                // join i2c bus with address #8, Connect SCK to pin 7 and SDA to pin 5.
  Wire.onReceive(receiveI2CEvent); // register event
} 

// function that executes whenever I2C data is received from master
void receiveI2CEvent(int howMany) {
  // we expect only one byte, but just in case, loop through all to get rid of any buffered data
  while (Wire.available()) {
    brightness = Wire.read();    // receive byte as an integer
    
    // translate input range zero to 100 into brightness (phase start delay MAXCNT to MINCNT)
    // And put it in the timer comparator registers.
    OCR1A = map(brightness, 0, 100, MAXCNT, MINCNT); // start pulse
    OCR1B = OCR1A + PULSE;                           // end pulse
  }
}

// Pin change interrupt
ISR(PCINT0_vect) {
  // pin change is both rising and falling, so check for HIGH as that would be the wanted rising edge interrupt
  // The value of BACKOFFCNT check, is to ignore any random interrupts which come in earlier than expected (expected around 156)
  if (digitalRead(DETECT) == HIGH && TCNT1 > BACKOFFCNT) {
    TCNT1 = 0;   // reset timer - count from zero
    valid_zc = 1; // mark this as a valid zero crossing
  }
}

// Enable triac
ISR(TIMER1_COMPA_vect){    //comparator match
  if (brightness != 0 && valid_zc == 1) {
    digitalWrite(GATE,HIGH); //set triac gate to high
  }
  valid_zc = 0;
}

// Disable triac
ISR(TIMER1_COMPB_vect){    //comparator match
  if (digitalRead(GATE) == HIGH && brightness != 100) {
    digitalWrite(GATE,LOW);   //turn off triac gate
  }
}

// main loop - do nothing... (all is interrupt driven)
void loop(){     
   delay(100); // NO-OPERATION - sleep
}
