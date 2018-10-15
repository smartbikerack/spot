/*
    Based on Neil Kolban example for IDF: https://github.com/nkolban/esp32-snippets/blob/master/cpp_utils/tests/BLE%20Tests/SampleWrite.cpp
    Ported to Arduino ESP32 by Evandro Copercini
*/

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <MFRC522.h>
#include <SPI.h>
#include <Servo.h>
// See the following for generating UUIDs:
// https://www.uuidgenerator.net/

#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "21428402-c7c4-4673-b87b-3a6facc302b8"
#define USER_UUID "f2faeb57-d190-4657-ab17-b525e1d4ee53"
#define STATUS_UUID "6a023f58-3490-432f-893b-6292c522549f"
#define OCCUPIED_UUID "3ca17de4-73fe-4fd0-9e02-87433bb0385e"
#define TURN_TIME 175
const int RST_PIN = 22;            // Pin 9 para el reset del RC522
const int SS_PIN = 21;            // Pin 10 para el SS (SDA) del RC522
boolean moveLock = false;
Servo servo;
int pos = 0;

MFRC522 mfrc522(SS_PIN, RST_PIN);   // Crear instancia del MFRC522

BLECharacteristic *pCharacteristic;
BLECharacteristic *User;
BLECharacteristic *Status;
BLECharacteristic *Occupied;
BLEAdvertising *pAdvertising;

std::string oldValue;
bool occupied = false;

void printArray(byte *buffer, byte bufferSize) {
   for (byte i = 0; i < bufferSize; i++) {
      Serial.print(buffer[i] < 0x10 ? " 0" : " ");
      Serial.print(buffer[i], HEX);
   }
}

class MyCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      std::string value = pCharacteristic->getValue();
      if (value.length() > 0) {
        Serial.println("*********");
        Serial.print("New value: ");
        for (int i = 0; i < value.length(); i++)
          Serial.print(value[i]);

        Serial.println();
        Serial.println("*********");
       
      }
    }
};


void moveServo(int rotation){
  Serial.println("Moving servo");
   servo.attach(4);
   servo.write(0);
    delay(TURN_TIME);
    servo.write(90);
    servo.detach();
    delay(2000);
    servo.attach(4);
    servo.write(180);
    delay(TURN_TIME);
    servo.write(90);
    servo.detach();
}

void ledBlink(int pin, int blinks) {
  for (int i = 0; i < blinks; i++) {
    digitalWrite(pin, HIGH);
    delay(100);  
    digitalWrite(pin, LOW);
    delay (100);
  }
}
void changeOccupation(){
  occupied = !(occupied);
  switch (occupied) {
    case 0: Serial.println("Spot is free");
    case 1: Serial.println("Spot is occupied");
  }
  moveLock = true;
  if (occupied == true){
    Occupied->setValue("1");
  } else {
    Occupied->setValue("0");
  }
  ledBlink(2, 5);
  moveServo(10);
  
}



void waitResponse() {
  std::string value;
  while (true) {
    value = pCharacteristic->getValue();
    if (value.compare("0") != 0) {
      break;
    }
  }
  
  digitalWrite(12, LOW);
  pCharacteristic->setValue("0");
  int response = atoi(Status->getValue().c_str());
  
  switch (response) {
    case 1: ledBlink(13, 5); Serial.println("User allowed to open the spot");changeOccupation(); break;
    case 2: ledBlink(12, 5); Serial.println("An error ocurred");break;
    case 3: ledBlink(14, 5); Serial.println("User is not allowed to open the spot");break;
  }
}




void setup() {
  Serial.begin(9600);
  pinMode (12, OUTPUT);
  pinMode (13, OUTPUT);
  pinMode (14, OUTPUT);
  pinMode (2, OUTPUT);
  ledBlink(2, 5);

  SPI.begin();
  mfrc522.PCD_Init(); 
  mfrc522.PCD_DumpVersionToSerial();  
  BLEDevice::init("Spot1");
  BLEServer *pServer = BLEDevice::createServer();

  BLEService *pService = pServer->createService(SERVICE_UUID);

  pCharacteristic = pService->createCharacteristic(
                                         CHARACTERISTIC_UUID,
                                         BLECharacteristic::PROPERTY_READ |
                                         BLECharacteristic::PROPERTY_WRITE
                                       );
  pCharacteristic->setCallbacks(new MyCallbacks());

  pCharacteristic->setValue("0");

  Status = pService->createCharacteristic(
                                         STATUS_UUID,
                                         BLECharacteristic::PROPERTY_READ |
                                         BLECharacteristic::PROPERTY_WRITE
                                       );

  Occupied = pService->createCharacteristic(
                                         OCCUPIED_UUID,
                                         BLECharacteristic::PROPERTY_READ |
                                         BLECharacteristic::PROPERTY_WRITE
                                       );
                                       
  User = pService->createCharacteristic(
                                         USER_UUID,
                                         BLECharacteristic::PROPERTY_READ
                                       );

  pService->start();
  Occupied->setValue("0");
  pAdvertising = pServer->getAdvertising();
  Serial.println("Starting spot");
  //Serial.println("Spot free");
  // pAdvertising->start();
  //moveServo(10);
}

void loop() {
   // Detectar tarjeta
   if (mfrc522.PICC_IsNewCardPresent()) {
      if (mfrc522.PICC_ReadCardSerial()) {
        //Show UID on serial monitor
        Serial.print("UID tag :");
        String content= "";
        byte letter;
        for (byte i = 0; i < mfrc522.uid.size; i++) {
          Serial.print(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " ");
          Serial.print(mfrc522.uid.uidByte[i], HEX);
          content.concat(String(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " "));
          content.concat(String(mfrc522.uid.uidByte[i], HEX));
        }
        Serial.println();
        Serial.println("Message : ");
        content.toUpperCase();
        //Serial.println(content);  
         // Finalizar lectura actual
        mfrc522.PICC_HaltA();
        char cStr[50] = {};
        content.toCharArray(cStr, 50);
        std::string stlString(cStr);
        Serial.println(cStr);
        Serial.println("Start Advertising");
        User->setValue(cStr);
        pAdvertising->start();
        digitalWrite(12, HIGH);
        waitResponse();
      }
   }

  pAdvertising->stop();
  delay(1000);
 
 
  
  
  
}
