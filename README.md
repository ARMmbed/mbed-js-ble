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
