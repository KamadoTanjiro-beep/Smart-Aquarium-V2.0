/*
  The MIT License (MIT)
   Copyright (c) by respective library files owners
   Copyright (c) 2021 by Aniket Patra

   Permission is hereby granted, free of charge, to any person obtaining a copy
   of this software and associated documentation files (the "Software"), to deal
   in the Software without restriction, including without limitation the rights
   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
   copies of the Software, and to permit persons to whom the Software is
   furnished to do so, subject to the following conditions:
   The above copyright notice and this permission notice shall be included in all
   copies or substantial portions of the Software.
   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
   SOFTWARE.

   Initial Beta Version
   V1.0 OTA update, oled connection, wifi icons(connection/disconnection and WiFi signal strength display) 15/11/21
   v1.2 Added DS3231 for back time keeping, added a way to power up when there is no wifi signal 16/11/21
   v1.3 Added relays to the circuit (4 channel active-low) 17/11/21
   v1.4 Added relay initialization function (helpful after powerloss), WEB SERVER (for controlling relays, getting temperature and time), further optimizations 25/11/21
   v1.5 Added control to turn off/on oled display as you want (via Web Server) 09/12/21
   v1.6 Added control to turn off/on Aqaurium Light as you want (via Web Server). Previously the timer would overwrite the settings. {Needs optimization for restarts} 14/12/21
   v1.7 Optimizations for timers {Still needs optimizations} 16/12/21
   v1.71 Bug updates 21/12/21
   v1.8 Added Auto/Manual Control for all relays. Updates to HTML of Web Server.
   v1.9 Added option to update time via web-server and other updates
   v1.10 Added option to auto start relay (when off) and added visual message over screen when time is updating
   v1.11 Updated the UI of AutoTimers to give options for 5 min, 10min, 15 min, 25 min, 30 min, 45 min timers and other mild updates
*/

#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <NTPClient.h>
#include <DS3231.h>
#include <string.h>

DS3231 Clock;
bool century = false;
bool h12Flag = false;
bool pmFlag;

#ifndef STASSID
#define STASSID "XXXXXXXXXXXXX" // WIFI NAME/SSID
#define STAPSK "YYYYYYYYYYY"    // WIFI PASSWORD
#endif

const char *ssid = STASSID;
const char *password = STAPSK;

// Set web server port number to 80
WiFiServer server(80);

// Variable to store the HTTP request
String header;

// Auxiliar variables to store the current output state
String Relay1State = "off";
String Relay2State = "off";
String Relay3State = "off";
String Relay4State = "off";

// Define timeout time in milliseconds (example: 2000ms = 2s)
unsigned long currentTime = millis();
unsigned long previousTime = 0;
const long timeoutTime = 2000; // client timeout for the webserver

unsigned long lastTime = 0;
const long timerDelay = 30000; // check relay status every 30 seconds

unsigned long lastTime1 = 0;
const long timerDelay1 = 5000; // check wifi status every 900 m.seconds

// timer settings for autotimers
unsigned long lastAutoTimer1 = 0, lastAutoTimer2 = 0, lastAutoTimer3 = 0, lastAutoTimer4 = 0;
long autoTimerDelay1 = 0, autoTimerDelay2 = 0, autoTimerDelay3 = 0, autoTimerDelay4 = 0; // example if it would have to check after 5 mins i.e. 5*60*1000=300000

// utc offset according to India, please change according to your location
const long utcOffsetInSeconds = 19800;

char week[7][20] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};

// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "asia.pool.ntp.org", utcOffsetInSeconds);

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

#define OLED_RESET LED_BUILTIN // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C    // 0x3c for 0x78
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// different WiFi icons Full, Medium, Half and Low Signal. ! for no connection
static const unsigned char PROGMEM wifiFull[] = {
    0x00, 0x00, 0x00, 0x00, 0x07, 0xE0, 0x3F, 0xFC, 0x70, 0x0E, 0xE0, 0x07, 0x47, 0xE2, 0x1E, 0x78,
    0x18, 0x18, 0x03, 0xC0, 0x07, 0xE0, 0x00, 0x00, 0x01, 0x80, 0x01, 0x80, 0x00, 0x00, 0x00, 0x00};

static const unsigned char PROGMEM wifiMed[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0xE0, 0x1E, 0x78,
    0x18, 0x18, 0x03, 0xC0, 0x07, 0xE0, 0x00, 0x00, 0x01, 0x80, 0x01, 0x80, 0x00, 0x00, 0x00, 0x00};

static const unsigned char PROGMEM wifiHalf[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x03, 0xC0, 0x07, 0xE0, 0x00, 0x00, 0x01, 0x80, 0x01, 0x80, 0x00, 0x00, 0x00, 0x00};

static const unsigned char PROGMEM wifiLow[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x80, 0x01, 0x80, 0x00, 0x00, 0x00, 0x00};

