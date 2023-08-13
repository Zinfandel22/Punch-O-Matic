/*INTRO
 *-------------------------------------------------------------------------------------------------
* Objective is to automate the wine must cap management with a solution 
  that simulates the manual process as closely as possible while providing 
  time and temp-initiated events

/*OWNERSHIP
 *-----------------------------------------------------
 * Created by: Dave Williams 
 * Contact Info: dave@satoricellars.com
 * Wine Must Punch Controller
 * Dec 2022
 *-----------------------------------------------------*/

/*CODE HISTORY
 *-------------------------------------------------------------------------------------------------
* NEW December 2022
*- Set up Main and Config Page
*- Add Stroke Time
*- Add Plunge repeats
*- Add Temp repeats and delay
*- Add temp and time punch loops 
*- Added Interval rollback logic
*- '23 Jan 15 Added Cycle mode
*- '23 Jan 31 Fixed interval rolback logic after testing
*- '23 Jan 31 Rearranged routines for interval reduction after long term tests*
*- '23 Apr 15 Updated cycle routine to better show state on buttons
*- '23 Jul 26 Revised plung sequences to be all down or all up rather than down and up individually
/

//LIBRARYS
/*-------------------------------------------------------------------------------------------------*/
#include <Elegoo_GFX.h>         // Core graphics library
#include <Elegoo_TFTLCD.h>      // Hardware-specific library
#include <TouchScreen.h>        // Touch Screen Library
#include <OneWire.h>            // Serial bus
#include <DallasTemperature.h>  // Temperature
#include <EEPROM.h>             // EEPROM

//DEFINE HW control pins
/*-------------------------------------------------------------------------------------------------*/
#define ONE_WIRE_BUS 22  // Serial data wire is on pin 22 for dallas temp sensor
#define Sol1 24          // Solenoid 1 for Act1
#define Sol2 26          // Solenoid 2 for Act2
#define Sol3 28          // Solenoid 3 for Act3
#define Sol4 30          // Solenoid 4 for Act4

//DEFINE OneWire INSTANCE TO COMMUNICATE WITH MAXIM/DALLAS TEMPERATURE SENSOR
/*-------------------------------------------------------------------------------------------------*/
OneWire oneWire(ONE_WIRE_BUS);

//Pass oneWire reference to Dallas Temperature Sensor.
DallasTemperature sensors(&oneWire);

//DEFINE LCD ANALOG CONTROL PINS ASSIGNMENT
/*-------------------------------------------------------------------------------------------------*/
#define LCD_CS A3     // Chip Select goes to Analog 3
#define LCD_CD A2     // Command/Data goes to Analog 2
#define LCD_WR A1     // LCD Write goes to Analog 1
#define LCD_RD A0     // LCD Read goes to Analog 0
#define LCD_RESET A4  // Can alternately just connect to Arduino's reset pin
#define YP A3         // must be an analog pin, use "An" notation!
#define XM A2         // must be an analog pin, use "An" notation!
#define YM 9          // can be a digital pin
#define XP 8          // can be a digital pin

//DEFINE TOUCH POINTS FOR ILI9341 PANEL
//UPDATE AFTER RUNNING CALIBRATION ROUTINE
/*-------------------------------------------------------------------------------------------------*/
#define TS_LEFT 110
#define TS_RIGHT 920
#define TS_BOT 90
#define TS_TOP 920

//Touch Pressure
/*-------------------------------------------------------------------------------------------------*/
#define MINPRESSURE 10
#define MAXPRESSURE 2000

//Screen Declaration
/*-------------------------------------------------------------------------------------------------*/
Elegoo_TFTLCD tft(LCD_CS, LCD_CD, LCD_WR, LCD_RD, LCD_RESET);

//DEFINE Colors
/*-------------------------------------------------------------------------------------------------*/
#define BLACK 0x0000       /*   0,   0,   0 */
#define NAVY 0x000F        /*   0,   0, 128 */
#define DARKGREEN 0x03E0   /*   0, 128,   0 */
#define DARKCYAN 0x03EF    /*   0, 128, 128 */
#define MAROON 0x7800      /* 128,   0,   0 */
#define PURPLE 0x780F      /* 128,   0, 128 */
#define OLIVE 0x7BE0       /* 128, 128,   0 */
#define LIGHTGREY 0xC618   /* 192, 192, 192 */
#define DARKGREY 0x7BEF    /* 128, 128, 128 */
#define BLUE 0x001F        /*   0,   0, 255 */
#define GREEN 0x07E0       /*   0, 255,   0 */
#define CYAN 0x07FF        /*   0, 255, 255 */
#define RED 0xF800         /* 255,   0,   0 */
#define MAGENTA 0xF81F     /* 255,   0, 255 */
#define YELLOW 0xFFE0      /* 255, 255,   0 */
#define WHITE 0xFFFF       /* 255, 255, 255 */
#define ORANGE 0xFD20      /* 255, 165,   0 */
#define GREENYELLOW 0xAFE5 /* 173, 255,  47 */
#define LIGHTPINK 0xF81F   /* 173, 255,  47 */
#define LIME 0x07FF

//DEFINE Button Size defaults
/*-------------------------------------------------------------------------------------------------*/
#define BUTTON_W 80
#define BUTTON_H 50


//TouchScreen Area Pin Declaration
/*-------------------------------------------------------------------------------------------------*/
TouchScreen ts = TouchScreen(XP, YP, XM, YM, 300);

//Declare Button objects
/*-------------------------------------------------------------------------------------------------*/
Elegoo_GFX_Button ButtonState;
Elegoo_GFX_Button ButtonPunch;
Elegoo_GFX_Button ButtonInfo;
Elegoo_GFX_Button ButtonReturn;
Elegoo_GFX_Button ButtonAct1;
Elegoo_GFX_Button ButtonAct2;
Elegoo_GFX_Button ButtonAct3;
Elegoo_GFX_Button ButtonAct4;
Elegoo_GFX_Button ButtonStart;
Elegoo_GFX_Button ButtonReset;
Elegoo_GFX_Button ButtonConfig;
Elegoo_GFX_Button ButtonCycle;

//Custom Variables
/*-------------------------------------------------------------------------------------------------*/
uint16_t identifier;  //Store Screen Identifier although this is hard coded later due to Elegoo display

