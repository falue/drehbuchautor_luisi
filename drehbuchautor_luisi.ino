// Hello
// makes LEDs dance with audio input thingy
#include <EEPROM.h>

// for peter luisi
#include <FastLED.h>

#define LED_PIN 2
#define NUM_LEDS 48
#define BRIGHTNESS 255
#define LED_TYPE WS2812B
#define COLOR_ORDER GRB  // If colors are wrong, use GRB / BGR / RGB (2801). \
                         // APA102=BGR, WS2812/Neopixel=GRB

// FOR TGHIS WORKS BITCHES
float ALPHA = 0.05;  // Smoothing factor. Bigger = less smoothing

const float ALPHA_FAST = 0.15;   // 0.3..0.85 // Higher alpha for faster response.
float ALPHA_SLOW = 0.2;  // 0.1...0.015   0.3  0.5=very jittery but responsive // Lower alpha for more smoothing. Higher frequency of peaks is smoothed = fast curves destroyer
// Variables to store the two-stage EMA values
float emaL_fast = 0, emaL_slow = 0;
float emaR_fast = 0, emaR_slow = 0;

int brightnessPin = A2;
int angryPin = A1;
int audioInPinL = A5;  // only LEFt channel
int audioInPinR = A6;  // both right and Left channel??
int calibrateBtn = 4;

int brightness = 0;
int angryness = 255;
CRGB color = CRGB(255, 255, 255);
CRGB leds[NUM_LEDS];

float ema = 0;  // Initial EMA value: "Exponential Moving Average"
float emaL = 0;  // Initial EMA value: "Exponential Moving Average"
float emaR = 0;  // Initial EMA value: "Exponential Moving Average"

int redScalingFalloff = 66;  // if dimming is below that, the red is disproportionally reduced due to red being stronger as other colors
float redShift = 0.75;  // factor with which the red gets shifted. smaller = more blue, approaching 1 = more red

int maxVol = 0;  // 100 may be good

int noise = 5;   // 5?                 // cut off base level noise
const int numMeasurements = 16;  // bigger the number, the softer the curve/"afterglow"
float currGlow = 0;
const float afterglow = .99;
int rollingAverages[numMeasurements] = { 0 };  // 1D array for rolling averages
int rollingAverage = 0;
const int minBrightness = 0;

// the setup routine runs once when you press reset:
void setup() {
  delay(2000);  // wait for flashing
  // initialize serial communication at 9600 bits per second:
  Serial.begin(230400);  // Makes println go prrr

  pinMode(audioInPinL, INPUT);
  pinMode(audioInPinR, INPUT);
  pinMode(brightnessPin, INPUT);
  pinMode(angryPin, INPUT);
  pinMode(calibrateBtn, INPUT_PULLUP);

  // LED TYPE WS2812
  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);

  // Show all leds
  fill_solid(leds, NUM_LEDS, color);
  FastLED.show();
  delay(500);
  fill_solid(leds, NUM_LEDS, CRGB(255,255,0));
  FastLED.show();

  // loop for 2s - if btn is pressed, do calibrate()
  // while(true && digitalRead(calibrateBtn)) {
    //   if(!digitalRead(calibrateBtn)) calibrate();
  // }

  unsigned long startTime = millis();
  bool calibrated = false;
  EEPROM.get(0, maxVol);
  brightness = map(analogRead(brightnessPin), 0,1024, 0,255);

  while (millis() - startTime < 3000) { // 2000 milliseconds = 2 seconds
    if (!digitalRead(calibrateBtn)) {
      while(!digitalRead(calibrateBtn)) {
        Serial.println("wait for btn release");
        delay(50);
      }
      Serial.println("Start calibration");
      calibrateMaxVol();
      calibrated = true;
      break; // Exit the loop after calibrating
    }
  }
  if (!calibrated) {
    Serial.println("Calibration skipped");
  }

  Serial.println("*********************************");
  Serial.println("Startup complete.");
  Serial.println("*********************************");
}

void calibrateBrightness() {
  // Calibrate dimming here a thing for 6s
  Serial.println("Set brightness");
  while(digitalRead(calibrateBtn)) {
    brightness = map(analogRead(brightnessPin), 0,1024, 0,255);
    brightness = brightness / 3 * 3;  // Remove jitter
    ///Serial.print("Brightness: ");
    ///Serial.println(brightness);
    fill_solid(leds, NUM_LEDS, CRGB(255-brightness, 255-brightness, 255-brightness));
    // Display lower LEDs blue
    for (int i = 0; i < 24; i++) {
      leds[i] = CRGB(0, 0, 255);  // Set LED color and brightness
    }
    FastLED.show();
    delay(1);
  }

  Serial.print("brightness set to ");
  Serial.println(brightness);

  // Blink four times
  blink(4, 400);
}

