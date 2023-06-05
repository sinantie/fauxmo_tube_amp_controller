#include <fauxmoESP.h>
#include <ESP8266WiFi.h> // Include the Wi-Fi library

fauxmoESP fauxmo;

#define SERIAL_BAUDRATE    74880
#define WIFI_SSID "VM4524677"
#define WIFI_PASS "rn3qwCshhdtd"

#define RELAY_1              1
#define RELAY_2              3
#define LED_PIN             1

#define ID_AMPLIFIER           "amplifier"
#define ID_STANDBY            "standby"

#define DEBUG false
static unsigned long amplifier_on_time = -1;
static bool amplifier_off_request = false;

void wifiSetup() {

    // Set WIFI module to STA mode
    WiFi.mode(WIFI_STA);

    // Connect
    if(DEBUG) {
        Serial.printf("[WIFI] Connecting to %s ", WIFI_SSID);
    }    
    WiFi.begin(WIFI_SSID, WIFI_PASS);

    // Wait
    while (WiFi.status() != WL_CONNECTED) {
        if(DEBUG) {
            Serial.print(".");
        }        
        delay(100);
    }
    if(DEBUG) {
        Serial.println();
        // Connected!
        Serial.printf("[WIFI] STATION Mode, SSID: %s, IP address: %s\n", WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());    
    }    
}

void setup() {

    // Init serial port and clean garbage
    if(DEBUG) {
        Serial.begin(SERIAL_BAUDRATE);
        Serial.println();
    }        

    // LEDs
    pinMode(RELAY_1, OUTPUT);
    pinMode(RELAY_2, OUTPUT);
    digitalWrite(RELAY_1, LOW);
    digitalWrite(RELAY_2, LOW);
    
    // Wifi
    wifiSetup();

    // By default, fauxmoESP creates it's own webserver on the defined port
    // The TCP port must be 80 for gen3 devices (default is 1901)
    // This has to be done before the call to enable()
    fauxmo.createServer(true); // not needed, this is the default value
    fauxmo.setPort(80); // This is required for gen3 devices

    // You have to call enable(true) once you have a WiFi connection
    // You can enable or disable the library at any moment
    // Disabling it will prevent the devices from being discovered and switched
    fauxmo.enable(true);

    // You can use different ways to invoke alexa to modify the devices state:
    // "Alexa, turn yellow lamp on"
    // "Alexa, turn on yellow lamp
    // "Alexa, set yellow lamp to fifty" (50 means 50% of brightness, note, this example does not use this functionality)

    // Add virtual devices - Note that many users have problems on ESP8266 with more than three devices
    fauxmo.addDevice(ID_AMPLIFIER);
    fauxmo.addDevice(ID_STANDBY);
    

    fauxmo.onSetState([](unsigned char device_id, const char * device_name, bool state, unsigned char value) {
        
        // Callback when a command from Alexa is received. 
        // You can use device_id or device_name to choose the element to perform an action onto (relay, LED,...)
        // State is a boolean (ON/OFF) and value a number from 0 to 255 (if you say "set kitchen light to 50%" you will receive a 128 here).
        // Just remember not to delay too much here, this is a callback, exit as soon as possible.
        // If you have to do something more involved here set a flag and process it in your main loop.
        
        if(DEBUG) {
            Serial.printf("[MAIN] Device #%d (%s) state: %s value: %d\n", device_id, device_name, state ? "ON" : "OFF", value);
        }        

        // Checking for device_id is simpler if you are certain about the order they are loaded and it does not change.
        // Otherwise comparing the device_name is safer.

        if (strcmp(device_name, ID_AMPLIFIER)==0) {
            if(state) { // if we just turned on the amplifiler
                amplifier_on_time = millis();
            } else { // if we just turned of the amplifier
                amplifier_off_request = true;
            }
            digitalWrite(RELAY_1, state ? HIGH : LOW);
        } else if (strcmp(device_name, ID_STANDBY)==0) {
            digitalWrite(RELAY_2, state ? HIGH : LOW);
        }
    });

}

void loop() {

    // fauxmoESP uses an async TCP server but a sync UDP server
    // Therefore, we have to manually poll for UDP packets
    fauxmo.handle();

    // This is a sample code to output free heap every 5 seconds
    // This is a cheap way to detect memory leaks
    static unsigned long last = millis();
    if (millis() - last > 5000) {
        last = millis();
        uint32_t free = ESP.getFreeHeap();
        if(DEBUG) {
            Serial.printf("[MAIN] Free heap: %d bytes\n", free);
        }        
    }
    if (millis() - amplifier_on_time > 60000 && amplifier_on_time != -1) {
        fauxmo.setState(ID_STANDBY, true, 255); // automatically turn standby on after 60 secs
        digitalWrite(RELAY_2, HIGH);
        if(DEBUG) {
            Serial.println("Auto turn standby on");
        }        
        amplifier_on_time = -1;
    }
    if (amplifier_off_request) {
        fauxmo.setState(ID_STANDBY, false, 0); // automatically turn standby off after turning off amplifier
        digitalWrite(RELAY_2, LOW);
        amplifier_off_request = false;
        if(DEBUG) {
            Serial.println("Auto turn standby off");
        }
    }  
    if (millis() - amplifier_on_time > 10800000 && amplifier_on_time != -1) {
        fauxmo.setState(ID_STANDBY, false, 0); // automatically turn standby and amplifier off after 3 hours
        digitalWrite(RELAY_2, LOW);
        fauxmo.setState(ID_AMPLIFIER, false, 0); // automatically turn standby and amplifier off after 3 hours
        digitalWrite(RELAY_1, LOW);
        if(DEBUG) {
            Serial.println("Auto turn amplifier and standby off");
        }        
        amplifier_on_time = -1;
    }  
    // If your device state is changed by any other means (MQTT, physical button,...)
    // you can instruct the library to report the new state to Alexa on next request:
    // fauxmo.setState(ID_YELLOW, true, 255);

}
