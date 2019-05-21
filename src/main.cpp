// SimpleMQTT for APA
// author: bastian Maeuser <mephisto@mephis.to>

#include <NeoPixelBrightnessBus.h>
#include <NeoPixelAnimator.h>
#include "esp_wpa2.h"
#include <WiFi.h>
#include <PubSubClient.h>
#include "EEPROM.h"
#include <Update.h>
#include <Preferences.h>
#include "Arduino.h"

#include "config.h"

#define colorSaturation 255

// PubSub MQTT Client
WiFiClient client;
PubSubClient mqttclient(client);

int contentLength = 0;
bool isValidContentType = false;
String deviceid;

//EEprom Positions
int eepromPosLedcount = 0;

// for software bit bang
//NeoPixelBus<DotStarBgrFeature, DotStarMethod> strip(PixelCount, DotClockPin, DotDataPin);
NeoPixelBrightnessBus<DotStarBgrFeature, DotStarMethod> strip(maxPixelCount, DotClockPin, DotDataPin);

// for hardware SPI (best performance but must use hardware pins)
//NeoPixelBus<DotStarBgrFeature, DotStarSpiMethod> strip(PixelCount);

// DotStars that support RGB color and a overall luminance/brightness value
// NeoPixelBus<DotStarLbgrFeature, DotStarMethod> strip(PixelCount, DotClockPin, DotDataPin);
// DotStars that support RGBW color with a seperate white element
//NeoPixelBus<DotStarWbgrFeature, DotStarMethod> strip(PixelCount, DotClockPin, DotDataPin);

RgbColor red(colorSaturation, 0, 0);
RgbColor green(0, colorSaturation, 0);
RgbColor blue(0, 0, colorSaturation);
RgbColor white(colorSaturation);
RgbColor black(0);

RgbColor CurrentStripColor;
uint8_t Brightness;
uint8_t State;

// for use with RGB DotStars when using the luminance/brightness global value
// note that its range is only 0 - 31 (31 is full bright) and 
// also note that it is not useful for POV displays as it will cause more flicker
RgbwColor redL(colorSaturation, 0, 0, 31); // use white value to store luminance
RgbwColor greenL(0, colorSaturation, 0, 31); // use white value to store luminance
RgbwColor blueL(0, 0, colorSaturation, 31); // use white value to store luminance
RgbwColor whiteL(255, 255, 255, colorSaturation / 8); // luminance is only 0-31


const RgbColor CylonEyeColor(HtmlColor(0x7f0000));

// Autoupdater Functions
String getHeaderValue(String header, String headerName) {
    return header.substring(strlen(headerName.c_str()));
}
String getBinName(String url) {
    int index = 0;

    // Search for last /
    for (int i = 0; i < url.length(); i++) {
        if (url[i] == '/') {
            index = i;
        }
    }

    String binName = "";

    // Create binName
    for (int i = index; i < url.length(); i++) {
        binName += url[i];
    }

    return binName;
}
String getHostName(String url) {
     int index = 0;

    // Search for last /
    for (int i = 0; i < url.length(); i++) {
        if (url[i] == '/') {
            index = i;
        }
    }

    String hostName = "";

    // Create binName
    for (int i = 0; i < index; i++) {
        hostName += url[i];
    }

    return hostName;
}

