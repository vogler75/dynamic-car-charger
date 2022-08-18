import json
import time
import threading
import paho.mqtt.client as mqtt
from datetime import datetime, timedelta
from config import *

CAR_NO_VEHICLE = 1
CAR_VEHICLE_LOADS = 2
CAR_WAITING_FOR_VEHICLE = 3
CAR_CHARGE_FINISHED = 4

g_mode = 1  # Auto-Charge 1...On 2...On without Stopping Chargin 0...Off
g_flow_ampere = 0.0
g_act_loading_state = 0  # Off, MIN_AMPERE - MAX_AMPERE
g_last_state_change_time = datetime.now()
g_last_start_stop_time = datetime(1970, 1, 1)
g_charger_status = None
g_stop_counter = 0


def get_flow(data):
    global g_flow_ampere, g_act_loading_state

    flow = -round(data["Value"])
    if SIMULATION:
        flow = flow - (g_act_loading_state * 220)

    g_flow_ampere = (g_flow_ampere + flow / 220.0) / (2.0 if g_flow_ampere != 0 else 1)

    # print(dt, "Get", flow, g_flow_ampere, flow)


def check_state(dt):
    global g_flow_ampere, g_act_loading_state, g_last_state_change_time, g_last_start_stop_time, g_stop_counter

    if dt < g_last_state_change_time + timedelta(seconds=MIN_TIME_BETWEEN_STATE_CHANGE):
        return

    flow_ampere = g_flow_ampere
    g_flow_ampere = 0

    new_loading_state = None
    if g_act_loading_state == 0:
        if flow_ampere >= MIN_AMPERE:
            new_loading_state = int(flow_ampere)

    elif g_act_loading_state > 0:
        if flow_ampere <= 0:
            new_loading_state = g_act_loading_state + int(flow_ampere)
        elif flow_ampere >= 1:
            new_loading_state = g_act_loading_state + int(flow_ampere)

    if new_loading_state is not None:
        if new_loading_state < MIN_AMPERE:  # Stop            
            g_stop_counter += 1
            print(dt, "Stop counter", g_stop_counter)            
            if g_stop_counter <= MIN_TIME_BETWEEN_START_STOP or g_mode == 2:
                new_loading_state = MIN_AMPERE
            else:                
                new_loading_state = 0
        else:
            g_stop_counter = 0

        if new_loading_state > MAX_AMPERE:  # Limit          
            new_loading_state = MAX_AMPERE

    if new_loading_state is not None and g_act_loading_state != new_loading_state:
        print(dt, "Ampere", flow_ampere, "Act", g_act_loading_state, "New", new_loading_state)
        g_last_state_change_time = dt

        if g_charger_status is None:
            print(dt, "No charger status available")
            return

        err = int(g_charger_status["err"])
        if err > 0:
            print(dt, "Charger error: ", g_charger_status["err"])
            return

        car = int(g_charger_status["car"])
        if car == CAR_NO_VEHICLE:
            print(dt, "No vehicle")
            return

        if new_loading_state == 0 and g_act_loading_state > 0:
            print(dt, "STOP Charging")

            g_mqtt.publish(MQTT_CHARGER_REQUEST, "amx=" + str(MIN_AMPERE))
            g_mqtt.publish(MQTT_CHARGER_REQUEST, "alw=0")

            g_last_start_stop_time = dt
            g_act_loading_state = new_loading_state                

        elif MIN_AMPERE <= new_loading_state <= MAX_AMPERE:

            if g_act_loading_state == 0:
                print(dt, "START Charging", new_loading_state)

                g_mqtt.publish(MQTT_CHARGER_REQUEST, "amx=" + str(new_loading_state))
                g_mqtt.publish(MQTT_CHARGER_REQUEST, "alw=1")

                g_last_start_stop_time = dt
                g_act_loading_state = new_loading_state
                g_mqtt.publish(MQTT_PROGRAM_OUTPUT, json.dumps({"Value": new_loading_state}))

            else:
                print(dt, "UPDATE", new_loading_state)
                g_mqtt.publish(MQTT_CHARGER_REQUEST, "amx=" + str(new_loading_state))
                g_act_loading_state = new_loading_state
                g_mqtt.publish(MQTT_PROGRAM_OUTPUT, json.dumps({"Value": new_loading_state}))


def state_loop():
    global g_mode
    while True:
        time.sleep(10)
        if g_mode > 0:
            check_state(datetime.now())


def on_connect(client_data, user_data, flags, rc):
    g_mqtt.subscribe(MQTT_PROGRAM_CONFIG)
    g_mqtt.subscribe(MQTT_ELECTRICITY_FLOW)
    g_mqtt.subscribe(MQTT_CHARGER_STATUS)


def on_message(client_data, user_data, message):
    global g_mqtt, g_mode, g_charger_status, g_act_loading_state

    try:
        payload = message.payload.decode('utf-8')
        data = json.loads(payload)
    except:
        print(datetime.now(), "JSON Error!", message.topic, message.payload)
        data = None

    if data is not None:
        if message.topic == MQTT_PROGRAM_CONFIG:
            if "Mode" in data:
                g_mode = data["Mode"]
                print(datetime.now(), "Got mode " +str(g_mode))
        elif message.topic == MQTT_ELECTRICITY_FLOW:
            get_flow(data)
        elif message.topic == MQTT_CHARGER_STATUS:
            g_charger_status = data
            amp = int(data["amp"])
            alw = int(data["alw"])
            if alw == 0 and g_act_loading_state != 0:
                g_act_loading_state = 0
                print(datetime.now(), "Got Loading State 0 from Charger!")
            elif alw == 1 and g_act_loading_state != amp:
                g_act_loading_state = amp
                print(datetime.now(), "Got Loading State " + str(amp) + " from Charger!")
                
            g_mqtt.publish(MQTT_PROGRAM_OUTPUT, json.dumps({"Value": amp * alw}))                


g_mqtt = mqtt.Client(clean_session=True)
g_mqtt.on_connect = on_connect
g_mqtt.on_message = on_message
g_mqtt.connect("nuc1.rocworks.local", 1883, 60)
g_mqtt.loop_start()
threading.Thread(target=state_loop).start()
print(datetime.now(), "Started.")