const unsigned char picture[] PROGMEM = { // picture of a window (during boot)
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0xff, 0xfe, 0x7f, 0xff, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0xff, 0xfe, 0x7f, 0xff, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0xff, 0xfe, 0x7f, 0xff, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0xff, 0xfe, 0x7f, 0xff, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0xff, 0xfe, 0x7f, 0xff, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0xff, 0xfe, 0x7f, 0xff, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0xff, 0xfe, 0x7f, 0xff, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0xff, 0xfe, 0x7f, 0xff, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0xff, 0xfe, 0x7f, 0xff, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0xff, 0xfe, 0x7f, 0xff, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0xff, 0xfe, 0x7f, 0xff, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0xff, 0xfe, 0x7f, 0xff, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0xff, 0xfe, 0x7f, 0xff, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0xff, 0xfe, 0x7f, 0xff, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0xff, 0xfe, 0x7f, 0xff, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0xff, 0xfe, 0x7f, 0xff, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0xff, 0xfe, 0x7f, 0xff, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0xff, 0xfe, 0x7f, 0xff, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0xff, 0xfe, 0x7f, 0xff, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0xff, 0xfe, 0x7f, 0xff, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0xff, 0xfe, 0x7f, 0xff, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0xff, 0xfe, 0x7f, 0xff, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0xff, 0xfe, 0x7f, 0xff, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0xff, 0xfe, 0x7f, 0xff, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0xff, 0xfe, 0x7f, 0xff, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0xff, 0xfe, 0x7f, 0xff, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0xff, 0xfe, 0x7f, 0xff, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0xff, 0xfe, 0x7f, 0xff, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0xff, 0xfe, 0x7f, 0xff, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0xff, 0xfe, 0x7f, 0xff, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0xff, 0xfe, 0x7f, 0xff, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0xff, 0xfe, 0x7f, 0xff, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0xff, 0xfe, 0x7f, 0xff, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0xff, 0xfe, 0x7f, 0xff, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0xff, 0xfe, 0x7f, 0xff, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0xff, 0xfe, 0x7f, 0xff, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0xff, 0xfe, 0x7f, 0xff, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0xff, 0xfe, 0x7f, 0xff, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

// forward declarations

void checkTimeFor(int, int, int);
void showWifiSignal();
void showTime();
void updateRTC();
void relayStatusPrinter();
void clientPage(WiFiClient, String);

// relay gpio pins

const int relayPin1 = 0;
const int relayPin2 = 12;
const int relayPin3 = 13;
const int relayPin4 = 14;

// status flags

byte R1Flag = 0, R2Flag = 0, R3Flag = 0, R4Flag = 0;
byte OLEDConfig = 1, R1Config = 1, R2Config = 1, R3Config = 1, R4Config = 1; // Config=1 means auto (based on timer),Config=0 means Manual. Same for aquarium light
byte OLEDStatus = 1, updateTime = 0;                                         // Status = 1 means OLED DISPLAY is ON and reversed for 0 (works only if Config is 0)
byte autoTimer1 = 0, autoTimer2 = 0, autoTimer3 = 0, autoTimer4 = 0;

// Relay activation
#define RELAY1 false // mark true to activate, and false to deactivate (activation means the timer activation)
#define RELAY2 false
#define RELAY3 false
#define RELAY4 true

// Relay Timing and definitions
#define RELAY1TIME 1100, 1900, 1 // first number is "ON" time in 24 hours. i.e. 2:35pm would be 1435, second one is turn "OFF" time, and the last one is Relay Number
#define RELAY1ON digitalWrite(relayPin1, HIGH)
#define RELAY1OFF digitalWrite(relayPin1, LOW)

#define RELAY2TIME 1200, 1900, 2
#define RELAY2ON digitalWrite(relayPin2, HIGH)
#define RELAY2OFF digitalWrite(relayPin2, LOW)

#define RELAY3TIME 1200, 1900, 3
#define RELAY3ON digitalWrite(relayPin3, HIGH)
#define RELAY3OFF digitalWrite(relayPin3, LOW)

#define RELAY4TIME 1100, 1700, 4
#define RELAY4ON digitalWrite(relayPin4, HIGH)
#define RELAY4OFF digitalWrite(relayPin4, LOW)

// oled display off at night timings (needs improvement)
#define OLED_OFF 9 // Hour only. In 24 hours mode i.e. 23 for 11 pm
#define OLED_ON 7

const String newHostname = "AquariumNode";

void relayInitialize() // checks and initializes all relay in case of powerloss (takes effect only if relay is activated above). Default: Turns ON the relay
{
  if (RELAY1)
  {
    checkTimeFor(RELAY1TIME);
  }
  else
  {
    RELAY1ON;
    R1Flag = 1;
    Relay1State = "on";
  }

  if (RELAY2)
  {
    checkTimeFor(RELAY2TIME);
  }
  else
  {
    RELAY2ON;
    R2Flag = 1;
    Relay2State = "on";
  }

  if (RELAY3)
  {
    checkTimeFor(RELAY3TIME);
  }
  else
  {
    RELAY3ON;
    R3Flag = 1;
    Relay3State = "on";
  }

  if (RELAY4)
  {
    checkTimeFor(RELAY4TIME);
  }
  else
  {
    RELAY4ON;
    R4Flag = 1;
    Relay4State = "on";
  }
}

void setup()
{
  pinMode(relayPin1, OUTPUT);
  pinMode(relayPin2, OUTPUT);
  pinMode(relayPin3, OUTPUT);
  pinMode(relayPin4, OUTPUT);

  Wire.begin();
  Serial.begin(57600);
  relayInitialize();

  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS))
  {
    Serial.println(F("SSD1306 allocation failed"));
    delay(3000);
    ESP.restart();
  }
  display.clearDisplay();
  display.drawBitmap(0, 0, picture, 128, 64, WHITE);
  display.display();
  delay(1000);

  display.clearDisplay();
  display.setCursor(30, 25);   // Start at top-left corner
  display.setTextSize(1);      // Normal 1:1 pixel scale
  display.setTextColor(WHITE); // Draw white text

  display.println(F("Connecting..."));
  display.display();

  // Wifi connection
  WiFi.mode(WIFI_STA);
  WiFi.hostname(newHostname.c_str()); // Set new hostname
  WiFi.begin(ssid, password);
  int count = 0;
  while (WiFi.waitForConnectResult() != WL_CONNECTED)
  {
    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(0, 0); // Start at top-left corner
    display.println(F("Connection\n\nFailed!"));
    display.display();
    delay(5000);
    // ESP.restart();
    count = 1;
    break;
  }
  // OTA UPDATE SETTINGS  !! CAUTION !! PROCEED WITH PRECAUTION
  ArduinoOTA.onStart([]()
                     {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH)
    {
      type = "sketch";
    }
    else
    { // U_FS
      type = "filesystem";
    }

    // NOTE: if updating FS this would be the place to unmount FS using FS.end()
    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(0, 0); // Start at top-left corner
    display.println(F("OTA Update\n"));
    display.setTextSize(1);
    display.print("Type: ");
    display.print(type);
    display.display();
    delay(2000); });
  ArduinoOTA.onEnd([]()
                   {
    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(15, 25); // Start at top-left corner
    display.println(F("Rebooting"));
    display.display(); });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total)
                        {
    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(0, 10); // Start at top-left corner
    display.print("Progress: ");
    display.setCursor(50, 35);
    display.print((progress / (total / 100)));
    display.setCursor(88, 35);
    display.print("%");
    display.display(); });

  ArduinoOTA.onError([](ota_error_t error)
                     {
    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(0, 10); // Start at top-left corner
    display.print("Error!");
    display.display();
    //Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR)
    {
      display.clearDisplay();
      display.setTextSize(2);
      display.setCursor(0, 10); // Start at top-left corner
      display.print("Auth Failed");
      display.display();
    }
    else if (error == OTA_BEGIN_ERROR)
    {
      display.clearDisplay();
      display.setTextSize(2);
      display.setCursor(0, 10); // Start at top-left corner
      display.print("Begin Failed");
      display.display();
    }
    else if (error == OTA_CONNECT_ERROR)
    {
      display.clearDisplay();
      display.setTextSize(2);
      display.setCursor(0, 10); // Start at top-left corner
      display.print("Connect Failed");
      display.display();
    }
    else if (error == OTA_RECEIVE_ERROR)
    {
      display.clearDisplay();
      display.setTextSize(2);
      display.setCursor(0, 10); // Start at top-left corner
      display.print("Receive Failed");
      display.display();
    }
    else if (error == OTA_END_ERROR)
    {
      display.clearDisplay();
      display.setTextSize(2);
      display.setCursor(0, 10); // Start at top-left corner
      display.print("End Failed");
      display.display();
    } });
  ArduinoOTA.begin();
  // OTA UPDATE END

  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(0, 0); // Start at top-left corner
  display.println(F("Connected!"));
  display.setTextSize(1);
  display.println("");
  display.println(WiFi.SSID());
  display.print("\nIP address:\n");
  display.println(WiFi.localIP());
  display.display();
  delay(2000);
  // checks if wifi connected
  if (count == 0)
  {
    timeClient.begin(); // NTP time
    timeClient.update();
    updateRTC();
  }
  server.begin();
}

