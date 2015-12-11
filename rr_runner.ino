
#include <Servo.h>
#include <EEPROM.h>

const bool demoDebug = false;

typedef struct {
  long offset;
  char* command;
} commandStruct;

commandStruct demoSequence[] = {
  {0, "cntr"},
  {1000, "sw 60"},
  {1500, "ud 60"},
  {2000, "fb 140"},
  {2500, "clc"},
  {3000, "ud 100"},
  {3500, "sw 120"},
  {4000, "ud 90"},
  {4500, "clo"},
  {5000, "fb 80"},
  {5500, "sw 90"},
  {6000, "clc"},
  {6500, "cntr"},
  {7000, "sw 120"},
  {7500, "ud 80"},
  {8000, "fb 140"},
  {8500, "clc"},
  {9000, "ud 100"},
  {9500, "sw 60"},
  {10000, "ud 60"},
  {10500, "clo"},
  {11000, "fb 80"},
  {11500, "cntr"},
  {12000, "clc"},
  {14000, "rnd"},
  {-1, "end"} // Must be present
};

const unsigned short cmdDelay = 25;

// detach the servos after 3 seconds
const unsigned long autoTurnOff = 1500;

// start the demo program after this much time (ms)
const unsigned long demoStartTimeout = 60000; // 120000;

void printVersion() {
  Serial.println("rr_runner v0.1");
}

const unsigned short idxLeft = 0;
const unsigned short idxRight = 1;
const unsigned short idxForward = 2;
const unsigned short idxBack = 3;
const unsigned short idxUp = 4;
const unsigned short idxDown = 5;
const unsigned short idxGrab = 6;
const unsigned short idxLetGo = 7;

const unsigned short ledPin = 13;

const unsigned short clawPin = 0;
const unsigned short swivelPin = 1;
const unsigned short fwdbackPin = 2;
const unsigned short updownPin = 3;

const unsigned short ldrPin = A1;

Servo clawServo;
Servo swivelServo;
Servo updownServo;
Servo fwdbackServo;

String inputString = "";
boolean stringComplete = false;

short minSwivel;
short maxSwivel;

short minUpDown;
short maxUpDown;

short minFwdBack;
short maxFwdBack;

short clawOpen;
short clawClose;

short currentSwivel;
short desiredSwivel;
short currentUpDown;
short desiredUpDown;
short currentFwdBack;
short desiredFwdBack;

bool slowMode = true;

bool servosAttached = false;

bool commandFromSerial = true;
bool runningDemo = false;
unsigned short nextStep = 0;

unsigned long lastCommandMillis = 0;
unsigned long lastStatusMillis = 0;
unsigned long demoStartMillis = 0;

int prevLightValue = 0;

void setup() {
  ledOn();
 
  Serial.begin(57600);
  inputString.reserve(16);

  short tmpMin;
  short tmpMax;

  EEPROM.get(idxLeft * sizeof(short), tmpMin);
  EEPROM.get(idxRight * sizeof(short), tmpMax);
  minSwivel = min(tmpMin, tmpMax);
  maxSwivel = max(tmpMin, tmpMax);

  EEPROM.get(idxUp * sizeof(short), tmpMin);
  EEPROM.get(idxDown * sizeof(short), tmpMax);
  minUpDown = min(tmpMin, tmpMax);
  maxUpDown = max(tmpMin, tmpMax);

  EEPROM.get(idxForward * sizeof(short), tmpMin);
  EEPROM.get(idxBack * sizeof(short), tmpMax);
  minFwdBack = min(tmpMin, tmpMax);
  maxFwdBack = max(tmpMin, tmpMax);

  EEPROM.get(idxGrab * sizeof(short), clawClose);
  EEPROM.get(idxLetGo * sizeof(short), clawOpen);

  dumpLimits();

  clawServo.attach(clawPin);
  swivelServo.attach(swivelPin);
  updownServo.attach(updownPin);
  fwdbackServo.attach(fwdbackPin);
  servosAttached = true;

  slowMode = false;
  gotoCenters(false);
  slowMode = true;

  printVersion();

  ledOff();

  lastCommandMillis = millis();
  lastStatusMillis = millis();
}

