#include <Adafruit_NeoPixel.h>

Adafruit_NeoPixel strip(36, 22, NEO_GRB + NEO_KHZ800);

void setup() {
  Serial.begin(115200);
  strip.begin();
  strip.setBrightness(90);
}

void loop() {
  if (Serial.peek() == 255) {
    Serial.read();
    strip.clear();
    strip.show();
    return;
  }

  if (Serial.available() >= 2) {
    int cmd = Serial.read(), idx = Serial.read();
    if (idx >= strip.numPixels()) return;
    if (cmd & 3) strip.setPixelColor(idx, strip.Color(0, cmd & 1 ? 255 : 0, cmd & 2 ? 255 : 0));
    else if (cmd & 12) strip.setPixelColor(idx, strip.Color(255, cmd & 4 ? 255 : 0, cmd & 8 ? 255 : 0));
    else strip.setPixelColor(idx, strip.Color(0, 0, 0));
    strip.show();
  }
}
