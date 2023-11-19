#line 1 "C:\\Users\\ztzac\\Documents\\GitHub\\Blueteeth-Master\\bluetooth_scanning.h"
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

extern int discoveryIdx;

class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) {
      Serial.print("\033[F"); //go up one line
      Serial.print("\033[2K"); //clear line
      Serial.printf("Device [%d] : %s \n\r", discoveryIdx++, advertisedDevice.getName().c_str());
      // Serial.printf("Advertised Device: %s \n\r", advertisedDevice.toString().c_str());
    }
};

BLEScan * bleScanSetup(){
  BLEDevice::init("");
  BLEScan * pBLEScan = BLEDevice::getScan(); //create new scan
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(99);  // less or equal setInterval value
  pBLEScan -> setActiveScan(true);
  return pBLEScan;
}


inline BLEScanResults performBLEScan(BLEScan * pBLEScan, int scanTimeSeconds){
    BLEScanResults foundDevices = pBLEScan->start(scanTimeSeconds, false);
    pBLEScan->clearResults();   // delete results fromBLEScan buffer to release memory
    return foundDevices;
}