void ledOn() {
  Bean.setLed(255, 255, 255);
}

void ledOff() {
  Bean.setLed(0, 0, 0);
}

void softwareReset() {
  asm volatile ("  jmp 0");
} 

void loop() {
  handleSerial();
  if (stringComplete) {
    if (commandFromSerial) {
      runningDemo = false;
    }
    turnOn();
    ledOn();
    if (inputString.startsWith("rst")) {
      softwareReset();
    } else if (inputString.startsWith("clo")) {
      openClaw(true);
    } else if (inputString.startsWith("clc")) {
      closeClaw(true);
    } else if (inputString.startsWith("sw")) {
      swTo((short)inputString.substring(2).toInt(), true);
    } else if (inputString.startsWith("ud")) {
      udTo((short)inputString.substring(2).toInt(), true);
    } else if (inputString.startsWith("fb")) {
      fbTo((short)inputString.substring(2).toInt(), true);
    } else if (inputString.startsWith("sm")) {
      slowMode = !inputString.equals("smoff");
    } else if (inputString.startsWith("cntr")) {
      gotoCenters(true);
    } else if (inputString.startsWith("rnd")) {
      randomize();
    } else if (inputString.startsWith("on")) {
      turnOn(true);
    } else if (inputString.startsWith("off")) {
      turnOff(true);
    } else if (inputString.startsWith("?") || inputString.startsWith("help")) {
      Serial.println("rst");
      Serial.println("clo");
      Serial.println("clc");
      Serial.println("sw<num>");
      Serial.println("ud<num>");
      Serial.println("fb<num>");
      Serial.println("cntr");
      Serial.println("rnd");
      Serial.println("on");
      Serial.println("off");
    } else if (inputString.startsWith("ver")) {
      printVersion();
    }
    inputString="";
    stringComplete = false;
    ledOff();
    lastCommandMillis = millis();
  }

  // If we're in slow mode, there is stuff to do later on as well
  if (slowMode) {
    if (desiredSwivel != currentSwivel) {
      short dSw = desiredSwivel - currentSwivel;
      if (dSw < -2)
        dSw = -2;
      if (dSw > 2)
        dSw = 2;
      currentSwivel += dSw;
      swivelServo.write(currentSwivel);
    }
    if (desiredUpDown != currentUpDown) {
      short dUd = desiredUpDown - currentUpDown;
      if (dUd < -2)
        dUd = -2;
      if (dUd > 2)
        dUd = 2;
      currentUpDown += dUd;
      updownServo.write(currentUpDown);
    }
    if (desiredFwdBack != currentFwdBack) {
      short dFb = desiredFwdBack - currentFwdBack;
      if (dFb < -2)
        dFb = -2;
      if (dFb > 2)
        dFb = 2;
      currentFwdBack += dFb;
      fwdbackServo.write(currentFwdBack);
    }
    delay(10);
  }

  // Do the automated stuff...
  unsigned long currentMillis = millis();

  if (servosAttached && currentMillis - lastCommandMillis > autoTurnOff) {
    turnOff();
  }

  if (!runningDemo) {
//    if (currentMillis - lastCommandMillis > demoStartTimeout) {
//      demoStartMillis = millis();
//      runningDemo = true;
//      commandFromSerial = false;
//      nextStep = 0;
//      if (demoDebug) {
//        Serial.println("demo start");
//      }
//    }
    if (currentMillis - lastStatusMillis > 1000) {
      int val = analogRead(ldrPin);

      // TODO: Calculate the light values, and start the demo mode if appropriate
      if (val < prevLightValue / 2) {
        demoStartMillis = millis();
        runningDemo = true;
        commandFromSerial = false;
        nextStep = 0;
        if (demoDebug) {
          Serial.println("demo start");
        }
      }

      prevLightValue = val;
      lastStatusMillis = millis();
    }
  } else {
    if (demoSequence[nextStep].offset == -1) {
      lastCommandMillis = millis();
      runningDemo = false;
      if (demoDebug) {
        Serial.println("demo end");
      }
    } else if (currentMillis - demoStartMillis > demoSequence[nextStep].offset) {
      inputString = demoSequence[nextStep].command;
      if (demoDebug) {
        Serial.print("demo step: ");
        Serial.println(inputString);
      }
      stringComplete = true;
      nextStep++;
    }
  }
}

