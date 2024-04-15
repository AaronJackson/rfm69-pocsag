#include <ESP8266WiFi.h>
#include <PubSubClient.h>

const char* ssid = "Rhwyd_2.4GHz";
const char* password = "";
const char* mqtt_server = "192.168.1.228";

#define NAME "pocsag-gw"
#define ENABLE_FEATHER 2

WiFiClient espClient;
PubSubClient client(espClient);

void setup_wifi() {

  delay(10);
  // We start by connecting to a WiFi network

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }

  randomSeed(micros());

}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    // Create a random client ID
    String clientId = NAME;
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str())) {
      client.subscribe("pocsag/send");
      client.subscribe("pocsag/power");
    } else {
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  if (strcmp(topic, "pocsag/send") == 0) {
    char msg[length+1] = {0};
    memcpy(msg, payload, length);
    Serial.println(msg);
    return;
  }

  if (strcmp(topic, "pocsag/power") == 0) {
    char msg[length+1] = {0};
    memcpy(msg, payload, length);
    if (strcmp(msg, "ON") == 0) {
      digitalWrite(ENABLE_FEATHER, HIGH);
    }
    if (strcmp(msg, "OFF") == 0) {
      digitalWrite(ENABLE_FEATHER, LOW);
    }
    return;
  }
}

void setup() {
  pinMode(ENABLE_FEATHER, OUTPUT);  // Adafruit Feather EN pin

  Serial.begin(9600);
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  digitalWrite(ENABLE_FEATHER, true);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
}
