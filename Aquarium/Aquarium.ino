#include "Aquarium.h"
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Wire.h>
#include <TimeLib.h>
#include <DS1307RTC.h>
#include <SoftwareSerial.h>   

#define speed8266         9600
#define TEMP_SENSOR_BUS   (2)
#define RELAY_LAMP        (12)
#define LED               (13)
#define LAMP_ON_HOUR      (9)
#define LAMP_OFF_HOUR     (21)
#define timeBetweenWrites (30)
#define timeBetweenReads  (5)
#define timeBetweenCtl    (5)

struct Aquariumdata {
  boolean b_Filter;
  boolean b_Heater;
  boolean b_Lamp;
  boolean b_Aerator;
  float   f_Temperature;
};

struct AquariumControl {
  boolean b_Filter;
  boolean b_Heater;
  boolean b_Lamp;
  boolean b_Aerator;
  String  s_FilterField;
  String  s_HeaterField;
  String  s_LampField;
  String  s_AeratorField;
};

/* DECLARATIONS */
// Data
Aquariumdata    Aquarium    = {false, false, false, false, 25};
AquariumControl AquaControl = {false, false, false, false, "1", "2", "3", "4"};
int     i_Test = 0;
long    startCtlTime     = 0;
long    elapsedCtlTime   = 0;

SoftwareSerial  esp8266(6,7);  //Rx ==> Pin 6; TX ==> Pin7 

// Thingspeak
String  statusChWriteKey  = STATUS_CH_WRITE_KEY;
String  controlChID       = CONTROL_CH_ID;
long    startWriteTime    = 0;
long    elapsedWriteTime  = 0;
long    startReadTime     = 0;
long    elapsedReadTime   = 0;

boolean b_Error = false;

// Time
tmElements_t  tm_Time;

// Temperature
OneWire oneWire(TEMP_SENSOR_BUS);
DallasTemperature TempSensor(&oneWire);

void setup() 
{
  pinMode(RELAY_LAMP, OUTPUT);
  
  Serial.begin(speed8266);
  
  esp8266.begin (speed8266); 
  
  Serial.println("Aquarium Controller");

  RTC.read(tm_Time);

  TempSensor.begin();

  startWriteTime  = millis();
  startReadTime   = millis();
  startCtlTime    = millis();
}

void loop() 
{
  elapsedWriteTime  = millis() - startWriteTime;
  elapsedReadTime   = millis() - startReadTime;
  elapsedCtlTime    = millis() - startCtlTime;

  // Send data to ThingSpeak
  if (elapsedWriteTime > (timeBetweenWrites*1000))
  {
    SendDataToThingSpeak();
    startWriteTime = millis();
  }

  // Get data from ThingSpeak
  if (elapsedReadTime > (timeBetweenReads*1000))
  {
    AquaControl.b_Filter = ReceiveDataFromThingSpeak(AquaControl.s_FilterField);
    AquaControl.b_Heater = ReceiveDataFromThingSpeak(AquaControl.s_HeaterField);
    AquaControl.b_Lamp = ReceiveDataFromThingSpeak(AquaControl.s_LampField);
    AquaControl.b_Aerator = ReceiveDataFromThingSpeak(AquaControl.s_AeratorField);
    startReadTime   = millis();
  }

  // Control loop
  if (elapsedCtlTime > (timeBetweenCtl*1000))
  {
    LampTimeControl();
    ActuatorControl();
    TempSensor.requestTemperatures();
    Aquarium.f_Temperature = TempSensor.getTempCByIndex(0);
    Serial.print("Temperature: ");
    Serial.println(Aquarium.f_Temperature);
    startCtlTime    = millis();
  }
  
  if (b_Error == true)
  {
    Serial.println(" <<<< ERROR >>>>");
    delay (2000);
    b_Error = false;
  }
}

void LampTimeControl(void)
{
  RTC.read(tm_Time);
  if ((tm_Time.Hour >= LAMP_ON_HOUR) && (tm_Time.Hour < LAMP_OFF_HOUR)) {
    Serial.println("LampTimeControl --> Turn ON the lights!");
    Aquarium.b_Lamp = true;
  }
  else {
    Serial.println("LampTimeControl --> Turn OFF the lights");
    Aquarium.b_Lamp = false;
  }  
}

void ActuatorControl(void)
{
  if (AquaControl.b_Lamp == true) {
    digitalWrite(RELAY_LAMP, !Aquarium.b_Lamp);
    digitalWrite(LED, Aquarium.b_Lamp);
  }
  else {
    digitalWrite(RELAY_LAMP, HIGH);
    digitalWrite(LED, LOW);
  }
}

