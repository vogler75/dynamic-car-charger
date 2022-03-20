SIMULATION = False

CHARGER_ID = "018455"  # Id of your go-eCharger

MQTT_PROGRAM_CONFIG = "Charger/config"  # Enable/Disable Auto-Charging (0...Off 1...On); Must be a JSON with a "Mode" Key: { "Mode": 1 }
MQTT_PROGRAM_OUTPUT = "Original/Charger/command"  # Just an information about current loading state

MQTT_ELECTRICITY_FLOW = "Original/PV/Calc/FlowWatt"  # (+)=consuming (-)=sending Energy; Must be a JSON with a "Value" Key: { "Value": -120 }

MIN_TIME_BETWEEN_STATE_CHANGE = 60  # Seconds
MIN_TIME_BETWEEN_START_STOP = 3  # Minutes

MIN_AMPERE = 6
MAX_AMPERE = 16