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
#include "jerryscript-mbed-event-loop/EventLoop.h"

#include "jerryscript-mbed-ble/ble-js.h"
#include "jerryscript-mbed-ble/BLEJS.h"
#include "jerryscript-mbed-ble/blejs_types.h"

#include "ble/BLE.h"

#include "Callback.h"

void BLEService__destructor(uintptr_t native_ptr) {
    js_ble_service_data_t *data = (js_ble_service_data_t*)native_ptr;
    delete data->service;
    free(data->characteristics);
    free(data);
}

DECLARE_CLASS_FUNCTION(BLEService, getUUID) {
    CHECK_ARGUMENT_COUNT(BLEService, getUUID, args_count==0);

    // get the native pointer
    uintptr_t native_handle;
    jerry_get_object_native_handle(this_obj, &native_handle);

    js_ble_service_data_t *native_ptr = (js_ble_service_data_t*)native_handle;
    GattService *service = native_ptr->service;

    const UUID & uuid = service->getUUID();

    // @todo, support 128-bit UUIDs
    if (uuid.shortOrLong() == UUID::UUID_TYPE_LONG) {
        const uint8_t *longUUID= uuid.getBaseUUID();
        char buf[37] = {0};
        for(int i = 0; i < 16; i+=1){
            sprintf(&buf[i*2], "%02x", longUUID[15-i]);
        }
        return jerry_create_string((const jerry_char_t *) buf);
    }
    else {
        uint16_t shortUuid = uuid.getShortUUID();

        char buf[5];
        sprintf(buf, "%04x", shortUuid);
        return jerry_create_string((const jerry_char_t *) buf);
    }
}

DECLARE_CLASS_CONSTRUCTOR(BLEService) {
    CHECK_ARGUMENT_COUNT(BLEService, __constructor, (args_count == 2));
    CHECK_ARGUMENT_TYPE_ALWAYS(BLEService, __constructor, 0, string);
    CHECK_ARGUMENT_TYPE_ALWAYS(BLEService, __constructor, 1, array);

    jerry_value_t service_uuid = args[0];
    jerry_value_t characteristics = args[1];

    uint32_t characteristics_count = jerry_get_array_length(characteristics);

    LOG_PRINT_ALWAYS("discovered %li characteristics\r\n", characteristics_count);

    GattCharacteristic **characteristics_array = (GattCharacteristic**)calloc(characteristics_count, sizeof(GattCharacteristic*));

    for (uint32_t i = 0; i < characteristics_count; i++) {
        jerry_value_t char_obj = jerry_get_property_by_index(characteristics, i);
        uintptr_t native_ptr;
        jerry_get_object_native_handle(char_obj, &native_ptr);
        characteristics_array[i] = (GattCharacteristic*)native_ptr;
        jerry_release_value(char_obj);
    }

    GattService *jsService = NULL;

    int uuid_length = (int)jerry_get_utf8_string_length(service_uuid);
    printf("\nLength of Advertisement UUID: %i\n", uuid_length);
    if(uuid_length == 4){
        // It's short UUID
        // unwrap the uuid
        char uuid_buf[4] = {0};
        jerry_string_to_char_buffer(service_uuid, (jerry_char_t*)uuid_buf, 4);
        jerry_release_value(service_uuid);
        uint16_t uuid = hex_str_to_u16(uuid_buf, 4);

        jsService = new GattService(uuid, characteristics_array, characteristics_count);
    }
    else{
        // It's a complete 128bit UUID
        
        // unwrap the uuid
        char uuid_buf[42] = {0};
        jerry_string_to_char_buffer(service_uuid, (jerry_char_t*)uuid_buf, 42);
        jerry_release_value(service_uuid);
        jsService = new GattService(uuid_buf, characteristics_array, characteristics_count);
    }
    
    
    
    
    
    js_ble_service_data_t *serviceData = (js_ble_service_data_t*)malloc(sizeof(js_ble_service_data_t));
    serviceData->service = jsService;
    serviceData->characteristics = characteristics_array;
    serviceData->characteristics_count = characteristics_count;

    uintptr_t native_ptr = (uintptr_t)serviceData;

    // create the jerryscript object
    jerry_value_t js_object = jerry_create_object();
    jerry_set_object_native_handle(js_object, native_ptr, BLEService__destructor);

    ATTACH_CLASS_FUNCTION(js_object, BLEService, getUUID);

    return js_object;
}
