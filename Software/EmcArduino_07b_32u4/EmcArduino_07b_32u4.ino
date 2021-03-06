/* -*- c -*-
This work is public domain.
 
This is a fork of https://github.com/dewy721/EMC-2-Arduino/downloads
and has been heavily modified to:

  1. Work with the arduino leonardo chipset.
  2. Be simple for novice programmers to follow (possibly futile)
  3. Work with mm
  4. Use a servo for it's z axis
  5. Support an LCD
  6. Support custom hardware https://github.com/lukeweston/CNCPlotter
 
Work on this was mostly performed by John Spencer and Bob Powers.
 
Please note: Although there are a LOT pin settings here you can get by
with as few as TWO pins per Axis (dir & step).

   ie: 3 axies = 6 pins used. (minimum)
       9 axies = 18 pins or an entire UNO
                 (using virtual limits switches only)

This version is cut down to 3 axis.
 
Note concerning switches: Be smart!
  - AT LEAST use HOME switches.
  - Switches are cheap insurance.
  - You'll find life a lot easier if you use them entirely.
 
If you choose to build with threaded rod for lead screws but leave out
the switches you'll have one of two possible outcomes:

  1. You'll get tired really quickly of resetting the machine by hand;
     or worse

  2. You'll forget (only once) to reset it, and upon homing it WILL
     destroy itself while you go -> WTF!? -> OMG! -> PANIC! ->
     FACEPALM!
 
List of axies. All 3 of them.
  - AXIS_0 = X (Left/Right)
  - AXIS_1 = Y (Near/Far) Lathes use this for tool depth.
  - AXIS_2 = Z (Up/Down) Not typically used for lathes. Except
    lathe/mill combo.
  
Remember, LinuxCNC sends out a bunch of stepper commands rather than
an exact location which means tuning the speed LinuxCNC sends out
commands is important.
 
DYI robot builders: You can monitor/control this sketch via a serial
interface.  Example commands:
 
  jog x200;
  jog x-215.25 y1200 z0.002 a5;
 
PS: If you choose to control this with your own interface then also
modify the divisor variable further down.

*/

// You'll need this library. Get the interrupt safe version.
#include <digitalWriteFast.h>      // http://code.google.com/p/digitalwritefast/
#include <LiquidCrystal_SR_LCD3.h> // https://github.com/marcmerlin/NewLiquidCrystal

#include <Servo.h> 

#include <avr/io.h>
#include <avr/interrupt.h>

//Servo settings MUST BE AN INT! BETWENEN 0-180
//servoDownZ is where your pen is engaged - pressing down 
#define servoDownZ 90
//servoUpZ is where your pen is disengaged - let up.
#define servoUpZ 160

// LCD Stuff
const int PIN_LCD_STROBE         =  2;  // Out: LCD IC4094 shift-register strobe
const int PIN_LCD_DATA           =  3;  // Out: LCD IC4094 shift-register data
const int PIN_LCD_CLOCK          =  4;  // Out: LCD IC4094 shift-register clock
LiquidCrystal_SR_LCD3 lcd(PIN_LCD_DATA, PIN_LCD_CLOCK, PIN_LCD_STROBE);

Servo myservo;
byte pos;

//#define BAUD    (115200)
#define BAUD 115200

// These will be used in the near future.
#define VERSION "00072.2"    // 5 characters seems arbitrary.  the .1
			     // will give us an idea what version is
			     // uploaded to the arduino while we're
			     // developing.
#define ROLE    "hackCNC   " // 10 characters


//Pitch = mm/turn = 1.25 mm/turn
//mm/inch = 25.4
//turns/inch = 25.4/1.25 = 20.32
//turns/mm = 0.8
//steps/turn = 200
//fullsteps/inch = 200*20.32 = 4064
//fullsteps/mm = 200*0.8 = 160
//microstepping inch. 8 microsteps/step * 4064 = 32512 steps per in.
//microstepping mm. 8 microsteps/step / 0.8 = 1280 steps per mm.

#define stepsPerInchX 32512
#define stepsPerInchY 32512

#define stepsPerMmX 1280
#define stepsPerMmY 1280

//set very low because it's essentially digital, cutting down on z commands.
#define stepsPerInchZ 1
#define stepsPerMmZ   1

//#define minStepTime 25 //delay in MICROseconds between step pulses.
//#define minStepTime 625
// at 8x microstepping 1600 microsteps/rotation, so 625 = 1 rotation/sec
#define stepTime 77 // 8 rotations / second

