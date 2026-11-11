# Report: IoT-Based Irrigation Control System

## 1. Summary
An irrigation system built from open-source tools. ESP32 modules read soil moisture and switch solenoid valves. A Raspberry Pi acts as MQTT broker and hosts a Node-RED dashboard. Users view moisture and toggle valves from any browser with internet access.

## 2. Motivation
Iran faces a water crisis and about 90 percent of its water use is agricultural. Irrigation on farms, orchards and green spaces is often poorly managed, so efficiency is low. Controlling timing and quantity is a direct way to cut waste.

## 3. Architecture
Per tree: one soil moisture sensor, one ESP32, one solenoid valve. All nodes send data over Wi-Fi to one Raspberry Pi.

- Raspberry Pi: MQTT broker (Mosquitto) and Node-RED host.
- ESP32: smart object with analog/digital pins, memory and Wi-Fi.
- MQTT: publish-subscribe through a broker. Clients never talk directly.
- Node-RED: visual flow editor and dashboard (gauge per sensor, switch per valve).
- Valve driver: GPIO, NPN transistor, 5 V relay, 12 V valve, with a flyback diode.

### Data flow
1. Every 5 s the ESP32 reads the analog pin, converts to percent and publishes on `treeN/humidity`.
2. Node-RED is subscribed and shows the value on a gauge.
3. A dashboard switch publishes `on` or `off` on `treeN/output`.
4. The ESP32 is subscribed and sets the valve pin.

## 4. Build process
Follow these steps in order. Commands and file names refer to this repository.

### 4.1 Hardware wiring
| Part | Connects to |
|---|---|
| Soil sensor signal | ESP32 GPIO 34 (ADC1) |
| Soil sensor VCC / GND | ESP32 3.3 V / GND |
| GPIO 4 | Resistor (about 1 kOhm) to C945 NPN base |
| C945 emitter | GND (common with ESP32) |
| C945 collector | Relay coil (-) |
| Relay coil (+) | 5 V supply |
| Diode across relay coil | Cathode to coil (+), anode to coil (-) (flyback) |
| Relay COM / NO | In series with the 12 V adapter (+) and the valve |
| Valve other wire | 12 V adapter (-) |

Steps: (1) join all grounds of the ESP32, transistor and 5 V supply; (2) build the transistor-relay stage and place the diode across the coil; (3) wire the 12 V adapter and valve through the relay's COM and NO contacts only; (4) connect the sensor last. When GPIO 4 goes high, the transistor conducts, the relay closes, and the valve opens. Check your own relay module: many ready-made modules already include the transistor and diode, and some are active-low. If yours is active-low, swap HIGH and LOW in the sketch.

### 4.2 Raspberry Pi
Write Raspberry Pi OS to a microSD card, enable SSH, find the IP (`hostname -I`), then run `raspberry_pi/scripts/install.sh`. It installs Mosquitto, Node.js, Node-RED and node-red-dashboard and enables the services at boot. Copy `raspberry_pi/config/mosquitto.conf` to `/etc/mosquitto/conf.d/`, create a broker user with `mosquitto_passwd`, and restart Mosquitto.

### 4.3 Node-RED dashboard
Open `<pi-ip>:1880`, import `raspberry_pi/node-red/flow.json`, set the broker (host `localhost`, user, password) and press Deploy. The dashboard appears at `<pi-ip>:1880/ui`. Each tree has an MQTT-in node feeding a gauge, and a switch feeding an MQTT-out node. To add a tree, copy the three nodes and change the topic number.

### 4.4 ESP32 firmware
Install the ESP32 board package and PubSubClient. Create `secrets.h` from the example with Wi-Fi details, the Pi IP and a unique `TREE_ID`. The sketch connects to Wi-Fi and the broker, subscribes to `treeN/output`, and publishes moisture every 5 s. It reconnects automatically, keeps the valve closed at boot and averages 16 ADC samples.

### 4.5 Sensor calibration
1. Upload the sketch and open the Serial Monitor. It prints `raw ADC = ...` every 5 s.
2. Hold the sensor in dry air and note the value. Put it in a glass of water (up to the line marked on the probe) and note that value.
3. Set `ADC_DRY` to the dry reading and `ADC_WET` to the wet reading in the sketch, and re-upload.
4. Check in real soil. Capacitive sensors usually read high when dry, resistive ones often the opposite. The formula handles both as long as you enter the two readings in the right constants.

