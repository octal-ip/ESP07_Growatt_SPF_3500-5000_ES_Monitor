//Choose which protocol you'd like to post the statistics to your database by uncommenting one (or more) of the definitions below.
#define INFLUX_UDP
//#define INFLUX_HTTP
#define MQTT

#include <Arduino.h>
#include <ModbusMaster.h>
#include <ArduinoOTA.h>
#include <RunningAverage.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPClient.h>
#include <WiFiUdp.h>
#include <TelnetPrint.h>

#ifdef MQTT
  #include <PubSubClient.h>
  WiFiClient espClient;
  PubSubClient MQTTclient(espClient);
  const char *MQTTclientId = "Growatt-SPF";
  const char *MQTTtopicPrefix = "home/solar/growatt-spf";
  char MQTTtopic[70];
#endif

#ifdef INFLUX_HTTP
  #include <ESP8266HTTPClient.h>
  int httpResponseCode = 0;
#endif

#include <secrets.h> /*Edit this file to include the following details.
SECRET_INFLUXDB only required if using HTTP mode.
SECRET_INFLUX_IP_OCTETx only required if using UDP mode.

#define SECRET_SSID "<ssid>>"
#define SECRET_PASS "<password>"
#define SECRET_INFLUXDB "http://<IP Address>:8086/write?db=<db name>&u=<username>&p=<password>"
#define SECRET_INFLUX_IP_OCTET1 <first IP octet>
#define SECRET_INFLUX_IP_OCTET2 <second IP octet>
#define SECRET_INFLUX_IP_OCTET3 <third IP octet>
#define SECRET_INFLUX_IP_OCTET4 <last IP octet>
#define SECRET_MQTT_SERVER "<DNS name or IP>"
#define SECRET_MQTT_INVERTERMODE_TOPIC "<MQTT topic name>"
*/

#define debugEnabled 0

int avSamples = 20;

struct stats{
   const char *name;
   byte address;
   int type; //Whether the result is 16 or 32 bit number.
   float value;
   RunningAverage average;
   float multiplier;
};

stats arrstats[37] = {
  //Register name, MODBUS address, integer type (0 = uint16_t​, 1 = uint32_t​, 3 = int32_t​), value, running average, multiplier)
  {"System_Status", 0, 0, 0.0, RunningAverage(avSamples), 1.0},
  {"PV_Voltage", 1, 0, 0.0, RunningAverage(avSamples), 0.1},
  {"PV_Power", 3, 1, 0.0, RunningAverage(avSamples), 0.1},
  {"Buck_Converter_Current", 7, 0, 0.0, RunningAverage(avSamples), 0.1},
  {"Output_Watts", 9, 1, 0.0, RunningAverage(avSamples), 0.1},
  {"Output_VA", 11, 1, 0.0, RunningAverage(avSamples), 0.1},
  {"AC_Charger_Watts", 13, 1, 0.0, RunningAverage(avSamples), 0.1},
  {"AC_Charger_VA", 15, 1, 0.0, RunningAverage(avSamples), 0.1},
  {"Battery_Voltage", 17, 0, 0.0, RunningAverage(avSamples), 0.01},
  {"Battery_SOC", 18, 0, 0.0, RunningAverage(avSamples), 1.0},
  {"Bus_Voltage", 19, 0, 0.0, RunningAverage(avSamples), 0.1},
  {"AC_Input_Voltage", 20, 0, 0.0, RunningAverage(avSamples), 0.1},
  {"AC_Input_Frequency", 21, 0, 0.0, RunningAverage(avSamples), 0.01},
  {"AC_Output_Voltage", 22, 0, 0.0, RunningAverage(avSamples), 0.1},
  {"AC_Output_Frequency", 23, 0, 0.0, RunningAverage(avSamples), 0.01},
  {"Inverter_Temperature", 25, 0, 0.0, RunningAverage(avSamples), 0.1},
  {"DC_to_DC_Converter_Temperature", 26, 0, 0.0, RunningAverage(avSamples), 0.1},
  {"Load_Percentage", 27, 0, 0.0, RunningAverage(avSamples), 0.1},
  {"Buck_Converter_Temperature", 32, 0, 0.0, RunningAverage(avSamples), 0.1},
  {"Output_Current", 34, 0, 0.0, RunningAverage(avSamples), 0.1},
  {"Interter_Current", 35, 0, 0.0, RunningAverage(avSamples), 0.1},
  {"AC_Input_Watts", 36, 1, 0.0, RunningAverage(avSamples), 0.1},
  {"AC_Input_VA", 38, 1, 0.0, RunningAverage(avSamples), 0.1},
  {"PV_Energy_Today", 48, 1, 0.0, RunningAverage(avSamples), 0.1},
  {"PV_Energy_Total", 50, 1, 0.0, RunningAverage(avSamples), 0.1},
  {"AC_Charger_Today", 56, 1, 0.0, RunningAverage(avSamples), 0.1},
  {"AC_Charger_Total", 58, 1, 0.0, RunningAverage(avSamples), 0.1},
  {"Battery_Discharge_Today", 60, 1, 0.0, RunningAverage(avSamples), 0.1},
  {"Battery_Discharge_Total", 62, 1, 0.0, RunningAverage(avSamples), 0.1},
  {"AC_Charger_Battery_Current", 68, 0, 0.0, RunningAverage(avSamples), 0.1},
  {"AC_Discharge_Watts", 69, 1, 0.0, RunningAverage(avSamples), 0.1},
  {"AC_Discharge_VA", 71, 1, 0.0, RunningAverage(avSamples), 0.1},
  {"Battery_Discharge_Watts", 73, 1, 0.0, RunningAverage(avSamples), 0.1},
  {"Battery_Discharge_VA", 75, 1, 0.0, RunningAverage(avSamples), 0.1},
  {"Battery_Watts", 77, 2, 0.0, RunningAverage(avSamples), 0.1}, //This is a signed INT32
  {"Inverter_Fan_Speed", 82, 0, 0.0, RunningAverage(avSamples), 1.0},
  {"MPPT_Fan_Speed", 83, 0, 0.0, RunningAverage(avSamples), 1.0}
};