//Global Variables
/*-------------------------------------------------------------------------------------------------*/
int debounce = 200;  //Set button press debounce
//int deviceCount;                        //# of serial sensors
DeviceAddress devices[10];
byte devicesFound = 0;
unsigned long updateMillis = millis();  //Set for screen update time since last 1s update
unsigned long LastButton;               //Set on buttonpress
unsigned long LastPunch;                //Set on punch
unsigned long InfoAge;                  //Time on Info scnreen, used to reset to home after time interval. (30s)
unsigned long DwellAge = millis();      //Time for Dwell time interval
unsigned long IntervalAge = millis();   //Time since  Interval reduction
int Uptime;                             //Flag to indicate if up for 1 hour
byte _days;                             //Ease punch frequency after this many days
float TempAct;                          //Current Temperature
float TMax;                             //Record Max temp
float TMin;                             //Min temp
int TempSet;                            //Set point temp
float Interval;                         //Time to next punch
byte Index;                             //Interval set index counter
byte IntervalSet;                       //Set Interval per day 1,2,4,6,8
byte CurrentPage = 1;                   //Current Page indicator  "1" First Page,  "2" Second Page
byte CurrentState;                      //Current Operating State 0= Stopped, 1= Running
int Cycles;                             //Count of all punch cycles
byte Dwell;                             //Actuator Stroke Time variable (s)
byte SetDwellTemp;                      //Mins to wait between Temp Punches (min)
byte ActuatorState = 0;                 //Set if manual punch was triggered. 0 is all retracted, 1 is 1 or more extended
byte Act1;                              //State of individual actuators, 0 is retracted, 1 is extended
byte Act2;                              //State of individual actuators, 0 is retracted, 1 is extended
byte Act3;                              //State of individual actuators, 0 is retracted, 1 is extended
byte Act4;                              //State of individual actuators, 0 is retracted, 1 is extended
byte SetTimeRep;                        //# reps to punch for a timed trigger
byte SetTempRep;                        //# reps to punch for a temp trigger
int TempDwellTime;                      //Time since last temp punch. Used to wait this time between punches of temp is exceeded
byte CycleFlag;                         //Cycle mode unless on home
String s_UpTime;                        //String of uptime for UI

//Array definition
/*-------------------------------------------------------------------------------------------------*/
byte PinOutputs[4] = { Sol1, Sol2, Sol3, Sol4 };      //ARRAY of 4 config pins for digital HW output pin definitions for the for Actuator Solenoids !!!
byte _IntervalSet[8] = { 1, 2, 3, 4, 6, 8, 12, 24 };  //ARRAY for # time periods / day

//Declare functions
/*-------------------------------------------------------------------------------------------------*/
void ReadScreen();            //Read Touch Screen
void drawMainScreen();        //Page 1 Main
void drawInfoScreen();        //Page 2 Info
void drawManualScreen();      //Page 3 Manual
void drawConfigScreen();      //Page 4 Config
void updateInterval();        //Draw interval on display
void updateTemp();            //Draw temp on display
void Punch(int);              //Unattended full Punch. Variable controls the number of cycles and Dwell time
void PunchManual();           //Manual punch with Actuator dwell
void ResetPunch();            //Return actuators to home
void Wait();                  //1s Wait and read screen
void drawhomeicon();          //Draws home icon
float GetTemp();              //Read temp sensor and return current
void Cycle();                 //Continuously loop selected plungers
String IntervalReductTest();  //Calculate uptime and interval reduction as required
void UpdateSetInterval();     //Common point to update Set interval from various calls
/*-------------------------------------------------------------------------------------------------*/

void setup(void) {  //Initial Setup
  Serial.print("Free Ram: ");
  Serial.println(freeRam());

  Serial.begin(115200);  //Serial Port
  tft.begin(0x9341);     // hard code identifier for Elegoo display
  tft.setRotation(3);    //Portrait USB top left
  sensors.begin();       //Start temp sensor
  sensors.begin();       //Start temp sensor. Duplicated to improve starting reliability
  Serial.println("Locating temp devices...");
  Serial.print("Found ");
  devicesFound = sensors.getDeviceCount();
  Serial.print(devicesFound);
  Serial.println(" devices");
  //Checking temp sensor setup
  sensors.getAddress(devices[0], 0);
  //Report parasitic power requirements
  Serial.print("Parasitic power is: ");
  if (sensors.isParasitePowerMode()) Serial.println("ON");
  else Serial.println("OFF");
  for (int i = 0; i < devicesFound; i++)
    if (!sensors.getAddress(devices[i], i))
      Serial.println("Unable to find address for Device" + i);

  //Define Pinmodes for Ram Solenoids using PinOutputs Array 0-3
  /*-------------------------------------------------------------------------------------------------*/
  for (byte i = 0; i <= 3; i++) {
    pinMode(PinOutputs[i], OUTPUT);
  }


  //Define screen id - MUST BE THIS for an Elegoo display
  /*-------------------------------------------------------------------------------------------------*/
  uint16_t identifier = tft.readID();
  identifier = 0x9341;

  //Get state from EEPROM or initize and set defaults incase of new board
  /*-------------------------------------------------------------------------------------------------*/
  for (byte i = 0; i <= 11; i++) {
    if (EEPROM.read(i) == 255) {  //Assume no value initialize
      Serial.print("Initializing EEPROM - ");
      Serial.println(i);
      EEPROM.update(i, 0);
      EEPROM.update(1, 4);   //default IntervalSet Index
      EEPROM.update(4, 80);  //default Temp
      EEPROM.update(6, 2);   //default Time reps
      EEPROM.update(7, 4);   //default Temp Reps
      EEPROM.update(8, 30);  //default Dwell (30s stroke)
      EEPROM.update(11, 5);  //default punch ease (_days)
    }
  }

  //RESTORE current settings
  /*-------------------------------------------------------------------------------------------------*/
  CurrentState = EEPROM.read(0);          // 0 = Stopped, 1 = Running
  Index = EEPROM.read(1);                 //Restore Interval Index
  IntervalSet = _IntervalSet[Index];      // Interval range 2-24 /day from Array index
  Interval = 1440 / _IntervalSet[Index];  // Calculated from stored interval index
  TMax = EEPROM.read(2);                  //Last Max
  TMin = EEPROM.read(3);                  //Last Min
  TempSet = EEPROM.read(4);               //Temp threshold for punch start
  //EEPROM 5 is uptime flag, checked later
  SetTimeRep = EEPROM.read(6);    // # repeats for time
  SetTempRep = EEPROM.read(7);    // # repeats for temp
  Dwell = EEPROM.read(8);         // Stroke time for Actuator Extension
  SetDwellTemp = EEPROM.read(9);  // Time Between Temp initiated Punch actions
  Cycles = EEPROM.read(10);       //Restore number of cycles
  _days = EEPROM.read(11);        //Restores punch ease days

  //Get Current Temp for display
  /*-------------------------------------------------------------------------------------------------*/
  TempAct = GetTemp();

  //tft.fillScreen(BLACK);
  /*-------------------------------------------------------------------------------------------------*/
  drawMainScreen();  //Draw Temperature Box

  //Print setup and screen
  /*-------------------------------------------------------------------------------------------------*/
  Serial.println("Punch-n-Judy");
  Serial.print("TFT size is ");
  Serial.print(tft.width());
  Serial.print("x");
  Serial.println(tft.height());

  if (EEPROM.read(5) == 1) {  //Test if power fail and uptime > 1 hour
    Serial.println("Power fail so punch once now");
    Punch(SetTimeRep);  //Punch if power fail n Reps
  }
}
/*-------------------------------------------------------------------------------------------------*/