### 4.6 Testing
1. On the Pi, watch all messages: `mosquitto_sub -h localhost -t "tree1/#" -v` (add `-u irrigation -P <password>` if passwords are on).
2. Power the ESP32. In the Serial Monitor (115200 baud) you should see the IP address, `MQTT... connected`, then a moisture value every 5 s. The same values appear in the `mosquitto_sub` window.
3. Test the valve without the dashboard: `mosquitto_pub -h localhost -t tree1/output -m on` (valve opens), then `-m off` (valve closes).
4. Open `http://<pi-ip>:1880/ui`. The gauge should move and the switch should open and close the valve.
5. Only then add remote access.

### 4.7 Remote access
The router firewall blocks outside access. The original build used Dataplicity for a public link and nginx to forward `/ui` to Node-RED (`raspberry_pi/config/nginx-nodered.conf`). Node-RED `adminAuth` adds a login and should be enabled before exposing anything.

## 5. Results
The system works end to end: moisture is shown on the web page and valves open and close from the dashboard, including from outside the local network. The paper reports no quantitative measurements (water saved, latency, calibration), so none are claimed here.

## 6. Troubleshooting
| Symptom | Likely cause and fix |
|---|---|
| Stuck on `Connecting to ...` dots | Wrong SSID/password, or a 5 GHz-only network. The ESP32 needs 2.4 GHz. |
| `failed rc=-2` | Broker unreachable. Check the Pi IP, that Mosquitto is running (`systemctl status mosquitto`) and that the Pi and ESP32 are on the same network. |
| `failed rc=5` | Not authorized. Define `MQTT_USER` and `MQTT_PASS` in `secrets.h` and make sure the user exists on the Pi. |
| Dashboard opens but gauge is empty | Broker node in Node-RED not set or not connected (check the green "connected" status), or the topic does not match `treeN/humidity`. |
| Switch does nothing | Check `mosquitto_sub -t "tree1/#" -v`. If `on` does not show up, the problem is in Node-RED. If it does, the problem is on the ESP32 or the valve circuit. |
| Relay clicks but valve stays shut | The valve needs 12 V and enough current from the adapter, and some valves need minimum water pressure. Check the COM/NO wiring. |
| Moisture value is constant or jumps | Loose sensor wire, missing common ground, or wrong ADC pin. Use ADC1 pins (32-39) only, because ADC2 does not work while Wi-Fi is on. |
| Dashboard unreachable from outside | The router blocks it. Use a tunnel/reverse proxy and enable Node-RED `adminAuth`. |

## 7. Limitations
- Manual control in the original design.
- Linear moisture conversion that needs per-sensor calibration.
- The original setup used an unauthenticated plain-text broker.
- Public exposure through a third-party tunnel needs authentication and HTTPS.
- No data history in the original design.

## 8. Extensions in this repository
- Per-board `TREE_ID`, Wi-Fi and MQTT reconnection, valve closed at boot, ADC averaging.
- Password-protected broker configuration.
- `logger.py` for CSV history and `auto_control.py` for threshold control with hysteresis (defaults: open below 30 percent, close above 60 percent).

## 9. Future work
Soil mineral sensors, solar power for outdoor nodes, MQTT over TLS, Grafana and InfluxDB, deep sleep, PID or model-based control, a failsafe valve timeout.

## 10. References
1. Goap, Sharma, Shukla, Krishna (2018). An IoT based smart irrigation management system using machine learning and open source technologies. Computers and Electronics in Agriculture, 155, 41-49.
2. Pernapati, K. (2018). IoT based low cost smart irrigation system. ICICCT.
3. Lekic, Gardasevic (2018). IoT sensor integration to Node-RED platform. INFOTEH-JAHORINA.
4. Saraf, Gawali (2017). IoT based smart irrigation monitoring and controlling system. RTEICT.
5. Nordrum, A. (2016). Popular internet of things forecast of 50 billion devices by 2020 is outdated.