void update(String url, int port) {
    String bin = getBinName(url);
    String host = getHostName(url);

    Serial.println("Connecting to: " + host);
    if (client.connect(host.c_str(), port)) {
        // Connection Succeed.
        // Fecthing the bin
        Serial.println("Fetching Bin: " + bin);

        // Get the contents of the bin file
        client.print(String("GET ") + bin + " HTTP/1.1\r\n" +
                        "Host: " + host + "\r\n" +
                        "Cache-Control: no-cache\r\n" +
                        "Connection: close\r\n\r\n");

        unsigned long timeout = millis();

        while (client.available() == 0) {
            if (millis() - timeout > 5000) {
                Serial.println("Client Timeout !");
                client.stop();
                return;
            }
        }
        while (client.available()) {
            // read line till /n
            String line = client.readStringUntil('\n');
            // remove space, to check if the line is end of headers
            line.trim();

            // if the the line is empty,
            // this is end of headers
            // break the while and feed the
            // remaining `client` to the
            // Update.writeStream();
            if (!line.length()) {
                //headers ended
                break; // and get the OTA started
            }

            // Check if the HTTP Response is 200
            // else break and Exit Update
            if (line.startsWith("HTTP/1.1")) {
                if (line.indexOf("200") < 0) {
                    Serial.println("Got a non 200 status code from server. Exiting OTA Update.");
                    break;
                }
            }

            // extract headers here
            // Start with content length
            if (line.startsWith("Content-Length: ")) {
                contentLength = atoi((getHeaderValue(line, "Content-Length: ")).c_str());
                Serial.println("Got " + String(contentLength) + " bytes from server");
            }

            // Next, the content type
            if (line.startsWith("Content-Type: ")) {
                String contentType = getHeaderValue(line, "Content-Type: ");
                Serial.println("Got " + contentType + " payload.");
                if (contentType == "application/octet-stream") {
                    isValidContentType = true;
                }
            }
        }
    }
    else {
        // Connect to S3 failed
        // May be try?
        // Probably a choppy network?
        Serial.println("Connection to " + host + " failed. Please check your setup");
        // retry??
    }

    // Check what is the contentLength and if content type is `application/octet-stream`
    Serial.println("contentLength : " + String(contentLength) + ", isValidContentType : " + String(isValidContentType));

    // check contentLength and content type
    if (contentLength && isValidContentType) {
        // Check if there is enough to OTA Update
        bool canBegin = Update.begin(contentLength);
        if (canBegin) {
            Serial.println("Begin OTA. This may take 2 - 5 mins to complete. Things might be quite for a while.. Patience!");
            size_t written = Update.writeStream(client);

            if (written == contentLength) {
                Serial.println("Written : " + String(written) + " successfully");
            }
            else {
                Serial.println("Written only : " + String(written) + "/" + String(contentLength) + ". Retry?" );
                // retry??
            }

            if (Update.end()) {
                Serial.println("OTA done!");
                if (Update.isFinished()) {
                    Serial.println("Update successfully completed. Rebooting.");
                    ESP.restart();
                }
                else {
                    Serial.println("Update not finished? Something went wrong!");
                }
            }
            else {
                Serial.println("Error Occurred. Error #: " + String(Update.getError()));
            }
        }
        else {
            // not enough space to begin OTA
            // Understand the partitions and
            // space availability
            Serial.println("Not enough space to begin OTA");
            client.flush();
        }
    }
    else {
        Serial.println("There was no content in the response");
        client.flush();
    }
}


NeoPixelAnimator animations(AnimationChannels); // NeoPixel animation management object
boolean fadeToColor = true;  // general purpose variable used to store effect state
struct MyAnimationState
{
    RgbColor StartingColor;
    RgbColor EndingColor;
};

AnimEaseFunction moveEase =
//      NeoEase::Linear;
//      NeoEase::QuadraticInOut;
//      NeoEase::CubicInOut;
        NeoEase::QuarticInOut;
//      NeoEase::QuinticInOut;
//      NeoEase::SinusoidalInOut;
//      NeoEase::ExponentialInOut;
//      NeoEase::CircularInOut;

uint16_t lastPixel = 0; // track the eye position
int8_t moveDir = 1; // track the direction of movement

// one entry per pixel to match the animation timing manager
MyAnimationState animationState[AnimationChannels];


void BlendAnimUpdate(const AnimationParam& param) {
    // this gets called for each animation on every time step
    // progress will start at 0.0 and end at 1.0
    // we use the blend function on the RgbColor to mix
    // color based on the progress given to us in the animation
    RgbColor updatedColor = RgbColor::LinearBlend(
        animationState[param.index].StartingColor,
        animationState[param.index].EndingColor,
        param.progress);

    // apply the color to the strip
    for (uint16_t pixel = 0; pixel < PixelCount; pixel++) {
        strip.SetPixelColor(pixel, updatedColor);
    }
}

void FadeAll(uint8_t darkenBy)
{
    RgbColor color;
    for (uint16_t indexPixel = 0; indexPixel < strip.PixelCount(); indexPixel++)
    {
        color = strip.GetPixelColor(indexPixel);
        color.Darken(darkenBy);
        strip.SetPixelColor(indexPixel, color);
    }
}

void FadeAnimUpdate(const AnimationParam& param)
{
    if (param.state == AnimationState_Completed)
    {
        FadeAll(10);
        animations.RestartAnimation(param.index);
    }
}

void MoveAnimUpdate(const AnimationParam& param)
{
    // apply the movement animation curve
    float progress = moveEase(param.progress);

    // use the curved progress to calculate the pixel to effect
    uint16_t nextPixel;
    if (moveDir > 0)
    {
        nextPixel = progress * PixelCount;
    }
    else
    {
        nextPixel = (1.0f - progress) * PixelCount;
    }

    // if progress moves fast enough, we may move more than
    // one pixel, so we update all between the calculated and
    // the last
    if (lastPixel != nextPixel)
    {
        for (uint16_t i = lastPixel + moveDir; i != nextPixel; i += moveDir)
        {
            strip.SetPixelColor(i, CylonEyeColor);
        }
    }
    strip.SetPixelColor(nextPixel, CylonEyeColor);

    lastPixel = nextPixel;

    if (param.state == AnimationState_Completed)
    {
        // reverse direction of movement
        moveDir *= -1;

        // done, time to restart this position tracking animation/timer
        animations.RestartAnimation(param.index);
    }
}

