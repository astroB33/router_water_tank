#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <time.h>
#include <AsyncUDP.h>
#include <WebSerial.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#include <esp_sntp.h>

#include <esp_task_wdt.h>
#include <Preferences.h>

#include "src/water_tank_power_handler.h"
#include "src/power_manager.h"

// Wifi
const char* ssid = "XXXX"; 
const char* password = "YYYY";

// UDP port
#define UDP_PORT_POWER 8080

// Time
#define MY_NTP_SERVER "pool.ntp.org"
// https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv
#define MY_TZ "CET-1CEST,M3.5.0,M10.5.0/3"

#define PIN_LED 2

#define PIN_RELAY_1 4
#define PIN_RELAY_2 16
#define PIN_RELAY_DIMMER_FULL 17
#define PIN_RELAY_DIMMER_DIM 5
#define PIN_DIMMER_OUT 18
#define PIN_DIMMER_ZERO_CROSSING 19

// GPIO where the DS18B20 is connected to
#define PIN_TEMPERATURE 21  

#define POWER_PER_RELAY 800 // W
#define NIGHT_POWER 2400 // W
#define WATER_VOLUME 200 // l
#define MIN_DIMMER_POWER 0

#define MIN_TIME_DAY 8
#define MAX_TIME_DAY 20
#define MIN_TIME_NIGHT 0
#define MAX_TIME_NIGHT 6

#define TIME_UPDATE_SECONDS 60
#define WATCHDOG_TIMEOUT_MS 70 * 1000
#define DAY_SAVING_SECONDS 60 * 60
#define MISSED_POWER_UPDATES_SECONDS 5 * 60

#define DEFAULT_MIN_NIGHT_TEMP 59

// #define USE_WEB_SERIAL 1

Preferences preferences;
OneWire oneWire(PIN_TEMPERATURE);
DallasTemperature sensors(&oneWire);
WaterTankPowerHandler powerHandler(PIN_RELAY_1, PIN_RELAY_2, PIN_RELAY_DIMMER_FULL, PIN_RELAY_DIMMER_DIM, PIN_DIMMER_OUT, PIN_DIMMER_ZERO_CROSSING, POWER_PER_RELAY);
PowerManager powerManager(&powerHandler, MIN_TIME_DAY, MAX_TIME_DAY, MIN_TIME_NIGHT, MAX_TIME_NIGHT, DEFAULT_MIN_NIGHT_TEMP, NIGHT_POWER, WATER_VOLUME);

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <title>ESP Water Tank</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <link rel="icon" href="data:,">
</head>
<body>
  <h3>Setup</h3>
<label for="manual">Manual mod</label> <input type="checkbox" onchange="onManualMod()" id="manual"/><br/>
<label for="inhibition_days">Inhibition days</label> <input type="text" size="9" id="inhibition_days" readonly/><button onclick="removeInhibition()"> - </button><button onclick="addInhibition()"> + </button><br/>
<label for="min_night_temp">Min night temperature</label> <input type="text" size="9" id="min_night_temp" readonly/><button onclick="removeMinNightTemp()"> - </button><button onclick="addMinNightTemp()"> + </button><br/>
<label for="legionelosis_max_delay">Legionelosis period</label> <input type="text" size="9" id="legionelosis_max_delay" readonly/><button onclick="removeLegionelosisMaxDelay()"> - </button><button onclick="addLegionelosisMaxDelay()"> + </button><br/>
<label for="legionelosis_temp">Legionelosis temperature</label> <input type="text" size="9" id="legionelosis_temp" readonly/><button onclick="removeLegionelosisTemp()"> - </button><button onclick="addLegionelosisTemp()"> + </button><br/>
  <h3>Info</h3>
<label for="temperature">Temperature</label> (<span id="temp_sensor_valid"></span>/<span id="temp_sensor_count"></span>) <input type="text" size="9" id="temperature" readonly/><br/>
<label for="total_power">Total Power</label> <input type="text" size="9" id="total_power" readonly/><br/>
<label for="day_energy">Today day energy (Wh)</label> <input type="text" size="9" id="day_energy" readonly/><br/>
<label for="night_energy">Today night energy (Wh)</label> <input type="text" size="9" id="night_energy" readonly/><br/>
<label for="total_day_energy">Total day energy (kWh)</label> <input type="text" size="9" id="total_day_energy" readonly/><br/>
<label for="total_night_energy">Total night energy (kWh)</label> <input type="text" size="9" id="total_night_energy" readonly/><br/>
<button onclick="resetEnergyCounter()">Reset energy count</button><br/>
<label for="legionelosis_delay">Days since anti-legionelosis</label> <input type="text" size="9" id="legionelosis_delay" readonly/><br/>
  <h3>Raw info</h3>
