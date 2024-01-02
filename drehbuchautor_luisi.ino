// for peter luisi
#include <FastLED.h>

#define LED_PIN 2
#define NUM_LEDS 48
#define LED_TYPE WS2812B
#define COLOR_ORDER GRB  // If colors are wrong, use GRB / BGR / RGB (2801). \
                         // APA102=BGR, WS2812/Neopixel=GRB
CRGB leds[NUM_LEDS];

bool SINUS = false;      // If defined, makes sinus waves, read brightness poti
bool BLINKTEST = false;  // Cycles between min / max brightness every 2.5s, read brightness poti
bool CALIBRATE = false;  // Display color sent via serial command `255,255,255`, read brightness poti

// Colors
CRGB colorLowerLeds = CRGB(30,255,100);  // MAIN COLORS! bizli meh cyan
CRGB colorUpperLeds = CRGB(20,222,255);  // MAIN COLORS! babyblau
CRGB thungsten = CRGB(255,120,2);  // pseudo warm
CRGB white = CRGB(255,255,255);
CRGB warmWhite = CRGB(255,130,20);
CRGB black = CRGB(0,0,0);
CRGB red = CRGB(255,0,0);
CRGB green = CRGB(0,255,0);
CRGB yellow = CRGB(255,255,0);
CRGB blue = CRGB(0,0,255);

// Define the button pins
int breakpointPin = A1;
int brightnessPin = A2;
int audioInPinL = A5;       // only LEFt channel
int audioInPinR = A6;       // both right and Left channel somehow
const int calibrateBtn = 4; // push btn

int breakpoint = 15;
// Hysteresis threshold for volume level
int hysteresis = 8; // Adjust this value to create a buffer around the breakpoint; was 10 which kinda worked

int baselight = 20;  // Brightness when waiting between text
int baselightInitial = baselight;

float incrementAngle = 0.396;  // 0.66 Speed of increment when fastest
float decrementAngle = 0.465;  // incrementAngle//3.141 Speed of decrement

bool isIncreasing = false; // Start with increasing mode
float value = 0;           // The sinusoidal value AND the main value to send to the FASTLEDs
float smoothedOutputBrightness = 0;  // smoothed value that is sent to the FASTLEDs
float angle = 0;           // Angle for the waves
int lastState = false;     // Last read state of the button
int noise = 8;             // Threshold of vloume, if lower than this noise is not triggering the LEDs
int brightness = 0;        // Value read from the potentiometer
float smoothedBrightness = 0; // Variable to store the smoothed brightness of potentiometer
const float alpha = 0.1;   // Smoothing factor - lower is smoother for analogRead Potis

int serialRed = 0;
int serialGreen = 0;
int serialBlue = 0;

unsigned long sinusStartTime = 0; 
unsigned long sinusCurrentTime = 0;

void resetColors() {
  // UNTEN
  for (int x = 0; x < 24; x++) {
    // QUASI GUT :
    leds[x] = colorLowerLeds;  // Set LED color and brightness
    if(x % 2 == 0) leds[x] = black;  // Only every second LED is on
    //leds[x] = CRGB(15,125,50);  // Set LED color and brightness
  }
  // OBEN
  for (int i = 24; i < NUM_LEDS; i++) {
    // QUASI GUT leds[i] = colorUpperLeds;  // Set LED color and brightness
    leds[i] = colorUpperLeds;  // Set LED color and brightness
    //leds[i] = CRGB(10,111,125);  // Set LED color and brightness
  }

  // Show all leds
  FastLED.show();
}

