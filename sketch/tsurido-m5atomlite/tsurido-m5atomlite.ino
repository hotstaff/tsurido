/*****************************************************************************/
//  Function:    Get the accelemeter of X/Y/Z axis and print out on the 
//                  serial monitor and bluetooth.
//  Usage:       This program is for fishing. Use the accelerometer at the end
//               of the rod to see if the fish is caught. Acceleration is 
//               transmitted in real time via Bluetooth and can be monitored 
//               from a laptop.
//  Hardware:    M5Atom Lite + ADXL345(Grove)
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
#include <M5Atom.h>
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

#define COLOR_NEON_RED      {0xFE, 0x00, 0x00}
#define COLOR_NEON_GREEN    {0x0B, 0xFF, 0x01}
#define COLOR_NEON_BLUE     {0x01, 0x1E, 0xFE}

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

uint8_t color_error[3] = COLOR_NEON_RED;
uint8_t color_success[3] = COLOR_NEON_GREEN;
uint8_t color_bluetooh[3] = COLOR_NEON_BLUE;

uint8_t DisBuff[2 + 1 * 3];

void setBuff(uint8_t Rdata, uint8_t Gdata, uint8_t Bdata) {
        DisBuff[0] = 0x01;
        DisBuff[1] = 0x01;
        DisBuff[2 + 0] = Rdata;
        DisBuff[2 + 1] = Gdata;
        DisBuff[2 + 2] = Bdata;
}

void setColor(uint8_t *rgb) {
        setBuff(*rgb, *(rgb + 1), *(rgb + 2));
        M5.dis.displaybuff(DisBuff);
}

class MyServerCallbacks: public BLEServerCallbacks {
        void onConnect(BLEServer* pServer) {
                deviceConnected = true;
        }

        void onDisconnect(BLEServer* pServer) {
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
        M5.begin(true, false, true);
        setBuff(0x88, 0x88, 0x88);
        M5.dis.displaybuff(DisBuff);
        // Check i2c pin number SDA=26, SDL=32
        Wire.begin(26, 32);
        Serial.begin(BAUDRATE);
        setup_adxl345();
        setup_ble();
}

void loop() {
        // get ADXL345 data
        adxl.readXYZ(&x, &y, &z);

        scalar = SCALAR(x, y, z);
        sprintf(msg, "Ax, Ay, Az, A: %d, %d, %d, %d", x, y, z, scalar);

        if (scalar != 0) {
                if (deviceConnected) {
                        setColor(color_bluetooh);
                } else {
                        setColor(color_success);
                }
        } else {
                setColor(color_error);
        }
                          
        
        if (SERIAL)
            Serial.println(msg);

        if (deviceConnected) {
                pCharacteristic->setValue(msg);
                pCharacteristic->notify();
        }

        M5.update();
        delay(DELAY);

}