// step pins (required)
#define stepPin0 10 // x step D10 - pin 30
#define stepPin1  8 // y step D8 - pin 28
#define stepPin2 -1 // z step

// dir pins (required)
#define dirPin0 11 // x dir D11 - pin 12
#define dirPin1  9 // y dir D9 - pin 29
#define dirPin2 -1 // z dir

// microStepping pins (optional)

#define chanXms1 -1
#define chanXms2 -1
#define chanXms3 -1
#define chanYms1 -1
#define chanYms2 -1
#define chanYms3 -1
#define chanZms1 -1
#define chanZms2 -1
#define chanZms3 -1

#define xEnablePin -1
#define yEnablePin -1
#define zEnablePin -1


#define useEstopSwitch  false
#define usePowerSwitch  false
#define useStartSwitch  false
#define useStopSwitch   false
#define usePauseSwitch  false
#define useResumeSwitch false
#define useStepSwitch   false

// Set to true if your using real switches for MIN positions.
#define useRealMinX false
#define useRealMinY false
#define useRealMinZ false

// Set to true if your using real switches for HOME positions.
#define useRealHomeX false
#define useRealHomeY false
#define useRealHomeZ false

// Set to false if your using real switches for MAX positions.
#define useRealMaxX false
#define useRealMaxY false
#define useRealMaxZ false

// If your using REAL switches you'll need real pins (ignored if using Virtual switches).
// -1 = not used.
#define xMinPin  -1
#define yMinPin  -1
#define zMinPin  -1

#define xHomePin -1
#define yHomePin -1
#define zHomePin -1

#define xMaxPin  -1
#define yMaxPin  -1
#define zMaxPin  -1

//#define powerSwitchIsMomentary true // Set to true if your using a momentary switch.
//#define powerPin    -1 // Power switch. Optional
//#define powerLedPin -1 // Power indicator. Optional

//#define eStopPin         -1 // E-Stop switch. You really, REALLY should have this one.
//#define eStopLedPin      -1 // E-Stop indicator. Optional

//#define startPin  -1 // CNC Program start switch.  Optional
//#define stopPin   -1 // CNC Stop program switch.   Optional
//#define pausePin  -1 // CNC Pause program switch.  Optional
//#define resumePin -1 // CNC Resume program switch. Optional
//#define stepPin   -1 // CNC Program step switch.   Optional

// Signal inversion for real switch users. (false = ground trigger signal, true = +5vdc trigger signal.)
// Note: Inverted switches will need pull-down resistors (less than 10kOhm) to lightly ground the signal wires.
#define xMinPinInverted   false
#define yMinPinInverted   false
#define zMinPinInverted   false

#define xHomePinInverted  false
#define yHomePinInverted  false
#define zHomePinInverted  false

#define xMaxPinInverted   false
#define yMaxPinInverted   false
#define zMaxPinInverted   false

#define eStopPinInverted  false
#define powerPinInverted  false
#define probePinInverted  false
#define startPinInverted  false
#define stopPinInverted   false
#define pausePinInverted  false
#define resumePinInverted false
#define stepPinInverted   false

// Minimum position of virtual "homed" axes, in mm.  The HAL layer
// doesn't change its point of reference when homing, so we need to
// move the minimum positions so we know when we are at the home
// position
#define xMin   0
#define yMin   0
#define zMin -10

// Where should the VIRTUAL home switches be set to (ignored if using real switches).
// Set to whatever you specified in the StepConf wizard.
#define xHome  0
#define yHome  0
#define zHome  0

// Where should the VIRTUAL Max switches be set to (ignored if using real switches).
// Set to whatever you specified in the StepConf wizard.
// updated for mm
#define xMax 140
#define yMax 100
#define zMax  10

const bool giveFeedBackX = false;
const bool giveFeedBackY = false;
const bool giveFeedBackZ = false;

/*
  This indicator led will let you know how hard you pushing the Arduino.
 
 To test: Issue a G0 in the GUI command to send all axies to near min limits then to near max limits.
 Watch the indicator led as you do this. Adjust "Max Velocity" setting to suit.
 
 MOSTLY ON  = Pushing it too hard. Slow down! The Arduino can't cope, your CNC will break bits and make garbage.
 FREQUENT BLINK = Your a speed demon. Pushing it to the limits.
 OCCASIONAL BLINK = This is a safe speed. The best choice.
 OFF COMPLETELY = You can safely go faster.
 
 */