void SendDataToThingSpeak(void)
{
  esp8266.flush();  // Clean buffer

  String cmd = "AT+CIPSTART=\"TCP\",\"";
  cmd += "184.106.153.149"; // IP for api.thingspeak.com
  cmd += "\",80";
  esp8266.println(cmd);
  Serial.print("SENT ==> Start cmd: ");
  Serial.println(cmd);
  
  if(esp8266.find("Error"))
  {
    Serial.println("AT+CIPSTART error");
    return;
  }

  // Prepare data
  String getStr = "GET /update?api_key=";
  getStr += statusChWriteKey;
  getStr +="&field1=";
  getStr += String(Aquarium.b_Filter);
  getStr +="&field2=";
  getStr += String(Aquarium.b_Heater);
  getStr +="&field3=";
  getStr += String(Aquarium.b_Lamp);
  getStr +="&field4=";
  getStr += String(Aquarium.b_Aerator);
  getStr +="&field5=";
  getStr += String(Aquarium.f_Temperature);
  getStr +="&field6=";
  getStr += String(i_Test);
  getStr += "\r\n\r\n";

  cmd = "AT+CIPSEND=";
  cmd += String(getStr.length());
  esp8266.println(cmd);
  Serial.print("SENT ==> lenght cmd: ");
  Serial.println(cmd);

  if(esp8266.find((char *)">"))
  {
    esp8266.print(getStr);
    Serial.print("SENT ==> getStr: ");
    Serial.println(getStr);
    delay(500);

    String messageBody = "";
    while (esp8266.available()) 
    {
      String line = esp8266.readStringUntil('\n');
      if (line.length() == 1) 
      { //actual content starts after empty line (that has length 1)
        messageBody = esp8266.readStringUntil('\n');
      }
    }
    Serial.print("MessageBody received: ");
    Serial.println(messageBody);
  }
  else
  {
    esp8266.println("AT+CIPCLOSE");     // alert user
    Serial.println("ESP8266 CIPSEND ERROR: RESENDING"); //Resend...
    i_Test = i_Test + 1;
    b_Error = true;
  } 
}

int ReceiveDataFromThingSpeak(String fieldID)
{
  int i_Command = 0;
  esp8266.flush();  // Clean buffer

  String cmd = "AT+CIPSTART=\"TCP\",\"";
  cmd += "184.106.153.149"; // IP for api.thingspeak.com
  cmd += "\",80";
  esp8266.println(cmd);
  Serial.print("RECV ==> Start cmd: ");
  Serial.println(cmd);
  
  if(esp8266.find("Error"))
  {
    Serial.println("AT+CIPSTART error");
    b_Error = true;
    return;
  }

  // Prepare data
//  String getStr = "GET /channels/";
//  getStr += controlChID;
//  getStr +="/fields/";
//  getStr += fieldID;
//  getStr +=".json?results=1";
//  getStr += "\r\n";
  String getStr = "GET /channels/";
  getStr += controlChID;
  getStr +="/fields/";
  getStr +=fieldID;
  getStr +="/last";
  getStr += "\r\n";

  cmd = "AT+CIPSEND=";
  cmd += String(getStr.length());
  esp8266.println(cmd);
  Serial.print("RECV ==> lenght cmd: ");
  Serial.println(cmd);

  String messageBody = "";
  if(esp8266.find((char *)">"))
  {
    esp8266.print(getStr);
    Serial.print("RECV ==> getStr: ");
    Serial.println(getStr);
    delay(500);

    while (esp8266.available()) 
    {
      String line = esp8266.readStringUntil('\n');
      if (line.length() == 1) 
      { //actual content starts after empty line (that has length 1)
        messageBody = esp8266.readStringUntil('\n');
      }
    }
    Serial.print("RECV ==> MessageBody received: ");
    Serial.println(messageBody);
  }
  else
  {
    esp8266.println("AT+CIPCLOSE");     // alert user
    Serial.println("ESP8266 CIPSEND ERROR: RESENDING"); //Resend...
    i_Test = i_Test + 1;
    b_Error = true;
  }
  
  if (messageBody[5] == 49)
  {
    i_Command = messageBody[7]-48; 
    Serial.print("Command received: ");
    Serial.println(i_Command);
  }
  else {
    i_Command = 9;
    Serial.print("Command not received: ");
    Serial.println(i_Command);    
  }

  Serial.println();
  return i_Command;
}

