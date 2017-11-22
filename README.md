# mbed BLE API for JerryScript

This library exposes the mbed BLE API to JerryScript targets.

The following objects are exposed:

* BLEDevice - holds reference to the Bluetooth stack.
* BLEService - references a GATT service.
* BLECharacteristic - references a GATT characteristic.

## Usage

```js
// instantiate BLEDevice, only do this once
var ble = BLEDevice();

// takes in: characteristic UUID (16 bit only), array of properties (r/w/n), data size
var characteristic = BLECharacteristic('9101', ['read', 'write', 'notify'], 1);
// takes in: service UUID (16 bit only), array of BLECharacteristic objects
var service = BLEService('9100', [ characteristic ]);

// ready callback, wait before interacting with the API
ble.ready(function() {
    print("ble is ready");

    // takes in an array of BLEService objects
    ble.addServices([
        service
    ]);
    // takes: name to advertise, array of UUIDs (strings), advertisement interval (default: 1000)
    ble.startAdvertising("YOUR_NAME", [
        service.getUUID()
    ], 1000);
});

// connection callback
ble.onConnection(function() {
    print("GATT connection established");
});

// disconnection callback
ble.onDisconnection(function() {
    print("GATT disconnected, restarting advertisements");

    // call without parameters to use the last used set
    ble.startAdvertising();
});

// is connected? returns Boolean
print("BLE is connected? " + ble.isConnected());
```

## Interacting with characteristics

```js
// write to a characteristic
characteristic.write([ 0x98, 0x37 ]);

// reading a characteristic (returns an array)
var arr = characteristic.read();
print("Length is " + arr.length + ", first element is " + arr[0]);

// receiving updates when written over GATT
characteristic.onUpdate(function (newValue) {
    // newValue is an array (same value as read() returns)
    print("Updated! New value is " + newValue.length + ", first element is " + newValue[0]);
});
```
## Fork Details

Implememted support for 128-bit UUIDs

Examples:

```js

// BLECharacteristic
// takes in: characteristic UUID (16 bit or 128 bit), array of properties (r/w/n), data size
characteristic = BLECharacteristic('XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX', ['read', 'write', 'notify'], 1);
characteristic = BLECharacteristic('9100', ['read', 'write', 'notify'], 1);

// BLEService
// takes in: characteristic UUID (16 bit or 128 bit), array of characteristics
service = BLEService('9100', [ characteristic ]);
service = BLEService('XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX', [ characteristic ]);

```
Implemented Long UUID support in BLEService.GetUUID()
```js
    uuid = service.GetUUID()
```

Added Manufacturer info in the startAdveritising method:

```js
// Valid methods
// takes: name to advertise, array of UUIDs (strings)
BLE ble = BLEDevice();
ble.startAdvertising('advertisingName', [ service.getUUID() ]);

// takes: name to advertise, array of UUIDs (strings), advertisement interval (default: 1000),
ble.startAdvertising('advertisingName', [ service.getUUID() ], 100);

// takes: name to advertise, array of UUIDs (strings), advertisement interval (default: 1000), Manufacturer's info
ble.startAdvertising('advertisingName', [ service.getUUID() ], 100, '018000E00000');

// takes: name to advertise, array of UUIDs (strings), Manufacturer's info
ble.startAdvertising('advertisingName', [ service.getUUID() ], '018000E00000');

```
