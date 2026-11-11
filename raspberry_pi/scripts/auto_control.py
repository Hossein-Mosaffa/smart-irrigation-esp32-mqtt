#!/usr/bin/env python3
"""Optional: automatic on/off control with hysteresis.
Opens the valve when moisture < LOW, closes it when moisture > HIGH.
"""
import os
import paho.mqtt.client as mqtt

HOST = os.getenv("MQTT_HOST", "localhost")
USER = os.getenv("MQTT_USER")
PASS = os.getenv("MQTT_PASS")
LOW, HIGH = float(os.getenv("LOW", 30)), float(os.getenv("HIGH", 60))
state = {}

def on_connect(c, u, f, rc, props=None):
    c.subscribe("tree+/humidity")

def on_message(c, u, msg):
    tree = msg.topic.split("/")[0]
    try:
        m = float(msg.payload.decode())
    except ValueError:
        return
    s = state.get(tree, "off")
    if m < LOW and s != "on":
        state[tree] = "on";  c.publish(f"{tree}/output", "on")
    elif m > HIGH and s != "off":
        state[tree] = "off"; c.publish(f"{tree}/output", "off")

c = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
if USER: c.username_pw_set(USER, PASS)
c.on_connect, c.on_message = on_connect, on_message
c.connect(HOST, 1883)
c.loop_forever()