#define idleIndicator 13

// Invert direction of movement for an axis by setting to false.
boolean dirState0=true;
boolean dirState1=true;
boolean dirState2=true;

////////////////////////////////////////////////////////////////////////////////
/////////////////////////////END OF USER SETTINGS///////////////////////////////
////////////////////////////////////////////////////////////////////////////////

// This buffer's a single command.
char buffer[128];
int sofar;

// Offsets from HAL positions to real positions (in mm)
float xOffset;
float yOffset;
float zOffset;

float pos_x;
float pos_y;
float pos_z;

volatile boolean stepState=LOW;
volatile long stepper0Pos=0;
volatile long stepper0Goto=0;
volatile long stepper1Pos=0;
volatile long stepper1Goto=0;
volatile long stepper2Pos=0;
volatile long stepper2Goto=0;

boolean xMinState=false;
boolean yMinState=false;
boolean zMinState=false;

boolean xMinStateOld=false;
boolean yMinStateOld=false;
boolean zMinStateOld=false;

boolean xHomeState=false;
boolean yHomeState=false;
boolean zHomeState=false;

boolean xHomeStateOld=false;
boolean yHomeStateOld=false;
boolean zHomeStateOld=false;

boolean xMaxState=false;
boolean yMaxState=false;
boolean zMaxState=false;

boolean xMaxStateOld=false;
boolean yMaxStateOld=false;
boolean zMaxStateOld=false;


// no support for these at the moment
/*
boolean eStopStateOld=false;
 boolean powerStateOld=false;
 //boolean probeStateOld=false;
 boolean startStateOld=false;
 boolean stopStateOld=false;
 boolean pauseStateOld=false;
 boolean resumeStateOld=false;
 boolean stepStateOld=false;
 */

// Soft Limit and Power
boolean eStopState=false;
boolean powerState=false;

// Axis homed
boolean xHomed=false;
boolean yHomed=false;
boolean zHomed=false;

// this is a representation on how busy the arduino is.
// at 0 the arduino is idle, at 255 it is maxed out
volatile int globalBusy=0;

long divisor=1000000; // input divisor. Our HAL script wont send the six decimal place floats that EMC cranks out.
// A simple workaround is to multply it by 1000000 before sending it over the wire.
// So here we have to put the decimal back to get the real numbers.
// Used in: processCommand()

// these are feedback values
volatile float fbx=1;
volatile float fby=1;
volatile float fbz=1;

volatile float fbxOld=0;
volatile float fbyOld=0;
volatile float fbzOld=0;
volatile bool fbxTrigger = false;
volatile bool fbyTrigger = false;
volatile bool fbzTrigger = false;

// The current state of the program
// mainly used for the LCD
char runState='o'; // Off, Stopped, Running, Paused.

// Last update of the LCD panel.
unsigned long lastUpdate;

// These are dodgy hacks to tell the LCD to clear a line next cycle
boolean clearLine0 = false;
boolean clearLine1 = false;
boolean clearLine2 = false;
boolean clearLine3 = false;

//void jog(float x, float y, float z, float a, float b, float c, float u, float v, float w)

