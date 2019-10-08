/**
   This program uses a Sharp GP2Y1010 dust sensor as a Homie Node on an ESP8266 board.
   It runs continuously, reporting the counts over MQTT at a configurable interval.

   The GP2Y1010 sensor outputs an analog pulse with a pulse height proportional to the
   particle size / density.

   Due to a bug in the ESP8266 analog read function (WiFi becomes unstable if read too 
   often, this program uses a digital GPIO input and calculates an approximate analog 
   value by counting the number of loops that the sensor output is above the digital 
   LOW/HIGH threshold.

   It also computes a rough histogram, based on the number of counts per 10ms cycle.

   The interval (in seconds) is set via the "interval" setting in the config.json:

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

   TODO: Make it sleep for X minutes, sample for Y seconds, then report.

 **/

#include <Homie.h>

void onHomieEvent(const HomieEvent& event) {
  if  (event.type == HomieEventType::WIFI_CONNECTED) {
    Serial << " Event -> HomieEventType::WIFI_CONNECTED" << endl;
    Serial << "Wi-Fi connected, IP: " << event.ip << ", gateway: " << event.gateway << ", mask: " << event.mask << endl;

    const HomieInternals::ConfigStruct& config = Homie.getConfiguration();
    Serial << " Configuration name: " << config.name << endl;
    Serial << " Configuration deviceId: " << config.deviceId << endl;

  } else if (event.type == HomieEventType::MQTT_READY) {
    Serial << "MQTT connected" << endl;
  } else if (event.type == HomieEventType::MQTT_DISCONNECTED) {
    Serial << "MQTT disconnected, reason: " << (int8_t)event.mqttReason << endl;
    delay(500);
  }
}

// ------------------------------------------ DustSensor stuff -------------------
const int SENSOR_TRIGGER_PIN = 4; // Use GPIO4 as trigger
const int PARTICLE_PIN = 14;      // Use GPIO14 as sensor

#define PULSE_INTERVAL 10         // milliseconds between pulses
#define PULSE_INTERVAL_COUNT 500  // How many loops to integrate pulse counter? Manually configured.
#define SENSOR_LED_ON LOW
#define SENSOR_LED_OFF HIGH
unsigned long nextPulseTimeMs = 0;
unsigned long dust_counts = 0;

#define HISTO_BINS 10
unsigned int histogram[HISTO_BINS];

// Reporting interval
unsigned long messageInterval = 60000; // Default: Once per minute
unsigned long nextMessageTimeMs = 0;

HomieNode dustCounterNode("dust-counts", "SHARP GP2Y1010 Dust particle counter" , "sensor");
HomieSetting<long> delaySetting("interval", "Reporting interval (s)");  // id, description


void setup() {
  Serial.begin(115200);
  Serial << endl << endl;

  pinMode(SENSOR_TRIGGER_PIN, OUTPUT);
  pinMode(PARTICLE_PIN, INPUT);

  // Set Homie LED pin to GPIO2
  Homie.setLedPin(2, LOW);
  Homie_setFirmware("sharp-GP2Y1010", "0.1"); // The underscore is not a typo! See Magic bytes

  // Populate the node name with default + setting
  delaySetting.setDefaultValue(60);

  Homie.onEvent(onHomieEvent);
  Homie.setSetupFunction(setupHandler);//.setLoopFunction(loopHandler);
  dustCounterNode.advertise("summary");
  dustCounterNode.advertise("unit");
  dustCounterNode.advertise("count");
  dustCounterNode.advertise("histo");

  Homie.setup();

  const HomieInternals::ConfigStruct& config = Homie.getConfiguration();
  Homie.getLogger() << "SHARP GP2Y1010 particle counter implementation v1.0" << endl;
  Homie.getLogger() << "MQTT topic root: " << config.mqtt.baseTopic << config.deviceId << endl;
  Homie.getLogger() << "Reporting interval set to " << delaySetting.get() << " seconds" << endl;
  messageInterval = delaySetting.get() * 1000;
}

void setupHandler() {
  dustCounterNode.setProperty("unit").send("#");

  char summary[100];
  snprintf(summary, sizeof(summary), "Integrating for %ld seconds", delaySetting.get());
  dustCounterNode.setProperty("summary").send(summary);
}

// histogram functions...
// Update the histogram, but only if there are any counts at all.
void updateHistogram(unsigned int tempCounts) {
  if (tempCounts > 0) {
    unsigned int histo_interval = PULSE_INTERVAL_COUNT / HISTO_BINS; // 500/10 -> min of 50
    unsigned int index = tempCounts / histo_interval;
    // Some sanity checks
    if (index >= HISTO_BINS) {
      index = HISTO_BINS - 1;
    }
    histogram[index]++;
  }
}

// Convert the histogram into a string
String histoToString() {
  String result = "";
  for (int i = 0; i < HISTO_BINS - 1; i++) {
    result += String(histogram[i]) + ", ";
  }
  result += String(histogram[HISTO_BINS - 1]);

  return result;
}

void histoReset() {
  for (int i = 0; i < HISTO_BINS - 1; i++) {
    histogram[i] = 0;
  }
}
// ... histogram functions

// Our sensor's loop
//WARNING: Only use this if the ".setLoopFunction(loopHandler);" is not used!
void loopHandler() {
  unsigned long now = millis();

  // TODO: Do this properly. The code below will produce spam on every tick until the rollover occurs.
  // Check for roll-over of time
  if ((now + PULSE_INTERVAL) < nextPulseTimeMs) {
    Homie.getLogger() << "millis() roll-over detected. Resetting timers." << " counts" << endl;
    nextPulseTimeMs = now + PULSE_INTERVAL;
    nextMessageTimeMs = now + messageInterval;
    return;
  }

  // Handle waveform: High
  if (now >= nextPulseTimeMs) {
    unsigned long tempCounts = 0;
    digitalWrite(SENSOR_TRIGGER_PIN, SENSOR_LED_ON);
    nextPulseTimeMs = now + PULSE_INTERVAL;

    // Brute loop: 450 loops ~ 0.280ms
    for (int i = 0; i < PULSE_INTERVAL_COUNT; i++) {
      tempCounts += digitalRead(PARTICLE_PIN);  // IO0
    }
    digitalWrite(SENSOR_TRIGGER_PIN, SENSOR_LED_OFF);

    updateHistogram(tempCounts);

    // Simple, just aggregrate
    dust_counts += (tempCounts / (PULSE_INTERVAL_COUNT / HISTO_BINS));
  }

  if (now >= nextMessageTimeMs) {
    nextMessageTimeMs = now + messageInterval;

    if (Homie.isConnected()) {
      dustCounterNode.setProperty("count").send(String(dust_counts));
      if (dust_counts > 0) {
        dustCounterNode.setProperty("histo").send(histoToString());
      }
    }

    // Report the statistics over the serial port even if not connected to the MQTT broker
    Homie.getLogger() << "Dust counts: " << dust_counts << " counts" << endl;
    if (dust_counts > 0) {
      Homie.getLogger() << "Dust histo: " << histoToString() << endl;
      histoReset();
      dust_counts = 0;
    }
  }
}

void loop() {
  Homie.loop();
  //WARNING: Only use this if the ".setLoopFunction(loopHandler);" is not used!
  // Running loopHanlder here allows reporting over the serial port
  // even if the broker is not connected.
  loopHandler();
}