ModbusMaster Growatt;
uint8_t MODBUSresult;
unsigned long lastUpdate = 0;
int failures = 0; //The number of failed WiFi or send attempts. Will automatically restart the ESP if too many failures occurr in a row.

#ifdef INFLUX_UDP
  WiFiUDP udp;
  IPAddress influxhost = {SECRET_INFLUX_IP_OCTET1, SECRET_INFLUX_IP_OCTET2, SECRET_INFLUX_IP_OCTET3, SECRET_INFLUX_IP_OCTET4}; // The IP address of the InfluxDB host for UDP packets.
  int influxport = 8091; // The port that the InfluxDB server is listening on
#endif

#ifdef MQTT
  void MQTTcallback(char* topic, byte* payload, unsigned int length) {
    TelnetPrint.printf("MQTT message arrived [%s]: %c \r\n", topic, payload[0]);

    if ((char)payload[0] == '0') {
      TelnetPrint.println("Standby off, Output enable.");
      MODBUSresult = Growatt.writeSingleRegister(0, 0x0000);
      if (MODBUSresult != Growatt.ku8MBSuccess) {
        TelnetPrint.printf("MODBUS write failed: %d \r\n", MODBUSresult);
      }
    }
    else if ((char)payload[0] == '1') {
      TelnetPrint.println("Standby off, Output disable.");
      MODBUSresult = Growatt.writeSingleRegister(0, 0x0001);
      if (MODBUSresult != Growatt.ku8MBSuccess) {
        TelnetPrint.printf("MODBUS write failed: %d \r\n", MODBUSresult);
      }
    }
    else if ((char)payload[0] == '2') {
      TelnetPrint.println("Standby on, Output enable.");
      MODBUSresult = Growatt.writeSingleRegister(0, 0x0100);
      if (MODBUSresult != Growatt.ku8MBSuccess) {
        TelnetPrint.printf("MODBUS write failed: %d \r\n", MODBUSresult);
      }
    }
    else if ((char)payload[0] == '3') {
      TelnetPrint.println("Standby on, Output disable.");
      MODBUSresult = Growatt.writeSingleRegister(0, 0x0101);
      if (MODBUSresult != Growatt.ku8MBSuccess) {
        TelnetPrint.printf("MODBUS write failed: %d \r\n", MODBUSresult);
      }
    }
  }
#endif

