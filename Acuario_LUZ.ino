#include <Wire.h>
#include <TimeLib.h>
#include <DS1307RTC.h>
#define RELAY 12
#define LED   13
#define TURN_ON_HOUR  (9)
#define TURN_OFF_HOUR (21)

void setup() {
  // put your setup code here, to run once:
  pinMode(RELAY, OUTPUT);
  Serial.begin(9600);
  while (!Serial) ; // wait for serial
  delay(200);
  Serial.println("Aquarium Control");
  Serial.println("-------------------");  
}

void loop() {
  // put your main code here, to run repeatedly:
  tmElements_t tm;

  
  if (RTC.read(tm)) {
    print2digits(tm.Hour);
    Serial.write(':');
    print2digits(tm.Minute);
    Serial.write(':');
    print2digits(tm.Second);
    if ((tm.Hour >= TURN_ON_HOUR) && (tm.Hour < TURN_OFF_HOUR)) {
      Serial.println(" --> Turn ON the lights!");
      digitalWrite(RELAY, LOW);
    }
    else {
      Serial.println(" --> Turn OFF the lights");
      digitalWrite(RELAY, HIGH);
    }
  }
  else {
    Serial.println("ERROR! Couldn't get hour from RTC");
  }

  delay(1000);
}

void print2digits(int number) {
  if (number >= 0 && number < 10) {
    Serial.write('0');
  }
  Serial.print(number);
}
