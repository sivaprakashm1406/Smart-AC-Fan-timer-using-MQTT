#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>


#define DHTPIN          4
#define DHTTYPE         DHT22
#define BUTTON_UP       18
#define BUTTON_DOWN     19
#define RELAY_PIN       23 
#define STEP_PIN        25
#define DIR_PIN         26


#define SCREEN_WIDTH    128
#define SCREEN_HEIGHT   64
#define OLED_RESET      -1
#define SCREEN_ADDRESS  0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);


DHT dht(DHTPIN, DHTTYPE);


const char* ssid = "Wokwi-GUEST";
const char* password = "";
const char* mqtt_server = "broker.hivemq.com"; 

WiFiClient espClient;
PubSubClient client(espClient);


const char* topic_temp      = "hvac/room/temp";
const char* topic_setpoint  = "hvac/room/setpoint";
const char* topic_timer     = "hvac/timer/runtime";
const char* topic_power     = "hvac/analytics/power";


float roomTemp = 0.0;
float roomHumid = 0.0;
int targetTemp = 24;            
unsigned long timerMillis = 0;  
bool systemActive = true;       


unsigned long totalAcOnTimeMs = 0;
unsigned long totalFanOnTimeMs = 0;
unsigned long acStartTimeMs = 0;
unsigned long fanStartTimeMs = 0;
bool lastAcState = false;       
bool lastFanState = false;
bool powerSavedCalculated = false;

// --- Timing Trackers (Non-blocking loops) ---
unsigned long lastSensorRead = 0;
const unsigned long sensorInterval = 2000;

unsigned long lastDisplayUpdate = 0;
const unsigned long displayInterval = 500;

unsigned long lastTimerTick = 0;
const unsigned long tickInterval = 1000;

unsigned long lastStepTime = 0;
unsigned long stepDelayUs = 2000;
bool stepState = false;

// --- Hysteresis Threshold ---
const float HYSTERESIS = 0.5;

// --- Function Declarations ---
void setupWiFi();
void reconnect();
void callback(char* topic, byte* payload, unsigned int length);
void handleButtons();
void runThermostatLogic();
void handleStepperFan();
void updateOLED();
void calculatePowerSaved();

void setup() {
  Serial.begin(115200);

  
  pinMode(BUTTON_UP, INPUT_PULLUP);
  pinMode(BUTTON_DOWN, INPUT_PULLUP);
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(STEP_PIN, OUTPUT);
  pinMode(DIR_PIN, OUTPUT);

  
  digitalWrite(RELAY_PIN, HIGH);
  digitalWrite(DIR_PIN, HIGH);

  
  dht.begin();
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }
  display.clearDisplay();
  display.display();

  
  setupWiFi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

void loop() {
  
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  unsigned long currentMillis = millis();

  
  if (systemActive && timerMillis > 0) {
    if (currentMillis - lastTimerTick >= tickInterval) {
      if (timerMillis >= 1000) {
        timerMillis -= 1000;
      } else {
        timerMillis = 0;
      }
      lastTimerTick = currentMillis;

      
      if (timerMillis == 0) {
        systemActive = false;
        calculatePowerSaved();
      }
    }
  }

  
  if (currentMillis - lastSensorRead >= sensorInterval) {
    float t = dht.readTemperature();
    float h = dht.readHumidity();
    if (!isnan(t) && !isnan(h)) {
      roomTemp = t;
      roomHumid = h;
      client.publish(topic_temp, String(roomTemp, 1).c_str());
    }
    lastSensorRead = currentMillis;
  }

  
  handleButtons();
  
  if (systemActive) {
    runThermostatLogic();
    handleStepperFan(); // Spin the fan if active
  } else {
    
    if (lastAcState) {
      digitalWrite(RELAY_PIN, HIGH); 
      totalAcOnTimeMs += (currentMillis - acStartTimeMs);
      lastAcState = false;
    }
    if (lastFanState) {
      totalFanOnTimeMs += (currentMillis - fanStartTimeMs);
      lastFanState = false;
    }
  }

  
  if (currentMillis - lastDisplayUpdate >= displayInterval) {
    updateOLED();
    lastDisplayUpdate = currentMillis;
  }
}


