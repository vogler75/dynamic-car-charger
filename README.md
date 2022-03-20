# Dynamic Car Charger

Adaptive sustainable-energy charging with a [go-eCharger](https://go-e.co/produkte/go-echarger-home/).  

You need a summeriazed value of a Smart-Meter which indicates how many Watt you currently receive from or delivery to your power provider. This value must be available on a MQTT topic. The go-eCharger will be turned on and off automatically, also the Amperes are automatically adjusted depending on how much energy is available.

Note: Currently it is made for a single line connection of the go-eCharger only (no 3 phase connection). Because with that it is possible to adjust the ampere from 6 to 16. If you connect the charger with 3 phases then the minimum charging power will be 3 * 6 ampere, this is to much for my PV power plant.  

![Image](README.jpg)

