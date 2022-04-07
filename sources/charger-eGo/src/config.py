SIMULATION = False

CHARGER_ID = "018455"

MQTT_PROGRAM_CONFIG = "Charger/config"  # 0...Off 1...Auto-Charge
MQTT_PROGRAM_OUTPUT = "Original/Charger/command"

MQTT_ELECTRICITY_FLOW = "Original/PV/Calc/FlowWatt"  # (+)=consuming; (-)=sending

MIN_TIME_BETWEEN_STATE_CHANGE = 60  # Seconds
MIN_TIME_BETWEEN_START_STOP = 5  # n x MIN_TIME_BETWEEN_STATE_CHANGE 

MIN_AMPERE = 6
MAX_AMPERE = 16

MQTT_CHARGER_STATUS = f"go-eCharger/{CHARGER_ID}/status"
MQTT_CHARGER_REQUEST = f"go-eCharger/{CHARGER_ID}/cmd/req"