<label for="power_requested">Power requested</label> <input type="checkbox" id="power_requested" disabled/><br/>
<label for="power_received">Power received</label> <input type="checkbox" id="power_received" disabled/><br/>
<label for="raw_power_received">Raw power received</label> <input type="text" size="9" id="raw_power_received" readonly/><br/>
<label for="current_time">Current time</label> <input type="text" id="current_time" readonly/><br/>
<label for="up_time">Up time</label> <input type="text" id="up_time" readonly/><br/>
<script>
function onManualMod() {
  var xhr = new XMLHttpRequest();
  xhr.open("GET", "/update?manual="+(document.getElementById("manual").checked ? '1' : '0'), true);
  xhr.send();
  getValues();
}
function removeInhibition() {
  var xhr = new XMLHttpRequest();
  xhr.open("GET", "/update?inhibition=remove", true);
  xhr.send();
  getValues();
}
function addInhibition() {
  var xhr = new XMLHttpRequest();
  xhr.open("GET", "/update?inhibition=add", true);
  xhr.send();
  getValues();
}
function removeMinNightTemp() {
  var xhr = new XMLHttpRequest();
  xhr.open("GET", "/update?minNightTemp=remove", true);
  xhr.send();
  getValues();
}
function addMinNightTemp() {
  var xhr = new XMLHttpRequest();
  xhr.open("GET", "/update?minNightTemp=add", true);
  xhr.send();
  getValues();
}
function removeLegionelosisMaxDelay() {
  var xhr = new XMLHttpRequest();
  xhr.open("GET", "/update?legionelosisMaxDelay=remove", true);
  xhr.send();
  getValues();
}
function addLegionelosisMaxDelay() {
  var xhr = new XMLHttpRequest();
  xhr.open("GET", "/update?legionelosisMaxDelay=add", true);
  xhr.send();
  getValues();
}
function removeLegionelosisTemp() {
  var xhr = new XMLHttpRequest();
  xhr.open("GET", "/update?legionelosisTemp=remove", true);
  xhr.send();
  getValues();
}
function addLegionelosisTemp() {
  var xhr = new XMLHttpRequest();
  xhr.open("GET", "/update?legionelosisTemp=add", true);
  xhr.send();
  getValues();
}
function resetEnergyCounter() {
  var xhr = new XMLHttpRequest();
  xhr.open("GET", "/update?resetEnergyCounter", true);
  xhr.send();
  getValues();
}
function getValues() {
  var xhr = new XMLHttpRequest();
  xhr.onreadystatechange = () => {
    if (xhr.readyState === 4) {
      var res = xhr.response.split(',');
      document.getElementById("manual").checked = res[0] === '1';
      document.getElementById("power_requested").checked = res[1] === '1';
      document.getElementById("power_received").checked = res[2] === '1';
      document.getElementById("raw_power_received").value = res[3];
      document.getElementById("total_power").value = res[4];
      document.getElementById("temperature").value = res[5];
      document.getElementById("temp_sensor_valid").innerHTML = res[6];
      document.getElementById("temp_sensor_count").innerHTML = res[7];
      document.getElementById("current_time").value = res[8];
      document.getElementById("up_time").value = res[9];
      document.getElementById("inhibition_days").value = res[10];
      document.getElementById("min_night_temp").value = res[11];
      document.getElementById("day_energy").value = res[12];
      document.getElementById("night_energy").value = res[13];
      document.getElementById("total_day_energy").value = res[14];
      document.getElementById("total_night_energy").value = res[15];
      document.getElementById("legionelosis_max_delay").value = res[16];
      document.getElementById("legionelosis_delay").value = res[17];
      document.getElementById("legionelosis_temp").value = res[18];
    }
  };
  xhr.open("GET", "/values", true);
  xhr.send();
}

function init() {
  setInterval(getValues, 1000);
}