void setup() {
  Serial.begin(38400);
  // Initialize the button pin as an input with internal pull-up
  pinMode(calibrateBtn, INPUT_PULLUP);
  pinMode(brightnessPin, INPUT);
  pinMode(breakpointPin, INPUT);
  pinMode(audioInPinL, INPUT);
  pinMode(audioInPinR, INPUT);

  // LED TYPE WS2812
  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);

  smoothedBrightness = analogRead(brightnessPin); // Initialize with the first reading
  // analogRead(breakpointPin) = 695 was best for attack (?)

  // Enter SINUS mode for calibration
  if(!digitalRead(calibrateBtn)) {
    Serial.println("Button pressed during startup");
    SINUS = true;
  }
  long btnPress = millis();
  while(!digitalRead(calibrateBtn)) {
    Serial.println("wait for btn release");
    fill_solid(leds, NUM_LEDS, red);
    FastLED.show();
    delay(50);
  }

  // Set standard colors
  resetColors();

  // reset baselight to not be off - waits for first audio fragment
  baselight = 0;
}

void loop() {
  if(BLINKTEST) {
    loopBLINKTEST();

  } else if(CALIBRATE) {
    catchSerialComs();
    fill_solid(leds, NUM_LEDS, warmWhite);

    int brightness = map(analogRead(brightnessPin), 0,1023, 255,0);
    FastLED.setBrightness(brightness);
    FastLED.show();
    Serial.print(serialRed);
    Serial.print("\t");
    Serial.print(serialGreen);
    Serial.print("\t");
    Serial.print(serialBlue);
    Serial.println("");
    delay(10);
  
  } else if(SINUS) {
    loopSinus();

  } else {
    loopMain();
  }
}


void loopMain() {
    // Read the current state of the button and volume
  int currentButtonState = digitalRead(calibrateBtn);
  int rawBrightness = analogRead(brightnessPin);
  breakpoint = map(analogRead(breakpointPin), 0,1024, 50,noise+1);  // => breakpoint 19 is TESTED and good
  noise = breakpoint / 5;  // 10 was good but flickering sometimes
  int left = analogRead(audioInPinL);
  int right = analogRead(audioInPinR);
  
  // Smooth the readings
  smoothedBrightness = alpha * rawBrightness + (1 - alpha) * smoothedBrightness;
  brightness = map((int)smoothedBrightness, 0,1023, 255,0);

  // Reset base light to zero when btn is pressed
  if(!digitalRead(calibrateBtn)) {
    baselight = 0;
    while(!digitalRead(calibrateBtn)) {
      Serial.println("wait for btn release");
      fill_solid(leds, NUM_LEDS, green);
      FastLED.show();
      delay(666);
      resetColors();
      delay(250);
    }
  }
  
  int loudest = max(left, right);                                          // pick the loudest channel
  int volume = map(loudest, 0,breakpoint*2, 0, 255);                       // remap to volume range
  volume = constrain(volume, noise < baselight ? baselight : noise, 255);  // ensure volume is within 0-255

  hysteresis = breakpoint / 10;  /// wtf chatGPT is this?

  // Hysteresis logic to prevent re-triggering
  if (isIncreasing && volume < (breakpoint - hysteresis)) {
    isIncreasing = false;
    angle = acos(sqrt(value / 255));
    
  } else if (!isIncreasing && volume > (breakpoint + hysteresis)) {
    isIncreasing = true;
    angle = asin(sqrt(value / 255));
  }

  if (isIncreasing) {
    angle += incrementAngle;
    if (angle > PI / 2) {
      angle = PI / 2;
      value = 255;
      // ??? FIXME: does not zero out if pressed btn
      // MAYBE  0,brightness)?
      baselight = map(baselightInitial, 0,255, baselightInitial,brightness);
    } else {
      value = (pow(sin(angle), 2) * (255 - baselight)) + baselight; // Adjusted to range from baselight to 255
    }
  } else {
    angle += decrementAngle;
    if (angle > PI / 2) {
      angle = PI / 2;
      // ??? FIXME: does not zero out if pressed btn
      // MAYBE  0,brightness)?
      // if(baselight > 0)
      value = map(baselight, 0,255, baselightInitial,brightness); // Lower limit adjusted to baselight
    } else {
      value = (255 - (pow(sin(angle), 2) * (255 - baselight))) + baselight; // Adjusted to range from 255 to baselight
    }
  }

  // Output value smoothing
  smoothedOutputBrightness = .25 * value + (1 - .25) * smoothedOutputBrightness;

  // Apply the smoothed value to the brightness
  FastLED.setBrightness(map(smoothedOutputBrightness, 0,255, 0,brightness));
  FastLED.show();

  // Print the values for debugging
  Serial.print("isIncreasing:");
  Serial.print(isIncreasing ? 255 : 0);
  Serial.print("\tloudest:");
  Serial.print(loudest);
  Serial.print("\tsmoothedOutputBrightness:");
  Serial.print((int)smoothedOutputBrightness);
  Serial.print("\tsmoothedOutputBrightness - brightness:");
  Serial.print(map(smoothedOutputBrightness, 0,255, 0,brightness));
  Serial.print("\tbrightness:");
  Serial.print(brightness);
  Serial.print("\tbreakpoint:");
  Serial.print(breakpoint);
  Serial.print("\tx:");
  Serial.print(300);
  Serial.print("\tz:");
  Serial.print(0);
  Serial.println();
  delay(10); // Short delay to stabilize the output
}


