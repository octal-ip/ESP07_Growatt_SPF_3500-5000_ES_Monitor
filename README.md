# ESP07_Growatt_SPF_3500-5000_ES_Monitor
This project provides Platform.io code for building custom firmware for the WiFi-F module included with the Growatt SPF 3500-5000 ES off-grid inverters. It collects all available metrics through the MODBUS interface and sends them to InfluxDB or MQTT. The inverter's AC output can also be placed in and out of standby mode remotely through MQTT to reduce idle power consumption when the AC output isn't needed. This is a function not available through the inverter's interface and can save quite a lot of energy over time.

[Otti's project](https://github.com/otti/Growatt_ShineWiFi-S) provided some inspiration, however the newer generation Wifi-F module can be a challenge to interface with. Many serial adaptors (including the popular FTDI and CH340) will fail to communicate with the ESP8266 when plugged directly into the TX/RX pin headers on the PCB. There are two options for overcoming this issue:

1. The SIPEX SP3232EE RS-232 transceiver on the rear of the PCB, which is also connected to the RX and TX pins of the ESP8266, can be removed or disconnected temporarily to uploaded new firmware. After the initial upload, the SP3232 chip should be replaced or reconnected before the dongle is attached to the inverter.

2. (Preferred) Use an RS232 interface that can better handle the impedance of the SP3232 chip. A USB to UART adaptor utilising the Silicon Labs CP2102 can be bought for a few dollars and is able to upload firmware without any need to modify the dongle.

The onboard ESP8266 is easy to work with: all the necessary pins (GND, TX, RX and GPIO-0) are broken out to a standard 0.1” through holes that a pin header can be soldered to. The chip can be placed in flash mode by shorting GPIO-0 to ground as it's powered on, then standard ESP8266 firmware upload procedures can be followed.
I've included OTA components in the firmware so any future updates can be done without a serial interface.

The WiFi-F board seems to be quite power hungry. Powering it through the USB plug from a source slightly above 5v will ensure the onboard buck converter can provide enough current while flashing the ESP.

The Growatt-Grafana-Dashboard.json file can be imported into Grafana to display metrics from InfluxDB as in the screenshot below.

MQTT-Dash.txt can be imported into the [MQTT Dash app](https://play.google.com/store/apps/details?id=net.routix.mqttdash) to provide metrics and inverter control from your Android device.

Further details and discussion at DIYSolarForum: https://diysolarforum.com/threads/hacking-the-new-growatt-wifi-f-modules.43231/

![WiFi-F Dongle Case](https://github.com/octal-ip/ESP07_Growatt_SPF_3500-5000_ES_Monitor/blob/main/pics/Wifi-F%20Case.jpg "WiFi-F Dongle Case")

![WiFi-F PCB Top](https://github.com/octal-ip/ESP07_Growatt_SPF_3500-5000_ES_Monitor/blob/main/pics/WiFi-F%20PCB%20Top.jpg "WiFi-F Dongle Top")

![WiFi-F PCB Bottom](https://github.com/octal-ip/ESP07_Growatt_SPF_3500-5000_ES_Monitor/blob/main/pics/WiFi-F%20PCB%20Bottom.jpg "WiFi-F Dongle Bottom")

![Grafana Display](https://github.com/octal-ip/ESP07_Growatt_SPF_3500-5000_ES_Monitor/blob/main/pics/Growatt_Grafana.png "Grafana Display")

![MQTT Dash Screenshot](https://github.com/octal-ip/ESP07_Growatt_SPF_3500-5000_ES_Monitor/blob/main/pics/Screenshot_MQTT%20Dash.png "MQTT Dash Screenshot")



### Credits:
[Otti for his inspiration and prior work flashing other Growatt dongles](https://github.com/otti/Growatt_ShineWiFi-S)

[4-20ma for ModbusMaster](https://github.com/4-20ma/ModbusMaster)

[RobTillaart for RunningAverage](https://github.com/RobTillaart/RunningAverage)

[JAndrassy for TelnetStream](https://github.com/jandrassy/TelnetStream)

[Nick O'Leary for PubSubClient](https://github.com/knolleary/pubsubclient)



### Release notes:
#### Jul 20, 2022:
	- Initial release

#### Jul 25, 2022:
	- Moved SSID, WiFi password and InfluxDB connection string to include/secrets.h
	- Removed WebSerial and replaced it with the more efficient TelnetStream.

#### Sep 11, 2022:
	- Added a sanity check for AC_Discharge_Watts which will return very large numbers if the inverter has been in standby mode.
	
#### Sep 22, 2022:
	- Added UDP mode for InfluxDB, which is far faster and more efficient.
	HTTP mode is still available for those who want the added reliability of a TCP connection, at the expense of performance.
	Either mode can be enabled by toggling the UDP_MODE definition.
	
#### Oct 1, 2022:
	- Added MQTT as an optional destination for the statistics.
	Influx in HTTP or UDP mode and MQQT can be enabled or disabled by uncommenting or commenting the definitions at the beginning of main.cpp.
	
#### Oct 14, 2022:
	- Added the ability to remotely turn on/off standby mode for the inverter through MQTT.
	This is useful for reducing overall energy usage when the AC output isn't needed.
	Battery charging will still occur while the inverter is in standby.
	Note that 4 modes are available and documented in the Growatt protocol, however only option 0 (Standby off, Output enable.) and option 3 (Standby on, Output disable) seem to have the intended effect.
	The behaviour of options 1 and 2 are not well defined or implemented.
	- Added Growatt-Grafana-Dashboard.json, which can be imported into Grafana to quickly construct the dashboard showing all metrics pulled from InfluxDB.
	- Added MQTT-Dash.txt which can be imported into MQTT Dash for inverter metrics and control from an Android device.
