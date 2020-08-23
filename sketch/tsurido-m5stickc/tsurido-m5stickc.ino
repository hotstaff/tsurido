/*****************************************************************************/
//  Function:    Get the accelemeter of X/Y/Z axis and print out on the 
//                  serial monitor and bluetooth.
//  Usage:       This program is for fishing. Use the accelerometer at the end
//               of the rod to see if the fish is caught. Acceleration is 
//               transmitted in real time via Bluetooth and can be monitored 
//               from a laptop.
//  Hardware:    M5StickC + ADXL345(Grove)
//  Arduino IDE: Arduino-1.8.13
//  Author:  Hideto Manjo     
//  Date:    Aug 9, 2020
//  Version: v0.1
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU Lesser General Public
//  License as published by the Free Software Foundation; either
//  version 2.1 of the License, or (at your option) any later version.
//  This library is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//  Lesser General Public License for more details.
//  You should have received a copy of the GNU Lesser General Public
//  License along with this library; if not, write to the Free Software
//  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
//
/*******************************************************************************/
#include <M5StickC.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include <ADXL345.h>
#include <Wire.h>

#define SERVICE_UUID        "8da64251-bc69-4312-9c78-cbfc45cd56ff"
#define CHARACTERISTIC_UUID "deb894ea-987c-4339-ab49-2393bcc6ad26"
#define DEVICE_NAME         "Tsurido"
#define LCD_ROTATION        0      // 90 * num (degree) [Counterclockwise]
#define DELAY               50     // microseconds
#define SERIAL              true   // With/without serial communication
#define BAUDRATE            115200 // Serial communication baud rate

#define SCALAR(x, y, z)     sqrt(x*x + y*y + z*z)

BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;
ADXL345 adxl;

// FLAGS
bool deviceConnected = false;

int x = 0;
int y = 0;
int z = 0;
int scalar;
char msg[128];


class MyServerCallbacks: public BLEServerCallbacks {
                void onConnect(BLEServer* pServer) {
                        M5.Lcd.setCursor(10, 110);
                        M5.Lcd.println("Con");
                        deviceConnected = true;
                }

                void onDisconnect(BLEServer* pServer) {
                        M5.Lcd.setCursor(10, 110);
                        M5.Lcd.println("   ");
                        deviceConnected = false;
                }
};

void setup_adxl345() {
        adxl.powerOn();
}

void setup_ble(){
        BLEDevice::init(DEVICE_NAME);
        BLEServer *pServer = BLEDevice::createServer();
        pServer->setCallbacks(new MyServerCallbacks());
        BLEService *pService = pServer->createService(SERVICE_UUID);
        pCharacteristic = pService->createCharacteristic(
                                  CHARACTERISTIC_UUID,
                                  BLECharacteristic::PROPERTY_READ |
                                  BLECharacteristic::PROPERTY_WRITE |
                                  BLECharacteristic::PROPERTY_NOTIFY |
                                  BLECharacteristic::PROPERTY_INDICATE
                          );
        pCharacteristic->addDescriptor(new BLE2902());

        pService->start();
        BLEAdvertising *pAdvertising = pServer->getAdvertising();
        pAdvertising->start();
}

void setup() {
        M5.begin();

        // Check i2c pin assignment SDA=32, SDL=33
        Wire.begin(32, 33);

        Serial.begin(BAUDRATE);
        setup_adxl345();
        setup_ble();
        M5.Lcd.setRotation(LCD_ROTATION);
        M5.Lcd.fillScreen(BLACK);
        M5.Lcd.setTextSize(2);
        M5.Lcd.setCursor(10, 0);
        M5.Lcd.println("Tsuri");
        M5.Lcd.println("  do ");
}

void loop() {
        M5.update();

        // get ADXL345 data
        adxl.readXYZ(&x, &y, &z);

        scalar = SCALAR(x, y, z);
        M5.Lcd.setCursor(0, 60);
        M5.Lcd.println("Acc");
        M5.Lcd.printf("  %d\n", scalar);
        sprintf(msg, "Ax, Ay, Az, A: %d, %d, %d, %d", x, y, z, scalar);
        
        if (SERIAL)
            Serial.println(msg);

        if (deviceConnected) {
                pCharacteristic->setValue(msg);
                pCharacteristic->notify();
        }

        delay(DELAY);
}
