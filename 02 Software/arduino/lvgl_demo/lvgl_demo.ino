#include <lvgl.h>
#include <TFT_eSPI.h>
#include <lv_examples/lv_examples.h>
#include <Wire.h>
#include <Bounce2.h>
#include <INA.h>
#include <SPI.h>
#include "udp_debug.h"
#include "fusb302.h"

#define BUTTON_LEFT  15 
#define BUTTON_ENTER  0 
#define BUTTON_RIGHT  2 
#define FUSB302_INT   19
#define POWER_SWITCH  4
#define LCD_BL      9
#define ANALOG_PIN   37

TFT_eSPI tft = TFT_eSPI(); /* TFT instance */
static lv_disp_buf_t disp_buf;
static lv_color_t buf[LV_HOR_RES_MAX * 10];



INA_Class         INA; 

Bounce enter_debouncer = Bounce();
bool enter_key_value = HIGH;
bool enter_key_old_value = HIGH;

Bounce left_debouncer = Bounce();
bool left_key_value = HIGH;
bool left_key_old_value = HIGH;

Bounce right_debouncer = Bounce();
bool right_key_value = HIGH;
bool right_key_old_value = HIGH;

int8_t request_index=0;

volatile uint8_t  deviceNumber    = UINT8_MAX;  ///< Device Number to use in example
volatile uint64_t sumBusMillVolts = 0;          ///< Sum of bus voltage readings
volatile int64_t  sumBusMicroAmps = 0;          ///< Sum of bus amperage readings
volatile uint8_t  readings        = 0;          ///< Number of measurements taken

bool power_on = 0;

lv_obj_t * label1;
lv_obj_t * label2;
lv_obj_t * label3;
lv_obj_t * label4;


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

    int analog_value = 0;
    analog_value = analogReadMilliVolts(ANALOG_PIN)*11;    

    sprintf(tmp,"#FFFFFF IN : %d V",analog_value/1000);
    lv_label_set_text(label1,tmp);      

    sprintf(tmp,"#00FF00 Out : %0.3f V",vol/1000.0);
    lv_label_set_text(label2,tmp);

    sprintf(tmp,"#0000FF Cur : %04d mA",cur/1000);
    lv_label_set_text(label3,tmp);

    sprintf(tmp,"#FF0000 Pow : %0.3f W",(vol/1000.0)*(cur/1000/1000.0));
    lv_label_set_text(label4,tmp);

    sprintf(tmp,"vol:%0.3fV cur:%dmA, power:%0.3fW\n", vol/1000.0, cur/1000, (vol/1000.0)*(cur/1000/1000.0));
    Serial.print(tmp);
    //udp_debug(tmp);


  }  
}



#if LV_USE_LOG != 0
/* Serial debugging */
void my_print(lv_log_level_t level, const char* file, uint32_t line, const char* fun, const char* dsc)
{
	Serial.printf("%s@%d %s->%s\r\n", file, line, fun, dsc);
	Serial.flush();
}
#endif

/* Display flushing */
void my_disp_flush(lv_disp_drv_t* disp, const lv_area_t* area, lv_color_t* color_p)
{
	uint32_t w = (area->x2 - area->x1 + 1);
	uint32_t h = (area->y2 - area->y1 + 1);

	tft.startWrite();
	tft.setAddrWindow(area->x1, area->y1, w, h);
	tft.pushColors(&color_p->full, w * h, true);
	tft.endWrite();

	lv_disp_flush_ready(disp);
}

static void list_event_handler(lv_obj_t * obj, lv_event_t event)
{
  if (event == LV_EVENT_CLICKED) {
    printf("Clicked: %s\n", lv_list_get_btn_text(obj));
  }
}

