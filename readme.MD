## Simple MQTT controlled ESP32 Controller for APA102

- MQTT commanded OTA Updater
- Currently basic commandset:

```
led/deviceid/on                     //decimal colors eg. 255,0,0 R,G,B
led/deviceid/off                    //decimal colors eg. 255,0,0 R,G,B
led/deviceid/color                  //decimal colors eg. 255,0,0 R,G,B
led/deviceid/hexcolor               //hex colors eg. ff0000 RRGGBB
led/deviceid/brightness             //brightness (0-255)
led/deviceid/cylone                 //cylone animation
led/deviceid/update                 //command OTA update (payload == url to firmware image without http:// eg. "router.home/firmware.bin")
```

The deviceid is the Mac address of the Wifi Interface

## Building

Copy config.h.dist to config.h and adjust values according your needs