void loop()
{
  ArduinoOTA.handle();
  WiFiClient client = server.available(); // Listen for incoming clients

  if (updateTime == 1)
  {
    updateRTC();
    updateTime = 0;
  }

  if (client)
  {                          // If a new client connects
    String currentLine = ""; // make a String to hold incoming data from the client
    currentTime = millis();
    previousTime = currentTime;
    clientPage(client, currentLine);
    // Clear the header variable
    header = "";
    // Close the connection
    client.stop();
  }

  display.clearDisplay();

  // checking timers for relays
  if ((millis() - lastTime) > timerDelay)
  {

    if (R1Config == 0)
    {
    }
    else
    {
      if (RELAY1)
      {
        checkTimeFor(RELAY1TIME);
      }
    }
    if (R2Config == 0)
    {
    }
    else
    {
      if (RELAY2)
      {
        checkTimeFor(RELAY2TIME);
      }
    }
    if (R3Config == 0)
    {
    }
    else
    {
      if (RELAY3)
      {
        checkTimeFor(RELAY3TIME);
      }
    }
    if (R4Config == 0)
    {
    }
    else
    {
      if (RELAY4)
      {
        checkTimeFor(RELAY4TIME);
      }
    }
    lastTime = millis();
  }

  // autoTimer checkings
  if (autoTimer1 == 1)
  {
    if ((millis() - lastAutoTimer1) > autoTimerDelay1)
    {
      R1Flag = 1;
      RELAY1ON;
      Relay1State = "on";
      autoTimer1 = 0;
    }
  }
  if (autoTimer2 == 1)
  {
    if ((millis() - lastAutoTimer2) > autoTimerDelay2)
    {
      R2Flag = 1;
      RELAY2ON;
      Relay2State = "on";
      autoTimer2 = 0;
    }
  }
  if (autoTimer3 == 1)
  {
    if ((millis() - lastAutoTimer3) > autoTimerDelay3)
    {
      R3Flag = 1;
      RELAY3ON;
      Relay3State = "on";
      autoTimer3 = 0;
    }
  }
  if (autoTimer4 == 1)
  {
    if ((millis() - lastAutoTimer4) > autoTimerDelay4)
    {
      R4Flag = 1;
      RELAY4ON;
      Relay4State = "on";
      autoTimer4 = 0;
    }
  }

  // checking wifi strength
  if ((millis() - lastTime1) > timerDelay1)
  {
    showTime();
    showWifiSignal(); // at (112,0), wifi sampling once in 900 m.seconds
    relayStatusPrinter();

    if (OLEDConfig == 0) // Automatic/Manual Control of OLED Display
    {

      if (OLEDStatus == 0)
      {
        display.clearDisplay();
        display.display();
      }
      else
        display.display();
    }
    else if (OLEDConfig == 1)
    {
      byte currTime = Clock.getHour(h12Flag, pmFlag); // stores current hour temporarily
      if (OLED_OFF > OLED_ON)
      {
        if ((currTime >= OLED_ON) && (currTime <= OLED_OFF))
          display.display();
        else
        {
          display.clearDisplay();
          display.display();
        }
      }
      else
      {
        if (((currTime >= OLED_ON) && (currTime >= OLED_OFF)) || ((currTime <= OLED_ON) && (currTime <= OLED_OFF))) // oled display turn of at night
          display.display();
        else
        {
          display.clearDisplay();
          display.display();
        }
      }
    }

    lastTime1 = millis();
  }
}

