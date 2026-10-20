int ledPin = 2;   
char myData = 0;

int motor1Pin1 = 27;
int motor1Pin2 = 26;
int enable1Pin = 12;

// Setting PWM properties (ESP32 Core v2)
const int freq = 30000;
const int pwmChannel = 0; // Channel PWM 0
const int resolution = 8;
int dutyCycle = 200;

const byte pin_rpm = 10;
int volatile rev = 0;

void setup() {
  pinMode(ledPin, OUTPUT);
  Serial.begin(115200);
  
  pinMode(motor1Pin1, OUTPUT);
  pinMode(motor1Pin2, OUTPUT);
  pinMode(pin_rpm, INPUT_PULLUP);

  // Konfigurasi PWM
  ledcSetup(pwmChannel, freq, resolution);
  ledcAttachPin(enable1Pin, pwmChannel);

  Serial.print("Testing result...");
}

void MotorOn(){
  dutyCycle = 250;
  digitalWrite(motor1Pin1, HIGH);
  digitalWrite(motor1Pin2, LOW);
  ledcWrite(pwmChannel, dutyCycle);
}

void MotorOff(){
  dutyCycle = 0;
  digitalWrite(motor1Pin1, HIGH);
  digitalWrite(motor1Pin2, LOW);
  ledcWrite(pwmChannel, dutyCycle);
}

void loop() {
  myData = int(Serial.read());
  if (myData == '1'){
    Serial.println("LED is ON !!!");
    digitalWrite(ledPin, HIGH);
    MotorOn();
  } else if (myData == '0'){
    Serial.println("LED is OFF !!!");
    digitalWrite(ledPin, LOW);
    MotorOff();
  }
}