void calibrateMaxVol() {
  // Set maxVol
  fill_solid(leds, NUM_LEDS, color);
  FastLED.show();
  // READ LAST VALUE FROM internal EEPROM
  int currVol = 0;
  maxVol = 0;
  
  // TODO: how to lower the maxVol without having to re-calibrate
  Serial.println("Set max volume. Scream as loud as you will now.");
  while(digitalRead(calibrateBtn)) {
    // if (!digitalRead(calibrateBtn)) {
    //   maxVol = 0;
    //   fill_solid(leds, NUM_LEDS, CRGB(0, 255, 255));
    //   FastLED.show();
    //   delay(500);
    // }
    currVol = (analogRead(audioInPinL) + analogRead(audioInPinR)) / 2;

    if(currVol > maxVol) maxVol = currVol;
    // maxVol = currVol;
    int tempColor = map(currVol, 0,100, 0, 255);
    // Fill upper ring according to cu
    fill_solid(leds, NUM_LEDS, CRGB(tempColor, tempColor, tempColor));
    // Display lower LEDs red
    for (int i = 0; i < 24; i++) {
      leds[i] = CRGB(255, 0, 0);  // Set LED color and brightness
    }
    FastLED.show();

    ///Serial.print("currVol: ");
    ///Serial.print(currVol);
    ///Serial.print("\tmaxVol: ");
    ///Serial.print(maxVol);
    ///Serial.println();
    delay(1);
  }

  // if vol was not succesful, fill in arbitrary value
  if(maxVol < 25) maxVol  = 100;
  // Save maxVol to memory
  Serial.print("Set maxVol to ");
  Serial.println(maxVol);
  EEPROM.put(0, maxVol);

  // Blink four times
  blink(4, 400);
}

void loop() {
  if (!digitalRead(calibrateBtn)) {
    while(!digitalRead(calibrateBtn)) {
      Serial.println("wait for btn release");
      delay(50);
    }
    Serial.println("Start calibration");
    calibrateBrightness();
  }


  int left = analogRead(audioInPinL);
  int right = analogRead(audioInPinR);  // - left ??  is both L+R

  // if(left > maxVol) maxVol = left;
  // if(right > maxVol) maxVol = right;

  // if(left > tempMax) tempMax = left;
  // if(right > tempMax) tempMax = right;

  // Scale input to 0-255
  left = map(left, 0, maxVol, 0,255);
  right = map(right, 0, maxVol, 0,255);

  // Remove everything that may be static noise
  if(left < noise ) left = 0;
  if(right < noise ) right = 0;

  // Apply the first stage of EMA (faster)
  emaL_fast = (left * ALPHA_FAST) + (emaL_fast * (1 - ALPHA_FAST));
  emaR_fast = (right * ALPHA_FAST) + (emaR_fast * (1 - ALPHA_FAST));

  // Read smoothness form angry potentiometer from 0.01 to 0.666
  ALPHA_SLOW = map(analogRead(angryPin), 0,1024, 10,666)/1000.0;

  // Apply the second stage of EMA (slower)
  emaL_slow = (emaL_fast * ALPHA_SLOW) + (emaL_slow * (1 - ALPHA_SLOW));
  emaR_slow = (emaR_fast * ALPHA_SLOW) + (emaR_slow * (1 - ALPHA_SLOW));



  // Set colors of upper LED ring
  // Left is solo channel somehow
  for (int i = 0; i < 24; i++) {
    int redValueL = emaL_slow;
    if (emaL_slow < redScalingFalloff) {
        redValueL = int(emaL_slow * redShift);
    }
    leds[i] = CRGB(redValueL, emaL_slow, emaL_slow);  // Set LED color and brightness
  }

  // Set colors of lower LED ring
  // Right is left & right channel combined somehow
  for (int x = 24; x < NUM_LEDS; x++) {
    int redValueR = emaR_slow;
    if (emaR_slow < redScalingFalloff) {
        redValueR = int(emaR_slow * redShift);
    }
    leds[x] = CRGB(redValueR, emaR_slow, emaR_slow);  // Set LED color and brightness
  }

  // Adjsut brightness according to potentiometer
  // TODO should not contribute to emaL/R. only for colors, but don't go beneath 0
  brightness = map(analogRead(brightnessPin), 0,1024, 0,255);
  // left = left - brightness;
  // right = right - brightness;

  // Do not go under 0 or over 255
  // left = clamp(left, 0, 254);
  // right = clamp(right, 0, 254);
  FastLED.setBrightness(255 - brightness);
  FastLED.show();

  if(false) {  // ignore for now
    Serial.print("\temaL_slow:");
    Serial.print(emaL_slow);
    Serial.print("\temaR_slow:");
    Serial.print(emaR_slow + 255);
    Serial.print("\tbrightness:");
    Serial.print(brightness);
    Serial.print("\tALPHA_SLOW:");
    Serial.print(ALPHA_SLOW);
    Serial.print("\tfake3:");
    Serial.print(555);
    Serial.println();

    Serial.print("L:");
    Serial.print(left);
    Serial.print("\tR:");
    Serial.print(right + 255);

    Serial.print("\temaL_slow:");
    Serial.print(emaL_slow);
    Serial.print("\temaR_slow:");
    Serial.print(emaR_slow + 255);
    Serial.print("\tALPHA_SLOW:");
    Serial.print(ALPHA_SLOW);

    Serial.print("\tfake:");
    Serial.print(255);
    Serial.print("\tfake3:");
    Serial.print(555);
    Serial.print("\tfake2:");
    Serial.print(0);
    Serial.print("\tmaxVolume:");
    Serial.print(maxVol);
    Serial.println();
  }

}

void blink(int amount, int duration) {
  for (int i = 0; i < amount; i++) {
    Serial.print("blink ");
    fill_solid(leds, NUM_LEDS, CRGB(0, 0, 0));
    FastLED.show();
    delay(duration/3*2);
    fill_solid(leds, NUM_LEDS, CRGB(0, 255-brightness, 0));
    FastLED.show();
    delay(duration);
  }
  Serial.println();
}

int clamp(int value, int min, int max) {
  if (value < min) return min;
  if (value > max) return max;
  return value;
}

float mapfloat(long x, long in_min, long in_max, long out_min, long out_max) {
  return (float)(x - in_min) * (out_max - out_min) / (float)(in_max - in_min) + out_min;
}

