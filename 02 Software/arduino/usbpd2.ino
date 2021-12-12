/*
 * 程序参考自：
 * http://bbs.mydigit.cn/read.php?tid=2595019
 * 
 */

#include <Wire.h>
#include <Bounce2.h>
#include <INA.h>
#include "udp_debug.h"
#include "fusb302.h"


#define BUTTON_LEFT  34
#define BUTTON_ENTER  35
#define BUTTON_RIGHT  33

INA_Class         INA; 
Bounce enter_debouncer = Bounce();
bool enter_key_value = HIGH;
bool enter_key_old_value = HIGH;



uint8_t request_index=0;

volatile uint8_t  deviceNumber    = UINT8_MAX;  ///< Device Number to use in example
volatile uint64_t sumBusMillVolts = 0;          ///< Sum of bus voltage readings
volatile int64_t  sumBusMicroAmps = 0;          ///< Sum of bus amperage readings
volatile uint8_t  readings        = 0;          ///< Number of measurements taken

bool power_on = 0;




void ina226_read() {
  /*!
    @brief Interrupt service routine for the INA pin
    @details Routine is called whenever the INA_ALERT_PIN changes value
  */
 
  sumBusMillVolts += INA.getBusMilliVolts(deviceNumber);  // Add current value to sum
  sumBusMicroAmps += INA.getBusMicroAmps(deviceNumber);   // Add current value to sum
  readings++;

}  // of ISR for handling interrupts

void ina266_task()
{
  char tmp[64];
  static long lastMillis = millis();  // Store the last time we printed something
  volatile uint64_t vol,cur, wat;
  
  if((millis() - lastMillis) >= 500 )
  {
    vol = INA.getBusMilliVolts(deviceNumber);
    cur = INA.getBusMicroAmps(deviceNumber);
    wat = INA.getBusMicroWatts(deviceNumber);
    lastMillis = millis();
    
    sprintf(tmp,"vol:%0.3fV cur:%dmA, power:%0.3fW\n", vol/1000.0, cur/1000, (vol/1000.0)*(cur/1000/1000.0));
    Serial.print(tmp);
    udp_debug(tmp);
  }  
}

void setup() {
 
  Serial.begin(115200);

  wifi_init();

  pinMode(19, INPUT_PULLUP);
  pinMode(BUTTON_LEFT, INPUT_PULLUP);
  pinMode(BUTTON_RIGHT, INPUT_PULLUP);
  pinMode(BUTTON_ENTER, INPUT_PULLUP);
  pinMode(4, OUTPUT);
  digitalWrite(4,LOW);

  enter_debouncer.attach(BUTTON_ENTER);
  enter_debouncer.interval(100);
  
  Wire.begin();
  
  while (USB302_Init() == 0) //如果初始化一直是0 就说明没插入啥 等
  {
    delay(100);
  }
  
  Serial.println("OK");

  uint8_t devicesFound = 0;
  while (deviceNumber == UINT8_MAX)  // Loop until we find the first device
  {
    devicesFound = INA.begin(10, 10000);  // +/- 1 Amps maximum for 0.1 Ohm resistor
    Serial.println(INA.getDeviceName(devicesFound - 1));
    for (uint8_t i = 0; i < devicesFound; i++) {
      /* Change the "INA226" in the following statement to whatever device you have attached and
         want to measure */
      if (strcmp(INA.getDeviceName(i), "INA226") == 0) {
        deviceNumber = i;
        INA.reset(deviceNumber);  // Reset device to default settings
        break;
      }  // of if-then we have found an INA226
    }    // of for-next loop through all devices found
    if (deviceNumber == UINT8_MAX) {
      Serial.print(F("No INA found. Waiting 5s and retrying...\n"));
      delay(5000);
    }  // of if-then no INA226 found
  }    // of if-then no device found
  Serial.print(F("Found INA at device number "));
  Serial.println(deviceNumber);
  Serial.println();
  INA.setAveraging(64, deviceNumber);                   // Average each reading 64 times
  INA.setBusConversion(8244, deviceNumber);             // Maximum conversion time 8.244ms
  INA.setShuntConversion(8244, deviceNumber);           // Maximum conversion time 8.244ms
  INA.setMode(INA_MODE_CONTINUOUS_BOTH, deviceNumber);  // Bus/shunt measured continuously
}



void loop() {
  
  PD_Show_Service();

  if (!digitalRead(19)) {
    USB302_Data_Service();
    udp_debug("USB302_INT\n");
  }

  enter_debouncer.update();
  enter_key_value = enter_debouncer.read();
  if(enter_key_value == LOW && enter_key_old_value == HIGH)
  {
    Serial.println("enter Key Down");
    
    if(power_on)
    {
        digitalWrite(4,LOW);
        power_on = LOW;
    }
    else
    {
        request_index++;
        if(request_index>=4)
          request_index = 0;

        USB302_Send_Requse(request_index);
        digitalWrite(4,HIGH);
        power_on = HIGH;

        
    }
  }
  enter_key_old_value = enter_key_value;

  ina266_task();
}