void setup()
{
	Serial.begin(115200); /* prepare for possible serial debug */

  pinMode(FUSB302_INT, INPUT_PULLUP);
  pinMode(BUTTON_LEFT, INPUT_PULLUP);
  pinMode(BUTTON_RIGHT, INPUT_PULLUP);
  pinMode(BUTTON_ENTER, INPUT_PULLUP);
  pinMode(POWER_SWITCH, OUTPUT);
  digitalWrite(POWER_SWITCH,LOW);
  pinMode(LCD_BL, OUTPUT);
  digitalWrite(LCD_BL,LOW);  
  pinMode(ANALOG_PIN,INPUT);

	lv_init();

#if LV_USE_LOG != 0
	lv_log_register_print_cb(my_print); /* register print function for debugging */
#endif

	tft.begin(); /* TFT init */
	tft.setRotation(3); /* mirror */

	lv_disp_buf_init(&disp_buf, buf, NULL, LV_HOR_RES_MAX * 10);

	/*Initialize the display*/
	lv_disp_drv_t disp_drv;
	lv_disp_drv_init(&disp_drv);
	disp_drv.hor_res = 160;
	disp_drv.ver_res = 80;
	disp_drv.flush_cb = my_disp_flush;
	disp_drv.buffer = &disp_buf;
	lv_disp_drv_register(&disp_drv);

  lv_obj_t* bgk;
  bgk = lv_obj_create(lv_scr_act(), NULL);//创建对象
  lv_obj_clean_style_list(bgk, LV_OBJ_PART_MAIN); //清空对象风格
  lv_obj_set_style_local_bg_opa(bgk, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_OPA_100);//设置颜色覆盖度100%，数值越低，颜色越透。
  lv_obj_set_style_local_bg_color(bgk, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_BLACK);//设置背景颜色为绿色
  lv_obj_set_size(bgk, 160, 80);//设置覆盖大小  

  label1 = lv_label_create(lv_scr_act(), NULL);
  lv_label_set_long_mode(label1, LV_LABEL_LONG_SROLL_CIRC);     /*Break the long lines*/
  lv_label_set_recolor(label1, true);                      /*Enable re-coloring by commands in the text*/
  lv_label_set_align(label1, LV_LABEL_ALIGN_CENTER);       /*Center aligned lines*/
  lv_obj_set_width(label1, 160);
  lv_obj_align(label1, NULL, LV_ALIGN_IN_TOP_MID, 0, 2);

  label2 = lv_label_create(lv_scr_act(), NULL);
  lv_label_set_long_mode(label2, LV_LABEL_LONG_SROLL_CIRC);     /*Break the long lines*/
  lv_label_set_recolor(label2, true);                      /*Enable re-coloring by commands in the text*/
  lv_label_set_align(label2, LV_LABEL_ALIGN_CENTER);       /*Center aligned lines*/
  lv_obj_set_width(label2, 160);
  lv_label_set_text(label2,"");
  lv_obj_align(label2, label1, LV_ALIGN_OUT_BOTTOM_MID, 0, 2);  

  label3 = lv_label_create(lv_scr_act(), NULL);
  lv_label_set_long_mode(label3, LV_LABEL_LONG_SROLL_CIRC);     /*Break the long lines*/
  lv_label_set_recolor(label3, true);                      /*Enable re-coloring by commands in the text*/
  lv_label_set_align(label3, LV_LABEL_ALIGN_CENTER);       /*Center aligned lines*/
  lv_obj_set_width(label3, 160);
  lv_label_set_text(label3,"");
  lv_obj_align(label3, label2, LV_ALIGN_OUT_BOTTOM_MID, 0, 2); 

  label4 = lv_label_create(lv_scr_act(), NULL);
  lv_label_set_long_mode(label4, LV_LABEL_LONG_SROLL_CIRC);     /*Break the long lines*/
  lv_label_set_recolor(label4, true);                      /*Enable re-coloring by commands in the text*/
  lv_label_set_align(label4, LV_LABEL_ALIGN_CENTER);       /*Center aligned lines*/
  lv_obj_set_width(label4, 160);
  lv_label_set_text(label4,"");
  lv_obj_align(label4, label3, LV_ALIGN_OUT_BOTTOM_MID, 0, 2);    

  enter_debouncer.attach(BUTTON_ENTER);
  enter_debouncer.interval(100);

  left_debouncer.attach(BUTTON_LEFT);
  left_debouncer.interval(100);

  right_debouncer.attach(BUTTON_RIGHT);
  right_debouncer.interval(100); 

  digitalWrite(LCD_BL,HIGH);     
  
  Wire.begin();
  lv_label_set_text(label1,"USB302 Init...");
  while (USB302_Init() == 0) //如果初始化一直是0 就说明没插入啥 等
  {
    delay(100);
  }
  
  lv_label_set_text(label1,"USB302 Init OK");

  uint8_t devicesFound = 0;

  lv_label_set_text(label2,"INA226 Init...");
  while (deviceNumber == UINT8_MAX)  // Loop until we find the first device
  {
    devicesFound = INA.begin(10, 10000);  // +/- 1 Amps maximum for 0.01 Ohm resistor
    Serial.println(INA.getDeviceName(devicesFound - 1));
    for (uint8_t i = 0; i < devicesFound; i++) {
      /* Change the "INA226" in the following statement to whatever device you have attached and
         want to measure */
      if (strcmp(INA.getDeviceName(i), "INA226") == 0) {
        deviceNumber = i;
        INA.reset(deviceNumber);  // Reset device to default settings
        lv_label_set_text(label2,"INA226 Init OK");
        break;
      }  // of if-then we have found an INA226
    }    // of for-next loop through all devices found
    if (deviceNumber == UINT8_MAX) {
      Serial.print(F("No INA found. Waiting 5s and retrying...\n"));
      lv_label_set_text(label1,"No INA found. Waiting 5s and retrying...");
      delay(5000);
    }  // of if-then no INA226 found
  }    // of if-then no device found
  Serial.print(F("Found INA at device number "));
  Serial.println(deviceNumber);
  Serial.println();
  INA.setAveraging(4, deviceNumber); 
  INA.setBusConversion(8244, deviceNumber);             // Maximum conversion time 8.244ms
  INA.setShuntConversion(8244, deviceNumber);           // Maximum conversion time 8.244ms
  INA.setMode(INA_MODE_CONTINUOUS_BOTH, deviceNumber);  // Bus/shunt measured continuously

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
    
    
    if(request_index>=1)
    {
      request_index--;
      USB302_Send_Requse(request_index);
    }
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
    
    if(request_index<4)
    {
      request_index++;
      USB302_Send_Requse(request_index);
    }
  }
  right_key_old_value = right_key_value;
}


void loop()
{
	lv_task_handler(); /* let the GUI do its work */

  PD_Show_Service();
  if (!digitalRead(FUSB302_INT)) {
    USB302_Data_Service();
  }

  right_key_update();
  left_key_update();
  enter_key_update();

  ina266_task();
  
	delay(5);
}