void jog(float x, float y, float z)
{
  x -= xOffset;
  y -= yOffset;
  z -= zOffset;

  // Handle our limit switches.
  // Compressed to save visual space. Otherwise it would be several pages long!

  // only check the limit switches if it's not homed
  // We want to mark as true if either pos or homed are true, not both
  // if(!useRealMinX){
  //   if(pos_x > xMin || ( pos_x <= xMin && !xHomed))
  //     {xMinState=true;}
  //   else
  //     {xMinState=false;}
  // }else{
  //   xMinState=digitalReadFast(xMinPin);
  //   if(xMinPinInverted)xMinState=!xMinState;
  // }

  // make some of this a function?
  // TODO: Home errors for all axis should go to the LCD
  if(!useRealMinX){
    xMinState=true;
    if(x < xMin && xHomed){
      xMinState=false;
      // This is probably a good opportunity to update the LCD
      lcd.setCursor (0, 2);
      lcd.print(F("X axis limit Reached"));
      clearLine2 = true;
    }
    else if(clearLine2){
      lcd.setCursor (0, 2);
      lcd.print(F("                    "));
      clearLine2 = false;
    }
  }else{
    xMinState=digitalReadFast(xMinPin);
    if(xMinPinInverted){
      xMinState=!xMinState;
    }
  }

  if(!useRealMinY){yMinState=true;if(y < yMin && yHomed){yMinState=false;lcd.setCursor (0, 2);lcd.print(F("Y axis limit Reached"));clearLine2 = true;}else if(clearLine2){lcd.setCursor (0, 2);lcd.print(F("                    "));clearLine2 = false;}}else{yMinState=digitalReadFast(yMinPin);if(yMinPinInverted)yMinState=!yMinState;}
  if(!useRealMinZ){zMinState=true;if(z < zMin && zHomed){zMinState=false;lcd.setCursor (0, 2);lcd.print(F("Z axis limit Reached"));clearLine2 = true;}else if(clearLine2){lcd.setCursor (0, 2);lcd.print(F("                    "));clearLine2 = false;}}else{zMinState=digitalReadFast(zMinPin);if(zMinPinInverted)zMinState=!zMinState;}

  // don't really need the homed question here, because if the machine is unhomed there will be physical limits as to how much it can move
  if(!useRealMaxX){if(x > xMax){xMaxState=true;}else{xMaxState=false;}}else{xMaxState=digitalReadFast(xMaxPin);if(xMaxPinInverted)xMaxState=!xMaxState;}
  if(!useRealMaxY){if(y > yMax){yMaxState=true;}else{yMaxState=false;}}else{yMaxState=digitalReadFast(yMaxPin);if(yMaxPinInverted)yMaxState=!yMaxState;}
  if(!useRealMaxZ){if(z > zMax){zMaxState=true;}else{zMaxState=false;}}else{zMaxState=digitalReadFast(zMaxPin);if(zMaxPinInverted)zMaxState=!zMaxState;}

  // These are only used to send a serial character if the machine is homed.
  // I can't see how they'd work at all with virtual home switches.
  if(!useRealHomeX){if(x > xHome){xHomeState=true;}else{xHomeState=false;}}else{xHomeState=digitalReadFast(xHomePin);if(xHomePinInverted)xHomeState=!xHomeState;}
  if(!useRealHomeY){if(y > yHome){yHomeState=true;}else{yHomeState=false;}}else{yHomeState=digitalReadFast(yHomePin);if(yHomePinInverted)yHomeState=!yHomeState;}
  if(!useRealHomeZ){if(z > zHome){zHomeState=true;}else{zHomeState=false;}}else{zHomeState=digitalReadFast(zHomePin);if(zHomePinInverted)zHomeState=!zHomeState;}

  if(xMinState != xMinStateOld){xMinStateOld=xMinState;Serial.print("x");Serial.print(xMinState);}
  if(yMinState != yMinStateOld){yMinStateOld=yMinState;Serial.print("y");Serial.print(yMinState);}
  if(zMinState != zMinStateOld){zMinStateOld=zMinState;Serial.print("z");Serial.print(zMinState);}

  if(xHomeState != xHomeStateOld){xHomeStateOld=xHomeState;Serial.print("x");Serial.print(xHomeState+4);}
  if(yHomeState != yHomeStateOld){yHomeStateOld=yHomeState;Serial.print("y");Serial.print(yHomeState+4);}
  if(zHomeState != zHomeStateOld){zHomeStateOld=zHomeState;Serial.print("z");Serial.print(zHomeState+4);}

  if(xMaxState != xMaxStateOld){xMaxStateOld=xMaxState;Serial.print("x");Serial.print(xMaxState+1);}
  if(yMaxState != yMaxStateOld){yMaxStateOld=yMaxState;Serial.print("y");Serial.print(yMaxState+1);}
  if(zMaxState != zMaxStateOld){zMaxStateOld=zMaxState;Serial.print("z");Serial.print(zMaxState+1);}

  if(xMinState && !xMaxState){pos_x=x+xOffset;stepper0Goto=pos_x*stepsPerMmX*2;}
  if(yMinState && !yMaxState){pos_y=y+yOffset;stepper1Goto=pos_y*stepsPerMmY*2;}
  if(zMinState && !zMaxState){pos_z=z+zOffset;stepper2Goto=pos_z*stepsPerMmZ*2;} // we need the *2 as we're driving a flip-flop routine (in stepLight function)

}

// Forward declaration: Arduino doesn't need this but arduino-mk does
void move_servo(float pos);