void showWifiSignal()
{
  int x = WiFi.RSSI();
  if (WiFi.status() != WL_CONNECTED)
  {
    display.setCursor(112, 0);
    // display.setTextSize(2);
    display.println(F("!"));
  }
  else
  {
    if (x <= (-80))
    {
      display.drawBitmap(112, 0, wifiLow, 16, 16, WHITE); // worst signal
    }
    else if ((x <= (-70)) && (x > (-80)))
    {
      display.drawBitmap(112, 0, wifiHalf, 16, 16, WHITE); // poor signal
    }
    else if ((x <= (-60)) && (x > (-70))) // good signal
    {
      display.drawBitmap(112, 0, wifiMed, 16, 16, WHITE); // best signal
    }
    else if (x > (-60))
    {
      display.drawBitmap(112, 0, wifiFull, 16, 16, WHITE); // excellent signal
    }
  }
}

void showTime()
{
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);

  if (Clock.getHour(h12Flag, pmFlag) < 10)
    display.print(0, DEC);

  display.print(Clock.getHour(h12Flag, pmFlag)); // Time
  display.print(":");
  if (Clock.getMinute() < 10)
    display.print(0, DEC);
  display.print(Clock.getMinute());
  display.print(":");
  if (Clock.getSecond() < 10)
    display.print(0, DEC);
  display.println(Clock.getSecond());

  display.setTextSize(1);
  display.print(week[Clock.getDoW()]);
  display.print(", ");
  display.print(Clock.getDate());
  display.print("-");
  display.print(Clock.getMonth(century));
  display.print("-20");
  display.println(Clock.getYear());

  display.setCursor(90, 25);
  display.print(Clock.getTemperature(), 1);
  display.println(" C");
}

void updateRTC()
{
  display.clearDisplay();
  display.setCursor(30, 25);   // Start at top-left corner
  display.setTextSize(1);      // Normal 1:1 pixel scale
  display.setTextColor(WHITE); // Draw white text

  display.println(F("Updating Time"));
  display.display();
  delay(2000);
  timeClient.update();
  time_t rawtime = timeClient.getEpochTime();
  struct tm *ti;
  ti = localtime(&rawtime);

  uint16_t year = ti->tm_year + 1900;
  uint8_t x = year % 10;
  year = year / 10;
  uint8_t y = year % 10;
  year = y * 10 + x;

  uint8_t month = ti->tm_mon + 1;

  uint8_t day = ti->tm_mday;

  uint8_t hours = ti->tm_hour;

  uint8_t minutes = ti->tm_min;

  uint8_t seconds = ti->tm_sec;

  uint8_t dow = ti->tm_wday;

  Clock.setClockMode(false); // set to 24h
  // setClockMode(true); // set to 12h

  Clock.setYear(year);
  Clock.setMonth(month);
  Clock.setDate(day);
  Clock.setDoW(dow);
  Clock.setHour(hours);
  Clock.setMinute(minutes);
  Clock.setSecond(seconds);
}