void loop() {  // Main
  /*-------------------------------------------------------------------------------------------------*/
  EEPROM.update(5, millis() > 3600000);  //Set flag if run time exceeds 1 hour, else it resets

  ReadScreen();  // Check for action on screen press

  if (millis() - DwellAge >= 60000) {  //If 1 min passes, increase time since TEMP punch counter
    TempDwellTime += 1;                //Add one minute to the Temp Dwell Time
    DwellAge = millis();               //Reset Temp wait interval measurement
    Serial.print("Temp Timer Counter: ");
    Serial.println(TempDwellTime);
  }

  if (TempAct >= TempSet && TempDwellTime >= SetDwellTemp) {  //Temp limit reached and more than Time since last Temp punch exceeded
    Serial.println("Temp Exceeded and longer than Temp Wait Period");
    Punch(SetTempRep);              //Punch(n) n = Temp rep times
    TempDwellTime = 0;              //Reset temp wait time if temp punch occurs
    Interval = 1440 / IntervalSet;  //Reset time interval if temp punch occurs
  }

  // Screen update each second
  /*-------------------------------------------------------------------------------------------------*/
  if (millis() - updateMillis >= 1000) {  //1 second interval check
                                          //Serial.print("Free Ram: ");
                                          //Serial.println(freeRam());
    updateMillis = millis();              //Display timer
    if (CurrentState == 1) {              //Running
      Interval -= .016667;                //Decrease time wait punch interval if running. *** Reduce to speed time when testing - default = 0.016667
      if (Interval <= 0) {                //Time limit reached
        Serial.println("Time Interval Reached");
        Interval = 1440 / IntervalSet;  //Reset Interval
        TempDwellTime = 0;              //Reset temp wait interval if time interval reached
        Punch(SetTimeRep);              //Punch(n); Time rep times
      }
    }
    GetTemp();  //Read real Temp anf check for max/min
    if (TempAct > TMax) {
      EEPROM.update(2, TMax);
      TMax = TempAct;
    }
    if (TempAct < TMin) {
      EEPROM.update(3, TMin);
      TMin = TempAct;
    }
    if (CurrentPage == 1) {  //Always display current temp
      updateTemp();
      Serial.println(TempAct);
      IntervalReductTest();
      if (CurrentState == 1) {  //Also display interval if running
        updateInterval();
      }
    }
  }
  if ((CurrentPage == 2 || CurrentPage == 4) && millis() - InfoAge >= 60000) {  //Timeout for INFO & CONFIG screen after 1 min back to main
    CurrentPage = 1;
    Serial.print("Screen Timeout- ");
    drawMainScreen();
  }
}
/*-------------------------------------------------------------------------------------------------*/