void processCommand()
{
  float xx=pos_x;
  float yy=pos_y;
  float zz=pos_z;

  char *ptr=buffer;
  while(ptr && ptr<buffer+sofar)
  {
    ptr=strchr(ptr,' ')+1;
    switch(*ptr) {
      
      // These are axis move commands
      case 'x': case 'X': xx=atof(ptr+1); xx=xx/divisor; break;
      case 'y': case 'Y': yy=atof(ptr+1); yy=yy/divisor; break;
      case 'z': case 'Z': zz=atof(ptr+1); zz=zz/divisor; move_servo(zz); break;
      //      case 'z': case 'Z': zz=atof(ptr+1); break;

      default: ptr=0; break;
    }
  }

  jog(xx,yy,zz);
}

void move_servo(float pos){
  int diff=75;
  if(pos<=-1.0){
    myservo.write(servoDownZ);  
  }
  else if (pos>=0.0){
    myservo.write(servoUpZ);    
  }
  else{
    diff = servoUpZ-servoDownZ;
    //doesn't matter if diff is pos/neg - handles just fine.
    myservo.write(servoUpZ+pos*diff);
  }

}


// Timer3 interrupt vector to step the motors as needed
//
// Values used here are set by the jog() method
ISR( TIMER3_COMPA_vect )
{
  bool busy;

  stepState=!stepState;
  busy=false;

  if(stepper0Pos != stepper0Goto) {
    // so, the stepper position hasn't reached it's destination.
    busy = true;
    if(stepper0Pos > stepper0Goto){
      digitalWriteFast(dirPin0,!dirState0);
      digitalWriteFast(stepPin0,stepState);
      stepper0Pos--;
    }
    else {
      digitalWriteFast(dirPin0, dirState0);
      digitalWriteFast(stepPin0,stepState);
      stepper0Pos++;
    }
  }

  if(stepper1Pos != stepper1Goto) {
    busy = true;
    if(stepper1Pos > stepper1Goto) {
      digitalWriteFast(dirPin1,!dirState1);
      digitalWriteFast(stepPin1,stepState);
      stepper1Pos--;
    } else {
      digitalWriteFast(dirPin1, dirState1);
      digitalWriteFast(stepPin1,stepState);
      stepper1Pos++;}
  }

  if(busy){
    // we have a busy value, so we're still moving.
    // This is meant to flash an LED when it's busy
    digitalWrite(idleIndicator,HIGH);
    // increment to say we're still busy.
    if(globalBusy<255){
      globalBusy++;
    }
  }
  else {
    // not busy, time to do some feedback
    digitalWrite(idleIndicator,LOW);
    if(globalBusy>0){
      globalBusy--;
    }
    
    // Set flag if we need to give feedback
    if(giveFeedBackX){
      fbx=stepper0Pos/4/(stepsPerMmX*0.5);
      fbxTrigger = fbxTrigger || (fbx != fbxOld);
      fbxOld = fbx;
    }
    if(giveFeedBackY) {
      fby=stepper1Pos/4/(stepsPerMmY*0.5);
      fbyTrigger = fbyTrigger || (fby != fbyOld);
      fbyOld = fby;
    }
  }
}

