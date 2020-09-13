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
#include <M5StickC.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include <ADXL345.h>
#include <Wire.h>

#define SERVICE_UUID        "8da64251-bc69-4312-9c78-cbfc45cd56ff"
#define CHARACTERISTIC_UUID "deb894ea-987c-4339-ab49-2393bcc6ad26"
#define DEVICE_NAME         "Tsurido"

// device select
#define USE_INTERNAL_IMU    true   // Use internal IMU unit as acc sensor

// basic
#define LCD_ROTATION        0      // 90 * num (degree) [Counterclockwise]
#define SCREENBREATH        12     // LCD brightness (max 12)
#define DELAY               50     // milliseconds
#define SERIAL              true   // With/without serial communication
#define BAUDRATE            115200 // Serial communication baud rate 
#define CPU_FREQ            240    // Set FREQ MHz -> 240 or 160
                                   // (80, 40, 20, 10) is not work normally
// warning
#define WARN                true   // Enable warning LED
#define BUFFER_SIZE         200    // Buffer size for statics

// plot
#define X0                  5      // Plot left padding
#define TH_WARN             5      // Warning threshold(sigma)
#define TH_MAX              10     // Maxrange(sigma)

// battery
#define BATT_FULL_VOLTAGE   4.2    // Battery full voltage
#define BATT_LOW_VOLTAGE    3.4    // Battery low voltage

// menu offsets
#define OFFSET_MENU         2      // Menu
#define OFFSET_MAIN         12     // Main


#define SCALAR(x, y, z)     sqrt(x*x + y*y + z*z)
#define BATT_CHARGE(v, low, full)     ((v - low) / (full - low) * 100)


BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;
ADXL345 adxl;

// FLAGS
bool deviceConnected = false;
bool oldDeviceConnected = false;
bool plotEnabled = true;
bool lowEnergyMode = false;

// batt
int batt_charge = 100;

// online algorism
int K = 0;
int n = 0;
double Ex = 0;
double Ex2 = 0;