void FadeTo(float luminance, RgbColor tgt, uint16_t time=2000) {
    Serial.println("FadeTo called.." + String(fadeToColor));
    if (fadeToColor) {
        // Fade upto a random color
        // we use HslColor object as it allows us to easily pick a hue
        // with the same saturation and luminance so the colors picked
        // will have similiar overall brightness
        //RgbColor target = HslColor(random(360) / 360.0f, 1.0f, luminance);
        //uint16_t time = random(800, 2000);

        animationState[0].StartingColor = strip.GetPixelColor(0);
        animationState[0].EndingColor = tgt;

        animations.StartAnimation(0, time, BlendAnimUpdate);
    } /*else {
        // fade to black
        //uint16_t time = 1000;

        //animationState[0].StartingColor = strip.GetPixelColor(0);
        //animationState[0].EndingColor = RgbColor(0);

        //animations.StartAnimation(0, time, BlendAnimUpdate);
    }*/
    // toggle to the next effect state
    fadeToColor = !fadeToColor;
}


void setupWiFi() {
  WiFi.disconnect(true);
  WiFi.mode(WIFI_STA);
  if (strcmp(WIFI_MODE,"WPA")==0) {
    Serial.print("WPA2 PSK: Connecting to '");
    Serial.print(WIFI_SSID);
    Serial.println("' ...");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }
  } else {
    Serial.print("WPA2 Enterprise: Connecting to '");
    Serial.print(WIFI_SSID);
    Serial.println("' ...");
    if( esp_wifi_sta_wpa2_ent_set_ca_cert((uint8_t *)wpa_root_ca, strlen(wpa_root_ca)+1) ){
      Serial.println("Failed to set WPA2 CA Certificate");
      return;
    } else {
      Serial.println("WPA2 CA loaded");
    }
    esp_wpa2_config_t config = WPA2_CONFIG_INIT_DEFAULT();
    if( esp_wifi_sta_wpa2_ent_enable(&config) ){
      Serial.println("Failed to enable WPA2");
      return;
    }
    esp_wifi_sta_wpa2_ent_set_username((uint8_t *)EAP_USERNAME, strlen(EAP_USERNAME));
    esp_wifi_sta_wpa2_ent_set_password((uint8_t *)EAP_PASSWORD, strlen(EAP_PASSWORD));
    esp_wifi_sta_wpa2_ent_enable(&config);
    WiFi.begin(WIFI_SSID);
  }

  //wait until connect
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.print("Mac address: ");
  Serial.println(WiFi.macAddress());

  //write deviceid to global
  String lmac = WiFi.macAddress();
  char cnt;
  char no = ':';
  for (int i=0; i<lmac.length()-1;++i){
    cnt = lmac.charAt(i);
    if(cnt==no){
      lmac.remove(i, 1);
    }
  }
  deviceid = lmac.c_str();
}

void callback(char* topic, byte* payload, unsigned int length) {
    Serial.print("Message arrived [");
    Serial.print(topic);
    Serial.print("] ");
    String sPayload;
    for (int i=0;i<length;i++) {
        Serial.print((char)payload[i]);
        sPayload.concat((char)payload[i]);
    }
    Serial.println("");

    if (strcmp(topic,String("led/" + deviceid + "/color").c_str())==0) {
        int r,g,b;
        const char *cPayload = sPayload.c_str();
        if (sscanf(cPayload, "%d,%d,%d", &r, &g, &b) == 3) {
            Serial.println("MQTT: Fade to: " + String(r) + "," + String(g) + "," + String(b));
            fadeToColor = true;
            CurrentStripColor=RgbColor(r,g,b);
            FadeTo (0.2f,RgbColor(r,g,b),TransitionTime);
            animations.StopAnimation(1);
            animations.StopAnimation(2);
            Serial.println("Current Brightness" + String(strip.GetBrightness()));
        } 
    }

    if (strcmp(topic,String("led/" + deviceid + "/hexcolor").c_str())==0) {
        long r,g,b;
        long long number = strtoll( &sPayload[0], NULL, 16);
        r = number >> 16;
        g = number >> 8 & 0xFF;
        b = number & 0xFF;
        Serial.println("MQTT: Fade to: " + String(r) + "," + String(g) + "," + String(b));
        fadeToColor = true;
        CurrentStripColor=RgbColor(r,g,b);
        FadeTo (0.2f,RgbColor(r,g,b),TransitionTime);
        animations.StopAnimation(1);
        animations.StopAnimation(2);
        Serial.println("Current Brightness" + String(strip.GetBrightness()));
    }


    if (strcmp(topic,String("led/" + deviceid + "/on").c_str())==0) {
        Serial.println("On called");
        fadeToColor = true;
        FadeTo (0.2f,CurrentStripColor,TransitionTime);
        animations.StopAnimation(1);
        animations.StopAnimation(2);
        strip.Show();
    }
    if (strcmp(topic,String("led/" + deviceid + "/off").c_str())==0) {
        Serial.println("Off called");
        fadeToColor = true;
        FadeTo(0.0f,black,TransitionTime);
        animations.StopAnimation(1);
        animations.StopAnimation(2);
    }
    if (strcmp(topic,String("led/" + deviceid + "/brightness").c_str())==0) {
        Serial.println("Set Brightness called");
        strip.SetBrightness(sPayload.toInt());
        fadeToColor = true;
        FadeTo (0.2f,CurrentStripColor,TransitionTime);

        strip.Show();
    }
    if (strcmp(topic,String("led/" + deviceid + "/cylone").c_str())==0) {
        Serial.println("Cylone called");
        animations.StartAnimation(1, 5, FadeAnimUpdate);
        animations.StartAnimation(2, TransitionTime, MoveAnimUpdate);
    }
    if (strcmp(topic,String("led/" + deviceid + "/config/pixelcount").c_str())==0) {
        Serial.println("Config LED Count");
        PixelCount = sPayload.toInt();
        EEPROM.writeInt(eepromPosLedcount,sPayload.toInt());
        EEPROM.commit();
    }
    if (strcmp(topic,String("led/" + deviceid + "/update").c_str())==0) {
        animations.StartAnimation(1, 5, FadeAnimUpdate);
        Serial.println("Update requested called");
        update(sPayload, 80);
    }
}