void setup()
{
  Serial.begin(9600);

  // ****Start ESP8266 OTA and Wifi Configuration****
  if (debugEnabled == 1) {
    Serial.println();
    Serial.print("Connecting to "); Serial.println(SECRET_SSID);
  }

  WiFi.mode(WIFI_STA);
  WiFi.begin(SECRET_SSID, SECRET_PASS); //Edit include/secrets.h to update this data.

  unsigned long connectTime = millis();
  if (debugEnabled == 1) {
    Serial.print("Waiting for WiFi to connect");
  }
  while (!WiFi.isConnected() && (unsigned long)(millis() - connectTime) < 5000) { //Wait for the wifi to connect for up to 5 seconds.
    delay(100);
    if (debugEnabled == 1) {
      Serial.print(".");
    }
  }
  if (!WiFi.isConnected()) {
    if (debugEnabled == 1) {
      Serial.println();
      Serial.println("WiFi didn't connect, restarting...");
    }
    ESP.restart(); //Restart if the WiFi still hasn't connected.
  }
  if (debugEnabled == 1) {
    Serial.println();
    Serial.print("IP address: "); Serial.println(WiFi.localIP());
  }

  // Port defaults to 8266
  ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[MAC]
  ArduinoOTA.setHostname("esp8266-Growatt-monitor");

  // No authentication by default
  // ArduinoOTA.setPassword("admin");

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_SPIFFS
      type = "filesystem";
    }

    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });
  ArduinoOTA.begin();
  // ****End ESP8266 OTA and Wifi Configuration****

  // Growatt Device ID 1
  Growatt.begin(1, Serial);

  #ifdef MQTT
    MQTTclient.setServer(SECRET_MQTT_SERVER, 1883);
    MQTTclient.setCallback(MQTTcallback);
  #endif

  //Telnet log is accessible at port 23
  TelnetPrint.begin();
}

void sendInfluxData (const char *postData) {
  TelnetPrint.print("Posting to InfluxDB: "); TelnetPrint.println(postData);

  #ifdef INFLUX_UDP
    udp.beginPacket(influxhost, influxport);
    udp.printf(postData);
    udp.endPacket();
    delay(5); //This is required to allow the UDP transmission to complete
  #endif

  #ifdef INFLUX_HTTP
    WiFiClient client;
    HTTPClient http;
    http.begin(client, SECRET_INFLUXDB);
    http.addHeader("Content-Type", "text/plain");
    
    httpResponseCode = http.POST(postData);
    delay(10); //For some reason this delay is critical to the stability of the ESP.
    
    if (httpResponseCode >= 200 && httpResponseCode < 300){ //If the HTTP post was successful
      String response = http.getString(); //Get the response to the request
      //Serial.print("HTTP POST Response Body: "); Serial.println(response);
      TelnetPrint.print("HTTP POST Response Code: "); TelnetPrint.println(httpResponseCode);

      if (failures >= 1) {
        failures--; //Decrement the failure counter.
      }
    }
    else {
      TelnetPrint.print("Error sending HTTP POST: "); TelnetPrint.println(httpResponseCode);
      if (httpResponseCode <= 0) {
        failures++; //Incriment the failure counter if the server couldn't be reached.
      }
    }
    http.end();
    client.stop();
  #endif
}