void changeCPUFreq(int freq)
{
        while(!setCpuFrequencyMhz(freq)) {
        ;
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

void updateStateBLE()
{   
        if (!lowEnergyMode) {
                if (deviceConnected) {
                        M5.Lcd.setCursor(M5.Lcd.width() - 22, OFFSET_MENU);
                        M5.Lcd.setTextColor(WHITE, BLUE);
                        M5.Lcd.println("BLE");
                } else {
                        M5.Lcd.setCursor(M5.Lcd.width() - 22, OFFSET_MENU);
                        M5.Lcd.setTextColor(WHITE, BLACK);
                        M5.Lcd.println("   ");
                }
        }
}

void updateStateBATT()
{
        if (!lowEnergyMode) {
                M5.Lcd.setCursor(0, OFFSET_MENU);
                if (batt_charge > 33) {
                    M5.Lcd.setTextColor(WHITE, BLACK);
                } else {
                    M5.Lcd.setTextColor(RED, BLACK);
                }
                M5.Lcd.printf("%d%%", batt_charge);
        }
}

class MyServerCallbacks: public BLEServerCallbacks
{
        void onConnect(BLEServer* pServer)
        {       
                deviceConnected = true;
                updateStateBLE();
        }

        void onDisconnect(BLEServer* pServer)
        {       
                deviceConnected = false;
                updateStateBLE();
        }
};

void setup_acc()
{
        if (USE_INTERNAL_IMU) {
                M5.IMU.Init();
                return;
        }
        
        adxl.powerOn();
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
        if (USE_INTERNAL_IMU) {
                int16_t ax, ay, az;
                M5.IMU.getAccelAdc(&ax, &ay, &az);
                *x = (int) ax;
                *y = (int) ay;
                *z = (int) az;
                return;
        }

        adxl.readXYZ(x, y, z);
}

void plot(int* val, double* standard)
{
        static int i = 0;
        static int diff[BUFFER_SIZE] = {};

        int sigma = 0;
        int y0 = 0;
        int y1 = 0;
        int top = (int) TH_MAX * (*standard);
        int height = M5.Lcd.height() - 5;
        int width = M5.Lcd.width() - X0;

        if(i == width)
                i = 0;

        diff[i] = *val;

        if (i != 0) {
                // plot
                y0 = map((int)(diff[i - 1]), 0, top, height, 0);
                y1 = map((int)(diff[i]), 0, top, height, 0);
                M5.Lcd.drawLine(i - 1 + X0, y0, i + X0, y1, GREEN);
        } else {
                // new page
                M5.Lcd.fillScreen(BLACK);
                updateStateBLE();
                updateStateBATT();

                sigma = map((int)TH_WARN * (*standard), 0, top, height, 0);
                M5.Lcd.drawLine(X0, sigma, width + X0, sigma, YELLOW);
        }

        M5.Lcd.setCursor(0, OFFSET_MAIN);
        M5.Lcd.setTextColor(YELLOW, BLACK);
        M5.Lcd.printf("Warn %4.0lf ", TH_WARN * (*standard));
        M5.Lcd.setCursor(0, OFFSET_MAIN + 10);
        M5.Lcd.setTextColor(GREEN, BLACK);
        M5.Lcd.printf("Value%4d ", *val);
        i++;
}

bool warn(int* val, double* standard) {
        static long lastring = 0;
        static bool ring = false;

        if (micros() - lastring > 2000 * 1000) { 
                if (*val > TH_WARN * (*standard)) {
                        lastring = micros();
                        ring = true;
                }else{
                        ring = false; 
                }
        }

        if (ring) {
                digitalWrite(GPIO_NUM_10, !digitalRead(GPIO_NUM_10));
                return true;
        }

        digitalWrite(GPIO_NUM_10, HIGH);

        return false;
}

int battery_charge()
{       
        double vbat = M5.Axp.GetVbatData() * 1.1 / 1000;
        int charge = BATT_CHARGE(vbat, BATT_LOW_VOLTAGE, BATT_FULL_VOLTAGE);

        if(charge > 100) {
              return 100;

        }else if(charge < 0) {
              return 1;
        }

        return charge;
}

void setup()
{
        M5.begin(true, true, false);

        // Check i2c pin assignment SDA=32, SDL=33
        Wire.begin(32, 33);

        Serial.begin(BAUDRATE);
        Serial.flush();
        setup_acc();
        setup_ble();

        changeCPUFreq(CPU_FREQ);

        pinMode(GPIO_NUM_10, OUTPUT);

        M5.Axp.ScreenBreath(SCREENBREATH);
        M5.Lcd.setRotation(LCD_ROTATION);
        M5.Lcd.fillScreen(BLACK);
}

void loop()
{       
        // sensor
        int x = 0;
        int y = 0;
        int z = 0;
        int scalar = 0;
        char msg[128];

        // plot
        int diff = 0;
        double mean = 0.0;
        double standard = 0.0;

        long t = micros();
        long wait = 0;

        // get ADXL345 data
        read_acc(&x, &y, &z);
        scalar = SCALAR(x, y, z);

        // get battery charge(%)
        batt_charge = battery_charge();
        updateStateBATT();

        sprintf(msg, "Ax, Ay, Az, A: %d, %d, %d, %d", x, y, z, scalar);

        if (M5.BtnB.wasPressed()) {

                if (lowEnergyMode) {
                        changeCPUFreq(CPU_FREQ);
                        M5.Axp.ScreenBreath(SCREENBREATH);
                        lowEnergyMode = false;
                } else {
                        M5.Axp.ScreenBreath(0);
                        changeCPUFreq(80);
                        digitalWrite(GPIO_NUM_10, HIGH);
                        lowEnergyMode = true;
                }
                
        }

        if (!lowEnergyMode) {

                if (M5.BtnA.wasPressed()) {
                        plotEnabled = !plotEnabled;
                        M5.Lcd.fillScreen(BLACK);
                        updateStateBLE();
                        updateStateBATT();

                        if (!plotEnabled) {
                                M5.Lcd.setTextSize(2);
                                M5.Lcd.setTextColor(WHITE, BLACK);
                                M5.Lcd.setCursor(M5.Lcd.width() / 2 - 32,
                                                 OFFSET_MAIN);
                                M5.Lcd.printf("Tsuri");
                                M5.Lcd.setCursor(M5.Lcd.width() / 2 - 32, 
                                                 OFFSET_MAIN + 20);
                                M5.Lcd.printf("  do");
                                M5.Lcd.setTextSize(1);

                                M5.Lcd.setTextColor(YELLOW, BLACK);
                                M5.Lcd.setCursor(0, M5.Lcd.height() - 20);
                                M5.Lcd.printf("Side button\n"
                                              "-> power save");
                        }
                }

                if ((WARN || plotEnabled)) {
                        get_stat(&scalar, &mean, &standard);
                        diff = (int) abs(scalar - mean);
                }

                if (WARN)
                        warn(&diff, &standard);

                if (plotEnabled) {
                        plot(&diff, &standard);
                }else{
                        M5.Lcd.setTextSize(2);
                        M5.Lcd.setCursor(M5.Lcd.width() / 2 - 8 * 4,
                                         M5.Lcd.height() / 2);
                        M5.Lcd.setTextColor(WHITE, BLACK);
                        M5.Lcd.printf("%5d", scalar);
                        M5.Lcd.setTextSize(1);
                }

                if (SERIAL)
                        Serial.println(msg);
        }

        if (deviceConnected) {
                pCharacteristic->setValue(msg);
                pCharacteristic->notify();
        }

        if (!deviceConnected && oldDeviceConnected) {
                delay(500);
                pServer->startAdvertising();
                oldDeviceConnected = deviceConnected;
        }

        if (deviceConnected && !oldDeviceConnected)
                oldDeviceConnected = deviceConnected;

        M5.update();

        wait = DELAY * 1000 - (micros() - t);
        if (wait > 0)
                delayMicroseconds(wait);
}