void setup()
{
  //Servo pin 
  myservo.attach(12); //Pin 26 - PD6 or D12
  //Go home.
  move_servo(1);

  lcd.begin(20, 4);               // initialize the lcd 
  lcd.home ();                   // go home
  lcd.setCursor (0, 0);
  lcd.print(F("hackCNC             "));

  // Setup Min limit switches.
  if(useRealMinX){pinMode(xMinPin,INPUT);if(!xMinPinInverted)digitalWriteFast(xMinPin,HIGH);}
  if(useRealMinY){pinMode(yMinPin,INPUT);if(!yMinPinInverted)digitalWriteFast(yMinPin,HIGH);}
  if(useRealMinZ){pinMode(zMinPin,INPUT);if(!zMinPinInverted)digitalWriteFast(zMinPin,HIGH);}

  // Setup Max limit switches.
  if(useRealMaxX){pinMode(xMaxPin,INPUT);if(!xMaxPinInverted)digitalWriteFast(xMaxPin,HIGH);}
  if(useRealMaxY){pinMode(yMaxPin,INPUT);if(!yMaxPinInverted)digitalWriteFast(yMaxPin,HIGH);}
  if(useRealMaxZ){pinMode(zMaxPin,INPUT);if(!zMaxPinInverted)digitalWriteFast(zMaxPin,HIGH);}


  // Setup Homing switches.
  if(useRealHomeX){pinMode(xHomePin,INPUT);if(!xHomePinInverted)digitalWriteFast(xHomePin,HIGH);}
  if(useRealHomeY){pinMode(yHomePin,INPUT);if(!yHomePinInverted)digitalWriteFast(yHomePin,HIGH);}
  if(useRealHomeZ){pinMode(zHomePin,INPUT);if(!zHomePinInverted)digitalWriteFast(zHomePin,HIGH);}


  // Setup step pins.
  pinMode(stepPin0,OUTPUT);
  pinMode(stepPin1,OUTPUT);
  // pinMode(stepPin2,OUTPUT);

  // Setup dir pins.
  pinMode(dirPin0,OUTPUT);
  pinMode(dirPin1,OUTPUT);
  //  pinMode(dirPin2,OUTPUT);


  // Setup eStop, power, start, stop, pause, resume, program step, spindle, coolant, LED and probe pins.
  //if(useEstopSwitch){pinMode(eStopPin,INPUT);if(!eStopPinInverted){digitalWriteFast(eStopPin,HIGH);}}
  //if(usePowerSwitch){pinMode(powerPin,INPUT);if(!powerPinInverted){digitalWriteFast(powerPin,HIGH);}}
  //if(useStartSwitch){pinMode(startPin,INPUT);if(!startPinInverted){digitalWriteFast(startPin,HIGH);}}
  //if(useStopSwitch){pinMode(stopPin,INPUT);if(!stopPinInverted){digitalWriteFast(stopPin,HIGH);}}
  //if(usePauseSwitch){pinMode(pausePin,INPUT);if(!pausePinInverted){digitalWriteFast(pausePin,HIGH);}}
  //if(useResumeSwitch){pinMode(resumePin,INPUT);if(!resumePinInverted){digitalWriteFast(resumePin,HIGH);}}
  //if(useStepSwitch){pinMode(stepPin,INPUT);if(!stepPinInverted){digitalWriteFast(stepPin,HIGH);}}
  //if(powerLedPin > 0){pinMode(powerLedPin,OUTPUT);digitalWriteFast(powerLedPin,HIGH);}
  //if(eStopLedPin>0){pinMode(eStopLedPin,OUTPUT);digitalWriteFast(eStopLedPin,LOW);}

  // Setup idle indicator led.
  pinMode(idleIndicator,OUTPUT);

  lcd.setCursor (0, 1);
  lcd.print(F("Startup Complete.   "));
  lcd.setCursor (0, 2);
  lcd.print(F("Waiting For LinuxCNC"));
  lcd.setCursor (0, 3);
  lcd.print(F("                    ")); // 20 spaces! must be a better way

  // Setup serial link.
  Serial.begin(BAUD);

  while (!Serial) {
    ; // wait for serial port to connect. Needed for Leonardo/32u4
  }

  // nothing will happen until we have a valid serial connection.
  lcd.setCursor (0, 1);
  lcd.print(F("LinuxCNC Connected. "));
  lcd.setCursor (0, 2);
  lcd.print(F("                    "));
  lcd.setCursor (15, 0);
  lcd.print(F("_____"));

  // Initialize serial command buffer.
  sofar=0;

  // set up timer3 to fire interrupt for transmitting step pulses
  cli();
  TCCR3A = 0;
  TCCR3B = _BV(CS31) | _BV(WGM32); // /8 prescaler, means a 2mhz clock
  OCR3A = stepTime*2; // step time is in us, so double that value for 2mhz clock ticks
  TCNT3 = 0;
  TIMSK3 |= _BV(OCIE3A);
  sei();

}

