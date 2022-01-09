/*
 * 程序参考自：
 * http://bbs.mydigit.cn/read.php?tid=2595019
 * 
 */

#include <Wire.h>
#include <Bounce2.h>
#include <INA.h>
#include <TFT_eSPI.h> // Graphics and font library for ST7735 driver chip
#include <SPI.h>
#include "udp_debug.h"
#include "fusb302.h"


#define BUTTON_LEFT  4 //34
#define BUTTON_ENTER  0 //35
#define BUTTON_RIGHT  2 //33
#define FUSB302_INT   19
#define POWER_SWITCH  16 //4
#define LCD_BL      9

#define STARTX    32
#define STARTY    4

INA_Class         INA; 
TFT_eSPI tft = TFT_eSPI();  // Invoke library, pins defined in User_Setup.h

Bounce enter_debouncer = Bounce();
bool enter_key_value = HIGH;
bool enter_key_old_value = HIGH;

Bounce left_debouncer = Bounce();
bool left_key_value = HIGH;
bool left_key_old_value = HIGH;

Bounce right_debouncer = Bounce();
bool right_key_value = HIGH;
bool right_key_old_value = HIGH;

int8_t request_index=1;

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

    if(cur > 20*1000*1000)
      cur = 0;

    if(power_on)
    {
      tft.setTextColor(TFT_RED, TFT_BLACK);
    }
    else
    {
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
    }
    //sprintf(tmp,"Vol :              ");
    //tft.drawString(tmp,STARTX,STARTY,2);
    sprintf(tmp,"Vol : %0.3f V      ",vol/1000.0);
    tft.drawString(tmp,STARTX,STARTY,2);

    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    sprintf(tmp,"Cur :              ");
    tft.drawString(tmp,STARTX,STARTY+28,2);
    sprintf(tmp,"Cur : %04d mA ",cur/1000);
    tft.drawString(tmp,STARTX,STARTY+28,2);

    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    sprintf(tmp,"Pow :              ");
    tft.drawString(tmp,STARTX,STARTY+56,2);
    sprintf(tmp,"Pow : %0.3f W     ",(vol/1000.0)*(cur/1000/1000.0));
    tft.drawString(tmp,STARTX,STARTY+56,2);        
    
    sprintf(tmp,"vol:%0.3fV cur:%dmA, power:%0.3fW\n", vol/1000.0, cur/1000, (vol/1000.0)*(cur/1000/1000.0));
    Serial.print(tmp);
    udp_debug(tmp);
  }  
}

void setup() {
 
  Serial.begin(115200);

  
  Serial.println("Init...");
  tft.init();
  tft.setRotation(3);
  tft.fillScreen(TFT_WHITE);

  tft.setTextColor(TFT_WHITE, TFT_BLACK);  
  tft.drawString("init...",STARTX+20,STARTY+30,4);
  
  //wifi_init();

  pinMode(FUSB302_INT, INPUT_PULLUP);
  pinMode(BUTTON_LEFT, INPUT_PULLUP);
  pinMode(BUTTON_RIGHT, INPUT_PULLUP);
  pinMode(BUTTON_ENTER, INPUT_PULLUP);
  pinMode(POWER_SWITCH, OUTPUT);
  digitalWrite(POWER_SWITCH,LOW);



  enter_debouncer.attach(BUTTON_ENTER);
  enter_debouncer.interval(100);

  left_debouncer.attach(BUTTON_LEFT);
  left_debouncer.interval(100);

  right_debouncer.attach(BUTTON_RIGHT);
  right_debouncer.interval(100);    
  
  Wire.begin();
  Serial.println("USB302_Init...");
  tft.drawString("USB302_Init...",STARTX+10,STARTY+30,4);
  while (USB302_Init() == 0) //如果初始化一直是0 就说明没插入啥 等
  {
    delay(100);
  }
  
  Serial.println("OK");

  uint8_t devicesFound = 0;
  while (deviceNumber == UINT8_MAX)  // Loop until we find the first device
  {
    devicesFound = INA.begin(15, 10000);  // +/- 1 Amps maximum for 0.01 Ohm resistor
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

  tft.fillScreen(TFT_BLACK);
  pinMode(LCD_BL, OUTPUT);
  digitalWrite(LCD_BL,HIGH);  
}

void enter_key_update()
{
  enter_debouncer.update();
  enter_key_value = enter_debouncer.read();
  if(enter_key_value == LOW && enter_key_old_value == HIGH)
  {
    Serial.println("enter Key Down");
    
    if(power_on)
    {
        digitalWrite(POWER_SWITCH,LOW);
        power_on = LOW;
        Serial.println("POWER Down");
    }
    else
    {
        digitalWrite(POWER_SWITCH,HIGH);
        power_on = HIGH; 
        Serial.println("POWER UP");
    }
  }
  enter_key_old_value = enter_key_value;
}

void left_key_update()
{
  left_debouncer.update();
  left_key_value = left_debouncer.read();
  if(left_key_value == LOW && left_key_old_value == HIGH)
  {
    Serial.println("left Key Down");
    
    request_index--;
    if(request_index<=-1)
      request_index = 4;

    USB302_Send_Requse(request_index);
  }
  left_key_old_value = left_key_value;
}

void right_key_update()
{
  right_debouncer.update();
  right_key_value = right_debouncer.read();
  if(right_key_value == LOW && right_key_old_value == HIGH)
  {
    Serial.println("right Key Down");
    
    request_index++;
    if(request_index>=5)
      request_index = 0;

    USB302_Send_Requse(request_index);
  }
  right_key_old_value = right_key_value;
}

void loop() {
  
  PD_Show_Service();

  if (!digitalRead(FUSB302_INT)) {
    USB302_Data_Service();
  }

  right_key_update();
  left_key_update();
  enter_key_update();

  ina266_task();
}
