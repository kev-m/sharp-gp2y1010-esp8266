**About**

This program uses a [Sharp GP2Y1010](http://www.sharp-world.com/products/device/lineup/data/pdf/datasheet/gp2y1010au_e.pdf) 
dust sensor as a [Homie Node](https://homieiot.github.io/) on an [ESP8266](https://wiki.wemos.cc/products:d1:d1_mini_lite) board.
It runs continuously, reporting the counts over [MQTT]() at a configurable interval.

It works best with [Homie for ESP8266 / ESP32 v3](https://github.com/homieiot/homie-esp8266/tree/develop-v3)

---

## Basic Operation

The GP2Y1010 sensor outputs an analog pulse with a pulse height proportional to the
particle size / density.

Due to a bug in the ESP8266 analog read function (WiFi becomes unstable if read too 
often, this program uses a digital GPIO input and calculates an approximate analog 
value by counting the number of loops that the sensor output is above the digital 
LOW/HIGH threshold.

It also computes a rough histogram, based on the number of counts per 10ms cycle.

The interval (in seconds) is set via the "interval" setting in the config.json

```json
{
    "name": "Dust Sensor",
    "device_id": "dust-1",
    "device_stats_interval": 300,
    "wifi": {
      "ssid": "<SSID>",
      "password": "<Password>"
    },
    "mqtt": {
      "host": "<MQTT broker IP>",
      "port": 1883,
      "base_topic": "<base topic>/"
    },
    "ota": {
      "enabled": true
    },
    "settings": {
      "interval" : 60
    }
}
```

The config.json must be installed to the ESP8266, e.g. using
[ESP8266FS-0.4.0](https://github.com/esp8266/arduino-esp8266fs-plugin/releases/download/0.4.0/ESP8266FS-0.4.0.zip) 
to upload the configuration to the ESP to /homie/config.json, 
[as documented here](http://arduino.esp8266.com/Arduino/versions/2.3.0/doc/filesystem.html#uploading-files-to-file-system)

See [JSON configuration file](https://homieiot.github.io/homie-esp8266/docs/develop-v3/configuration/json-configuration-file/).

## Compilation

This project is intended for the Ardiono IDE.

Clone this repository and load the "Homiev3_DustSensor_Sharp" project into the Arduino IDE.

You will need to manually install the Homie [dependencies](https://homieiot.github.io/homie-esp8266/docs/develop-v3/quickstart/getting-started/).

Examples that worked for me are:
1. [async-mqtt-client-0.8.1](https://codeload.github.com/marvinroger/async-mqtt-client/zip/v0.8.1)
1. [ESPAsyncWebServer](https://codeload.github.com/me-no-dev/ESPAsyncWebServer/zip/2f3703702987e31249d4c5c9d1f90cebf1ffa9e8)
1. [ESPAsyncTCP](https://codeload.github.com/me-no-dev/ESPAsyncTCP/zip/b4f18df384c291bf15a4d7c499e06b7e0a9884c5)
1. [https://github.com/bblanchon/ArduinoJson]() 

## Wiring Diagram

While the SHARP GPY2Y1010 is a 5V device, I used the 3.3V supply to power the photo-diode sensor to guarantee
that the sesnor output (Vo) will not exceed the maximum rated voltage of the ESP8266 GPIO line.

| SHARP GPY2Y1010 | Weimos D1 Mini | Signal  |
| -------------   |:-------------:| -----:|
| 1. V-LED        | 5V 			| LED power |
| 2. LED-GND      | Gnd      	| LED ground  |
| 3. LED-Enable   | D2      	| LED enable  |
| 4. S-GND        | Gnd      	| Sensor ground  |
| 5. Vo           | D5      	| Sensor output  |
| 6. Vcc          | 3V3      	| Sensor power   |

## The Data

The data is output on two MQTT topics, *count* and *histo* as follows:
1. <topic_prefix>/<device_id>/dust-counts/count
1. <topic_prefix>/<device_id>/dust-counts/histo

Where "topic_prefix" and "device_id" are specified in the JSON configuration file.

The *count* topic is produced at the interval specified in the JSON configuration.

The *histo* topic is only produced when count is greater than 0 and contains a 10-bin histogram of the psuedo analog signal:
```
1, 0, 1, 4, 0, 0, 0, 0, 0, 0
0, 0, 1, 1, 3, 9, 2, 0, 0, 0
0, 0, 2, 0, 2, 0, 0, 0, 0, 0
0, 0, 0, 1, 1, 2, 6, 0, 0, 0
```

## OTA
[Over-the-Air (OTA) updates](https://homieiot.github.io/homie-esp8266/docs/develop-v3/others/ota-configuration-updates/) is 
enabled and supported using the included [scripts](https://github.com/homieiot/homie-esp8266/tree/develop/scripts/ota_updater).

In Arduino.cc, use CTRL-SHIFT-S to export the binary to the project directory.

## TODOs
For future work:
1. Make it sleep for X minutes, sample for Y seconds, then report.