/* reconnect mqtt */
void reconnectmqtt() {
  // Loop until we're reconnected
  while (!mqttclient.connected()) {
    Serial.print("Attempting MQTT connection to '");
    Serial.print(MQTT_SERVER);
    Serial.print("'....");
    String clientId = "iot-";
    clientId += deviceid;
    if (mqttclient.connect(clientId.c_str(),MQTT_USER,MQTT_PASSWORD,String("led/" + deviceid + "/state").c_str(),0,false,"disconnected")) {
      Serial.println("connected");
      // subscripe to commands
      mqttclient.subscribe(String("led/" + deviceid + "/on").c_str());       //decimal colors eg. 255,0,0 R,G,B
      mqttclient.subscribe(String("led/" + deviceid + "/off").c_str());       //decimal colors eg. 255,0,0 R,G,B
      mqttclient.subscribe(String("led/" + deviceid + "/color").c_str());       //decimal colors eg. 255,0,0 R,G,B
      mqttclient.subscribe(String("led/" + deviceid + "/hexcolor").c_str());    //hex colors eg. ff0000 RRGGBB
      mqttclient.subscribe(String("led/" + deviceid + "/brightness").c_str());  
      mqttclient.subscribe(String("led/" + deviceid + "/cylone").c_str());
      mqttclient.subscribe(String("led/" + deviceid + "/update").c_str());
      mqttclient.subscribe(String("led/" + deviceid + "/config/pixelcount").c_str());
      mqttclient.publish(String("led/" + deviceid + "/state").c_str(), "connected");

    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttclient.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void setup()
{
    Serial.begin(115200);
    while (!Serial); // wait for serial attach

    Serial.println("\nTesting EEPROM Library\n");
    if (!EEPROM.begin(128)) {
        Serial.println("Failed to initialise EEPROM");
        Serial.println("Restarting...");
        delay(1000);
        ESP.restart();
    }

    PixelCount = EEPROM.readInt(eepromPosLedcount);
    Serial.println("Configured for " + String(PixelCount) + " Pixels");

    setupWiFi();
    yield();

    Serial.println("DeviceID: " + deviceid);

    mqttclient.setServer(MQTT_SERVER,1883);
    mqttclient.setClient(client);
    mqttclient.setCallback(callback);

    Serial.println(String("led/" + deviceid + "/state"));

    Serial.println();
    Serial.println("Initializing...");
    Serial.flush();

    // this resets all the neopixels to an off state
    strip.Begin();
    strip.ClearTo(black);
    strip.Show();

    Serial.println();
    Serial.println("Running...");

    FadeTo(0.2f,white,TransitionTime);
}



void changeColor(RgbColor cl) {
    // set the colors, 
    int i;
    for (i=0; i<PixelCount; i++) {
        strip.SetPixelColor(i, cl);
    }
    strip.Show();
}


void loop() {
    if (!mqttclient.connected()) {
        Serial.println("Connecting MQTT");
        reconnectmqtt();
    }

    if (animations.IsAnimating()) {
        // the normal loop just needs these two to run the active animations
        animations.UpdateAnimations();
        strip.Show();
    } /*else {
      FadeTo (0.0,red,4000); // 0.0 = black, 0.25 is normal, 0.5 is bright  
    }*/
    mqttclient.loop();
}