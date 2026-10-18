/****************************************************
   SMART RFID DOOR SECURITY SYSTEM
   NodeMCU ESP8266 + RC522 + Servo + Blynk
   Green LED + Red LED + Buzzer
   Authorized UID: 91:66:2A:F4
****************************************************/

#define BLYNK_TEMPLATE_ID "YOUR_TEMPLATE_ID"
#define BLYNK_TEMPLATE_NAME "Smart door"
#define BLYNK_AUTH_TOKEN "YOUR_NEW_AUTH_TOKEN"

#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Servo.h>

// Wi-Fi credentials
char ssid[] = "YOUR_WIFI_NAME";
char pass[] = "YOUR_WIFI_PASSWORD";

// RFID pins
#define SS_PIN  D2
#define RST_PIN D3
MFRC522 rfid(SS_PIN, RST_PIN);

// Servo
#define SERVO_PIN D0
#define LOCK_POSITION 0
#define UNLOCK_POSITION 90
Servo doorServo;

// LED and buzzer pins
#define GREEN_LED D1
#define RED_LED   D4
#define BUZZER    D8

// Authorized RFID UID
byte authorizedUID[] = {0x91, 0x66, 0x2A, 0xF4};
const byte authorizedUIDSize = 4;

// State and timers
BlynkTimer timer;

bool doorUnlocked = false;
bool deniedAlarm = false;

unsigned long unlockStartTime = 0;
unsigned long deniedStartTime = 0;
unsigned long lastCardTime = 0;

const unsigned long UNLOCK_TIME = 5000;
const unsigned long DENIED_TIME = 2000;
const unsigned long CARD_COOLDOWN = 1500;

// Convert UID to string
String getUID() {
  String uid = "";

  for (byte i = 0; i < rfid.uid.size; i++) {
    if (rfid.uid.uidByte[i] < 0x10) {
      uid += "0";
    }

    uid += String(rfid.uid.uidByte[i], HEX);

    if (i < rfid.uid.size - 1) {
      uid += ":";
    }
  }

  uid.toUpperCase();
  return uid;
}

// Check authorized card
bool isAuthorizedCard() {
  if (rfid.uid.size != authorizedUIDSize) {
    return false;
  }

  for (byte i = 0; i < authorizedUIDSize; i++) {
    if (rfid.uid.uidByte[i] != authorizedUID[i]) {
      return false;
    }
  }

  return true;
}

// Lock the door
void lockDoor() {
  doorServo.write(LOCK_POSITION);

  digitalWrite(GREEN_LED, LOW);
  digitalWrite(RED_LED, LOW);
  digitalWrite(BUZZER, LOW);

  doorUnlocked = false;

  Blynk.virtualWrite(V0, "SECURE");
  Blynk.virtualWrite(V2, "LOCKED");

  Serial.println("DOOR LOCKED");
}

// Authorized access
void unlockDoor(String uid) {
  Serial.println("ACCESS GRANTED");
  Serial.println(uid);

  digitalWrite(GREEN_LED, HIGH);
  digitalWrite(RED_LED, LOW);
  digitalWrite(BUZZER, LOW);

  doorServo.write(UNLOCK_POSITION);

  doorUnlocked = true;
  unlockStartTime = millis();

  Blynk.virtualWrite(V0, "ACCESS GRANTED");
  Blynk.virtualWrite(V1, uid);
  Blynk.virtualWrite(V2, "UNLOCKED");

  Blynk.logEvent(
    "access_granted",
    String("Authorized RFID detected: ") + uid
  );
}

// Unauthorized access
void accessDenied(String uid) {
  Serial.println("ACCESS DENIED");
  Serial.println(uid);

  doorServo.write(LOCK_POSITION);

  digitalWrite(GREEN_LED, LOW);
  digitalWrite(RED_LED, HIGH);
  digitalWrite(BUZZER, HIGH);

  doorUnlocked = false;
  deniedAlarm = true;
  deniedStartTime = millis();

  Blynk.virtualWrite(V0, "ACCESS DENIED");
  Blynk.virtualWrite(V1, uid);
  Blynk.virtualWrite(V2, "LOCKED");

  Blynk.logEvent(
    "access_denied",
    String("Unauthorized RFID detected: ") + uid
  );
}

// Scan RFID
void checkRFID() {
  if (deniedAlarm || doorUnlocked) {
    return;
  }

  if (!rfid.PICC_IsNewCardPresent()) {
    return;
  }

  if (!rfid.PICC_ReadCardSerial()) {
    return;
  }

  if (millis() - lastCardTime < CARD_COOLDOWN) {
    rfid.PICC_HaltA();
    rfid.PCD_StopCrypto1();
    return;
  }

  lastCardTime = millis();

  String uid = getUID();

  Serial.print("CARD UID: ");
  Serial.println(uid);

  Blynk.virtualWrite(V1, uid);

  if (isAuthorizedCard()) {
    unlockDoor(uid);
  } else {
    accessDenied(uid);
  }

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
}

// Auto-lock after 5 seconds
void handleDoorTimer() {
  if (doorUnlocked &&
      millis() - unlockStartTime >= UNLOCK_TIME) {
    Serial.println("Auto-locking door");
    lockDoor();
  }
}

// Stop alarm after 2 seconds
void handleAlarmTimer() {
  if (deniedAlarm &&
      millis() - deniedStartTime >= DENIED_TIME) {
    digitalWrite(RED_LED, LOW);
    digitalWrite(BUZZER, LOW);

    deniedAlarm = false;

    Blynk.virtualWrite(V0, "SECURE");

    Serial.println("Alarm stopped. Door remains locked.");
  }
}

// Setup
void setup() {
  Serial.begin(115200);

  pinMode(GREEN_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);
  pinMode(BUZZER, OUTPUT);

  digitalWrite(GREEN_LED, LOW);
  digitalWrite(RED_LED, LOW);
  digitalWrite(BUZZER, LOW);

  doorServo.attach(SERVO_PIN);
  doorServo.write(LOCK_POSITION);

  SPI.begin();
  rfid.PCD_Init();

  Serial.println("Connecting to Blynk...");

  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);

  Blynk.virtualWrite(V0, "SECURE");
  Blynk.virtualWrite(V1, "WAITING FOR RFID");
  Blynk.virtualWrite(V2, "LOCKED");

  timer.setInterval(100L, handleDoorTimer);
  timer.setInterval(100L, handleAlarmTimer);

  Serial.println("SYSTEM READY");
  Serial.println("Authorized UID: 91:66:2A:F4");
  Serial.println("Scan RFID card");
}

// Main loop
void loop() {
  Blynk.run();
  timer.run();
  checkRFID();
}