# Smart Irrigation with ESP32, MQTT and Node-RED

Low-cost IoT irrigation control. ESP32 nodes read soil moisture and switch 12 V solenoid valves. A Raspberry Pi runs the Mosquitto MQTT broker and a Node-RED dashboard, so you can watch moisture and open or close each valve from a browser.

Paper: *Using the Internet of Things (IoT) in Building an Irrigation Control System*, 3rd International Conference on IoT and Applications, University of Isfahan, 2019. The paper and poster are in [`docs/`](docs/), with a longer [report](docs/REPORT.md).

## Architecture

```
Soil sensor -> ESP32 -> transistor -> relay -> solenoid valve
                 |  Wi-Fi / MQTT
                 v
   Raspberry Pi: Mosquitto + Node-RED  ->  browser (http://<pi-ip>:1880/ui)
```

| Topic | Direction | Payload |
|---|---|---|
| `treeN/humidity` | ESP32 -> Pi | moisture, 0-100 |
| `treeN/output` | Pi -> ESP32 | `on` / `off` |

## Repo layout

```
firmware/esp32_irrigation/   Arduino sketch + secrets.h.example
raspberry_pi/scripts/        install.sh, logger.py, auto_control.py
raspberry_pi/node-red/       flow.json (3 trees)
raspberry_pi/config/         mosquitto.conf, nginx-nodered.conf
docs/                        paper, poster, REPORT.md
```

## Parts (per tree)

ESP32 board, analog soil moisture sensor, 5 V relay, C945 NPN transistor, flyback diode, resistor, 12 V solenoid valve with 12 V adapter. One Raspberry Pi (3 or 4) with a microSD card serves all trees.

## Step 1: Wire the hardware

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

## Step 2: Set up the Raspberry Pi

```bash
git clone https://github.com/Hossein-Mosaffa/smart-irrigation-esp32-mqtt.git && cd smart-irrigation-esp32-mqtt
bash raspberry_pi/scripts/install.sh
sudo cp raspberry_pi/config/mosquitto.conf /etc/mosquitto/conf.d/irrigation.conf
sudo mosquitto_passwd -c /etc/mosquitto/passwd irrigation
sudo systemctl restart mosquitto
```

Open `http://<pi-ip>:1880`, import `raspberry_pi/node-red/flow.json` (menu, Import), double-click an MQTT node, edit the broker (server `localhost`, add the user and password in the Security tab), then Deploy. Find the Pi IP with `hostname -I`.

## Step 3: Flash the ESP32

1. In the Arduino IDE, install the ESP32 board package and the **PubSubClient** library.
2. Copy `secrets.h.example` to `secrets.h` in the sketch folder and fill in Wi-Fi, Pi IP and a unique `TREE_ID` per board. Uncomment `MQTT_USER` and `MQTT_PASS` if you set a broker password.
3. Calibrate the sensor (below), then upload.

### Calibrate the sensor

1. Upload the sketch and open the Serial Monitor. It prints `raw ADC = ...` every 5 s.
2. Hold the sensor in dry air and note the value. Put it in a glass of water (up to the line marked on the probe) and note that value.
3. Set `ADC_DRY` to the dry reading and `ADC_WET` to the wet reading in the sketch, and re-upload.
4. Check in real soil. Capacitive sensors usually read high when dry, resistive ones often the opposite. The formula handles both as long as you enter the two readings in the right constants.

## Step 4: Test

1. On the Pi, watch all messages: `mosquitto_sub -h localhost -t "tree1/#" -v` (add `-u irrigation -P <password>` if passwords are on).
2. Power the ESP32. In the Serial Monitor (115200 baud) you should see the IP address, `MQTT... connected`, then a moisture value every 5 s. The same values appear in the `mosquitto_sub` window.
3. Test the valve without the dashboard: `mosquitto_pub -h localhost -t tree1/output -m on` (valve opens), then `-m off` (valve closes).
4. Open `http://<pi-ip>:1880/ui`. The gauge should move and the switch should open and close the valve.
5. Only then add remote access.

## Step 5 (optional): Logging and automatic control

```bash
cd raspberry_pi/scripts && python3 -m venv .venv && . .venv/bin/activate && pip install -r requirements.txt
MQTT_USER=irrigation MQTT_PASS=... python logger.py        # CSV history
MQTT_USER=irrigation MQTT_PASS=... python auto_control.py  # LOW=30 HIGH=60 hysteresis
```

Stop `auto_control.py` when you want to use the dashboard switch by hand, since both publish to the same topic. To run either script at boot, wrap it in a systemd service.

## Remote access

By default the dashboard works only on your local network. `raspberry_pi/config/nginx-nodered.conf` proxies `/ui` to Node-RED, and the original build exposed it with the Dataplicity service. Turn on `adminAuth` in Node-RED `settings.js` first, and prefer HTTPS.

## Troubleshooting

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

## Security

- Never commit `secrets.h` (it is git-ignored).
- Keep `allow_anonymous false` on the broker.
- Do not expose the dashboard publicly without a login.

## Limitations and roadmap

- The dashboard is manual control. `auto_control.py` is a simple threshold controller.
- No MQTT TLS and no database yet.
- Ideas: PID or model-based control, Grafana, solar power, deep sleep, a failsafe valve timeout.

## Authors

Hossein Mosaffa, Hajar Atrianfar - Amirkabir University of Technology.

## License

MIT

[Hossein Mosaffa's personal website](https://hossein-mosaffa.github.io)