window.onload = init;
</script>
</body>
</html>
)rawliteral";

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);
// UDP server/client
AsyncUDP udp;

#ifdef USE_WEB_SERIAL
#define Serial WebSerial
#endif

void broadcastState(bool state) {
  uint8_t buf[10];
  size_t total_size = 0;
  memcpy(buf + total_size, &state, sizeof(state));
  total_size += sizeof(state);
  udp.broadcastTo(buf, total_size, UDP_PORT_POWER);
}

static volatile bool isTimeSync = false;
static volatile bool isUpTimeSet = false;
static struct tm up_timeinfo;

static volatile int validSensorCount = 0;
static volatile int sensor_count = 0;

#define PREFERENCE_WORKSPACE "water_tank"
#define MIN_NIGHT_TEMP_NAME "MinNightTemp"
static int8_t lastSavedMinNightTemp = DEFAULT_MIN_NIGHT_TEMP;
#define INHIBITION_DAYS_LEFT_NAME "InhibDays"
static int8_t lastSavedInhibitionDaysLeft = 0;
#define DAY_ENERGY_NAME "DayNRJ"
static float lastSavedDayEnergy = 0;
#define NIGHT_ENERGY_NAME "NightNRJ"
static float lastSavedNightEnergy = 0;
#define TOTAL_DAY_ENERGY_NAME "TotDayNRJ"
static float lastSavedTotalDayEnergy = 0;
#define TOTAL_NIGHT_ENERGY_NAME "TotNightNRJ"
static float lastSavedTotalNightEnergy = 0;
#define LEGIONELOSIS_TEMPERATURE_NAME "LegTemp"
static int8_t lastSavedLegionelosisTemperature = 0;
#define LEGIONELOSIS_DELAY_NAME "LegDelay"
static int8_t lastSavedLegionelosisDelay = 0;
#define LEGIONELOSIS_MAX_DELAY_NAME "LegMaxDelay"
static int8_t lastSavedLegionelosisMaxDelay = 0;

void savePreferenceInt(const char* key, int8_t val, int8_t &savedVal) {
  if (val != savedVal) {
     Serial.println("Save preference");
     Serial.println(key);
     Serial.println(val);   
     preferences.begin(PREFERENCE_WORKSPACE, false);
     preferences.putInt(key, val);
     preferences.end(); 
     savedVal = val;
  }
}

void savePreferenceFloat(const char* key, float val, float &savedVal) {
  if (val != savedVal) {
     Serial.println("Save preference");
     Serial.println(key);
     Serial.println(val);   
     preferences.begin(PREFERENCE_WORKSPACE, false);
     preferences.putFloat(key, val);
     preferences.end(); 
     savedVal = val;
  }
}

