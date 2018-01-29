/* Copyright (c) 2016 ARM Limited. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef _JERRYSCRIPT_MBED_BLE_BLEJS_H
#define _JERRYSCRIPT_MBED_BLE_BLEJS_H

#include "jerryscript-mbed-event-loop/EventLoop.h"
#include "jerryscript-mbed-ble/ble-js.h"

#include <map>

#include "Callback.h"
#include "ble/BLE.h"

using namespace std;

typedef struct {
    uint8_t *buffer;
    size_t buffer_length;
    GattCharacteristic *characteristic;
} char_data_t;

static uint16_t hex_str_to_u16(const char* buf, const size_t buf_size) {
    if (buf_size > 4) {
        return 0;
    }

    uint16_t ans = 0;
    for (size_t i = 0; i < buf_size; i++) {
        ans <<= 4;
        if (buf[i] >= 'A' && buf[i] <= 'F') {
            ans |= (buf[i] - 'A') + 10;
        } else if (buf[i] >= 'a' && buf[i] <= 'f') {
            ans |= (buf[i] - 'a') + 10;
        } else if (buf[i] >= '0' && buf[i] <= '9') {
            ans |= (buf[i] - '0');
        } else {
            return 0;
        }
    }

    return ans;
}

static void man_str_to_u8_array(const char*buf, const size_t buf_size, uint8_t *out){
    for(size_t i = 0; i < buf_size; i+=2){
        const char buf_temp[2] = {buf[i], buf[i+1]};
        out[i/2] = hex_str_to_u16(buf_temp, 2);
    }
}

class BLEJS {
 public:
    static BLEJS& Instance() {
        static BLEJS instance(BLE::Instance());
        return instance;
    }

    static jerry_value_t getJsValueFromCharacteristic(GattCharacteristic* characteristic) {
        BLE* ble = &BLE::Instance();

        uint8_t buffer[23];
        uint16_t length = 23;

        ble->gattServer().read(characteristic->getValueHandle(), buffer, &length);

        jerry_value_t out_array = jerry_create_array(length);

        for (uint16_t i = 0; i < length; i++) {
            jerry_value_t val = jerry_create_number(double(buffer[i]));
            jerry_set_property_by_index(out_array, i, val);
            jerry_release_value(val);
        }

        return out_array;
    }

    void init(jerry_value_t f) {
        if (!jerry_value_is_function(f)) {
            return;
        }

        init_cb_function = f;
        ble.onEventsToProcess(BLE::OnEventsToProcessCallback_t(this, &BLEJS::scheduleBleEvents));
        ble.init(this, &BLEJS::initComplete);
    }

    void startAdvertising(jerry_value_t device_name_js, jerry_value_t service_uuids, jerry_value_t adv_interval_js) {
        size_t device_name_length = jerry_get_string_length(device_name_js);
        uint32_t uuids_length = jerry_get_array_length(service_uuids);
        
        // add an extra character to ensure there's a null character after the device name
        char* device_name = (char*)calloc(device_name_length + 1, sizeof(char));
        jerry_string_to_char_buffer(device_name_js, (jerry_char_t*)device_name, device_name_length);

        // parse the advertisement interval
        uint16_t adv_interval = 1000;
        if (jerry_value_is_number(adv_interval_js)) {
            double v = jerry_get_number_value(adv_interval_js);
            adv_interval = static_cast<uint16_t>(v);
        }

        for (uint32_t i = 0; i < uuids_length; i++) {
            // Check for UUID length
            jerry_value_t service_uuid = jerry_get_property_by_index(service_uuids, i);
            int uuid_length = (int)jerry_get_utf8_string_length(service_uuid);

            if(uuid_length == 4){
                // It's short UUID
                char buf[5] = {0};
                jerry_value_t uuid_str_obj = jerry_get_property_by_index(service_uuids, i);

                if (!jerry_value_is_string(service_uuid)) {
                    LOG_PRINT_ALWAYS("invalid uuid argument (%lu). Ignoring.\r\n", i);
                    jerry_release_value(service_uuid);
                    continue;
                }

                jerry_string_to_char_buffer(service_uuid, (jerry_char_t*)buf, 4);
                jerry_release_value(service_uuid);

                uint16_t uuid = hex_str_to_u16(buf, 4);
                ble.gap().accumulateAdvertisingPayload(GapAdvertisingData::INCOMPLETE_LIST_16BIT_SERVICE_IDS, (uint8_t *)uuid, 2);
            }
            else{
                // It's a complete 128bit UUID
                // unwrap the uuid
                char uuid_buf[37] = {0};
                jerry_string_to_char_buffer(service_uuid, (jerry_char_t*)uuid_buf, 37);
                jerry_release_value(service_uuid);
                
                ble.gap().accumulateAdvertisingPayload(GapAdvertisingData::INCOMPLETE_LIST_128BIT_SERVICE_IDS, (jerry_char_t*)uuid_buf, 37);
        
            }
        }

        ble.gap().accumulateAdvertisingPayload(GapAdvertisingData::BREDR_NOT_SUPPORTED | GapAdvertisingData::LE_GENERAL_DISCOVERABLE);
        ble.gap().accumulateAdvertisingPayload(GapAdvertisingData::COMPLETE_LOCAL_NAME, (uint8_t *)device_name, device_name_length);
        ble.gap().setAdvertisingType(GapAdvertisingParams::ADV_CONNECTABLE_UNDIRECTED);
        ble.gap().setDeviceName((const uint8_t*)device_name);
        ble.gap().setAdvertisingInterval(adv_interval); /* 1000ms. */
        ble.gap().startAdvertising();

        free(device_name);
    }

    void startAdvertising(jerry_value_t device_name_js, jerry_value_t service_uuids, jerry_value_t adv_interval_js, jerry_value_t manufacturer_data_js) {
        size_t device_name_length = jerry_get_string_length(device_name_js);
        uint32_t uuids_length = jerry_get_array_length(service_uuids);
        size_t manufacturer_data_length = jerry_get_string_length(manufacturer_data_js);
        
        // add an extra character to ensure there's a null character after the device name
        char* device_name = (char*)calloc(device_name_length + 1, sizeof(char));
        jerry_string_to_char_buffer(device_name_js, (jerry_char_t*)device_name, device_name_length);

        // parse the advertisement interval
        uint16_t adv_interval = 1000;
        if (jerry_value_is_number(adv_interval_js)) {
            double v = jerry_get_number_value(adv_interval_js);
            adv_interval = static_cast<uint16_t>(v);
        }

        for (uint32_t i = 0; i < uuids_length; i++) {
            // Check for UUID length
            jerry_value_t service_uuid = jerry_get_property_by_index(service_uuids, i);
            int uuid_length = (int)jerry_get_utf8_string_length(service_uuid);

            if(uuid_length == 4){
                // It's short UUID
                char buf[5] = {0};
                jerry_value_t uuid_str_obj = jerry_get_property_by_index(service_uuids, i);

                if (!jerry_value_is_string(service_uuid)) {
                    LOG_PRINT_ALWAYS("invalid uuid argument (%lu). Ignoring.\r\n", i);
                    jerry_release_value(service_uuid);
                    continue;
                }

                jerry_string_to_char_buffer(service_uuid, (jerry_char_t*)buf, 4);
                jerry_release_value(service_uuid);

                uint16_t uuid = hex_str_to_u16(buf, 4);
                ble.gap().accumulateAdvertisingPayload(GapAdvertisingData::INCOMPLETE_LIST_16BIT_SERVICE_IDS, (uint8_t *)uuid, 2);
            }
            else{
                // It's a complete 128bit UUID
                // unwrap the uuid
                char uuid_buf[37] = {0};
                jerry_string_to_char_buffer(service_uuid, (jerry_char_t*)uuid_buf, 37);
                jerry_release_value(service_uuid);
                
                ble.gap().accumulateAdvertisingPayload(GapAdvertisingData::INCOMPLETE_LIST_128BIT_SERVICE_IDS, (jerry_char_t*)uuid_buf, 37);
        
            }
        }

        // add an extra character to ensure there's a null character after the manufacturer data
        char* manufacturer_data = (char*)calloc(manufacturer_data_length, sizeof(char));
        jerry_string_to_char_buffer(manufacturer_data_js, (jerry_char_t*)manufacturer_data, manufacturer_data_length);
        
        uint8_t man_dat[6] = {0};
        man_str_to_u8_array(manufacturer_data, manufacturer_data_length, man_dat);

        ble.gap().accumulateScanResponse(GapAdvertisingData::MANUFACTURER_SPECIFIC_DATA, man_dat, manufacturer_data_length/2);    
        ble.gap().accumulateAdvertisingPayload(GapAdvertisingData::BREDR_NOT_SUPPORTED | GapAdvertisingData::LE_GENERAL_DISCOVERABLE);
        ble.gap().accumulateAdvertisingPayload(GapAdvertisingData::COMPLETE_LOCAL_NAME, (uint8_t *)device_name, device_name_length);
        ble.gap().setAdvertisingType(GapAdvertisingParams::ADV_CONNECTABLE_UNDIRECTED);
        ble.gap().setDeviceName((const uint8_t*)device_name);
        ble.gap().setAdvertisingInterval(adv_interval); /* 1000ms. */
        ble.gap().startAdvertising();

        free(device_name);
        free(manufacturer_data);
    }

    void startAdvertising() {
        ble.gap().startAdvertising();
    }

    void stopAdvertising() {
        ble.stopAdvertising();
    }

    void addService(GattService *srvc) {
        ble.addService(*srvc);
    }

    void onConnection(jerry_value_t f) {
        connect_cb_function = f;
        ble.gap().onConnection(this, &BLEJS::connectionCallback);
    }

    void onDisconnection(jerry_value_t f) {
        disconnect_cb_function = f;
        ble.gap().onDisconnection(this, &BLEJS::disconnectionCallback);
    }

    bool isConnected() {
        return ble.gap().getState().connected;
    }

    void setWriteCallback(GattCharacteristic* characteristic, jerry_value_t callback) {
        write_callbacks[characteristic] = callback;
    }

    void clearWriteCallback(GattCharacteristic* characteristic) {
        write_callbacks.erase(characteristic);
    }

 private:
     BLEJS(BLE &ble) : ble(ble) {
        this_obj = jerry_create_null();

        ble.gattServer().onDataWritten(this, &BLEJS::onDataWrittenCallback);
    }

    ~BLEJS() {
    }

    BLEJS(BLEJS const&);            // Empty on purpose
    void operator=(BLEJS const&);   // Empty on purpose

    void initComplete(BLE::InitializationCompleteCallbackContext *context) {
        if (context->error != BLE_ERROR_NONE) {
            LOG_PRINT_ALWAYS("Error while initialising BLE context\r\n");
            return;
        }

        if (jerry_value_is_function(init_cb_function)) {
            jerry_call_function(init_cb_function, this_obj, NULL, 0);
        }
    }

    void scheduleBleEvents(BLE::OnEventsToProcessCallbackContext* context) {
        BLE &ble = BLE::Instance();
        mbed::js::EventLoop::getInstance().nativeCallback(mbed::Callback<void()>(&ble, &BLE::processEvents));
    }

    void connectionCallback(const Gap::ConnectionCallbackParams_t *params) {
        if (jerry_value_is_function(connect_cb_function)) {
            jerry_call_function(connect_cb_function, this_obj, NULL, 0);
        }
    }

    void disconnectionCallback(const Gap::DisconnectionCallbackParams_t *params) {
        if (jerry_value_is_function(disconnect_cb_function)) {
            jerry_call_function(disconnect_cb_function, this_obj, NULL, 0);
        }
    }

    void onDataWrittenCallback(const GattWriteCallbackParams *params) {
        // see if we know for which char this message is...
        typedef std::map<GattCharacteristic*, jerry_value_t>::iterator it_type;
        for(it_type it = write_callbacks.begin(); it != write_callbacks.end(); it++) {
            if (it->first->getValueHandle() == params->handle) {
                if (jerry_value_is_function(it->second)) {
                    const jerry_value_t args[1] = {
                        getJsValueFromCharacteristic(it->first)
                    };

                    // @todo, this_obj is wrong
                    jerry_call_function(it->second, this_obj, args, 1);
                }
            }
        }
    }


    jerry_value_t this_obj;
    jerry_value_t init_cb_function;
    jerry_value_t connect_cb_function;
    jerry_value_t disconnect_cb_function;
    map<GattCharacteristic*, jerry_value_t> write_callbacks;

    BLE& ble;
};

#endif // _JERRYSCRIPT_MBED_BLE_BLEJS_H
