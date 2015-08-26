
#include <Servo.h>
#include <EEPROM.h>

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

const unsigned short clawPin = 8;
const unsigned short swivelPin = 9;
const unsigned short fwdbackPin = 10;
const unsigned short updownPin = 11;

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

void setup() {
  pinMode(ledPin, OUTPUT);
  ledOn();
  Serial.begin(9600);
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

  slowMode = false;
  gotoCenters();
  slowMode = true;

  printVersion();

  ledOff();
}

void ledOn() {
  digitalWrite(ledPin, HIGH);
}

void ledOff() {
  digitalWrite(ledPin, LOW);
}

void softwareReset() {
  asm volatile ("  jmp 0");
} 

void loop() {
  handleSerial();
  if (stringComplete) {
    ledOn();
    if (inputString.startsWith("rst")) {
      softwareReset();
    } else if (inputString.startsWith("clo")) {
      openClaw();
    } else if (inputString.startsWith("clc")) {
      closeClaw();
    } else if (inputString.startsWith("sw")) {
      swTo((short)inputString.substring(2).toInt());
    } else if (inputString.startsWith("ud")) {
      udTo((short)inputString.substring(2).toInt());
    } else if (inputString.startsWith("fb")) {
      fbTo((short)inputString.substring(2).toInt());
    } else if (inputString.startsWith("sm")) {
      slowMode = !inputString.equals("smoff");
    } else if (inputString.startsWith("cntr")) {
      gotoCenters();
    } else if (inputString.startsWith("rnd")) {
      randomize();
    } else if (inputString.startsWith("?") || inputString.startsWith("help")) {
      Serial.println("rst");
      Serial.println("clo");
      Serial.println("clc");
      Serial.println("sw<num>");
      Serial.println("ud<num>");
      Serial.println("fb<num>");
      Serial.println("cntr");
      Serial.println("rnd");
    } else if (inputString.startsWith("ver")) {
      printVersion();
    }
    inputString="";
    stringComplete = false;
    ledOff();
  }
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
}

void handleSerial() {
  while (Serial.available()) {
    char inChar = (char)Serial.read();
    if (inChar > 31 && inChar < 128) {
      inputString += inChar;
    }
    if (inChar == '\n') {
      stringComplete = true;
    }
  }
}

void openClaw() {
  clawServo.write(clawOpen);
}

void closeClaw() {
  clawServo.write(clawClose);
}

void swTo(short value) {
  short newValue = max(minSwivel, min(maxSwivel, value));
  if (slowMode) {
    desiredSwivel = newValue;
  } else {
    currentSwivel = desiredSwivel = newValue;
    swivelServo.write(newValue);
  }
}

void udTo(short value) {
  short newValue = max(minUpDown, min(maxUpDown, value));
  if (slowMode) {
    desiredUpDown = newValue;
  } else {
    currentUpDown = desiredUpDown = newValue;
    updownServo.write(newValue);
  }
}

void fbTo(short value) {
  short newValue = max(minFwdBack, min(maxFwdBack, value));
  if (slowMode) {
    desiredFwdBack = newValue;
  } else {
    currentFwdBack = desiredFwdBack = newValue;
    fwdbackServo.write(newValue);
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

void gotoCenters() {
  openClaw();
  swTo((minSwivel + maxSwivel) / 2);
  udTo((minUpDown + maxUpDown) / 2);
  fbTo((minFwdBack + maxFwdBack) / 2);
}

void randomize() {
  swTo(random(minSwivel, maxSwivel));
  udTo(random(minUpDown, maxUpDown));
  fbTo(random(minFwdBack, maxFwdBack));
}