void setupWiFi() {
  delay(10);
  Serial.print("\nConnecting to ");
  Serial.println(ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.print("\nWiFi connected. IP: ");
  Serial.println(WiFi.localIP());
}


void reconnect() {
  static unsigned long lastReconnectAttempt = 0;
  unsigned long now = millis();
  
  if (!client.connected()) {
    if (now - lastReconnectAttempt > 5000) {
      lastReconnectAttempt = now;
      Serial.print("Attempting MQTT connection...");
      
      String clientId = "ESP32Client-";
      clientId += String(random(0xffff), HEX);
      
      if (client.connect(clientId.c_str())) {
        Serial.println("connected");
        client.subscribe(topic_setpoint);
        client.subscribe(topic_timer);
      } else {
        Serial.print("failed, rc=");
        Serial.print(client.state());
        Serial.println(" try again in 5 seconds");
      }
    }
  }
}


void callback(char* topic, byte* payload, unsigned int length) {
  String message = "";
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  message.trim(); 
  if (String(topic) == topic_setpoint) {
    int incomingSetpoint = message.toInt();
    if (incomingSetpoint >= 16 && incomingSetpoint <= 30) {
      targetTemp = incomingSetpoint;
      Serial.print("Target Temp updated via MQTT: ");
      Serial.println(targetTemp);
    }
  } 
  else if (String(topic) == topic_timer) {
    long minutes = message.toInt();
    if (minutes > 0) {
      timerMillis = minutes * 60 * 1000;
      systemActive = true;
      powerSavedCalculated = false; 
      Serial.print("Runtime Timer set via MQTT: ");
      Serial.print(minutes);
      Serial.println(" minutes.");
    }
  }
}


void handleButtons() {
  static unsigned long lastDebounceTime = 0;
  const unsigned long debounceDelay = 250; 
  
  if (millis() - lastDebounceTime > debounceDelay) {
    
    if (digitalRead(BUTTON_UP) == LOW) {
      timerMillis += (15 * 60 * 1000); 
      systemActive = true;
      powerSavedCalculated = false;
      client.publish(topic_timer, String(timerMillis / (60 * 1000)).c_str(), true);
      lastDebounceTime = millis();
      Serial.println("Timer +15 mins via button.");
    }
    
    
    if (digitalRead(BUTTON_DOWN) == LOW) {
      if (timerMillis >= (15 * 60 * 1000)) {
        timerMillis -= (15 * 60 * 1000);
      } else {
        timerMillis = 0; 
      }
      
      if (timerMillis == 0) {
        systemActive = false;
        calculatePowerSaved();
      }
      
      client.publish(topic_timer, String(timerMillis / (60 * 1000)).c_str(), true);
      lastDebounceTime = millis();
      Serial.println("Timer -15 mins via button.");
    }
  }
}


void runThermostatLogic() {
  unsigned long currentMillis = millis();

  if (roomTemp > (targetTemp + HYSTERESIS)) {
    if (!lastAcState) {
      digitalWrite(RELAY_PIN, LOW); // Trigger relay ON (Active-LOW)
      acStartTimeMs = currentMillis;
      lastAcState = true;
    }
  } 
  else if (roomTemp < (targetTemp - HYSTERESIS)) {
    if (lastAcState) {
      digitalWrite(RELAY_PIN, HIGH); // Trigger relay OFF
      totalAcOnTimeMs += (currentMillis - acStartTimeMs);
      lastAcState = false;
    }
  }

  if (!lastFanState) {
    fanStartTimeMs = currentMillis;
    lastFanState = true;
  }
}


void handleStepperFan() {
  if (!lastFanState) return;

  unsigned long currentMicros = micros();
  if (currentMicros - lastStepTime >= stepDelayUs) {
    stepState = !stepState;
    digitalWrite(STEP_PIN, stepState);
    lastStepTime = currentMicros;
  }
}


void calculatePowerSaved() {
  if (powerSavedCalculated) return;

  unsigned long currentMillis = millis();
  if (lastAcState) totalAcOnTimeMs += (currentMillis - acStartTimeMs);
  if (lastFanState) totalFanOnTimeMs += (currentMillis - fanStartTimeMs);

  double acHoursRun = (double)totalAcOnTimeMs / 3600000.0;
  double fanHoursRun = (double)totalFanOnTimeMs / 3600000.0;

  double remainingMorningHours = 8.0 - (acHoursRun > fanHoursRun ? acHoursRun : fanHoursRun);
  if (remainingMorningHours < 0) remainingMorningHours = 0;

  double wattHoursSaved = remainingMorningHours * (1200.0 + 60.0);

  client.publish(topic_power, String(wattHoursSaved, 2).c_str(), true);
  Serial.print("Morning shutdown achieved. Power Saved: ");
  Serial.print(wattHoursSaved);
  Serial.println(" Wh");

  powerSavedCalculated = true;
}


void updateOLED() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  
  display.drawRect(0, 0, 128, 64, SSD1306_WHITE);
  
  
  display.setTextSize(1);
  display.setCursor(8, 8);
  display.print("TIMER SET:");
  
  display.setCursor(8, 20);
  display.setTextSize(2); 
  if (!systemActive || timerMillis == 0) {
    display.print("OFF (AM)");
  } else {
    unsigned long totalSecs = timerMillis / 1000;
    int hrs = totalSecs / 3600;
    int mins = (totalSecs % 3600) / 60;
    int secs = totalSecs % 60;
    
    if(hrs < 10) display.print("0"); display.print(hrs); display.print(":");
    if(mins < 10) display.print("0"); display.print(mins); display.print(":");
    if(secs < 10) display.print("0"); display.print(secs);
  }
  
  display.drawFastHLine(4, 38, 120, SSD1306_WHITE);
  
  
  display.setTextSize(1);
  display.setCursor(8, 46);
  display.print("CURRENT TEMP:");
  
  display.setCursor(90, 46);
  display.setTextSize(1);
  display.print(roomTemp, 1);
  display.print((char)247); 
  display.print("C");
  
  display.display();
}
