#include <Wire.h>
#include <RTClib.h>
#include <LiquidCrystal_I2C.h>

int RedLed = 12;
int GreenLed = 11;

RTC_DS3231 rtc;

const int coinPin = 9;  // Digital pin to which the coin acceptor is connected
unsigned long lastPulseTime = 0;
unsigned long pulseWidth = 0;
unsigned long pulseSequenceStartTime = 0;
//const unsigned long coin1PulseDuration = 600;   // Pulse duration for Coin 1 (in ms)
const unsigned long coin1PulseDurations[] = {600, 599, 601};
unsigned long coin1PulseDuration = 0;
const unsigned long coin2PulseDuration = 1000;  // Pulse duration for Coin 2 (in ms)
bool coin1Detected = false;                     // Flag to track Coin 1 detection
unsigned long coin1DetectionTime = 0;           // Time of the last Coin 1 detection

const int relayPin = 8;  // Digital pin to which the relay module is connected

bool relayActive = false;
unsigned long relayStartTime = 0;  // To store the relay start time
unsigned long relayDuration = 0;   // To store the relay duration

unsigned long previousMillis = 0;     // Stores the last time the action was performed
const unsigned long interval = 1000;  // 1 second in milliseconds

LiquidCrystal_I2C lcd(0x3F, 16, 2);

void setup() {
  Serial.begin(9600);
  pinMode(RedLed, OUTPUT);
  pinMode(GreenLed, OUTPUT);
  pinMode(coinPin, INPUT_PULLUP);
  pinMode(relayPin, OUTPUT);

  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC");
    while (1)
      ;
  }

  if (rtc.lostPower()) {
    Serial.println("RTC lost power, let's set the time!");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  lcd.init();
  lcd.backlight();

  digitalWrite(RedLed, HIGH);
  digitalWrite(GreenLed, LOW);
  digitalWrite(relayPin, LOW);

  lcd.clear();
  lcd.setCursor(5, 0);
  lcd.print("SYSTEM");
  lcd.setCursor(1, 1);
  lcd.print("INITIALIZATION");
  delay(2000);
}
void loop() {
  LedIndicator();
  int coinState = digitalRead(coinPin);
  unsigned long currentTime = millis();
  DateTime now = rtc.now();

  // Check for a falling edge (coin insertion)
  if (coinState == LOW && currentTime - lastPulseTime > 50) {
    pulseWidth = 0;
    pulseSequenceStartTime = currentTime;

    // Measure the pulse width
    while (coinState == LOW) {
      currentTime = millis();
      coinState = digitalRead(coinPin);
    }
    pulseWidth = currentTime - lastPulseTime;
    Serial.println(pulseWidth);
    // Wait for 100 milliseconds to see if additional pulses are generated
    delay(100);

    // Check for additional pulses within the 100 ms window
    coinState = digitalRead(coinPin);
    while (coinState == LOW && (millis() - pulseSequenceStartTime) <= 100) {
      coinState = digitalRead(coinPin);
    }
    for (int i = 0; i < sizeof(coin1PulseDurations) / sizeof(coin1PulseDurations[0]); i++) {
      if (pulseWidth >= coin1PulseDurations[i] - 1 && pulseWidth <= coin1PulseDurations[i] + 1) {
        coin1PulseDuration = coin1PulseDurations[i];
        break;  // Exit the loop once a valid value is found
      }
    }
    // Distinguish between coins based on pulse width and additional pulses
    if (pulseWidth <= coin1PulseDuration && coinState == LOW && !coin1Detected) {
      Serial.println("Coin 1 Detected");
      if (!relayActive) {
        relayDuration = 60;                  // 1 minute in seconds
        relayStartTime = now.secondstime();  // Store the current time as relay start time
        digitalWrite(relayPin, HIGH);        // Turn on the relay
        relayActive = true;
      } else {
        addTimeToRelay(60);  // Add 1 minute to the remaining time
      }
      coin1Detected = true;           // Set the flag to true to indicate Coin 1 detection
      coin1DetectionTime = millis();  // Record the time of Coin 1 detection
      // Perform your task here
    } else if (pulseWidth >= coin2PulseDuration && coinState == HIGH) {
      Serial.println("Coin 2 Detected");
      if (!relayActive) {
        relayDuration = 120;                 // 2 minutes in seconds
        relayStartTime = now.secondstime();  // Store the current time as relay start time
        digitalWrite(relayPin, HIGH);        // Turn on the relay
        relayActive = true;
      } else {
        addTimeToRelay(120);  // Add 2 minutes to the remaining time
      }
      // Perform your task for Coin 2 here
    } else {
      Serial.println("Unknown coin");
    }

    lastPulseTime = currentTime;
  }

  // Reset Coin 1 detection flag after one second
  if (coin1Detected && (millis() - coin1DetectionTime) >= 1000) {
    coin1Detected = false;
  }

  // Add a small delay to avoid false detections
  delay(10);

  // Check if it's time to turn off the relay based on RTC
  if (relayActive) {
    unsigned long elapsedTime = now.secondstime() - relayStartTime;

    if (elapsedTime >= relayDuration) {
      digitalWrite(relayPin, LOW);  // Turn off the relay
      relayActive = false;
      relayDuration = 0;
    } else {
      unsigned long timeRemaining = relayDuration - elapsedTime;
      unsigned long currentMillis = millis();
      if (currentMillis - previousMillis >= interval) {
        unsigned long minutes = timeRemaining / 60;
        unsigned long seconds = timeRemaining % 60;
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("SOCKET: ON");
        lcd.setCursor(0, 1);
        lcd.print("T: ");
        lcd.setCursor(3, 1);
        lcd.print(minutes);
        lcd.setCursor(5, 1);
        lcd.print("min");
        lcd.setCursor(10, 1);
        lcd.print(seconds);
        lcd.setCursor(13, 1);
        lcd.print("sec");
        Serial.print("Time Remaining: ");
        Serial.print(minutes);
        Serial.print(" min ");
        Serial.print(seconds);
        Serial.println(" sec");
        previousMillis = currentMillis;
      }
    }
  }


  // Add a small delay to avoid false detections
  delay(10);
}
void LedIndicator() {
  if (digitalRead(relayPin) == LOW) {
    digitalWrite(RedLed, HIGH);
    digitalWrite(GreenLed, LOW);
    lcd.clear();
    lcd.setCursor(1, 0);
    lcd.print("SMART CHARGING");
    lcd.setCursor(0, 1);
    lcd.print("SOCKET: OFF");
    delay(100);
  } else if (digitalRead(relayPin) == HIGH) {
    digitalWrite(GreenLed, HIGH);
    digitalWrite(RedLed, LOW);
  }
}
void addTimeToRelay(unsigned long additionalTime) {
  relayDuration += additionalTime;
}
