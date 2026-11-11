#!/usr/bin/env bash
# Sets up Mosquitto + Node-RED + dashboard on Raspberry Pi OS.
set -euo pipefail
sudo apt-get update && sudo apt-get upgrade -y
sudo apt-get install -y mosquitto mosquitto-clients python3-pip python3-venv nginx
sudo systemctl enable --now mosquitto.service

# Node-RED official installer (installs Node.js + Node-RED)
bash <(curl -sL https://raw.githubusercontent.com/node-red/linux-installers/master/deb/update-nodejs-and-nodered)
cd ~/.node-red && npm install node-red-dashboard
sudo systemctl enable nodered.service

echo "Next: create MQTT user, import raspberry_pi/node-red/flow.json at http://<pi-ip>:1880, dashboard at /ui"
