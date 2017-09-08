
#include "PID.h"

#include "pitches.h"

#define FORWARD 0
#define BACKWARD 1

const double mmToTicks = 7.6923;
const double wheelbase = 250; // in mm
unsigned int batteryCapacity = 2000; // mAh

// About 1/4 of the battery voltage is fed to pin battVoltage
// Adjust these constants as needed (plot a few pairs and fit a straight-line through them)
// voltage = analogRead(battVoltage) * batteryVoltageGradient + batteryVoltageOffset;
const double batteryVoltageGradient = 0.0206;
const double batteryVoltageOffset = -0.5964;

// Bias equation to convert required velocity to motor PWM power
//motorPower = velocityBiasGradient * velocity + velocityBiasOffset;
const double velocityBiasGradient = 5.8;
const double velocityBiasOffset = 12;

const int leftBumper =    2; 
const int IN4 =           3; 
const int speaker =       4; 
const int IN3 =           5; 
const int rightBumper =   6; 
const int leftWheelEnc =  7; 
const int rightWheelEnc = 8; 
const int IN2 =           9; 
const int IN1 =           10; 
const int wheelsUp =      11; 
const int switchL =       12; 
const int switchLedL =    13; // pin 13 must be an output (because of LED on arduino) and it gets toggled when bootloading

const int battVoltage =   A0;
const int mainBrush =     A1; 
const int greenLed =      A2; 
const int redLed =        A3; 
const int switchLedM =    A4; 
const int switchLedS =    A5; 
const int switchM =       A6; // A6 and A7 are analog-input only!
const int switchS =       A7; // 

volatile  int leftEncoderCount = 0;
volatile  int rightEncoderCount = 0;
bool leftEncoderForwards = true;
bool rightEncoderForwards = true;

boolean commandComplete = false;  
byte bytes[200];
int bytePos = 0;
int wanted = 0;

int direction = FORWARD;
bool clockwise = true;

int leftEncoder = 0;
int rightEncoder = 0;
double prevLeftEncoder;
double prevRightEncoder;

double leftSetpoint;
double rightSetpoint;

double syncError;
double syncPower;
double syncSetpoint = 0;
double syncFactor = 1;
double requestPower;

double velIP;
double velSP;
double velBias;

double rightPower;
double velRightIP;
double velRightSP;
double velRightBias;
int dirRight;

double leftPower;
double velLeftIP;
double velLeftSP;
double velLeftBias;
int dirLeft;

double distance = 0;
double angle = 0;
double lastDistance = 0;

PID sync(&syncError, &syncPower, &syncSetpoint, 1.0, 0.1, 0, P_ON_E, DIRECT);
PID velPID(&velIP, &requestPower, &velSP, &velBias, 2.0, 2.0, 0, P_ON_E, DIRECT);
PID velLeftPID(&velLeftIP, &leftPower, &velLeftSP, &velLeftBias, 2.0, 2.0, 0, P_ON_E, DIRECT);  
PID velRightPID(&velRightIP, &rightPower, &velRightSP, &velRightBias, 2.0, 2.0, 0, P_ON_E, DIRECT);   

int sampleTime = 15; //  ms
unsigned long lastTime;
bool driving = false;
bool spin = false;

const double ticksToMm = 1.0 / mmToTicks;
const double ticksToDegrees = ticksToMm * 180.0 / (2 * wheelbase * PI);

double voltage = 0;
unsigned long lastBatteryReadTime = 0;


enum DriveMode {
    DM_DRIVE_DIRECT = 0,
    DM_DRIVE = 1,
    DM_DRIVE_PWM = 2
};