void setup() {
  #ifndef USE_WEB_SERIAL
  Serial.begin(115200);
  #else
  Serial.begin(&server, "/webserial");
  #endif

  // Watchdog
  {
    esp_task_wdt_config_t config = {
      .timeout_ms = WATCHDOG_TIMEOUT_MS,
      .idle_core_mask = (1 << portNUM_PROCESSORS) - 1,    // Bitmask of all cores
      .trigger_panic = true,
    };
    esp_task_wdt_reconfigure(&config);
    enableLoopWDT();
  }

  // Get preferences
  {
    preferences.begin(PREFERENCE_WORKSPACE, false);
    lastSavedMinNightTemp = preferences.getInt(MIN_NIGHT_TEMP_NAME, DEFAULT_MIN_NIGHT_TEMP);
    lastSavedInhibitionDaysLeft = preferences.getInt(INHIBITION_DAYS_LEFT_NAME, 0);
    lastSavedTotalDayEnergy = preferences.getFloat(TOTAL_DAY_ENERGY_NAME, 0);
    lastSavedTotalNightEnergy = preferences.getFloat(TOTAL_NIGHT_ENERGY_NAME, 0);
    lastSavedDayEnergy = preferences.getFloat(DAY_ENERGY_NAME, 0.0);
    lastSavedNightEnergy = preferences.getFloat(NIGHT_ENERGY_NAME, 0.0);
    lastSavedLegionelosisTemperature = preferences.getInt(LEGIONELOSIS_TEMPERATURE_NAME, DEFAULT_MIN_NIGHT_TEMP);
    lastSavedLegionelosisDelay = preferences.getInt(LEGIONELOSIS_DELAY_NAME, 0.0);
    lastSavedLegionelosisMaxDelay = preferences.getInt(LEGIONELOSIS_MAX_DELAY_NAME, 0.0);
    preferences.end();
  }

  // Time
  {
    isTimeSync = false;
    isUpTimeSet = false;
    configTime(0, 0, MY_NTP_SERVER);  // 0, 0 because we will use TZ in the next line
    setenv("TZ", MY_TZ, 1);           
    sntp_set_sync_interval(12 * 60 * 60 * 1000UL); // 12 hours
    sntp_set_time_sync_notification_cb([](struct timeval *tv) {
      struct tm timeinfo;
      if (!getLocalTime(&timeinfo)) {
        isTimeSync = false;
        Serial.println("Time sync : Failed to obtain time");
      }
      else {
        isTimeSync = true;
        if (!isUpTimeSet) {
          isUpTimeSet = true;
          up_timeinfo = timeinfo;
        }
        Serial.println("Time sync done");
        Serial.println(&timeinfo);
      }
    });
    tzset();
  }
  
  // Other pin
  
  pinMode(PIN_LED, OUTPUT);
  
  // Wifi
 
  WiFi.mode(WIFI_STA); 

  WiFi.onEvent([](WiFiEvent_t wifi_event, WiFiEventInfo_t wifi_info) {
    Serial.println("Connected to the WiFi network");
  }, ARDUINO_EVENT_WIFI_STA_CONNECTED);

  volatile static int32_t raw_power_received = 0;
  
  WiFi.onEvent([](WiFiEvent_t wifi_event, WiFiEventInfo_t wifi_info) {
    Serial.println("Got IP");
    Serial.println(WiFi.localIP());

    if(udp.listen(UDP_PORT_POWER)) {
      udp.onPacket([](AsyncUDPPacket packet) {
        int32_t pow;
        if (packet.length() == sizeof(pow)) {
          memcpy(&pow, packet.data(), sizeof(pow));
          raw_power_received = pow;
          // Invert the power, since this is the power consumption
          powerManager.onPowerInfo(-pow);
        }
        else {
          Serial.println("Received power : Size error");
          Serial.println(packet.length());
        }
      });
    }
  }, ARDUINO_EVENT_WIFI_STA_GOT_IP);

  WiFi.onEvent([](WiFiEvent_t wifi_event, WiFiEventInfo_t wifi_info) {
    Serial.println("Disconnected from the WiFi");
    powerHandler.setWaterTankOff();
    WiFi.begin(ssid, password);
  }, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);

  // Web server
  
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    Serial.println("Incoming HTML page request");
    request->send(200, "text/html", index_html);
  });

  server.on("/update", HTTP_GET, [] (AsyncWebServerRequest *request) {
    Serial.println("GET request");
    if (request->hasParam("manual")) {
      String manual_mod = request->getParam("manual")->value();
      Serial.println("Set manual mod to " + manual_mod);
      powerManager.setManual(manual_mod != "0");
      powerManager.onRefresh();
      request->send(200, "text/plain", "OK");
    }
    else if (request->hasParam("inhibition")) {
      String inhibit = request->getParam("inhibition")->value();
      Serial.println("Set inhibition days to " + inhibit);
      int8_t current_inhibit = powerManager.getInhibitionsDaysLeft();
      if (inhibit == "add") {
        powerManager.setInhibitionsDaysLeft(current_inhibit + 1);
      }
      else if (inhibit == "remove" && current_inhibit > 0) {
        powerManager.setInhibitionsDaysLeft(current_inhibit - 1);
      }
      request->send(200, "text/plain", "OK");
    }
    else if (request->hasParam("minNightTemp")) {
      String minNightTemp = request->getParam("minNightTemp")->value();
      Serial.println("Set min night temperature to " + minNightTemp);
      int8_t current_minNightTemp = powerManager.getMinTemperatureNight();
      if (minNightTemp == "add") {
        powerManager.setMinTemperatureNight(current_minNightTemp + 1);
      }
      else if (minNightTemp == "remove" && current_minNightTemp > 0) {
        powerManager.setMinTemperatureNight(current_minNightTemp - 1);
      }
      request->send(200, "text/plain", "OK");
    }
    else if (request->hasParam("legionelosisMaxDelay")) {
      String delay = request->getParam("legionelosisMaxDelay")->value();
      Serial.println("Set legionelosis max delay to " + delay);
      int8_t current_delay = powerManager.getLegionelosisMaxDelay();
      if (delay == "add") {
        powerManager.setLegionelosisActivation(true);
        powerManager.setLegionelosisMaxDelay(current_delay + 1);
      }
      else if (delay == "remove" && current_delay > 0) {
        powerManager.setLegionelosisDelay(current_delay - 1);
        powerManager.setLegionelosisMaxDelay(current_delay - 1 > 0);
      }
      powerManager.setLegionelosisDelay(powerManager.getLegionelosisMaxDelay());
      request->send(200, "text/plain", "OK");
    }
    else if (request->hasParam("legionelosisTemp")) {
      String temp = request->getParam("legionelosisTemp")->value();
      Serial.println("Set legionelosis temperature to " + temp);
      int8_t current_temp = powerManager.getLegionelosisTemperature();
      if (temp == "add") {
        powerManager.setLegionelosisTemperature(current_temp + 1);
      }
      else if (temp == "remove" && current_temp > 0) {
        powerManager.setLegionelosisTemperature(current_temp - 1);
      }
      request->send(200, "text/plain", "OK");
    }
    else if (request->hasParam("resetEnergyCounter")) {
      Serial.println("Reset energy counter");
      powerManager.resetEnergyCounter();
      request->send(200, "text/plain", "OK");
    }
    else {
      Serial.println("Unkown request (ignored)");
      request->send(200, "text/plain", "KO");
    }

  });

  static volatile bool isPowerRequested = false;
  
  server.on("/values", HTTP_GET, [] (AsyncWebServerRequest *request) {
     struct tm timeinfo;
    char time_buf[26];
    char up_time_buf[26];
    if (!getLocalTime(&timeinfo)) {
      strcpy(time_buf, "Time error!");
      strcpy(up_time_buf, "Time error!");
    }
    else {
      strftime(time_buf, 26, "%Y-%m-%d %H:%M:%S", &timeinfo);
      strftime(up_time_buf, 26, "%Y-%m-%d %H:%M:%S", &up_timeinfo);
    }
    String s = String("");
    s += powerManager.getManual() ? 1 : 0;
    s += ",";
    s += isPowerRequested ? 1 : 0;
    s += ",";
    s += powerManager.isPowerReceiptionOk();
    s += ",";
    s += raw_power_received;
    s += ",";
    s += powerHandler.getWaterTankPower();
    s += ",";
    s += powerManager.getTemperature();
    s += ",";
    s += validSensorCount;
    s += ",";
    s += sensor_count;
    s += ",";
    s += time_buf;
    s += ",";
    s += up_time_buf;
    s += ",";
    s += powerManager.getInhibitionsDaysLeft();
    s += ",";
    s += powerManager.getMinTemperatureNight();
    s += ",";
    s += powerManager.getDayEnergyCounter();
    s += ",";
    s += powerManager.getNightEnergyCounter();
    s += ",";
    s += powerManager.getTotalDayEnergyCounter();
    s += ",";
    s += powerManager.getTotalNightEnergyCounter();
    s += ",";
    s += powerManager.getLegionelosisMaxDelay();
    s += ",";
    s += powerManager.getLegionelosisDelay();
    s += ",";
    s += powerManager.getLegionelosisTemperature();
    s += ",";
    request->send(200, "text/plain", s);
  });

  powerManager.setOnStartPowerListener([]() {
    // Enable the data server
    Serial.println("Start UDP Data server");
    isPowerRequested = true;
    raw_power_received = 0;
    broadcastState(true);
  });

  powerManager.setOnStopPowerListener([]() {
    // Disable the data server
    Serial.println("Stop UDP Data server");
    isPowerRequested = false;
    raw_power_received = 0;
    broadcastState(false);
  });

  powerManager.setPrintFunction([](String s) {
    Serial.println(s);
  });

  powerHandler.begin();
  powerHandler.setMinDimPower(MIN_DIMMER_POWER);
 
  powerManager.setNightAnticipation(1.5);
  powerManager.setMinTemperatureNight(lastSavedMinNightTemp);
  powerManager.setInhibitionsDaysLeft(lastSavedInhibitionDaysLeft);
  powerManager.setDayEnergyCounter(lastSavedDayEnergy);
  powerManager.setNightEnergyCounter(lastSavedNightEnergy);
  powerManager.setTotalDayEnergyCounter(lastSavedTotalDayEnergy);
  powerManager.setTotalNightEnergyCounter(lastSavedTotalNightEnergy);
  powerManager.setLegionelosisTemperature(lastSavedLegionelosisTemperature);
  powerManager.setLegionelosisDelay(lastSavedLegionelosisDelay);
  powerManager.setLegionelosisMaxDelay(lastSavedLegionelosisMaxDelay);
  powerManager.setLegionelosisActivation(lastSavedLegionelosisMaxDelay > 0);

  WiFi.begin(ssid, password);

  server.begin();

  sensors.begin();
  sensor_count = sensors.getDeviceCount();
}