//FUNCTIONS
void ReadScreen() {  //Read Touchscreen after each debounce period
  //Set up touchScreen Definition
  TSPoint p = ts.getPoint();
  pinMode(XP, OUTPUT);
  pinMode(XM, OUTPUT);
  pinMode(YP, OUTPUT);
  //Touch Screen Pressed
  /*-------------------------------------------------------------------------------------------------*/
  if (millis() - LastButton > debounce) {
    if (p.z > MINPRESSURE && p.z < MAXPRESSURE) {
      // scale and orientate touch to screen size and Orientation
      // Left/Right/Top/Bottom assuming portrait mode.
      p.x = (tft.height() - map(p.x, TS_LEFT, TS_RIGHT, tft.height(), 0));  //Scale range to adjust to rotation from Portrait to landscape
      p.y = (tft.width() - map(p.y, TS_BOT, TS_TOP, tft.width(), 0));
      int px = p.y;  //Swap axis due to rotation
      int py = p.x;
      //Print TouchScreen area touched (X-Y reversed due to landscape mode)
      /*
    tft.fillRect(px - 1, py - 1, 2, 2, YELLOW);  //draw on screen for touch debug
    Serial.print("Touch Pressure: ");
    Serial.println(p.z);
    Serial.print("X ");
    Serial.print(px);
    Serial.print(", Y ");
    Serial.println(py);
    */
      //Case for each expected touch action and update display as required
      //Temp Adj Pressed
      /*-------------------------------------------------------------------------------------------------*/
      if (px >= 60 && px < 150 && py >= 120 && py <= 240 && CurrentPage == 1) {
        Serial.println("Temp Adjust Pressed-");
        if (py <= 200) {
          Serial.println("Temp Up Pressed-");
          Serial.println(TempSet);
          if (TempSet < 120) {
            TempSet += 5;
            EEPROM.update(4, TempSet);
            tft.fillRect(20, 170, 60, 40, BLACK);  //Clear numeral area
            tft.setTextSize(3);
            tft.setCursor(20, 175);
            tft.println(TempSet);
          }
        } else {
          Serial.println("Temp Down Pressed-");
          if (TempSet > 50) {
            TempSet -= 5;
            EEPROM.update(4, TempSet);
            tft.fillRect(20, 170, 60, 40, BLACK);  //Clear numeral area
            tft.setTextSize(3);
            tft.setCursor(20, 175);
            tft.println(TempSet);
            Serial.println(TempSet);
          }
        }
        LastButton = millis();  //Reset debounce timer
      }

      //Per Day UP Pressed
      /*-------------------------------------------------------------------------------------------------*/
      if (px >= 150 && px <= 220 && py >= 120 && py <= 240 && CurrentPage == 1) {
        Serial.println("Interval Pressed: ");
        if (py <= 200) {
          Serial.println("interval Up Pressed: ");
          if (Index < 7) {
            Index += 1;
            EEPROM.update(1, Index);
            IntervalSet = _IntervalSet[Index];
            Interval = 1440 / IntervalSet;
            Serial.println(_IntervalSet[Index]);
          }
        } else {
          Serial.println("interval Down Pressed: ");
          if (Index > 0) {
            Index -= 1;
            EEPROM.update(1, Index);
            IntervalSet = _IntervalSet[Index];
            Interval = 1440 / IntervalSet;
            Serial.println(_IntervalSet[Index]);
          }
        }
        UpdateSetInterval();
        updateInterval();
        LastButton = millis();
      }

      //Run/Stop Pressed
      /*-------------------------------------------------------------------------------------------------*/
      if (px >= 235 && px <= 305 && py >= 50 && py <= 100 && CurrentPage == 1) {
        Serial.println("Stop/Go Pressed");
        CurrentState = !CurrentState;
        EEPROM.update(0, CurrentState);
        if (CurrentState == 0) {
          ButtonState.initButton(&tft, 280, 75, BUTTON_W, BUTTON_H, WHITE, BLACK, GREEN, (char *)"Run", 2);
          ResetPunch();
        } else {
          ButtonState.initButton(&tft, 280, 75, BUTTON_W, BUTTON_H, WHITE, WHITE, RED, (char *)"Stop", 2);
        }
        ButtonState.drawButton(true);
        LastButton = millis();
      }

      //Manual/Config Pressed
      /*-------------------------------------------------------------------------------------------------*/
      if (px >= 235 && px <= 305 && py >= 110 && py <= 160) {
        switch (CurrentPage) {
          case 1:
            Serial.println("Manual Punch Pressed ");
            CurrentPage = 3;
            LastButton = millis();
            drawManualScreen();
            break;
          case 2:
            Serial.println("Config Screen Pressed ");
            CurrentPage = 4;
            LastButton = millis();
            drawConfigScreen();
            break;
        }
      }

      //INFO page
      /*-------------------------------------------------------------------------------------------------*/
      if (px >= 250 && px <= 320 && py >= 180 && py <= 240 && CurrentPage == 1) {
        CurrentPage = 2;
        Serial.print("Info from Main Selected - ");
        LastButton = millis();
        drawInfoScreen();  //Change to Info screen
      }
      //On Manual Page
      /*-------------------------------------------------------------------------------------------------*/
      if (CurrentPage == 3) {
        //1 Pressed
        if (px >= 5 && px <= 95 && py >= 95 && py <= 145) {
          Serial.println("Act1 Pressed");
          ButtonAct1.drawButton(Act1);
          Act1 = !Act1;
          LastButton = millis();
        }
        //2 Pressed
        if (px >= 120 && px <= 210 && py >= 95 && py <= 145) {
          Serial.println("Act2 Pressed");
          ButtonAct2.drawButton(Act2);
          Act2 = !Act2;
          LastButton = millis();
        }
        //3 Pressed
        if (px >= 5 && px <= 95 && py >= 175 && py <= 225) {
          Serial.println("Act3 Pressed");
          ButtonAct3.drawButton(Act3);
          Act3 = !Act3;
          LastButton = millis();
        }
        //4 Pressed
        if (px >= 120 && px <= 210 && py >= 175 && py <= 225) {
          Serial.println("Act4 Pressed");
          ButtonAct4.drawButton(Act4);
          Act4 = !Act4;
          LastButton = millis();
        }
        //UP/Down
        if ((px >= 225 && px <= 315 && py >= 95 && py <= 145) && (Act1 == 1 || Act2 == 1 || Act3 == 1 || Act4 == 1)) {
          Serial.println("Up/Down Pressed");
          ButtonStart.drawButton();
          ActuatorState = !ActuatorState;
          LastButton = millis();
          PunchManual();
        }
        //Cycle
        if (px >= 225 && px <= 315 && py >= 175 && py <= 225) {
          CycleFlag = !CycleFlag;
          Serial.println("Cycle");
          Serial.println(CycleFlag);
          ButtonCycle.drawButton(!CycleFlag);
          LastButton = millis();
          Cycle();
        }
      }
      //Status Screen
      /*-------------------------------------------------------------------------------------------------*/
      //Reset on Status page
      if (px >= 235 && px <= 305 && py >= 50 && py <= 100 && CurrentPage == 2) {
        Serial.println("Reset Pressed");
        TMax = 0;
        TMin = 99;
        Cycles = 0;
        EEPROM.update(10, Cycles);
        LastButton = millis();
        drawInfoScreen();
      }

      //Status Reduce after _days Pressed
      if (px >= 100 && px < 250 && py >= 150 && py <= 240 && CurrentPage == 2) {
        Serial.println("Ease Pressed");
        if (px < 150) {
          Serial.println("Ease Down ");
          if (_days > 1) { _days -= 1; };
        } else {
          Serial.println("Ease Up");
          if (_days < 10) { _days += 1; }
        }
        tft.fillRect(160, 197, 35, 30, BLACK);  //Clear numeral area
        tft.setTextSize(3);
        tft.setTextColor(WHITE);
        tft.setCursor(160, 200);
        tft.println(_days);
        EEPROM.update(11, _days);
        LastButton = millis();
        InfoAge = millis();  //Update so screen does not time out while adjusting
      }

      //Config Screen
      /*-------------------------------------------------------------------------------------------------*/
      //Config Speed/Dwell Pressed
      if (px >= 110 && px <= 140 && py >= 70 && py <= 135 && CurrentPage == 4) {
        Serial.print("Speed Pressed-");
        if (py < 100) {
          Serial.print("Speed Up Pressed-");
          if (Dwell < 30) { Dwell += 1; }
        } else {
          Serial.print("Down Pressed-");
          if (Dwell > 2) { Dwell -= 1; }
        }
        tft.fillRect(30, 90, 60, 40, BLACK);  //Clear numeral area
        tft.setTextSize(3);
        tft.setCursor(30, 90);
        tft.println(Dwell);
        EEPROM.update(8, Dwell);
        LastButton = millis();
      }

      //Config Time Rep Cycles Pressed
      if (px >= 280 && px <= 310 && py >= 70 && py <= 135 && CurrentPage == 4) {
        Serial.println("Time Cycles Pressed");
        if (py < 100) {
          Serial.println("Up Pressed");
          if (SetTimeRep < 10) { SetTimeRep += 1; }
        } else {
          Serial.println("Down Pressed ");
          if (SetTimeRep > 1) { SetTimeRep -= 1; }
        }
        tft.fillRect(200, 90, 60, 40, BLACK);  //Clear numeral area
        tft.setTextSize(3);
        tft.setCursor(200, 90);
        tft.println(SetTimeRep);
        EEPROM.update(6, SetTimeRep);
        LastButton = millis();
      }
      //Config Temp Rep Cycles Pressed
      if (px >= 270 && px <= 310 && py >= 170 && py <= 235 && CurrentPage == 4) {
        Serial.println("Temp Cycles Pressed");
        if (py < 200) {
          Serial.println("Up Pressed");
          if (SetTempRep < 255) { SetTempRep += 1; }
        } else {
          Serial.println("Down Pressed ");
          if (SetTempRep > 1) { SetTempRep -= 1; }
        }
        tft.fillRect(200, 190, 60, 40, BLACK);  //Clear numeral area
        tft.setTextSize(3);
        tft.setCursor(200, 190);
        tft.println(SetTempRep);
        EEPROM.update(7, SetTempRep);
        LastButton = millis();
      }

      //Config Temp Dwell Pressed
      if (px >= 110 && px <= 140 && py >= 170 && py <= 235 && CurrentPage == 4) {
        Serial.println("Temp Dwell Pressed");
        if (py < 200) {
          Serial.println("Up Pressed");
          if (SetDwellTemp < 240) { SetDwellTemp += 1; };
        } else {
          Serial.println("Down Pressed ");
          if (SetDwellTemp > 0) { SetDwellTemp -= 1; }
        }
        tft.fillRect(30, 190, 60, 40, BLACK);  //Clear numeral area
        tft.setTextSize(3);
        tft.setCursor(30, 190);
        tft.println(SetDwellTemp);
        EEPROM.update(9, SetDwellTemp);
        LastButton = millis();
        InfoAge = millis();  //Update so screen does not time out while adjusting
      }

      //Home ICON
      if (px >= 260 && px <= 320 && py >= 0 && py <= 40 && CurrentPage != 1) {  //Home Pressed. Home on pages 2/3
        Serial.println("Home Pressed");
        CycleFlag == 0;  //Stop any cycles
        CurrentPage = 1;
        if (ActuatorState == 1) {  //One or more paddles are down
          ResetPunch();
        }
        drawMainScreen();
      }
    }
  }
}
/*-------------------------------------------------------------------------------------------------*/

