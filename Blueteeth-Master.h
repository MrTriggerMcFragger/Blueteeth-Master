#include <A2DPVolumeControl.h>
#include <BluetoothA2DP.h>
#include <BluetoothA2DPCommon.h>
#include <BluetoothA2DPSink.h>
#include <BluetoothA2DPSinkQueued.h>
#include <BluetoothA2DPSource.h>
#include <config.h>
#include <SoundData.h>

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include "bluetooth_scanning.h"

#include "terminal.h"

#include <BlueteethInternalNetworkStack.h>

#define MAX_BUFFER_SIZE 100