void loop()
{
  // These are all for physical switches, which we don't have
  //if(useEstopSwitch==true){boolean eStopState=digitalReadFast(eStopPin);if(eStopPinInverted){eStopState=!eStopState;}if(eStopState != eStopStateOld || eStopStateOld){eStopStateOld=eStopState;Serial.print("e");Serial.println(eStopState);delay(500);}}
  //if(usePowerSwitch==true){boolean powerState=digitalReadFast(powerPin);if(powerPinInverted){powerState=!powerState;}if(powerState != powerStateOld){powerStateOld=powerState;if(powerSwitchIsMomentary){Serial.println("pt");}else{Serial.print("p");Serial.println(powerState);}}}
  //if(useStartSwitch==true){boolean startState=digitalReadFast(startPin);if(startPinInverted){startState=!startState;}if(startState != startStateOld){startStateOld=startState;Serial.print("h");Serial.println(startState);}}
  //if(useStopSwitch==true){boolean stopState=digitalReadFast(stopPin);if(stopPinInverted){stopState=!stopState;}if(stopState != stopStateOld){stopStateOld=stopState;Serial.print("h");Serial.println(stopState+2);}}
  //if(usePauseSwitch==true){boolean pauseState=digitalReadFast(pausePin);if(pausePinInverted){pauseState=!pauseState;}if(pauseState != pauseStateOld){pauseStateOld=pauseState;Serial.print("h");Serial.println(pauseState+4);}}
  //if(useResumeSwitch==true){boolean resumeState=digitalReadFast(resumePin);if(resumePinInverted){resumeState=!resumeState;}if(resumeState != resumeStateOld){resumeStateOld=resumeState;Serial.print("h");Serial.println(resumeState+6);}}
  //if(useStepSwitch==true){boolean stepState=digitalReadFast(stepPin);if(stepPinInverted){stepState=!stepState;}if(stepState != stepStateOld){stepStateOld=stepState;Serial.print("h");Serial.println(stepState+8);}}


  // listen for serial commands
  while(Serial.available() > 0) {
    buffer[sofar++]=Serial.read();
    if(buffer[sofar-1]==';') break;  // in case there are multiple instructions
  }

  // Received a "+" turn something on.
  // Setting LCD status information here won't slow down processing.
  if(sofar>0 && buffer[sofar-3]=='+') {

    //if(sofar>0 && buffer[sofar-2]=='P') { /* Power LED & PSU   ON */ if(powerLedPin>0){digitalWriteFast(powerLedPin,HIGH);}if(powerSupplyPin>0){digitalWriteFast(powerSupplyPin,psuState);}}
    if(sofar>0 && buffer[sofar-2]=='E') { eStopState=true; lcd.setCursor (0, 1); lcd.print(F("eStop Open.         "));lcd.setCursor (15, 0); lcd.print(F("E"));};
    if(sofar>0 && buffer[sofar-2]=='P') { powerState=true; lcd.setCursor (0, 1); lcd.print(F("Power Initialised.  "));lcd.setCursor (16, 0); lcd.print(F("P"));};
    //if(sofar>0 && buffer[sofar-2]=='E') { /* E-Stop Indicator  ON */ if(eStopLedPin>0){digitalWriteFast(eStopLedPin,HIGH);}}

    // Running code state
    // These are mutually exclusive so don't need a - set.
    // On startup, don't set these.  purely to look cooler on the lcd
    // Should be a better way
    if(sofar>0 && buffer[sofar-2]=='r') { runState = 'r'; lcd.setCursor (12, 0); lcd.print(F("R"));lcd.setCursor (0, 1); lcd.print(F("Running gCode       "));lcd.setCursor (0, 2);lcd.print(F("Touch to PAUSE      "));};
    // extra check to make sure the machine wasn't off.
    if(sofar>0 && buffer[sofar-2]=='s' && runState!='o' ) { runState = 's'; lcd.setCursor (12, 0); lcd.print(F("S"));lcd.setCursor (0, 1); lcd.print(F("Run Stopped!        "));lcd.setCursor (0, 2);lcd.print(F("                    "));};
    if(sofar>0 && buffer[sofar-2]=='p') { runState = 'r'; lcd.setCursor (12, 0); lcd.print(F("P"));lcd.setCursor (0, 1); lcd.print(F("Run Paused!         "));lcd.setCursor (0, 2);lcd.print(F("Touch to RESUME     "));};


    // homing
    if(sofar>0 && buffer[sofar-2]=='0') { xHomed=true; xOffset = pos_x; lcd.setCursor (0, 1); lcd.print(F("X axis Homed        "));lcd.setCursor (17, 0); lcd.print(F("X"));};
    if(sofar>0 && buffer[sofar-2]=='1') { yHomed=true; yOffset = pos_y; lcd.setCursor (0, 1); lcd.print(F("Y axis Homed        "));lcd.setCursor (18, 0); lcd.print(F("Y"));};
    if(sofar>0 && buffer[sofar-2]=='2') { zHomed=true; zOffset = pos_z; lcd.setCursor (0, 1); lcd.print(F("Z axis Homed        "));lcd.setCursor (19, 0); lcd.print(F("Z"));};

    // reset the buffer
    sofar=0;  
  }

  // Received a "-" turn something off.
  // we have no physical pins, just software
  // so this is mostly not needed.
  if(sofar>0 && buffer[sofar-3]=='-') {
    //if(sofar>0 && buffer[sofar-2]=='P') { /* Power LED & PSU   OFF */ if(powerLedPin>0){ digitalWriteFast(powerLedPin,LOW);}if(powerSupplyPin>0){digitalWriteFast(powerSupplyPin,!psuState);}}
    if(sofar>0 && buffer[sofar-2]=='E') { eStopState=false; lcd.setCursor (0, 1); lcd.print(F("eStop ACTIVE! Halted"));lcd.setCursor (15, 0); lcd.print(F("_"));};
    if(sofar>0 && buffer[sofar-2]=='P') { powerState=false; lcd.setCursor (0, 1); lcd.print(F("Power Down.         "));lcd.setCursor (16, 0); lcd.print(F("_"));};

    //if(sofar>0 && buffer[sofar-2]=='E') { /* E-Stop Indicator  OFF */ if(eStopLedPin>0){digitalWriteFast(eStopLedPin,LOW);}}

    if(sofar>0 && buffer[sofar-2]=='0') { xHomed=false; lcd.setCursor (0, 1); lcd.print(F("X axis Unhomed      "));lcd.setCursor (17, 0); lcd.print(F("_"));};
    if(sofar>0 && buffer[sofar-2]=='1') { yHomed=false; lcd.setCursor (0, 1); lcd.print(F("Y axis Unhomed      "));lcd.setCursor (18, 0); lcd.print(F("_"));};
    if(sofar>0 && buffer[sofar-2]=='2') { zHomed=false; lcd.setCursor (0, 1); lcd.print(F("Z axis Unhomed      "));lcd.setCursor (19, 0); lcd.print(F("_"));};


    // clear the buffer.
    // this means we'll skip the processCommand step
    sofar=0;
  }

  // Received a "?" about something give an answer.
  if(sofar>0 && buffer[sofar-3]=='?') {
    if(sofar>0 && buffer[sofar-2]=='V') { 
      /* Report version */
      Serial.println(VERSION);
      lcd.setCursor (0, 3);
      lcd.print(F(VERSION));
      lcd.print(F("            ")); // match the Version length.  Cheating
    }
    if(sofar>0 && buffer[sofar-2]=='R') {
      /* Report role    */
      Serial.println(ROLE);
      lcd.setCursor (0, 3);
      lcd.print(F(ROLE));
      lcd.print(F("          ")); // match the Role length.  Cheating
    }

    // clear the buffer.
    // this means we'll skip the processCommand step
    sofar=0;
  }

  // if we hit a semi-colon, assume end of instruction.
  // Do NOT EXECUTE instructions if E-STOP or Power is off!
  // eStopState is not implemented yet.   awesome :(
  if(powerState && eStopState && sofar>0 && buffer[sofar-1]==';') {

    buffer[sofar]=0;

    // do something with the command
    processCommand();

    // reset the buffer
    sofar=0;
  }

  // globalBusy is all well and good, but if the machine is working it will never update.
  // We have to find a good balance between performance and update cycle
  // Split this up due to some weirdness using all three values
  if(powerState && eStopState)
  {
    // update every second (1000).  Adjust this value
    // shouldn't need to update the X:, Y: and Z: bits every iteration
    // just update the numbers.
    // X and Y values should be trimmed/padded to 5 characters.  z to 4
    if (lastUpdate < (millis() - 1000) || globalBusy<1 )
    {

      lcd.setCursor (0, 3);
      lcd.print(F("X:"));
      lcd.print(pos_x-xOffset,1);
      lcd.setCursor (7, 3);
      lcd.print(F("Y:"));
      lcd.print(pos_y-yOffset,1);
      lcd.setCursor (14, 3);
      lcd.print(F("Z:"));
      lcd.print(pos_z-zOffset,1);

      lastUpdate = millis();
    }
  }

  // Check if we need to send end-of-move feedback to linuxcnc
  // (this feedback isn't currently used upstream in the HAL)
  if(fbxTrigger){
    Serial.print("fx");
    Serial.println(fbx,6);
    fbxTrigger = false;
  }
  if(fbyTrigger) {
    Serial.print("fy");
    Serial.println(fby,6);
    fbyTrigger = false;
  }
}