void loop() {
  // Time
  if (!isTimeSync) {
    sntp_restart();
  }
 
  // Led
  {
    static int ledStatus = LOW;
    ledStatus = !ledStatus;
    digitalWrite(PIN_LED, ledStatus);
  }

  // Temp sensor
  {
    float valid_temperature[sensor_count];
    validSensorCount = 0;
    for (int i = 0; i < sensor_count; i++) {
      if (sensors.requestTemperaturesByIndex(i)) {
        float temperature = sensors.getTempCByIndex(i);
        if (temperature > 0.0 && temperature < 80.0) {
          valid_temperature[validSensorCount] = temperature;
          validSensorCount++;
        }
        else {
          Serial.print("Sensor temperature out of range ");
          Serial.println(i);
        }
      }
      else {
        Serial.print("Sensor temperature reading invalid ");
        Serial.println(i);
      }
    }

    if (validSensorCount > 0) {
      // Temperature is valid, if not, just wait for next loop...
      float temp = 0.0;
      for (int i = 0; i < validSensorCount; i++) {
        temp += valid_temperature[i];
      }
      powerManager.setTemperature(temp / validSensorCount);
    }
    else {
      Serial.println("No valid sensor temperature");
    }
  }

  // Power manager
  powerManager.onRefresh(true);

  // Preferences
  {
    savePreferenceInt(MIN_NIGHT_TEMP_NAME, powerManager.getMinTemperatureNight(), lastSavedMinNightTemp);
    savePreferenceInt(INHIBITION_DAYS_LEFT_NAME, powerManager.getInhibitionsDaysLeft(), lastSavedInhibitionDaysLeft); 
    savePreferenceFloat(TOTAL_DAY_ENERGY_NAME, powerManager.getTotalDayEnergyCounter(), lastSavedTotalDayEnergy);
    savePreferenceFloat(TOTAL_NIGHT_ENERGY_NAME, powerManager.getTotalNightEnergyCounter(), lastSavedTotalNightEnergy); 
    savePreferenceInt(LEGIONELOSIS_DELAY_NAME, powerManager.getLegionelosisDelay(), lastSavedLegionelosisDelay); 
    savePreferenceInt(LEGIONELOSIS_MAX_DELAY_NAME, powerManager.getLegionelosisMaxDelay(), lastSavedLegionelosisMaxDelay); 
    savePreferenceInt(LEGIONELOSIS_TEMPERATURE_NAME, powerManager.getLegionelosisTemperature(), lastSavedLegionelosisTemperature); 
  } 

  {
    static int dayPreferenceCount = 0;
    if (dayPreferenceCount >= DAY_SAVING_SECONDS / TIME_UPDATE_SECONDS) {
      dayPreferenceCount = 0;
      savePreferenceFloat(DAY_ENERGY_NAME, powerManager.getDayEnergyCounter(), lastSavedDayEnergy);
      savePreferenceFloat(NIGHT_ENERGY_NAME, powerManager.getNightEnergyCounter(), lastSavedNightEnergy);  
    }
    else {
      dayPreferenceCount++;
    }
  }

  if (powerManager.getMissedPowerUpdate() > MISSED_POWER_UPDATES_SECONDS / TIME_UPDATE_SECONDS) {
    esp_restart();
  }

  delay(TIME_UPDATE_SECONDS * 1000);
}