void drawMainScreen() {   //Main operating screen
  tft.fillScreen(BLACK);  //Clear screen
  Serial.println("Home Screen");
  //TITLE
  tft.setTextSize(3);
  tft.setTextColor(RED);
  tft.setCursor(15, 10);
  tft.println("Satori Cellars");
  tft.drawLine(10, 42, 320, 42, GREEN);

  //Labels
  tft.setTextSize(2);
  tft.setTextColor(CYAN);
  tft.setCursor(15, 50);
  tft.println("Temp(F)");
  tft.setCursor(15, 125);
  tft.println("Set");
  tft.setCursor(15, 140);
  tft.println("Temp(F)");
  tft.setCursor(125, 50);
  tft.println("Next(h:m)");
  tft.setCursor(125, 140);
  tft.println("Mix/day");

  //Incrementors
  tft.setTextSize(3);
  tft.setTextColor(GREEN);  //UP
  tft.setCursor(80, 160);   //Temp UP
  tft.println("+");
  tft.setCursor(190, 160);  //Interval UP
  tft.println("+");

  tft.setTextColor(RED);   //DOWN
  tft.setCursor(80, 200);  //Temp DOWN
  tft.println("-");
  tft.setCursor(190, 200);  //Interval Down
  tft.println("-");

  //Data
  tft.setTextColor(WHITE);
  tft.setTextSize(3);
  updateTemp();
  tft.setCursor(20, 175);
  tft.println(TempSet);
  updateInterval();
  UpdateSetInterval();

  //BUTTONS
  //Stop Go
  if (CurrentState == 0) {
    ButtonState.initButton(&tft, 280, 75, BUTTON_W, BUTTON_H, WHITE, BLACK, GREEN, (char *)"Run", 2);
  } else {
    ButtonState.initButton(&tft, 280, 75, BUTTON_W, BUTTON_H, WHITE, WHITE, RED, (char *)"Stop", 2);
  }
  ButtonState.drawButton(true);

  //Manual
  ButtonPunch.initButton(&tft, 280, 135, BUTTON_W, BUTTON_H, WHITE, WHITE, BLUE, (char *)"Manual", 2);
  ButtonPunch.drawButton(true);

  //INFO
  ButtonInfo.initButton(&tft, 280, 195, BUTTON_W, BUTTON_H, WHITE, WHITE, DARKGREY, (char *)"Status", 2);
  ButtonInfo.drawButton(true);
}
/*-------------------------------------------------------------------------------------------------*/

void drawInfoScreen() {  //Running status screen
  tft.fillScreen(BLACK);
  //  CurrentPage = 2;
  drawhomeicon();
  //TITLE
  tft.setCursor(15, 10);
  tft.setTextColor(RED);
  tft.setTextSize(3);
  tft.println("Status");
  // DIVIDERS LINES
  tft.drawLine(10, 42, 320, 42, GREEN);  //Title
  tft.drawLine(115, 45, 115, 240, GREEN);

  //TEMP
  tft.setTextSize(2);
  tft.setTextColor(CYAN);
  //TITLES
  tft.setCursor(5, 50);
  tft.println("Temp (F)");

  tft.setCursor(5, 110);
  tft.println("Temp Min");

  tft.setCursor(5, 180);
  tft.println("Temp Max");

  tft.setCursor(125, 50);
  tft.println("Cycles");

  tft.setCursor(125, 110);
  tft.println("Uptime");

  tft.setCursor(125, 180);
  tft.println("Reduce after");

  tft.setCursor(230, 205);
  tft.println("days");

  //VALUES
  tft.setTextColor(WHITE);
  tft.setTextSize(3);
  tft.setCursor(20, 70);
  tft.println(TempAct, 1);

  tft.setCursor(20, 130);
  tft.println(TMin, 1);

  tft.setCursor(20, 200);
  tft.println(TMax, 1);

  tft.setCursor(125, 70);
  tft.println(Cycles);

  tft.setCursor(125, 130);
  tft.println(s_UpTime);
  IntervalReductTest();
  tft.setCursor(160, 200);
  tft.println(_days);

  //Reduce Interval Down
  tft.setTextSize(3);
  tft.setTextColor(RED);
  tft.setCursor(125, 200);
  tft.println("-");

  //Increase Interval Up
  tft.setTextColor(GREEN);
  tft.setCursor(200, 200);
  tft.println("+");

  //BUTTONS
  //Reset
  ButtonReset.initButton(&tft, 280, 75, BUTTON_W, BUTTON_H, WHITE, WHITE, RED, (char *)"Reset", 2);
  ButtonReset.drawButton(true);
  //Manual
  ButtonConfig.initButton(&tft, 280, 135, BUTTON_W, BUTTON_H, WHITE, WHITE, BLUE, (char *)"Config", 2);
  ButtonConfig.drawButton(true);
  InfoAge = millis();
}
/*-------------------------------------------------------------------------------------------------*/