void readMODBUS() {
  Serial.flush(); //Make sure the hardware serial buffer is empty before communicating over MODBUS.
  
  for (int i = 0; i < 37; i++) {  //Iterate through each of the MODBUS queries and obtain their values.
    ArduinoOTA.handle();
    Growatt.clearResponseBuffer();
    MODBUSresult = Growatt.readInputRegisters(arrstats[i].address, 2); //Query each of the MODBUS registers.
    if (MODBUSresult == Growatt.ku8MBSuccess) {
      if (failures >= 1) {
        failures--; //Decrement the failure counter if we've received a response.
      }

      if (arrstats[i].type == 0) {
        //TelnetPrint.print("Raw MODBUS for address: "); TelnetPrint.print(arrstats[i].address); TelnetPrint.print(": "); TelnetPrint.println(Growatt.getResponseBuffer(0));
        arrstats[i].value = (Growatt.getResponseBuffer(0) * arrstats[i].multiplier); //Calculatge the actual value.
      }
      else if (arrstats[i].type == 1) {
        arrstats[i].value = ((Growatt.getResponseBuffer(0) << 8) | Growatt.getResponseBuffer(1)) * arrstats[i].multiplier;  //Calculatge the actual value.
      }
      else if (arrstats[i].type == 2) { //Signed INT32
        arrstats[i].value = (Growatt.getResponseBuffer(1) + (Growatt.getResponseBuffer(0) << 16)) * arrstats[i].multiplier;  //Calculatge the actual value.
      }

      if (arrstats[i].address == 69) {
        if (arrstats[i].value > 6000) { //AC_Discharge_Watts will return very large, invalid results when the inverter has been in stanby mode. Ignore the result if the number is greater than 6kW.
          arrstats[i].value = 0;
        }
      }

      TelnetPrint.print(arrstats[i].name); TelnetPrint.print(": "); TelnetPrint.println(arrstats[i].value);
      arrstats[i].average.addValue(arrstats[i].value); //Add the value to the running average.
      //TelnetPrint.print("Values collected: "); TelnetPrint.println(arrstats[i].average.getCount());

      if (arrstats[i].average.getCount() >= avSamples) { //If we have enough samples added to the running average, send the data to InfluxDB and clear the average.
        char realtimeAvString[8];
        dtostrf(arrstats[i].average.getAverage(), 1, 2, realtimeAvString);

        #if defined (INFLUX_HTTP) || defined (INFLUX_UDP)
          char post[70];
          sprintf(post, "%s,sensor=growatt value=%s", arrstats[i].name, realtimeAvString);
          sendInfluxData(post);
        #endif

        arrstats[i].average.clear();
      }

      #ifdef MQTT
        sprintf(MQTTtopic, "%s/%s", MQTTtopicPrefix, arrstats[i].name);
        if (MQTTclient.connect(MQTTclientId)) {
          char statString[8];
          dtostrf(arrstats[i].value, 1, 2, statString);
          TelnetPrint.printf("Posting %s to MQTT topic %s \r\n", statString, MQTTtopic);
          MQTTclient.publish(MQTTtopic, statString, (bool)1);
          if (failures >= 1) {
            failures--; //Decrement the failure counter.
          }
        }
        else {
          TelnetPrint.print("MQTT connection failed: "); TelnetPrint.println(MQTTclient.state());
          failures++;
        }
      #endif

    }
    else {
      TelnetPrint.print("MODBUS read failed. Returned value: "); TelnetPrint.println(MODBUSresult);
      failures++;
      TelnetPrint.print("Failure counter: "); TelnetPrint.println(failures);
    }
    yield();
  }
}

void loop()
{
  ArduinoOTA.handle();
  MQTTclient.loop();

  if (WiFi.status() != WL_CONNECTED) {
    if (debugEnabled == 1) {
      Serial.println("WiFi disconnected. Attempting to reconnect... ");
    }
    failures++;
    WiFi.begin(SECRET_SSID, SECRET_PASS);
    delay(1000);
  }

  if ((unsigned long)(millis() - lastUpdate) >= 15000) { //Get a MODBUS reading every 30 seconds.
    float rssi = WiFi.RSSI();
    TelnetPrint.println("WiFi signal strength is: "); TelnetPrint.println(rssi);
    TelnetPrint.println("30 seconds has passed. Reading the MODBUS...");
    readMODBUS();
    lastUpdate = millis();

    #ifdef MQTT
      if (!MQTTclient.connected()) {
        TelnetPrint.println("MQTT disconnected. Attempting to reconnect..."); 
        if (MQTTclient.connect(MQTTclientId)) {
          if (failures >= 1) {
            failures--; //Decrement the failure counter.
          }
          TelnetPrint.println("MQTT Connected.");
        }
        else {
          TelnetPrint.print("MQTT connection failed: "); TelnetPrint.println(MQTTclient.state());
          failures++;
        }
      }
      sprintf(MQTTtopic, "%s/%s", MQTTtopicPrefix, SECRET_MQTT_INVERTERMODE_TOPIC);
      MQTTclient.subscribe(MQTTtopic);
    #endif
  }

  if (failures >= 40) {  //Reboot the ESP if there's been too many problems retrieving or sending the data.
    if (debugEnabled == 1) {
      Serial.print("Too many failures, rebooting...");
    }
    TelnetPrint.print("Failure counter has reached: "); TelnetPrint.print(failures); TelnetPrint.println(". Rebooting...");
    ESP.restart();
  }
}