enum OIMode {
    OFF = 0, 
    PASSIVE = 1,
    SAFE = 2,
    FULL = 3
};
enum SensorPacketID {
    ID_GROUP_0 = 0,
    ID_GROUP_1 = 1,
    ID_GROUP_2 = 2,
    ID_GROUP_3 = 3,
    ID_GROUP_4 = 4,
    ID_GROUP_5 = 5,
    ID_GROUP_6 = 6,
    ID_GROUP_100 = 100,
    ID_GROUP_101 = 101,
    ID_GROUP_106 = 106,
    ID_GROUP_107 = 107,
    ID_BUMP_WHEELDROP = 7,
    ID_WALL = 8,
    ID_CLIFF_LEFT = 9,
    ID_CLIFF_FRONT_LEFT = 10,
    ID_CLIFF_FRONT_RIGHT = 11,
    ID_CLIFF_RIGHT = 12,
    ID_VIRTUAL_WALL = 13,
    ID_OVERCURRENTS = 14,
    ID_DIRT_DETECT_LEFT = 15,
    ID_DIRT_DETECT_RIGHT = 16,
    ID_IR_OMNI = 17,
    ID_IR_LEFT = 52,
    ID_IR_RIGHT = 53,
    ID_BUTTONS = 18,
    ID_DISTANCE = 19,
    ID_ANGLE = 20,
    ID_CHARGE_STATE = 21,
    ID_VOLTAGE = 22,
    ID_CURRENT = 23,
    ID_TEMP = 24,
    ID_CHARGE = 25,
    ID_CAPACITY = 26,
    ID_WALL_SIGNAL = 27,
    ID_CLIFF_LEFT_SIGNAL = 28,
    ID_CLIFF_FRONT_LEFT_SIGNAL = 29,
    ID_CLIFF_FRONT_RIGHT_SIGNAL = 30,
    ID_CLIFF_RIGHT_SIGNAL = 31,
    ID_CARGO_BAY_DIGITAL_INPUTS = 32,
    ID_CARGO_BAY_ANALOG_SIGNAL = 33,
    ID_CHARGE_SOURCE = 34,
    ID_OI_MODE = 35,
    ID_SONG_NUM = 36,
    ID_PLAYING = 37,
    ID_NUM_STREAM_PACKETS = 38,
    ID_VEL = 39,
    ID_RADIUS = 40,
    ID_RIGHT_VEL = 41,
    ID_LEFT_VEL = 42,
    ID_LEFT_ENC = 43,
    ID_RIGHT_ENC = 44,
    ID_LIGHT = 45,
    ID_LIGHT_LEFT = 46,
    ID_LIGHT_FRONT_LEFT = 47,
    ID_LIGHT_CENTER_LEFT = 48,
    ID_LIGHT_CENTER_RIGHT = 49,
    ID_LIGHT_FRONT_RIGHT = 50,
    ID_LIGHT_RIGHT = 51,
    ID_LEFT_MOTOR_CURRENT = 54,
    ID_RIGHT_MOTOR_CURRENT = 55,
    ID_MAIN_BRUSH_CURRENT = 56,
    ID_SIDE_BRUSH_CURRENT = 57,
    ID_STASIS = 58,
    ID_NUM = 52
};

enum OpCodes {
    OC_RESET = 7,
    OC_DRIVE = 137,
    OC_DRIVE_DIRECT = 145,
    OC_DRIVE_PWM = 146,
    OC_START = 128,
    OC_SAFE = 131,
    OC_FULL = 132,
    OC_STOP = 173,
    OC_CONTROL = 130,
    OC_LEDS = 139,
    OC_SENSORS = 142,
    OC_STREAM = 148,
    OC_TOGGLE_STREAM = 150,
    OC_BAUD = 129,
    OC_DATE = 168,
};

bool accum = false;

byte streamPacketIds[100];
int streamPacketIdsLength = 0;
bool streaming = false;
byte streamPacket[200];
int streamPacketLength = 0;

int requestedVelocity;
int requestedRadius;

int requestedVelRight;
int requestedVelLeft;

enum OIMode mode = OFF;
enum DriveMode driveMode = DM_DRIVE;

int pwmRight = 0;
int pwmLeft = 0;