void drawManualScreen() {  //Manual Punch Screen
  tft.fillScreen(BLACK);
  drawhomeicon();
  tft.setCursor(5, 10);
  tft.setTextColor(RED);
  tft.setTextSize(3);
  tft.println("Manual Punch");
  // DIVIDER LINES
  tft.drawLine(10, 42, 320, 42, GREEN);    //Title
  tft.drawLine(215, 42, 215, 230, GREEN);  //Vertical Center
  tft.setTextColor(BLUE);
  tft.setTextSize(2);
  tft.setCursor(15, 60);
  tft.println("Select Actuator");
  tft.setCursor(230, 60);
  tft.println("Action");

  //BUTTONS
  //Actuator 1
  ButtonAct1.initButton(&tft, 50, 120, BUTTON_W, BUTTON_H, WHITE, WHITE, PURPLE, (char *)"1", 3);
  ButtonAct1.drawButton(true);

  //Actuator 2
  ButtonAct2.initButton(&tft, 160, 120, BUTTON_W, BUTTON_H, WHITE, WHITE, PURPLE, (char *)"2", 3);
  ButtonAct2.drawButton(true);

  //Actuator 3
  ButtonAct3.initButton(&tft, 50, 200, BUTTON_W, BUTTON_H, WHITE, WHITE, PURPLE, (char *)"3", 3);
  ButtonAct3.drawButton(true);

  //Actuator 4
  ButtonAct4.initButton(&tft, 160, 200, BUTTON_W, BUTTON_H, WHITE, WHITE, PURPLE, (char *)"4", 3);
  ButtonAct4.drawButton(true);

  //DOWN/UP
  if (ActuatorState == 0) {
    ButtonStart.initButton(&tft, 270, 120, BUTTON_W, BUTTON_H, WHITE, BLACK, RED, (char *)"DOWN", 2);
    ButtonStart.drawButton(true);
  } else {
    ButtonStart.initButton(&tft, 270, 120, BUTTON_W, BUTTON_H, WHITE, BLACK, GREEN, (char *)"UP", 2);
    ButtonStart.drawButton(true);
  }
  //Cycle
  ButtonCycle.initButton(&tft, 270, 200, BUTTON_W, BUTTON_H, WHITE, BLACK, BLUE, (char *)"Cycle", 2);
  ButtonCycle.drawButton(!CycleFlag);
}
/*-------------------------------------------------------------------------------------------------*/

void drawConfigScreen() {  //Persistent config settings
  tft.fillScreen(BLACK);
  drawhomeicon();
  //TITLE
  tft.setTextSize(3);
  tft.setTextColor(RED);
  tft.setCursor(10, 10);
  tft.println("Plunge Settings");
  // DIVIDERS LINES
  tft.drawLine(0, 42, 320, 42, GREEN);  //Title

  //Increment selectors
  tft.setTextColor(GREEN);
  tft.setCursor(130, 75);  //Speed Up
  tft.println("+");
  tft.setCursor(290, 75);  //Time Cycles Up
  tft.println("+");
  tft.setCursor(130, 177);  //Temp Dwell Up
  tft.println("+");
  tft.setCursor(290, 177);  //Temp Cycles Up
  tft.println("+");

  tft.setTextColor(RED);
  tft.setCursor(130, 105);  //Speed Down
  tft.println("-");
  tft.setCursor(290, 105);  //Time Cycles Down
  tft.println("-");
  tft.setCursor(130, 207);  //Temp Dwell Down
  tft.println("-");
  tft.setCursor(290, 207);  //Temp Cycles Down
  tft.println("-");

  //Labels
  tft.setTextSize(2);
  tft.setTextColor(CYAN);
  tft.setCursor(10, 50);
  tft.println("Plunge");
  tft.setCursor(10, 66);
  tft.println("Time(s)");
  tft.setCursor(10, 150);
  tft.println("Temp Delay");
  tft.setCursor(10, 166);
  tft.println("(min)");
  tft.setCursor(185, 50);
  tft.println("Cycles/Time");
  tft.setCursor(185, 150);
  tft.println("Cycles/Temp");

  //VALUES
  tft.setTextColor(WHITE);
  tft.setTextSize(3);
  tft.setCursor(30, 90);
  tft.println(Dwell);
  tft.setCursor(30, 190);
  tft.println(SetDwellTemp);
  tft.setCursor(200, 90);
  tft.println(SetTimeRep);
  tft.setCursor(200, 190);
  tft.println(SetTempRep);

  InfoAge = millis();  //Reset screen timeout counter
}
/*-------------------------------------------------------------------------------------------------*/

void ResetPunch() {  //Ensure all actuators settings are zeroed
  ActuatorState = CycleFlag = 0;
  Serial.println("Resetting Punch");
  if (Act1) {
    LastPunch = millis();
    Serial.print("Act 1-");
    Serial.println(ActuatorState);
    digitalWrite(PinOutputs[0], ActuatorState);
    //    Wait();  //Check for touch
  }

  if (Act2) {
    LastPunch = millis();
    Serial.print("Act 2-");
    Serial.println(ActuatorState);
    digitalWrite(PinOutputs[1], ActuatorState);
    //    Wait();  //Check for touch
  }

  if (Act3) {
    LastPunch = millis();
    Serial.print("Act 3-");
    Serial.println(ActuatorState);
    digitalWrite(PinOutputs[2], ActuatorState);
    //    Wait();  //Check for touch
  }

  if (Act4) {
    LastPunch = millis();
    Serial.print("Act 4-");
    Serial.println(ActuatorState);
    digitalWrite(PinOutputs[3], ActuatorState);
    //    Wait();  //Check for touch
  }
  Act1 = Act2 = Act3 = Act4 = 0;
}
/*-------------------------------------------------------------------------------------------------*/

