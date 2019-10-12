// This example is inspired by ESP forum article:
// https://forum.arduino.cc/index.php?topic=314773.0
// Apr 09, 2015, 09:32 pm , by ArthurRep 
//
// See also AvrDimmerCoprocessor_V2A and AvrDimmerCoprocessor_V2B for the
// iteration steps which got me to this point.
//
// Changes; 
// - free up pin 7 for I2C usage.
//   done using info from:  https://thewanderingengineer.com/2014/08/11/pin-change-interrupts-on-attiny85/
// - remove analog input, and use Wire.h to get I2C input.
//   Configure as slave on I2C address 8.
//   Connect SCK to pin 7 and SDA to pin 5.
//
// And again, this works fine again. Nice!
//
// Thijs Kaper, October 2019.


// (Voltage controlled) dimmer with ATtiny45
//
// This arduino sketch includes a zero
// crossing detect function and an opto-isolated triac.
//
// AC Phase control is accomplished using the internal
// hardware timer1 in the ATtiny45
//
// Timing Sequence
// * timer is set up but disabled
// * zero crossing detected
// * timer starts counting from zero
// * comparator set to "delay to on" value
// * counter reaches comparator value
// * comparator ISR turns on triac gate
// * counter set to overflow - pulse width
// * counter reaches overflow
// * overflow ISR turns off triac gate
// * triac stops conducting at next zero cross

// The hardware timer runs at 8MHz.
// A half period of a 50Hz AC signal takes 10 ms is 80000 counts.
// Prescaler set to 1024 gives 78 counts per half period

#include <avr/io.h>
#include <avr/interrupt.h>
#include <Wire.h>

#define DETECT PB1 // 1 //zero cross detect, pcint1, is physical pin 6
#define GATE 3        //triac gate is physical pin 2
#define PULSE 2       //trigger pulse width (counts)

void setup(){
  // set up pins
  pinMode(DETECT, INPUT);      //zero cross detect
  digitalWrite(DETECT, HIGH);  //enable pull-up resistor
  pinMode(GATE, OUTPUT);       //triac gate control

  // set up Timer1
  TCCR1 = 0;     // stop timer
  OCR1A = 50;    //initialize the comparator
  TIMSK = _BV(OCIE1A) | _BV(TOIE1);  //interrupt on Compare Match A | enable timer overflow interrupt

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
    int x = Wire.read();    // receive byte as an integer
    
    // translate input range zero to 100 into brightness (phase start delay 65 to 2)
    // And put it in the timer comparator register.
    OCR1A = map(x, 0, 100, 65, 2);
  }
}

// Pin change interrupt
ISR(PCINT0_vect) {
  // pin change is both rising and falling, so check for LOW as that would be the wanted falling edge interrupt
  if (digitalRead(DETECT) == LOW) {
    TCNT1 = 0;   // reset timer - count from zero
    TCCR1 = B00001011;        // prescaler on 1024, see table 12.5 of the tiny85 datasheet
  }
}

ISR(TIMER1_COMPA_vect){    //comparator match
  digitalWrite(GATE,HIGH); //set triac gate to high
  TCNT1 = 255-PULSE;       //trigger pulse width, when TCNT1=255 timer1 overflows
}
 
ISR(TIMER1_OVF_vect){       //timer1 overflow
  digitalWrite(GATE,LOW);   //turn off triac gate
  TCCR1 = 0;                //disable timer stop unintended triggers
}

void loop(){     
   delay(100); // NO-OPERATION - sleep
}
