#include <Arduino.h>
#include <SPI.h>
#include "bletest.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "BluetoothSerial.h"

BLECharacteristic *characteristicMessage;
void bluetoothPrintLine(String);

bool deviceConnected;
void confugureBle();

int counter = 0;
int counterToSend = 0;
int rebootble = 0;
String formatBle = "1" ;

class MyServerCallbacks : public BLEServerCallbacks
{
    void onConnect(BLEServer *server)
    {
        deviceConnected = true;
        Serial.println("Connected");
    };

    void onDisconnect(BLEServer *server)
    {
        deviceConnected = false;
        Serial.println("Disconnected");
        rebootble = 1;
    }
};

class MessageCallbacks : public BLECharacteristicCallbacks
{
    void onWrite(BLECharacteristic *characteristic)
    {
        std::string data = characteristic->getValue();
        String tmp = data.c_str();
        if(tmp == "format:1\n"){
          formatBle = "1";
          characteristicMessage->setValue("ok:format:1");
          characteristicMessage->notify(); 
        }
        if(tmp == "format:2\n"){
          formatBle = "2";
          characteristicMessage->setValue("ok:format:2");
          characteristicMessage->notify(); 
        }

        Serial.println("Rec " + tmp); 
    }

    void onRead(BLECharacteristic *characteristic)
    {
        characteristic->setValue("BLE ESP32 Conectado!");
    }
};

void send(String data) {
   
    if(formatBle == "1"){
      String outputprotocol = data;
      bluetoothPrintLine(outputprotocol);
    }
    else if(formatBle == "2"){
      bluetoothPrintLine(" ");
      bluetoothPrintLine("Msg: " + data);
      bluetoothPrintLine("---------------");
    }
    Serial.println("Send " + data); 
    Serial.println();
}

void setup() {
 
  Serial.begin(9600);
  while (!Serial);
  Serial.println();
  
  deviceConnected = false;
  confugureBle();
  Serial.println("Bluetooth Started! Ready to pair...");

}

void loop() {
  
  delay(1000);
  
  //Send after 10s.
  if(deviceConnected && counterToSend >= 10){
    send("My Counter: " + String(counter++));
    counterToSend=0;
  }
  
  if(rebootble == 1){
    rebootble = 0;
    BLEDevice::deinit(false);
    confugureBle();
    Serial.println("Restart Ble");
  }

  counterToSend++;
}

void bluetoothPrintLine(String line)
{
    if(deviceConnected){
       characteristicMessage->setValue(line.c_str());
       characteristicMessage->notify(); 
    }
}

void confugureBle()
{
    deviceConnected = false;
    // Setup BLE Server
    BLEDevice::init(DEVICE_NAME);
    BLEServer *server = BLEDevice::createServer();
    server->setCallbacks(new MyServerCallbacks());

    // Register message service that can receive messages and reply with a static message.
    BLEService *service = server->createService(SERVICE_UUID);
    characteristicMessage = service->createCharacteristic(MESSAGE_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_WRITE);
    characteristicMessage->setCallbacks(new MessageCallbacks());
    characteristicMessage->addDescriptor(new BLE2902());
    service->start();

    // Register device info service, that contains the device's UUID, manufacturer and name.
    service = server->createService(DEVINFO_UUID);
    BLECharacteristic *characteristic = service->createCharacteristic(DEVINFO_MANUFACTURER_UUID, BLECharacteristic::PROPERTY_READ);
    characteristic->setValue(DEVICE_MANUFACTURER);
    characteristic = service->createCharacteristic(DEVINFO_NAME_UUID, BLECharacteristic::PROPERTY_READ);
    characteristic->setValue(DEVICE_NAME);
    characteristic = service->createCharacteristic(DEVINFO_SERIAL_UUID, BLECharacteristic::PROPERTY_READ);
    String chipId = String((uint32_t)(ESP.getEfuseMac() >> 24), HEX);
    characteristic->setValue(chipId.c_str());
    service->start();

    // Advertise services
    BLEAdvertising *advertisement = server->getAdvertising();
    BLEAdvertisementData adv;
    adv.setName(DEVICE_NAME);
    adv.setCompleteServices(BLEUUID(SERVICE_UUID));
    advertisement->setAdvertisementData(adv);
    advertisement->start();

    Serial.println("Ready");
}