void checkTimeFor(int onTime, int offTime, int number)
{
  int h = Clock.getHour(h12Flag, pmFlag);
  int m = Clock.getMinute();

  int timeString = h * 100 + m; // if h=12 and m=23 then 12*100 + 23 = 1223 hours

  if (offTime > onTime) // when off timing is greater than on timing
  {
    if ((timeString > onTime) && (timeString < offTime))
    {
      if (number == 1)
      {
        R1Flag = 1;
        RELAY1ON;
        Relay1State = "on";
        autoTimer1 = 0;
      }
      else if (number == 2)
      {
        R2Flag = 1;
        RELAY2ON;
        Relay2State = "on";
        autoTimer2 = 0;
      }
      else if (number == 3)
      {
        R3Flag = 1;
        RELAY3ON;
        Relay3State = "on";
        autoTimer3 = 0;
      }
      else if (number == 4)
      {
        R4Flag = 1;
        RELAY4ON;
        Relay4State = "on";
        autoTimer4 = 0;
      }
    }
    else
    {
      if (number == 1)
      {
        R1Flag = 0;
        RELAY1OFF;
        Relay1State = "off";
      }
      else if (number == 2)
      {
        R2Flag = 0;
        RELAY2OFF;
        Relay2State = "off";
      }
      else if (number == 3)
      {
        R3Flag = 0;
        RELAY3OFF;
        Relay3State = "off";
      }
      else if (number == 4)
      {
        R4Flag = 0;
        RELAY4OFF;
        Relay4State = "off";
      }
    }
  }
  else
  {
    if (((timeString > onTime) && (timeString > offTime)) || ((timeString < onTime) && (timeString < offTime)))
    {
      if (number == 1)
      {
        R1Flag = 1;
        RELAY1ON;
        Relay1State = "on";
      }
      else if (number == 2)
      {
        R2Flag = 1;
        RELAY2ON;
        Relay2State = "on";
      }
      else if (number == 3)
      {
        R3Flag = 1;
        RELAY3ON;
        Relay3State = "on";
      }
      else if (number == 4)
      {
        R4Flag = 1;
        RELAY4ON;
        Relay4State = "on";
      }
    }
    else
    {
      if (number == 1)
      {
        R1Flag = 0;
        RELAY1OFF;
        Relay1State = "off";
      }
      else if (number == 2)
      {
        R2Flag = 0;
        RELAY2OFF;
        Relay2State = "off";
      }
      else if (number == 3)
      {
        R3Flag = 0;
        RELAY3OFF;
        Relay3State = "off";
      }
      else if (number == 4)
      {
        R4Flag = 0;
        RELAY4OFF;
        Relay4State = "off";
      }
    }
  }
}

void relayStatusPrinter()
{
  display.setTextSize(1);
  display.setTextColor(WHITE);
  if (R1Flag)
  {
    display.setCursor(0, 57);
    display.print("R1");
  }
  if (R2Flag)
  {
    display.setCursor(20, 57);
    display.print("R2");
  }

  if (R3Flag)
  {
    display.setCursor(40, 57);
    display.print("R3");
  }

  if (R4Flag)
  {
    display.setCursor(60, 57);
    display.print("R4");
  }
}