void catchSerialComs() {
  while (Serial.available() > 0) {
    // Awaits a string like "255,111,23"
    // look for the next valid integer in the incoming serial stream:
    serialRed = Serial.parseInt();
    // do it again:
    serialGreen = Serial.parseInt();
    // do it again:
    serialBlue = Serial.parseInt();
    if (Serial.read() == '\n') {
      warmWhite = CRGB(serialRed, serialGreen, serialBlue);
    }
  }
}

void loopBLINKTEST() {
  int brightness = map(analogRead(brightnessPin), 0,1023, 255,0);
  int max = brightness;
  int min = max;  // /3.1875;

  for (int x = 0; x < 24; x++) {
    leds[x] = colorLowerLeds;  // Set LED color and brightness
    if(x % 2 == 0) leds[x] = black;  // Only every other
  }
  // OBEN
  for (int i = 24; i < NUM_LEDS; i++) {
    leds[i] = colorUpperLeds;  // Set LED color and brightness
  }
  FastLED.setBrightness(min);
  FastLED.show();
  delay(2500);

  FastLED.setBrightness(255);
  FastLED.show();

  Serial.print("\tmin:");
  Serial.print(min);
  Serial.print("\tmax:");
  Serial.print(max);
  Serial.println();
  delay(2500);
}

// SINUSOIDAL WAVE
void loopSinus() {
  sinusCurrentTime += 10; // Increment current time
  
  // Define the period of the sine wave in milliseconds
  unsigned long period = 7500;

  // Calculate the current phase of the sine wave, given by the current time modulo the period
  float phase = (sinusCurrentTime - sinusStartTime) * TWO_PI / period;
  
  // Calculate the sine of the phase, scale it to the range 0-1, and then scale to 0-255
  float value = (sin(phase) + 1) * 127.5;

  // Apply a longer delay when the value is at the extremes (0 or 255)
  // if(ceil(value) >= 254 || floor(value)  == 0) {
  if(ceil(value) >= 254) {
    delay(25); // Longer delay at the peak
  }

  baselight = baselightInitial;  // reset because setup();

  int brightness = map(analogRead(brightnessPin), 0,1023, 0,255);
  value = map(value, 0,255, baselight,255-brightness);

  // Set brightnes lower for lower LED ring
  // UNTEN
  for (int x = 0; x < 24; x++) {
    // QUASI GUT :
    leds[x] = colorLowerLeds;  // Set LED color and brightness
    if(x % 2 == 0) leds[x] = black;
  }
  // OBEN
  for (int i = 24; i < NUM_LEDS; i++) {
    leds[i] = colorUpperLeds;  // Set LED color and brightness
  }

  // Use the value for LED brightness
  Serial.print(value);
  Serial.print(" ceil:  ");
  Serial.print(ceil(value));
  Serial.print(" floor: ");
  Serial.println(floor(value));

  FastLED.setBrightness(value);
  FastLED.show();

  delay(10); // Short delay for the rest of the cycle
}