void handleSerial() {
  while (Serial.available()) {
    char inChar = (char)Serial.read();
    if (inChar > 31 && inChar < 128) {
      inputString += inChar;
    }
    if (inChar == '\n') {
      stringComplete = true;
      commandFromSerial = true;
    }
  }
}

void openClaw(bool reportStatus) {
  clawServo.write(clawOpen);
  if (reportStatus && !runningDemo) {
    delay(cmdDelay);
    Serial.println("ok open");
  }
}

void closeClaw(bool reportStatus) {
  clawServo.write(clawClose);
  if (reportStatus && !runningDemo) {
    delay(cmdDelay);
    Serial.println("ok closed");
  }
}

void swTo(short value, bool reportStatus) {
  short newValue = max(minSwivel, min(maxSwivel, value));
  if (slowMode) {
    desiredSwivel = newValue;
  } else {
    currentSwivel = desiredSwivel = newValue;
    swivelServo.write(newValue);
  }
  if (reportStatus && !runningDemo) {
    delay(cmdDelay);
    Serial.print("ok sw ");
    Serial.println(newValue);
  }
}

void udTo(short value, bool reportStatus) {
  short newValue = max(minUpDown, min(maxUpDown, value));
  if (slowMode) {
    desiredUpDown = newValue;
  } else {
    currentUpDown = desiredUpDown = newValue;
    updownServo.write(newValue);
  }
  if (reportStatus && !runningDemo) {
    delay(cmdDelay);
    Serial.print("ok ud ");
    Serial.println(newValue);
  }
}

void fbTo(short value, bool reportStatus) {
  short newValue = max(minFwdBack, min(maxFwdBack, value));
  if (slowMode) {
    desiredFwdBack = newValue;
  } else {
    currentFwdBack = desiredFwdBack = newValue;
    fwdbackServo.write(newValue);
  }
  if (reportStatus && !runningDemo) {
    delay(cmdDelay);
    Serial.print("ok fb ");
    Serial.println(newValue);
  }
}

void dumpLimits() {
  Serial.print("minSwivel: "); Serial.println(minSwivel);
  Serial.print("maxSwivel: "); Serial.println(maxSwivel);
  Serial.print("minUpDown: "); Serial.println(minUpDown);
  Serial.print("maxUpDown: "); Serial.println(maxUpDown);
  Serial.print("minFwdBack: "); Serial.println(minFwdBack);
  Serial.print("maxFwdBack: "); Serial.println(maxFwdBack);
  Serial.print("clawOpen: "); Serial.println(clawOpen);
  Serial.print("clawClose: "); Serial.println(clawClose);
}

void gotoCenters(bool reportStatus) {
  openClaw(false);
  swTo((minSwivel + maxSwivel) / 2, false);
  udTo((minUpDown + maxUpDown) / 2, false);
  fbTo((minFwdBack + maxFwdBack) / 2, false);
  delay(cmdDelay);
  if (!runningDemo && reportStatus) {
    Serial.println("ok cntr");
  }
}

void randomize() {
  swTo(random(minSwivel, maxSwivel), false);
  udTo(random(minUpDown, maxUpDown), false);
  fbTo(random(minFwdBack, maxFwdBack), false);
  delay(cmdDelay);
  if (!runningDemo) {
    Serial.println("ok rnd");
  }
}

void turnOn() {
  turnOn(false);  
}

void turnOn(bool reportStatus) {
  if (!servosAttached) {
    clawServo.attach(clawPin);
    swivelServo.attach(swivelPin);
    updownServo.attach(updownPin);
    fwdbackServo.attach(fwdbackPin);
    servosAttached = true;
    if (reportStatus && !runningDemo) {
      Serial.println("ok on");
    }
  }
}

void turnOff() {
  turnOff(false);  
}

void turnOff(bool reportStatus) {
  if (servosAttached) {
    clawServo.detach();
    swivelServo.detach();
    updownServo.detach();
    fwdbackServo.detach();
    servosAttached = false;
    if (reportStatus && !runningDemo) {
      Serial.println("ok off");
    }
  }
}

