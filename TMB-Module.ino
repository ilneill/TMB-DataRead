/* Read and Decode Serial data from an MB-TMB880CF-1 Temperature Module.
 *  Written for the Arduino Mega 2560 - affects the Analogue Comparator pins.
 * (c) Ian Neill 2024
 * GPL(v3) Licence
 *
 * References:
 *  https://github.com/leomil72/analogComp/tree/master
 *  https://microcontrollerslab.com/arduino-comparator-tutorial/
 *  https://www.austinblanco.com/blog/arduino-mega-2560-comparator/
 *  https://www.ee-diary.com/2021/07/arduino-analog-comparator-with-interrupt.html
 *  https://www.utopiamechanicus.com/article/handling-arduino-microsecond-overflow/
 */

#include <analogComp.h>

#define TMBCLKPIN  5    // A-Comp AIN1 pin - Connected to TMB module pin 10.
#define TMBDATAPIN A5   // Data IN, 1.5v p-p - Connected to TMB module pin 9.
#define BUILTINLED 13
#define CLKOUTPIN  10   // Recreated clock output for oscilloscope inspection.
#define DATAOUTPIN 9    // Recreated data output for oscilloscope inspection.

volatile bool tmbNewPulse = false;
volatile unsigned long tmbPulseWidth = 0;

void setup() {
  pinMode(BUILTINLED, OUTPUT);
  digitalWrite(BUILTINLED, LOW); // Stray capacitance seems to turn this LED on!

  pinMode(TMBCLKPIN,  INPUT);
  pinMode(TMBDATAPIN, INPUT);
  pinMode(CLKOUTPIN,  OUTPUT);
  pinMode(DATAOUTPIN, OUTPUT);

  analogComparator.setOn(INTERNAL_REFERENCE, AIN1); // Tell the library to use AIN0=1.1v (int ref) and the AIN1 pin (5).
  analogComparator.enableInterrupt(tmbClockPulse, CHANGE); // Set ISR and when it has to be called.

  Serial.begin (115200);
}

void loop() {
  static byte bitCounter = 13;
  static bool tmbSignBit = 0;
  static byte tmbD02D09  = 0;
  static byte tmbD10D13  = 0;
  float  tmbTemperature  = 0;

  if(tmbNewPulse) {
    tmbNewPulse = false;
    if(tmbPulseWidth > 250) {
      bitCounter = 0;
      tmbSignBit = tmbDataRead();
      Serial.print("Pulse Widths: ");
      Serial.print(tmbPulseWidth);
    }
    else {
      if(bitCounter != 13) {
        bitCounter += 1;
      }
      if(bitCounter < 13) {
        if(bitCounter >= 1 && bitCounter <= 8) {
          bitWrite(tmbD02D09, (8 - bitCounter), tmbDataRead());
        }
        if(bitCounter >= 9 && bitCounter <= 12) {
          bitWrite(tmbD10D13, (16 - bitCounter), tmbDataRead());
        }
        Serial.print(" ");
        Serial.print(tmbPulseWidth);
      }
    }
    if(bitCounter == 12){
      Serial.println();
      // Convert the temperature data from BCD.
      tmbTemperature = ((tmbD02D09 >> 4) * 10) + (tmbD02D09 & 0x0f) + (float(tmbD10D13 >> 4) / 10);
      if(tmbSignBit) {
        tmbTemperature *= -1; // Correct the calculated temperature according to the sign bit.
      }
      Serial.print(" Temperature = ");
      Serial.print(tmbTemperature, 1);
      Serial.println("°C");
      // Read the TMB Data again to finish the last bit in the stream. 
      delay(1);
      tmbDataRead();
    }
  }
}

void tmbClockPulse() {
  bool alogCompOP = !bitRead(ACSR, ACO);  // ACO low indicates a signal rising edge.
  static unsigned long tmbPulseStart = 0; // Used in the pulse width calculation.
  digitalWrite(CLKOUTPIN, alogCompOP);    // Output for oscilloscope inspection.
  if(alogCompOP) {
    tmbPulseStart = micros();
  }
  else {
    tmbPulseWidth = micros() - tmbPulseStart;
    tmbNewPulse = true;
  }
}

bool tmbDataRead() {
  int tmbData = analogRead(TMBDATAPIN);
  if(tmbData > 200) {
    digitalWrite(DATAOUTPIN, HIGH);       // Output for oscilloscope inspection.
    return(HIGH);
  }
    digitalWrite(DATAOUTPIN, LOW);        // Output for oscilloscope inspection.
    return(LOW);
}

//EOF