void Punch(int Rep) {  //Rep is the number of cycles that may vary between Time and Temp punches
  Cycles += +1;
  ActuatorState == 1;
  EEPROM.update(10, Cycles);
  Serial.print("Punch n times: ");
  Serial.println(Rep);
  Serial.print("Stroke Duration: ");
  Serial.println(Dwell);
  for (int r = 1; r <= Rep; r++) {
    for (int State = 1; State >= 0; State--) {
      for (int i = 0; i <= 3; i++) {
        Serial.print("Act ");
        Serial.print(i + 1);  //Actuator
        Serial.print(", ");
        Serial.println(State);  //State
        digitalWrite(PinOutputs[i], State);
        if (CurrentPage == 1) {  //Write  actuator state on main screen when doing a timed plunge
          tft.setTextSize(2);
          tft.setCursor(280, 17);
          tft.print(i + 1);
          tft.setCursor(300, 17);
          if (State == 0) {
            tft.print("^");
          } else {
            tft.print("v");
          }
        }
        Wait();                //Look for screen press
        LastPunch = millis();  //Reset punch time for next cycle
        if (CurrentPage == 1) {
          tft.fillRect(280, 15, 60, 20, BLACK);
        }
      }
    }
  }
  //  if (CurrentPage != 4) {
  //    tft.fillRect(280, 15, 60, 20, BLACK);
  //  }
  ActuatorState = 0;
  LastPunch = millis();
}
/*-------------------------------------------------------------------------------------------------*/

void PunchManual() {  //Manual Punch selected actuators
  Serial.println("Manual Punch");
  CycleFlag == 0;
  Cycles += 1;
  ActuatorState == 1;
  EEPROM.update(10, Cycles);
  //Punch all based on state set by Act1-4 and Up/Down State
  if (Act1) {
    Serial.print("Act 1-");
    Serial.println(ActuatorState);
    digitalWrite(PinOutputs[0], ActuatorState);
    ButtonAct1.drawButton(!Act1);
    Wait();  //Check for touch
  }

  if (Act2) {
    Serial.print("Act 2-");
    Serial.println(ActuatorState);
    digitalWrite(PinOutputs[1], ActuatorState);
    ButtonAct2.drawButton(!Act2);
    Wait();  //Check for touch
  }

  if (Act3) {
    Serial.print("Act 3-");
    Serial.println(ActuatorState);
    digitalWrite(PinOutputs[2], ActuatorState);
    ButtonAct3.drawButton(!Act3);
    Wait();  //Check for touch
  }

  if (Act4) {
    Serial.print("Act 4-");
    Serial.println(ActuatorState);
    digitalWrite(PinOutputs[3], ActuatorState);
    ButtonAct4.drawButton(!Act4);
    Wait();  //Check for touch
  }
  if (CurrentPage == 3) {
    if (ActuatorState == 0) {
      ButtonStart.initButton(&tft, 270, 120, BUTTON_W, BUTTON_H, WHITE, BLACK, RED, (char *)"DOWN", 2);
      ButtonStart.drawButton(true);
    } else {
      ButtonStart.initButton(&tft, 270, 120, BUTTON_W, BUTTON_H, WHITE, BLACK, GREEN, (char *)"UP", 2);
      ButtonStart.drawButton(true);
    }
  }
  LastPunch = millis();
}
/*-------------------------------------------------------------------------------------------------*/

void Cycle() {              //Continuously cycle all selected actuators
  ActuatorState = 1;        //Home will check and reset actuators if required
  int tempState = 1;        //Toggles as long as in cycle mode
  while (CycleFlag == 1) {  //loop till break
    //Punch all based on state set by Act1-4 and Up/Down State
    if (!(Act1 or Act2 or Act3 or Act4)) {
      Serial.println("Actuators deselected");
      break;
    }
    if (Act1) {
      Serial.print("Act 1-");
      Serial.println(tempState);
      digitalWrite(PinOutputs[0], tempState);
      ButtonAct1.initButton(&tft, 50, 120, BUTTON_W, BUTTON_H, WHITE, WHITE, RED, (char *)"1", 3);
      ButtonAct1.drawButton(Act1);
      Wait();                  //Check for touch
      tempState = !tempState;  //Toggle direction
      Serial.print("Act 1-");
      Serial.println(tempState);
      digitalWrite(PinOutputs[0], tempState);
      ButtonAct1.initButton(&tft, 50, 120, BUTTON_W, BUTTON_H, WHITE, WHITE, PURPLE, (char *)"1", 3);
      if (CurrentPage == 3) {
        ButtonAct1.drawButton(!Act1);
      }
      Wait();                  //Check for touch
      tempState = !tempState;  //Toggle direction
      Cycles += 1;             //Update cycle count for status information
      EEPROM.update(10, Cycles);
    }
    if (Act2) {
      Serial.print("Act 2-");
      Serial.println(tempState);
      digitalWrite(PinOutputs[1], tempState);
      ButtonAct2.initButton(&tft, 160, 120, BUTTON_W, BUTTON_H, WHITE, WHITE, RED, (char *)"2", 3);
      ButtonAct2.drawButton(Act2);
      Wait();                  //Check for touch
      tempState = !tempState;  //Toggle direction
      Serial.print("Act 2-");
      Serial.println(tempState);
      digitalWrite(PinOutputs[1], tempState);
      ButtonAct2.initButton(&tft, 160, 120, BUTTON_W, BUTTON_H, WHITE, WHITE, PURPLE, (char *)"2", 3);
      if (CurrentPage == 3) {
        ButtonAct2.drawButton(!Act2);
      }
      Wait();                  //Check for touch
      tempState = !tempState;  //Toggle direction
      Cycles += 1;             //Update cycle count for status information
      EEPROM.update(10, Cycles);
    }
    if (Act3) {
      Serial.print("Act 3-");
      Serial.println(tempState);
      digitalWrite(PinOutputs[2], tempState);
      ButtonAct3.initButton(&tft, 50, 200, BUTTON_W, BUTTON_H, WHITE, WHITE, RED, (char *)"3", 3);
      ButtonAct3.drawButton(Act3);
      Wait();                  //Check for touch
      tempState = !tempState;  //Toggle direction
      Serial.print("Act 3-");
      Serial.println(tempState);
      digitalWrite(PinOutputs[2], tempState);
      ButtonAct3.initButton(&tft, 50, 200, BUTTON_W, BUTTON_H, WHITE, WHITE, PURPLE, (char *)"3", 3);
      if (CurrentPage == 3) {
        ButtonAct3.drawButton(!Act3);
      }
      Wait();                  //Check for touch
      tempState = !tempState;  //Toggle direction
      Cycles += 1;             //Update cycle count for status information
      EEPROM.update(10, Cycles);
    }
    if (Act4) {
      Serial.print("Act 4-");
      Serial.println(tempState);
      digitalWrite(PinOutputs[3], tempState);
      ButtonAct4.initButton(&tft, 160, 200, BUTTON_W, BUTTON_H, WHITE, WHITE, RED, (char *)"4", 3);
      ButtonAct4.drawButton(Act4);
      Wait();                  //Check for touch
      tempState = !tempState;  //Toggle direction
      Serial.print("Act 4-");
      Serial.println(tempState);
      digitalWrite(PinOutputs[3], tempState);
      ButtonAct4.initButton(&tft, 160, 200, BUTTON_W, BUTTON_H, WHITE, WHITE, PURPLE, (char *)"4", 3);
      if (CurrentPage == 3) {
        ButtonAct4.drawButton(!Act4);
      }
      Wait();                  //Check for touch
      tempState = !tempState;  //Toggle direction
      Cycles += 1;             //Update cycle count for status information
      EEPROM.update(10, Cycles);
    }
  }
  CycleFlag = 0;
  LastPunch = millis();
  for (int i = 0; i < 4; i++) {  //Resetting
    Serial.print("Act ");
    Serial.print(i + 1);  //Actuator
    Serial.print(", ");
    Serial.println(0);  //State
    digitalWrite(PinOutputs[i], 0);
  }
}
/*-------------------------------------------------------------------------------------------------*/