void setup() 
{
  Serial.begin(115200);
  pinMode(leftBumper, INPUT);
  pinMode(IN4, OUTPUT);
  pinMode(speaker , OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(rightBumper, INPUT);
  pinMode(leftWheelEnc , INPUT);
  pinMode(rightWheelEnc, INPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN1, OUTPUT);
  pinMode(switchLedL, OUTPUT);
  pinMode(switchL , INPUT);
  pinMode(wheelsUp, INPUT);

  pinMode(switchM, INPUT);
  pinMode(switchS, INPUT);
  pinMode(greenLed, OUTPUT);
  pinMode(redLed , OUTPUT);
  pinMode(switchLedM, OUTPUT);
  pinMode(switchLedS, OUTPUT);
  pinMode(battVoltage, INPUT);
  pinMode(mainBrush, OUTPUT);

  setupInterupts();

  digitalWrite(switchLedS, HIGH);
  digitalWrite(switchLedM, HIGH);
  digitalWrite(switchLedL, HIGH);
  digitalWrite(greenLed, LOW);
  digitalWrite(redLed, HIGH);
  digitalWrite(mainBrush, HIGH);

  sync.SetOutputLimits(-128.0, 128.0);
  sync.SetTunings(1.0, 0.1, 0, P_ON_E);
  velPID.SetTunings(2.0, 2.0, 0, P_ON_E);
}

void loop() 
{
  if (commandComplete) 
  {
      ExecCommand(bytes);
      bytePos = 0;
      commandComplete = false;
  }
 
  digitalWrite(switchLedL, !digitalRead(rightBumper));
  digitalWrite(switchLedS, !digitalRead(leftBumper));
 
  if( mode != FULL && (digitalRead(rightBumper) || digitalRead(leftBumper) || !digitalRead(wheelsUp)))
  {
     allStop();
     mode = PASSIVE;
  } 
      
  if(!digitalRead(switchL))
  {
    digitalWrite(switchLedL, LOW);
    delay(500);
    digitalWrite(switchLedL, HIGH);
  }

  MonitorBattery();
  
  SampleSync();
}

void MonitorBattery()
{
   unsigned long now = millis();
    
   if(now - lastBatteryReadTime >= 1000)
   {    
       voltage = GetBatteryVoltage() * 1000;
       
       if(voltage > 12)
       {
          digitalWrite(greenLed, LOW);
          digitalWrite(redLed, HIGH);
       }
       else if (voltage > 4)
       {
          digitalWrite(greenLed, HIGH);
          digitalWrite(redLed, LOW);
       }
       else
       {
          digitalWrite(greenLed, HIGH);
          digitalWrite(redLed, HIGH);
       }

       lastBatteryReadTime = now;
   }
}

double GetBatteryVoltage()
{
  return analogRead(battVoltage) * batteryVoltageGradient + batteryVoltageOffset;
}

unsigned int GetBatteryCharge(unsigned int mV)
{
  if(mV > 14700) // 1400 - 2000
    return 1700;
  if(mV > 13700) // 200 1400
    return 800;
                 // 0 200
  return 0;
}

void ExecCommand(byte* command)
{
  switch(command[0])
  {
    case OC_DRIVE:
      {
        int vel = ((unsigned int)command[1] << 8) | (unsigned int)command[2];
        int rad = ((unsigned int)command[3] << 8) | (unsigned int)command[4];
        Drive(vel, rad);
      }
      break;
    case OC_DRIVE_DIRECT:
      {
        int velR = ((unsigned int)command[1] << 8) | (unsigned int)command[2];
        int velL = ((unsigned int)command[3] << 8) | (unsigned int)command[4];
        DriveDirect(velR, velL);
      }
      break;
    case OC_DRIVE_PWM:
      {
        int pwmR = ((unsigned int)command[1] << 8) | (unsigned int)command[2];
        int pwmL = ((unsigned int)command[3] << 8) | (unsigned int)command[4];
        DrivePWM(pwmR, pwmL);
      }
      break;
    case OC_LEDS:
      {
        byte ledBits = command[1];
        byte powerColour = command[2];
        byte powerIntensity = command[3];

        analogWrite(switchLedM, 256-powerIntensity );
      }
      break;
    case OC_SENSORS:
       {
          byte packetId = command[1];     
          Sensors(packetId);
       }
       break;
    case OC_STREAM:
       {
         streamPacketIdsLength = command[1];
         for(int i= 0; i < streamPacketIdsLength; i++)
           streamPacketIds[i] = command[i+2];
         streaming = true;
       }
       break;
    case OC_TOGGLE_STREAM:
       streaming = command[1] == 1;
       break;
    case OC_START:
       mode = PASSIVE;
       break;
    case OC_SAFE: case OC_CONTROL:
       mode = SAFE;
       break;
    case OC_FULL:
       mode = FULL;
       break;
    case OC_STOP:
       mode = OFF;
       streaming = false;
       break;   
    case OC_RESET:
       mode = OFF;
       streaming = false;
       Serial.println("r3_robot/tags/release-3.4.1");
       break;
    default:
       break;    
  }
}

void Sensors(int id)
{   
  int min;
  int max;
  switch(id)
  {
      case ID_GROUP_0:
         min = ID_BUMP_WHEELDROP; max = ID_CAPACITY;
         break;
      case ID_GROUP_1:
         min = ID_BUMP_WHEELDROP; max = ID_DIRT_DETECT_RIGHT;
         break;
      case ID_GROUP_2:
         min = ID_IR_OMNI; max = ID_ANGLE;
         break;
      case ID_GROUP_3:
         min = ID_CHARGE_STATE; max = ID_CAPACITY;
         break;
      case ID_GROUP_4:
         min = ID_WALL_SIGNAL; max = ID_CHARGE_SOURCE;
         break;
      case ID_GROUP_5:
         min = ID_OI_MODE; max = ID_LEFT_VEL;
         break;
      case ID_GROUP_6:
         min = ID_BUMP_WHEELDROP; max = ID_LEFT_VEL;
         break;
      case ID_GROUP_100:
         min = ID_BUMP_WHEELDROP; max = ID_STASIS;
         break;
      case ID_GROUP_101:
         min = ID_LEFT_ENC; max = ID_STASIS;
         break;
      case ID_GROUP_106:
         min = ID_LIGHT_LEFT; max = ID_LIGHT_RIGHT;
         break;
      case ID_GROUP_107:
         min = ID_LEFT_MOTOR_CURRENT; max = ID_STASIS ;
         break;
      default:
         min = max = id;
  }

  streamPacketIdsLength = 0;
  for(int i = min; i <= max; i++)
      streamPacketIds[streamPacketIdsLength++] = i;

  DoStream(false);
}

void DoStream(bool withChecksum)
{
  streamPacketLength = 0;

  if(withChecksum)
  {
     streamPacket[streamPacketLength++] = 19; // header
     streamPacket[streamPacketLength++] = 0;  // placeholder for length
  }
  
  for(int i = 0; i < streamPacketIdsLength; i++)
  {
    if(withChecksum)
       streamPacket[streamPacketLength++] = streamPacketIds[i];
    
    switch(streamPacketIds[i])
    {
      case ID_BUMP_WHEELDROP: 
         streamPacket[streamPacketLength++] = 
            digitalRead(rightBumper)       | 
            (digitalRead(leftBumper) << 1) | 
            (!digitalRead(wheelsUp)  << 2) | 
            (!digitalRead(wheelsUp)  << 3);
         break;
      case ID_DISTANCE:
         streamPacket[streamPacketLength++] =(( int)distance) >> 8; 
         streamPacket[streamPacketLength++] =(( int)distance) & 0xFF; 
         distance = distance - ((int)distance); // keep remainder for next request
         break;
      case ID_ANGLE:
         streamPacket[streamPacketLength++] =(( int)angle) >> 8; 
         streamPacket[streamPacketLength++] =(( int)angle) & 0xFF; 
         angle = angle - ((int)angle); // keep remainder for next request
         break;
      case ID_VOLTAGE: 
         streamPacket[streamPacketLength++] = ((unsigned int)voltage) >> 8; 
         streamPacket[streamPacketLength++] = ((unsigned int)voltage) & 0xFF; 
         break;
      case ID_CHARGE:
         {
           unsigned int c = GetBatteryCharge(voltage);
           streamPacket[streamPacketLength++] = c >> 8;
           streamPacket[streamPacketLength++] = c & 0xFF;
         }
         break;
      case ID_CAPACITY:
         streamPacket[streamPacketLength++] = batteryCapacity >> 8;;
         streamPacket[streamPacketLength++] = batteryCapacity &0xFF;
         break;
      case ID_OI_MODE:
         streamPacket[streamPacketLength++] = mode;
         break;
      case ID_VEL:
         streamPacket[streamPacketLength++] = (( int)requestedVelocity) >> 8; 
         streamPacket[streamPacketLength++] = (( int)requestedVelocity) & 0xFF; 
         break;
      case ID_RADIUS:
         streamPacket[streamPacketLength++] = (( int)requestedRadius) >> 8; 
         streamPacket[streamPacketLength++] = (( int)requestedRadius) & 0xFF; 
         break;
      case ID_RIGHT_VEL:
         streamPacket[streamPacketLength++] = (( int)requestedVelRight) >> 8; 
         streamPacket[streamPacketLength++] = (( int)requestedVelRight) & 0xFF; 
         break;
      case ID_LEFT_VEL:
         streamPacket[streamPacketLength++] = (( int)requestedVelLeft) >> 8; 
         streamPacket[streamPacketLength++] = (( int)requestedVelLeft) & 0xFF; 
         break;
      case ID_LEFT_ENC:
         streamPacket[streamPacketLength++] = leftEncoder >> 8;
         streamPacket[streamPacketLength++] = leftEncoder & 0xFF;
         break;    
      case ID_RIGHT_ENC:
         streamPacket[streamPacketLength++] = rightEncoder >> 8;
         streamPacket[streamPacketLength++] = rightEncoder & 0xFF;
         break;   
          
      // unimplemented single byte packets
      case ID_WALL: case ID_CLIFF_LEFT: case ID_CLIFF_FRONT_LEFT: case ID_CLIFF_FRONT_RIGHT:
      case ID_CLIFF_RIGHT: case ID_VIRTUAL_WALL : case ID_OVERCURRENTS: case ID_DIRT_DETECT_LEFT:
      case ID_DIRT_DETECT_RIGHT: case ID_IR_OMNI: case ID_BUTTONS:
      case ID_CHARGE_STATE: case ID_TEMP: case ID_CARGO_BAY_DIGITAL_INPUTS: case ID_CHARGE_SOURCE: case ID_SONG_NUM:    
      case ID_PLAYING: case ID_NUM_STREAM_PACKETS: case ID_LIGHT: case ID_IR_LEFT: case ID_IR_RIGHT: case ID_STASIS:
         streamPacket[streamPacketLength++] =0;
         break;

      // unimplemented double byte packets   
      case ID_CURRENT: case ID_WALL_SIGNAL: case ID_CLIFF_LEFT_SIGNAL: case ID_CLIFF_FRONT_LEFT_SIGNAL:
      case ID_CLIFF_FRONT_RIGHT_SIGNAL: case ID_CLIFF_RIGHT_SIGNAL: case ID_CARGO_BAY_ANALOG_SIGNAL:  
      case ID_LIGHT_LEFT: case ID_LIGHT_FRONT_LEFT: case ID_LIGHT_CENTER_LEFT:
      case ID_LIGHT_CENTER_RIGHT: case ID_LIGHT_FRONT_RIGHT: case ID_LIGHT_RIGHT:    
      case ID_LEFT_MOTOR_CURRENT: case ID_RIGHT_MOTOR_CURRENT:
      case ID_MAIN_BRUSH_CURRENT: case ID_SIDE_BRUSH_CURRENT:
         streamPacket[streamPacketLength++] = 0;
         streamPacket[streamPacketLength++] = 0;
         break;    
    }
  }

  if(withChecksum)
  {
    streamPacket[1] = streamPacketLength - 2; // set length byte
    unsigned int sum = 0;
    for(int i = 0; i < streamPacketLength; i++)
       sum += streamPacket[i];

    // set checksum byte
    streamPacket[streamPacketLength++] = (unsigned int)(256 - (sum & 0xFF)) & 0xFF;
  }
  
  Serial.write(streamPacket, streamPacketLength);  
}

void DrivePWM(int pwmR, int pwmL)
{
  driveMode = DM_DRIVE_PWM;
  
  if(pwmR == 0 && pwmL == 0)
  {
    allStop();
    return;
  }
  
  dirRight = pwmR >=0 ? FORWARD : BACKWARD;
  dirLeft =  pwmL >=0 ? FORWARD : BACKWARD;

  pwmRight = constrain(abs(pwmR), 0, 255);
  pwmLeft =  constrain(abs(pwmL), 0, 255);
 
  driving = true;
  lastTime = millis();  
}

void DriveDirect(int velRight, int velLeft)
{
  driveMode = DM_DRIVE_DIRECT;
  
  requestedVelRight = velRight;
  requestedVelLeft = velLeft;
  
  if(velRight == 0 && velLeft == 0)
  {
    allStop();
    return;
  }

  velRightSP = (abs(constrain(velRight, -500, 500)) * mmToTicks * 15)/1000;
  velLeftSP =  (abs(constrain(velLeft,  -500, 500)) * mmToTicks * 15)/1000;

  dirRight = velRight >=0 ? FORWARD : BACKWARD;
  dirLeft =  velLeft  >=0 ? FORWARD : BACKWARD;
   
  spin = dirLeft != dirRight;
  
  clockwise = spin ? dirRight == BACKWARD : velRight < velLeft;

  velRightPID.SetMode(AUTOMATIC);
  velRightPID.SetOutputLimits(0,255);
  velLeftPID.SetMode(AUTOMATIC);
  velLeftPID.SetOutputLimits(0,255);
  
  driving = true;
  lastTime = millis();  
}

void Drive(int velocity, int radius)
{
  driveMode = DM_DRIVE;
  requestedVelocity = velocity;
  requestedRadius = radius;
  
  if(velocity == 0)
  {
    allStop();
    return;
  }
  
  direction = velocity >=0 ? FORWARD : BACKWARD;
  clockwise = radius < 0;
  spin = radius == -1 || radius == 1;
  
  if(radius == 32768 || radius == -32768 || radius == 32767)
     syncFactor = 1;
  else if (spin)
     syncFactor = 1;
  else
     syncFactor = (2 * abs(constrain(radius, -2000, 2000)) - wheelbase) / (2 * abs(constrain(radius, -2000, 2000)) + wheelbase);

  velSP = (abs(constrain(velocity, -500, 500)) * mmToTicks * 15)/1000;
  
  syncPower = 0;
  sync.SetMode(AUTOMATIC);
  sync.SetOutputLimits(-128,128);
  velPID.SetMode(AUTOMATIC);
  velPID.SetOutputLimits(0,255);
  
  driving = true; 
  lastTime = millis(); 
}


void SampleSync()
{  
   unsigned long now = millis();
   unsigned long timeChange = now - lastTime;
   
   if(timeChange >= sampleTime)
   {    
      // get enc counts
      noInterrupts();
          leftEncoder = leftEncoderCount;
          rightEncoder = rightEncoderCount;
      interrupts();
        
      if(driving)
      {                   
        double leftEncoderDiff = abs(leftEncoder - prevLeftEncoder);
        double rightEncoderDiff = abs(rightEncoder - prevRightEncoder);

        switch(driveMode)
        {
          case DM_DRIVE_DIRECT:
          {
             velIP = (leftEncoderDiff + rightEncoderDiff) / 2.0;              
             velRightIP = rightEncoderDiff;      
             velLeftIP = leftEncoderDiff;  
  
             if(spin)
             {
                angle += (rightEncoderDiff + leftEncoderDiff) * 2 * (clockwise ? -ticksToDegrees : ticksToDegrees);
             }
             else
             {
               angle += (rightEncoderDiff - leftEncoderDiff) * ticksToDegrees;
            
               if(dirLeft == FORWARD)
                 distance += velIP * ticksToMm;
               else
                distance -= velIP * ticksToMm;
              }  
                 
              velRightBias = velocityBiasGradient * velRightSP + velocityBiasOffset;
              velLeftBias = velocityBiasGradient * velLeftSP + velocityBiasOffset;
              velRightPID.Compute(); // compute the rightPower   
              velLeftPID.Compute(); // compute the leftPower   
          
              LeftMotorPower(leftPower, dirLeft);
              RightMotorPower(rightPower, dirRight);                      
          }
          break;
          
          case DM_DRIVE:
          {
              velIP = (leftEncoderDiff + rightEncoderDiff) / 2.0;                  
              velBias = velocityBiasGradient * velSP + velocityBiasOffset;
  
              if(spin)
              {
                   angle += (rightEncoderDiff + leftEncoderDiff) * 2 * (clockwise ? -ticksToDegrees : ticksToDegrees);
              }
              else
              {
                  angle += (rightEncoderDiff - leftEncoderDiff) * ticksToDegrees;
            
                  if (direction == FORWARD)
                     distance += velIP * ticksToMm;
                  else
                     distance -= velIP * ticksToMm;
              }  
          
              if (clockwise)
                  syncError = syncFactor * leftEncoderDiff  - rightEncoderDiff;
              else
                  syncError = syncFactor * rightEncoderDiff - leftEncoderDiff;
           
              velPID.Compute(); // compute the requestPower         
              sync.Compute();   // compute the syncPower 
                  
              int pos = requestPower + syncPower;
              int neg = requestPower - syncPower;
  
  //           Serial.print(direction == FORWARD ? "FOR " : "BACK ");Serial.print(turn == CW ? "CW " : "CCW ");Serial.print(spin ? " SPIN " : "");
  //           Serial.print(" le:");Serial.print(leftEncoderDiff); Serial.print(" re:");Serial.print(rightEncoderDiff); 
  //           Serial.print(" ff:");Serial.print(followFactor); Serial.print(" req:");Serial.print(requestPower); Serial.print(" pos:");Serial.print(pos);Serial.print(" neg:");Serial.print(neg);Serial.println();
   
              if (clockwise)
              {
                  LeftMotorPower(pos, direction);
                  RightMotorPower(neg, spin ? ( direction == FORWARD ? BACKWARD : FORWARD) : direction);               
              }
              else
              {
                  LeftMotorPower( neg, spin ? ( direction == FORWARD ? BACKWARD : FORWARD) : direction);
                  RightMotorPower( pos, direction); 
              }
           }
           break;
           case DM_DRIVE_PWM:
           {
               LeftMotorPower(pwmLeft, dirLeft);
               RightMotorPower(pwmRight, dirRight);
           }
           break;
        }
      }
      
      if(streaming)
           DoStream(true);        
           
      lastTime = now;  
         
      prevLeftEncoder = leftEncoder;
      prevRightEncoder = rightEncoder;   
   }
}


void allStop()
{
  if(!driving)
     return;
  
  driving = false;
  
  RightMotorPower( 0, FORWARD); 
  LeftMotorPower(  0, FORWARD); 
}

void LeftMotorPower(int power, int dir)
{
  leftEncoderForwards = dir == FORWARD;
  analogWrite(leftEncoderForwards ? IN3 : IN4, 0);
  analogWrite(leftEncoderForwards ? IN4 : IN3, constrain(power, 0, 255)); //IN4 = left forward
}

void RightMotorPower(int power, int dir)
{
  rightEncoderForwards = dir == FORWARD;
  analogWrite(rightEncoderForwards ? IN2 : IN1, 0);
  analogWrite(rightEncoderForwards ? IN1 : IN2, constrain(power, 0, 255)); //IN1 = right forward  
}

void setupInterupts()
{
  PCMSK0 |= bit (PCINT0);  // want pin 8
  PCIFR  |= bit (PCIF0);   // clear any outstanding interrupts
  PCICR  |= bit (PCIE0);   // enable pin change interrupts for D8 to D13

  PCMSK2 |= bit (PCINT23);  // want pin 7
  PCIFR  |= bit (PCIF2);   // clear any outstanding interrupts
  PCICR  |= bit (PCIE2);   // enable pin change interrupts for D0 to D7

}

ISR (PCINT2_vect)
{
  // handle pin change interrupt for D0 to D7 here
  if(leftEncoderForwards)
      leftEncoderCount++;
  else
      leftEncoderCount--;     
}  


ISR (PCINT0_vect)
{
  // handle pin change interrupt for D8 to D13 here
  if(rightEncoderForwards)
      rightEncoderCount++;
  else
      rightEncoderCount--;
}  

void serialEvent() 
{
  while (Serial.available()) 
  {
    byte opCode = (byte)Serial.read(); 
    bytes[bytePos++] = opCode;

    if(accum)
    {
      if(wanted == -1)
      {
        wanted = (int)opCode;
        continue;
      }
      if (--wanted > 0)
        continue;

      accum = false;
      commandComplete = true;  
      break;
    }
    else
    {
      switch(opCode)
      {
        case OC_START:  case OC_STOP:  case OC_CONTROL:
           wanted = 0;
           break;
        case OC_SENSORS:  case OC_TOGGLE_STREAM: case OC_BAUD:
           wanted = 1;
           break;  
        case OC_LEDS:  case OC_DATE: 
           wanted = 3;
           break;  
        case OC_DRIVE: case OC_DRIVE_DIRECT: case OC_DRIVE_PWM:
           wanted = 4;
           break;
        case OC_STREAM:
           wanted = -1; // -1 means next byte contains byte count
           break;  
        deafult:
           wanted = 0;
           break;
      }

      if(wanted == 0)
      {
        accum = false;
        commandComplete = true;
        break;
      }
      else
        accum = true;
    }       
  }
}


void beep(int count)
{  
  for(int i = 0 ; i < count; i++)
  {
     tone(speaker, NOTE_A7,50);
     delay(200);
  }
  
  delay(700);
}
