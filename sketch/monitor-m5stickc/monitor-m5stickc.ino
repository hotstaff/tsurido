/******************************************************************************/
//  Function:    Monitor tsurido sensor.
//  Usage:       Tsurido acc sensor monitor
//  Hardware:    M5Stick
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
/******************************************************************************/

#include <M5StickC.h>
#include <BLEDevice.h>

#define SERVICE_UUID        "8da64251-bc69-4312-9c78-cbfc45cd56ff"
#define CHARACTERISTIC_UUID "deb894ea-987c-4339-ab49-2393bcc6ad26"
#define DEVICE_NAME         "Tsurido"

#define SERIAL              true   // With/without serial communication
#define BAUDRATE            115200 // Serial communication baud rate 

// menu offsets
#define OFFSET_MENU         2      // Menu
#define OFFSET_MAIN         12     // Main


static BLEUUID serviceUUID(SERVICE_UUID);
static BLEUUID    charUUID(CHARACTERISTIC_UUID);
static BLERemoteCharacteristic* pRemoteCharacteristic;
static BLEAdvertisedDevice* myDevice;

TFT_eSprite canvas = TFT_eSprite(&M5.Lcd);

static bool doConnect = false;
static bool connected = false;
static bool doScan = true;


// this is m5stack function
static void progressBar(int x, int y, int w, int h, uint8_t val) {
        canvas.drawRect(x, y, w, h, 0x09F1);
        canvas.fillRect(x + 1, y + 1, w * (((float)val) / 100.0), h - 1, 0x09F1);
}

static int split(char* str, char* demilitor, String* out, int outlen)
{
        char *ptr;
        int i = 1;

        // first
        ptr = strtok(str, demilitor);
        *(out) = String(ptr);

        // remain
        while(ptr != NULL) {
                if (i > outlen)
                        break;

                ptr = strtok(NULL, demilitor);

                if(ptr != NULL)
                        *(out + i++) = String(ptr);
        }
    
        return i;
}

static int parse(char* msg, String* labels, String* values, int length)
{
        int labels_len = 0;
        int values_len = 0;
        String splited[2];

        if (split(msg, ":", splited, 2) < 0)
                return -1;

        labels_len = split((char*) splited[0].c_str(), ",", labels, length);
        values_len = split((char*) splited[1].c_str(), ",", values, length);

        if (labels_len != values_len)
                return -1;

        for (int i = 0; i < labels_len; i++) {
                labels[i].trim();
                values[i].trim();
        }

        return labels_len;
}

static long scalor(String* labels, String* values, int size)
{
        for (int i = 0; i < size; i++) {
                if (labels[i].equals("A")) {
                        return values[i].toInt();
                }
        }
        return 0;
}

static double outlier(String* labels, String* values, int size)
{
        for (int i = 0; i < size; i++) {
                if (labels[i].equals("O")) {
                        return values[i].toDouble();
                }
        }
        return 0;
}

bool warn(double* outlier)
{
        static long lastring = 0;
        static bool ring = false;

        if (micros() - lastring > 2000 * 1000) {
                if (*outlier > 5) {
                        lastring = micros();
                        ring = true;
                } else {
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

static void updateStateBLE()
{
        canvas.fillRect(0, OFFSET_MENU, M5.Lcd.width(), 10, 0);

        if (connected) {
                canvas.setCursor(M5.Lcd.width() - 22, OFFSET_MENU);
                canvas.setTextColor(WHITE, BLUE);
                canvas.println("BLE");

                canvas.setCursor(0, OFFSET_MENU);
                canvas.setTextColor(GREEN, BLACK);
                canvas.println(DEVICE_NAME);
        } else {
                canvas.setTextSize(2);
                canvas.setCursor(15, 40);
                canvas.setTextColor(GREENYELLOW, BLACK);
                canvas.println("Scanning...");
                canvas.setTextSize(1);

                canvas.setCursor(0, OFFSET_MENU);
                canvas.setTextColor(WHITE, BLACK);
                canvas.println("Not Connected");
        }
}

static void updateBar(double* val)
{
        int outlier_pc = *val * 100 / 5.0;
        if (outlier_pc > 100)
                outlier_pc = 100;
        canvas.fillRect(5, 15, 150, 60, 0);
        progressBar(5, 15, 150, 60, (uint8_t) outlier_pc);
}

static void drawCanvas(double* val)
{       
        M5.Lcd.startWrite();
        updateBar(val);
        updateStateBLE();
        canvas.pushSprite(0, 0);
        M5.Lcd.endWrite();
}

static void notifyCallback(
        BLERemoteCharacteristic* pBLERemoteCharacteristic,
        uint8_t* pData,
        size_t length,
        bool isNotify)
{
        char msg[128];
        int size = 0;
        String labels[5];
        String values[5];
        double o = 0.0;

        strncpy(msg, (char*) pData, length);
        msg[length] = '\0'; // add NULL char

        if (SERIAL)
                Serial.println(msg);

        size = parse(msg, labels, values, 5);
        if (size < 0)
                return;

        o = outlier(labels, values, size);

        warn(&o);
        drawCanvas(&o);
}

class MyClientCallback : public BLEClientCallbacks
{
        void onConnect(BLEClient* pclient)
        {
        }

        void onDisconnect(BLEClient* pclient)
        {
                connected = false;
                doScan = true;
                digitalWrite(GPIO_NUM_10, HIGH);
        }
};

bool connectToServer()
{
        BLEClient*  pClient  = BLEDevice::createClient();
        pClient->setClientCallbacks(new MyClientCallback());

        // connect
        pClient->connect(myDevice);

        BLERemoteService* pRemoteService = pClient->getService(serviceUUID);
        if (pRemoteService == nullptr) {
                pClient->disconnect();
                return false;
        }

        pRemoteCharacteristic = pRemoteService->getCharacteristic(charUUID);
        if (pRemoteCharacteristic == nullptr) {
                pClient->disconnect();
                return false;
        }

        if (pRemoteCharacteristic->canNotify())
                pRemoteCharacteristic->registerForNotify(notifyCallback);

        connected = true;
        doScan = false;

        return true;
}


class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks
{
        void onResult(BLEAdvertisedDevice advertisedDevice)
        {
                if (!connected && advertisedDevice.haveName() && 
                    strcmp(advertisedDevice.getName().c_str(), DEVICE_NAME) == 0) {
                        BLEDevice::getScan()->stop();
                        myDevice = new BLEAdvertisedDevice(advertisedDevice);
                        doConnect = true;
                        doScan = true;
                }
        }
};

void setup_ble_scan() {
        BLEDevice::init("");
        BLEScan* pBLEScan = BLEDevice::getScan();
        pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
        pBLEScan->setInterval(1349);
        pBLEScan->setWindow(449);
        pBLEScan->setActiveScan(true);
}

void setup()
{
        M5.begin(true, true, false);
        M5.Lcd.setRotation(3);
        pinMode(GPIO_NUM_10, OUTPUT);
        digitalWrite(GPIO_NUM_10, HIGH);
        Serial.begin(BAUDRATE);
        Serial.flush();
        setup_ble_scan();
        canvas.createSprite(M5.Lcd.width(), M5.Lcd.height());
        canvas.setSwapBytes(false);

}

void loop()
{
        double dummy = 0.0;

        if (doConnect) {
                if (connectToServer()) {
                        // M5.Lcd.println("Connected.");
                } else {
                        // M5.Lcd.println("Failed to connect.");
                }
                doConnect = false;
        }

        if (doScan) {
                drawCanvas(&dummy);
                BLEDevice::getScan()->start(5);
        }

        delay(1000);
}