void updateTemp() {  //Display update of Temperature
  tft.fillRect(20, 65, 70, 50, BLACK);
  tft.setTextSize(3);
  tft.setTextColor(WHITE);
  tft.setCursor(20, 75);
  tft.println(TempAct, 1);
}

/*-------------------------------------------------------------------------------------------------*/
float GetTemp() {  //Call to temperature sensor
  //Get temp and update
  sensors.requestTemperatures();         //Send the command to get temperature readings
  TempAct = sensors.getTempFByIndex(0);  //Return result as Farenheit
  if (TempAct < -20) {                   //Possible error
    TempAct = 0.00;
    //    sensors.begin();                       //Try to start start temp sensor if invalid /HACK
    //    ReadScreen();
    //    sensors.requestTemperatures();         //Send the command to get temperature readings
    //    TempAct = sensors.getTempFByIndex(0);  //Return result as Farenheit
  }
  return TempAct;

  /*
  Routine to average if needed
  float reading = ???? ; // whatever code reads the current value
  average += k * (reading - average) ;  // simple 1st order digital low pass filter
  delay (10) ;    // sample rate set by a delay
  }*/
}

/*-------------------------------------------------------------------------------------------------*/
void Wait() {  //Check for buttons while waiting for punches - Interval is not being updated
  Serial.println("Stroking");
  LastPunch = millis();
  while (millis() - LastPunch <= Dwell * 1000) {  //Dwell is stroke period (s)
    ReadScreen();                                 //Go check screen
  }
  LastButton = millis();
}

/*-------------------------------------------------------------------------------------------------*/

String IntervalReductTest() {
  int time_mins = (int)floor(millis() / 60000);  //Get whole minutes
  int hours = time_mins / 60;                    //Change to *24 to have a minute = a day
  int minutes = time_mins % 60;                  //Get remainder
  String _hour = String(hours);
  String _min = String(minutes);
  //  String Time;
  if (_min.length() == 1) {  //Pad with leading zero
    _min = "0" + _min;
  }
  s_UpTime = _hour + ":" + _min;

  if (((millis() / 60000) - (IntervalAge / 60000)) / (1440) >= _days) {  //Get days since last reduction. Update millis to minutes and reduce to days
    IntervalAge = millis();                                              //Reset counter for last reduction
    if (Index > 0) {                                                     //If not last index value
      Index -= 1;
      EEPROM.update(1, Index);
      Interval = 1440 / _IntervalSet[Index];
      Serial.println("Index Interval Reduced");
      if (CurrentPage == 1) {
        UpdateSetInterval();
      }
    }
  }
  return s_UpTime;
}
/*-------------------------------------------------------------------------------------------------*/

void updateInterval() {         //Display Interval till update
  int hours = (Interval / 60);  //Hours
  int minutes = ((Interval / 60) - hours) * 60;
  int seconds = (((Interval / 60) - hours) * 60 - minutes) * 60;
  String s_NextTime;
  String _hour = String(hours);
  String _min = String(minutes);
  String _sec = String(seconds);
  if (_min.length() == 1) {
    _min = "0" + _min;
  }
  if (_sec.length() == 1) {
    _sec = "0" + _sec;
  }
  s_NextTime = _hour + ":" + _min;
  tft.setTextSize(3);
  tft.fillRect(105, 65, 110, 35, BLACK);
  tft.setCursor(125, 75);
  tft.println(s_NextTime);  //Time to next Punch
}
/*-------------------------------------------------------------------------------------------------*/

void UpdateSetInterval() {
  tft.setTextSize(3);
  tft.setCursor(125, 175);
  tft.fillRect(110, 170, 60, 40, BLACK);  // Clear per day
  tft.println(_IntervalSet[Index]);
  updateInterval();
}
/*-------------------------------------------------------------------------------------------------*/

void drawhomeicon() {  // draws a white home icon
  tft.drawLine(280, 19, 299, 0, WHITE);
  tft.drawLine(300, 0, 304, 4, WHITE);
  tft.drawLine(304, 3, 304, 0, WHITE);
  tft.drawLine(305, 0, 307, 0, WHITE);
  tft.drawLine(308, 0, 308, 8, WHITE);
  tft.drawLine(309, 9, 319, 19, WHITE);
  tft.drawLine(281, 19, 283, 19, WHITE);
  tft.drawLine(316, 19, 318, 19, WHITE);
  tft.drawRect(284, 19, 32, 21, WHITE);
  tft.drawRect(295, 25, 10, 15, WHITE);
}
/*-------------------------------------------------------------------------------------------------*/

int freeRam() {
  extern int __heap_start, *__brkval;
  int v;
  return (int)&v - (__brkval == 0 ? (int)&__heap_start : (int)__brkval);
}
/*-------------------------------------------------------------------------------------------------*/