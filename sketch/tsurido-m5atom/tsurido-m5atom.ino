/*****************************************************************************/
//  Function:    Get the accelemeter of X/Y/Z axis and print out on the 
//                  serial monitor and bluetooth.
//  Usage:       This program is for fishing. Use the accelerometer at the end
//               of the rod to see if the fish is caught. Acceleration is 
//               transmitted in real time via Bluetooth and can be monitored 
//               from a laptop.
//  Hardware:    M5Atom + ADXL345(Grove)
//  Arduino IDE: Arduino-1.8.13
//  Author:  Hideto Manjo     
//  Date:    Aug 9, 2020
//  Version: v0.2
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
#include <FastLED.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include <ADXL345.h>
#include <Wire.h>

#define SERVICE_UUID        "8da64251-bc69-4312-9c78-cbfc45cd56ff"
#define CHARACTERISTIC_UUID "deb894ea-987c-4339-ab49-2393bcc6ad26"
#define DEVICE_NAME         "Tsurido"

// device select
#define USE_MPU6886         false     // M5Atom Matrix Only

// basic
#define DELAY               50        // microseconds
#define SERIAL              true      // With/without serial communication
#define BAUDRATE            115200    // Serial communication baud rate

// warning
#define WARN                true      // Enable warning LED
#define BUFFER_SIZE         200       // Buffer size for statics

// LED
#define LED_BRIGHTNESS      10        // Brightness Max is 20

#define COLOR_NEON_RED      0x00FF00  // GRB
#define COLOR_NEON_GREEN    0xFF0B01  // GRB
#define COLOR_NEON_BLUE     0x1E01FE  // GRB
#define COLOR_NEON_YELLOW   0xFEFD02  // GRB

#define SCALAR(x, y, z)     sqrt(x*x + y*y + z*z)


CRGB color_error = CRGB(COLOR_NEON_RED);
CRGB color_warning = CRGB(COLOR_NEON_YELLOW);
CRGB color_success = CRGB(COLOR_NEON_GREEN);
CRGB color_bluetooh = CRGB(COLOR_NEON_BLUE);

BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;
ADXL345 adxl;

// FLAGS
bool deviceConnected = false;
bool oldDeviceConnected = false;

// online algorism
int K = 0;
int n = 0;
double Ex = 0;
double Ex2 = 0;

void fillColor(CRGB color)
{       
        static CRGB lastcolor = CRGB(0xFFFFFF);
        if (color != lastcolor) {
                for(int i = 0; i < 25; i++){
                        M5.dis.drawpix(i, color);
                }
                lastcolor = color;
        }
}

void add_variable(int* x)
{
        if (n == 0)
                K = *x;
        n += 1;
        Ex += *x - K;
        Ex2 += (*x - K) * (*x - K);
}

void remove_variable(int* x)
{
        n -= 1;
        Ex -= (*x - K);
        Ex2 -= (*x - K) * (*x - K);
}
double get_mean(void)
{
        return K + Ex / n;
}

double get_variance(void)
{
        return (Ex2 - (Ex * Ex) / n) / n;
}

void get_stat(int* x, double* mean, double* std)
{
        static int pos = 0;
        static int data[BUFFER_SIZE] = {};

        if(pos == BUFFER_SIZE)
                pos = 0;

        if(n == BUFFER_SIZE)
                remove_variable(&data[pos]);

        data[pos] = *x;
        add_variable(&data[pos]);

        *mean = get_mean();
        *std = sqrt(get_variance());

        pos++;
}

bool warn(int* val, double* standard) {
        static long lastring = 0;
        static bool ring = false;
        static bool state = false;

        if (micros() - lastring > 2000 * 1000) { 
                if (*val > 5 * (*standard)) {
                        lastring = micros();
                        ring = true;
                }else{
                        ring = false; 
                }
        }

        if (ring) {
                if (state) {
                        M5.dis.setBrightness(0);
                } else {
                        M5.dis.setBrightness(LED_BRIGHTNESS);
                }
                state = !state;
                return true;
        }
        
        M5.dis.setBrightness(LED_BRIGHTNESS);

        return false;
}

class MyServerCallbacks: public BLEServerCallbacks
{
        void onConnect(BLEServer* pServer)
        {
                deviceConnected = true;
                BLEDevice::startAdvertising();
        }

        void onDisconnect(BLEServer* pServer)
        {
                deviceConnected = false;
        }
};

void setup_adxl345()
{
        adxl.powerOn();
}

void setup_acc()
{
        if (USE_MPU6886) {
                M5.IMU.Init();
                return;
        }
        
        setup_adxl345();
}    

void setup_ble()
{       
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

        BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
        pAdvertising->addServiceUUID(SERVICE_UUID);
        pAdvertising->setScanResponse(false);
        pAdvertising->setMinPreferred(0x0);
        BLEDevice::startAdvertising();
}

void read_acc(int* x, int* y, int* z)
{       
        if (USE_MPU6886) {
                int16_t ax, ay, az;
                M5.IMU.getAccelAdc(&ax, &ay, &az);
                *x = (int) ax;
                *y = (int) ay;
                *z = (int) az;
                return;
        }

        adxl.readXYZ(x, y, z);
}

void setup()
{
        M5.begin(false, false, true);
        fillColor(CRGB(0x888888));
        M5.dis.setBrightness(LED_BRIGHTNESS);
        Wire.begin(26, 32);
        Serial.begin(BAUDRATE);
        Serial.flush();
        setup_acc();
        setup_ble();
}

void loop()
{
        int x = 0;
        int y = 0;
        int z = 0;
        int scalar = 0;
        char msg[128];

        int diff = 0;
        double mean = 0.0;
        double standard = 0.0;

        long wait;
        long t = micros();

        read_acc(&x, &y, &z);

        scalar = SCALAR(x, y, z);
        sprintf(msg, "Ax, Ay, Az, A: %d, %d, %d, %d", x, y, z, scalar);

        if (scalar != 0) {
                if (deviceConnected) {
                        fillColor(color_bluetooh);
                } else {
                        fillColor(color_success);
                }
        } else {
                fillColor(color_error);
        }
                          
        
        if (SERIAL)
                Serial.println(msg);

        if (WARN)
                get_stat(&scalar, &mean, &standard);
                diff = (int) abs(scalar - mean);
                warn(&diff, &standard);

        if (deviceConnected) {
                pCharacteristic->setValue(msg);
                pCharacteristic->notify();
        }

        if (!deviceConnected && oldDeviceConnected) {
                fillColor(color_warning);
                delay(500);
                pServer->startAdvertising();
                oldDeviceConnected = deviceConnected;
        }

        if (deviceConnected && !oldDeviceConnected) {
                oldDeviceConnected = deviceConnected;
        }

        wait = DELAY * 1000 - (micros() - t);

        if (wait > 0)
                delayMicroseconds(wait);

}
