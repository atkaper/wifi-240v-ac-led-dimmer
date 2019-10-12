// This example taken from an ESP forum:
// https://forum.arduino.cc/index.php?topic=314773.0
// Apr 09, 2015, 09:32 pm , by ArthurRep 
//
// In my case, programmed in an ATTiny45 (is same as 85, just a bit less memory).
// A 10K potmeter center pin connected to pin 3, with it's other pins on ground and vcc.
// This example worked like a charm!
//
// Thijs Kaper, October 2019.


// Voltage controlled dimmer with ATtiny85
//
// This arduino sketch includes a zero
// crossing detect function and an opto-isolated triac.
//
// AC Phase control is accomplished using the internal
// hardware timer1 in the ATtiny85
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

#define DETECT 2      //zero cross detect, interrupt 0, is physical pin 7
#define GATE 3        //triac gate is physical pin 2
#define PULSE 2       //trigger pulse width (counts)
#define INSTELPIN 2   // =A2 (digital pin4) is physical pin 3

void setup(){
  // set up pins
  pinMode(DETECT, INPUT);      //zero cross detect
  digitalWrite(DETECT, HIGH);  //enable pull-up resistor
  pinMode(GATE, OUTPUT);       //triac gate control

  // set up Timer1
  TCCR1 = 0;     // stop timer
  OCR1A = 50;    //initialize the comparator
  TIMSK = _BV(OCIE1A) | _BV(TOIE1);  //interrupt on Compare Match A | enable timer overflow interrupt
  sei();  // enable interrupts
  // set up zero crossing interrupt
  attachInterrupt(0,zeroCrossingInterrupt, FALLING);   
} 

//Interrupt Service Routines
void zeroCrossingInterrupt(){   
  TCNT1 = 0;   //reset timer - count from zero
  TCCR1 = B00001011;        // prescaler on 1024, see table 12.5 of the tiny85 datasheet
}

ISR(TIMER1_COMPA_vect){    //comparator match
  digitalWrite(GATE,HIGH); //set triac gate to high
  TCNT1 = 255-PULSE;       //trigger pulse width, when TCNT1=255 timer1 overflows
}
 
ISR(TIMER1_OVF_vect){       //timer1 overflow
  digitalWrite(GATE,LOW);   //turn off triac gate
  TCCR1 = 0;                //disable timer stop unintended triggers
}

void loop(){     // use analog input to set the dimmer
int instelwaarde = analogRead(INSTELPIN);
OCR1A = map(instelwaarde, 0, 1023, 65, 2);
}