void clientPage(WiFiClient client, String currentLine)
{
  while (client.connected() && currentTime - previousTime <= timeoutTime)
  { // loop while the client's connected
    currentTime = millis();
    if (client.available())
    {
      // if there's bytes to read from the client,
      char c = client.read(); // read a byte, then
      Serial.write(c);        // print it out the serial monitor
      header += c;
      if (c == '\n')
      { // if the byte is a newline character
        // if the current line is blank, you got two newline characters in a row.
        // that's the end of the client HTTP request, so send a response:
        if (currentLine.length() == 0)
        {
          // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
          // and a content-type so the client knows what's coming, then a blank line:
          client.println("HTTP/1.1 200 OK");
          client.println("Content-type:text/html");
          client.println("Connection: close");
          client.println();

          // turns the GPIOs on and off
          if (header.indexOf("GET /0/on") >= 0)
            R1Config = 1;
          else if (header.indexOf("GET /0/off") >= 0)
            R1Config = 0;
          else if (header.indexOf("GET /1/on") >= 0)
          {
            Relay1State = "on";
            R1Flag = 1;
            RELAY1ON;
            autoTimer1 = 0;
          }
          else if (header.indexOf("GET /1/off") >= 0)
          {
            Relay1State = "off";
            R1Flag = 0;
            RELAY1OFF;
          }
          if (header.indexOf("GET /2/on") >= 0)
            R2Config = 1;
          else if (header.indexOf("GET /2/off") >= 0)
            R2Config = 0;
          else if (header.indexOf("GET /3/on") >= 0)
          {
            Relay2State = "on";
            R2Flag = 1;
            RELAY2ON;
            autoTimer2 = 0;
          }
          else if (header.indexOf("GET /3/off") >= 0)
          {
            Relay2State = "off";
            R2Flag = 0;
            RELAY2OFF;
          }
          if (header.indexOf("GET /4/on") >= 0)
            R3Config = 1;
          else if (header.indexOf("GET /4/off") >= 0)
            R3Config = 0;
          else if (header.indexOf("GET /5/on") >= 0)
          {
            Relay3State = "on";
            R3Flag = 1;
            RELAY3ON;
            autoTimer3 = 0;
          }
          else if (header.indexOf("GET /5/off") >= 0)
          {
            Relay3State = "off";
            R3Flag = 0;
            RELAY3OFF;
          }
          if (header.indexOf("GET /6/on") >= 0)
            R4Config = 1;
          else if (header.indexOf("GET /6/off") >= 0)
            R4Config = 0;
          else if (header.indexOf("GET /7/on") >= 0)
          {
            // Serial.println("GPIO 4 off");
            Relay4State = "on";
            R4Flag = 1;
            RELAY4ON;
            autoTimer4 = 0;
          }
          else if (header.indexOf("GET /7/off") >= 0)
          {
            // Serial.println("GPIO 4 off");
            Relay4State = "off";
            R4Flag = 0;
            RELAY4OFF;
          }
          else if (header.indexOf("GET /8/on") >= 0)
            OLEDConfig = 1;
          else if (header.indexOf("GET /8/off") >= 0)
            OLEDConfig = 0;
          else if (header.indexOf("GET /9/off") >= 0)
            OLEDStatus = 1;
          else if (header.indexOf("GET /9/on") >= 0)
            OLEDStatus = 0;
          else if (header.indexOf("GET /10/update") >= 0)
            updateTime = 1;
          else if (header.indexOf("GET /11/5") >= 0)
          {
            autoTimer1 = 1;
            lastAutoTimer1 = millis();
            autoTimerDelay1 = 300000;
          }
          else if (header.indexOf("GET /11/10") >= 0)
          {
            autoTimer1 = 1;
            lastAutoTimer1 = millis();
            autoTimerDelay1 = 600000;
          }
          else if (header.indexOf("GET /11/15") >= 0)
          {
            autoTimer1 = 1;
            lastAutoTimer1 = millis();
            autoTimerDelay1 = 900000;
          }
          else if (header.indexOf("GET /11/25") >= 0)
          {
            autoTimer1 = 1;
            lastAutoTimer1 = millis();
            autoTimerDelay1 = 1500000;
          }
          else if (header.indexOf("GET /11/30") >= 0)
          {
            autoTimer1 = 1;
            lastAutoTimer1 = millis();
            autoTimerDelay1 = 1800000;
          }
          else if (header.indexOf("GET /11/45") >= 0)
          {
            autoTimer1 = 1;
            lastAutoTimer1 = millis();
            autoTimerDelay1 = 2700000;
          }
          else if (header.indexOf("GET /12/5") >= 0)
          {
            autoTimer2 = 1;
            lastAutoTimer2 = millis();
            autoTimerDelay2 = 300000;
          }
          else if (header.indexOf("GET /12/10") >= 0)
          {
            autoTimer2 = 1;
            lastAutoTimer2 = millis();
            autoTimerDelay2 = 600000;
          }
          else if (header.indexOf("GET /12/15") >= 0)
          {
            autoTimer2 = 1;
            lastAutoTimer2 = millis();
            autoTimerDelay2 = 900000;
          }
          else if (header.indexOf("GET /12/25") >= 0)
          {
            autoTimer2 = 1;
            lastAutoTimer2 = millis();
            autoTimerDelay2 = 1500000;
          }
          else if (header.indexOf("GET /12/30") >= 0)
          {
            autoTimer2 = 1;
            lastAutoTimer2 = millis();
            autoTimerDelay2 = 1800000;
          }
          else if (header.indexOf("GET /12/45") >= 0)
          {
            autoTimer2 = 1;
            lastAutoTimer2 = millis();
            autoTimerDelay2 = 2700000;
          }
          else if (header.indexOf("GET /13/5") >= 0)
          {
            autoTimer3 = 1;
            lastAutoTimer3 = millis();
            autoTimerDelay3 = 300000;
          }
          else if (header.indexOf("GET /13/10") >= 0)
          {
            autoTimer3 = 1;
            lastAutoTimer3 = millis();
            autoTimerDelay3 = 600000;
          }
          else if (header.indexOf("GET /13/15") >= 0)
          {
            autoTimer3 = 1;
            lastAutoTimer3 = millis();
            autoTimerDelay3 = 900000;
          }
          else if (header.indexOf("GET /13/25") >= 0)
          {
            autoTimer3 = 1;
            lastAutoTimer3 = millis();
            autoTimerDelay3 = 1500000;
          }
          else if (header.indexOf("GET /13/30") >= 0)
          {
            autoTimer3 = 1;
            lastAutoTimer3 = millis();
            autoTimerDelay3 = 1800000;
          }
          else if (header.indexOf("GET /13/45") >= 0)
          {
            autoTimer3 = 1;
            lastAutoTimer3 = millis();
            autoTimerDelay3 = 2700000;
          }
          else if (header.indexOf("GET /14/5") >= 0)
          {
            autoTimer4 = 1;
            lastAutoTimer4 = millis();
            autoTimerDelay4 = 300000;
          }
          else if (header.indexOf("GET /14/10") >= 0)
          {
            autoTimer4 = 1;
            lastAutoTimer4 = millis();
            autoTimerDelay4 = 600000;
          }
          else if (header.indexOf("GET /14/15") >= 0)
          {
            autoTimer4 = 1;
            lastAutoTimer4 = millis();
            autoTimerDelay4 = 900000;
          }
          else if (header.indexOf("GET /14/25") >= 0)
          {
            autoTimer4 = 1;
            lastAutoTimer4 = millis();
            autoTimerDelay4 = 1500000;
          }
          else if (header.indexOf("GET /14/30") >= 0)
          {
            autoTimer4 = 1;
            lastAutoTimer4 = millis();
            autoTimerDelay4 = 1800000;
          }
          else if (header.indexOf("GET /14/45") >= 0)
          {
            autoTimer4 = 1;
            lastAutoTimer4 = millis();
            autoTimerDelay4 = 2700000;
          }

          // Display the HTML web page
          client.print("<!DOCTYPE html><html lang=\"en\">");
          client.print("<head><meta charset=\"UTF-8\"><meta http-equiv=\"X-UA-Compatible\" content=\"IE=edge\"><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">");
          client.print("<link rel=\"icon\" type=\"image/png\" href=\"favicon.png\">");
          client.print("<link href=\"https://cdn.jsdelivr.net/npm/bootstrap@5.0.2/dist/css/bootstrap.min.css\" rel=\"stylesheet\" integrity=\"sha384-EVSTQN3/azprG1Anm3QDgpJLIm9Nao0Yz1ztcQTwFspd3yD65VohhpuuCOmLASjC\" crossorigin=\"anonymous\"/>");
          // CSS to style the on/off buttons
          // Feel free to change the background-color and font-size attributes to fit your preferences
          client.print("<title>Aqua Control</title><style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}");
          client.print("a {text-decoration: none; font-size: 30px;}");
          client.print("</style></head>");

          // Web Page Heading
          client.print("<body><div class=\"container\"><div class=\"row\"><h1 class=\"display-4 pb-3\"><u>Aquarium Web Server</u></h1>");

          // Display current state, and ON/OFF buttons for Relay 1
          if (autoTimer1 == 1)
            client.print("<div class=\"col-6\"><p class=\"text-white bg-dark pl-1 pr-1 p-1\">PowerHead - " + Relay1State + " &nbsp;&nbsp;<span class=\"badge alert-success\">Timer Active</span></p><div class=\"row\"><div class=\"col\">");
          else
            client.print("<div class=\"col-6\"><p class=\"text-white bg-dark pl-1 pr-1 p-1\">PowerHead - " + Relay1State + "</p><div class=\"row\"><div class=\"col\">");
          // If the AQUA LIGHT is off, it displays the ON button
          if (R1Config == 0)
          {
            client.print("<p><a href=\"/0/on\"><button type=\"button\" class=\"btn btn-outline-danger\">Manual</button></a></p></div>");
            if (Relay1State == "off")
              client.print("<div class=\"col\"><p><a href=\"/1/on\"><button type=\"button\" class=\"btn btn-outline-danger\">OFF</button></a></p></div><div class=\"col\"><select class=\"form-select form-select-sm\" aria-label=\".form-select-sm example\" onchange=myfunc(this,\"11\");><option value=\"0\" selected>Select Time</option><option value=\"5\">5 min</option><option value=\"10\">10 min</option><option value=\"15\">15 min</option><option value=\"25\">25 min</option><option value=\"30\">30 min</option><option value=\"45\">45 min</option></select><p><a href=\"\"><button type=\"button\" class=\"btn btn-outline-danger\" disabled>Choose time</button></a></p></div></div></div>");
            else
              client.print("<div class=\"col\"><p><a href=\"/1/off\"><button type=\"button\" class=\"btn btn-outline-success\">ON</button></a></p></div></div></div>");
          }
          else
            client.print("<p><a href=\"/0/off\"><button type=\"button\" class=\"btn btn-outline-success\">Auto</button></a></p></div></div></div>");

          // Display current state, and ON/OFF buttons for Relay 2
          if (autoTimer2 == 1)
            client.print("<div class=\"col-6\"><p class=\"text-white bg-dark pl-1 pr-1 p-1\">Skimmer - " + Relay2State + " &nbsp;&nbsp;<span class=\"badge alert-success\">Timer Active</span></p><div class=\"row\"><div class=\"col\">");
          else
            client.print("<div class=\"col-6\"><p class=\"text-white bg-dark pl-1 pr-1 p-1\">Skimmer - " + Relay2State + "</p><div class=\"row\"><div class=\"col\">");
          // If the output4State is off, it displays the ON button
          if (R2Config == 0)
          {
            client.print("<p><a href=\"/2/on\"><button type=\"button\" class=\"btn btn-outline-danger\">Manual</button></a></p></div>");
            if (Relay2State == "off")
              client.print("<div class=\"col\"><p><a href=\"/3/on\"><button type=\"button\" class=\"btn btn-outline-danger\">OFF</button></a></p></div><div class=\"col\"><select class=\"form-select form-select-sm\" aria-label=\".form-select-sm example\" onchange=myfunc(this,\"12\");><option value=\"0\" selected>Select Time</option><option value=\"5\">5 min</option><option value=\"10\">10 min</option><option value=\"15\">15 min</option><option value=\"25\">25 min</option><option value=\"30\">30 min</option><option value=\"45\">45 min</option></select><p><a href=\"\"><button type=\"button\" class=\"btn btn-outline-danger\" disabled>Choose time</button></a></p></div></div></div>");
            else
              client.print("<div class=\"col\"><p><a href=\"/3/off\"><button type=\"button\" class=\"btn btn-outline-success\">ON</button></a></p></div></div></div>");
          }
          else
            client.print("<p><a href=\"/2/off\"><button type=\"button\" class=\"btn btn-outline-success\">Auto</button></a></p></div></div></div>");

          // Display current state, and ON/OFF buttons for Relay3
          if (autoTimer3 == 1)
            client.print("<div class=\"col-6\"><p class=\"text-white bg-dark pl-1 pr-1 p-1\">Filter - " + Relay3State + " &nbsp;&nbsp;<span class=\"badge alert-success\">Timer Active</span></p><div class=\"row\"><div class=\"col\">");
          else
            client.print("<div class=\"col-6\"><p class=\"text-white bg-dark pl-1 pr-1 p-1\">Filter - " + Relay3State + "</p><div class=\"row\"><div class=\"col\">");
          // If the output4State is off, it displays the ON button
          if (R3Config == 0)
          {
            client.print("<p><a href=\"/4/on\"><button type=\"button\" class=\"btn btn-outline-danger\">Manual</button></a></p></div>");
            if (Relay3State == "off")
              client.print("<div class=\"col\"><p><a href=\"/5/on\"><button type=\"button\" class=\"btn btn-outline-danger\">OFF</button></a></p></div><div class=\"col\"><select class=\"form-select form-select-sm\" aria-label=\".form-select-sm example\" onchange=myfunc(this,\"13\");><option value=\"0\" selected>Select Time</option><option value=\"5\">5 min</option><option value=\"10\">10 min</option><option value=\"15\">15 min</option><option value=\"25\">25 min</option><option value=\"30\">30 min</option><option value=\"45\">45 min</option></select><p><a href=\"\"><button type=\"button\" class=\"btn btn-outline-danger\" disabled>Choose time</button></a></p></div></div></div>");
            else
              client.print("<div class=\"col\"><p><a href=\"/5/off\"><button type=\"button\" class=\"btn btn-outline-success\">ON</button></a></p></div></div></div>");
          }
          else
            client.print("<p><a href=\"/4/off\"><button type=\"button\" class=\"btn btn-outline-success\">Auto</button></a></p></div></div></div>");

          // Display current state, and ON/OFF buttons for Relay 4
          if (autoTimer4 == 1)
            client.print("<div class=\"col-6\"><p class=\"text-white bg-dark pl-1 pr-1 p-1\">Light - " + Relay4State + " &nbsp;&nbsp;<span class=\"badge alert-success\">Timer Active</span></p><div class=\"row\"><div class=\"col\">");
          else
            client.print("<div class=\"col-6\"><p class=\"text-white bg-dark pl-1 pr-1 p-1\">Light - " + Relay4State + "</p><div class=\"row\"><div class=\"col\">");
          // If the output4State is off, it displays the ON button
          if (R4Config == 0)
          {
            client.print("<p><a href=\"/6/on\"><button type=\"button\" class=\"btn btn-outline-danger\">Manual</button></a></p></div>");
            if (Relay4State == "off")
              client.print("<div class=\"col\"><p><a href=\"/7/on\"><button type=\"button\" class=\"btn btn-outline-danger\">OFF</button></a></p></div><div class=\"col\"><select class=\"form-select form-select-sm\" aria-label=\".form-select-sm example\" onchange=myfunc(this,\"14\");><option value=\"0\" selected>Select Time</option><option value=\"5\">5 min</option><option value=\"10\">10 min</option><option value=\"15\">15 min</option><option value=\"25\">25 min</option><option value=\"30\">30 min</option><option value=\"45\">45 min</option></select><p><a href=\"\"><button type=\"button\" class=\"btn btn-outline-danger\" disabled>Choose time</button></a></p></div></div></div>");
            else
              client.print("<div class=\"col\"><p><a href=\"/7/off\"><button type=\"button\" class=\"btn btn-outline-success\">ON</button></a></p></div></div></div>");
          }
          else
            client.print("<p><a href=\"/6/off\"><button type=\"button\" class=\"btn btn-outline-success\">Auto</button></a></p></div></div></div>");

          client.print("<div class=\"col-6\"><p class=\"text-white bg-dark pl-1 pr-1 p-1\">Temperature</p><p class=\"bg-info\">" + String(Clock.getTemperature()) + " &deg;C</p></div>");
          client.print("<div class=\"col-6\"><p class=\"text-white bg-dark pl-1 pr-1 p-1\">Time</p><p class=\"bg-info\">" + String(Clock.getHour(h12Flag, pmFlag)) + ":" + String(Clock.getMinute()) + ":" + String(Clock.getSecond()) + "</p></div>");
          client.print("<div class=\"col-6\"><p class=\"text-white bg-dark pl-1 pr-1 p-1\">OLED Display</p><div class=\"row\"><div class=\"col\">");
          if (OLEDConfig == 0)
          {
            client.print("<p><a href=\"/8/on\"><button type=\"button\" class=\"btn btn-outline-danger\">Manual</button></a></p></div>");
            if (OLEDStatus == 0)
              client.print("<div class=\"col\"><p><a href=\"/9/off\"><button type=\"button\" class=\"btn btn-outline-danger\">OFF</button></a></p></div></div></div>");
            else
              client.print("<div class=\"col\"><p><a href=\"/9/on\"><button type=\"button\" class=\"btn btn-outline-success\">ON</button></a></p></div></div></div>");
          }
          else
            client.print("<p><a href=\"/8/off\"><button type=\"button\" class=\"btn btn-outline-success\">Auto</button></a></p></div><div class=\"col\"><p><a href=\"\"><button type=\"button\" class=\"btn btn-outline-danger\" disabled>DISABLED</button></a></p></div></div></div>");
          client.print("<div class=\"col-6\"><p class=\"text-white bg-dark pl-1 pr-1 p-1\">Update Time</p><div class=\"row\"><div class=\"col\"><p><a href=\"/10/update\"><button type=\"button\" class=\"btn btn-outline-success\">Update</button></a> </p></div></div></div></div></div>");
          client.print("<script src=\"https://cdn.jsdelivr.net/npm/bootstrap@5.0.2/dist/js/bootstrap.bundle.min.js\" integrity=\"sha384-MrcW6ZMFYlzcLA8Nl+NtUVF0sA7MsXsP1UyJoMp4YLEuNSfAP+JcXn/tWtIaxVXM\" crossorigin=\"anonymous\"></script>");
          client.print("<script>function myfunc(x, y) {time = x.value;newString = \"Auto start in \" + time + \" mins\";nextSibling = x.nextElementSibling;button = nextSibling.lastElementChild.lastElementChild;newHref = nextSibling.lastElementChild;if (time == '0') {button.innerHTML = \"Choose time\";button.disabled = true;newHref.href = \"\";}else {button.disabled = false; button.innerHTML = newString; newHref.href = '/' + y + '/' + time;} }</script>");
          client.print("</body></html>");

          // The HTTP response ends with another blank line
          client.println();
          // Break out of the while loop
          break;
        }
        else
        { // if you got a newline, then clear currentLine
          currentLine = "";
        }
        // displays the page to client
      }
      else if (c != '\r')
      {                   // if you got anything else but a carriage return character,
        currentLine += c; // add it to the end of the currentLine
      }
    }
  }
}
