#!/usr/bin/env python3
"""Log all tree*/humidity messages to a CSV file."""
import csv, os, time
import paho.mqtt.client as mqtt

HOST = os.getenv("MQTT_HOST", "localhost")
USER = os.getenv("MQTT_USER")
PASS = os.getenv("MQTT_PASS")
FILE = os.getenv("LOG_FILE", "moisture_log.csv")

def on_connect(c, u, f, rc, props=None):
    c.subscribe("tree+/humidity")

def on_message(c, u, msg):
    with open(FILE, "a", newline="") as f:
        csv.writer(f).writerow([time.strftime("%Y-%m-%d %H:%M:%S"), msg.topic, msg.payload.decode()])

c = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
if USER: c.username_pw_set(USER, PASS)
c.on_connect, c.on_message = on_connect, on_message
c.connect(HOST, 1883)
c.loop_forever()
