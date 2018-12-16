// LED Lamp for Lightning Talks (https://en.wikipedia.org/wiki/Lightning_talk)
// Works with 4 LED Stripes with 56 LEDs each at a fadecandy USB controller
// Green = 60% time up, Yellow = 80% time up, Red = 100% time up

OPC opc;

void setup()
{
  opc = new OPC(this, "127.0.0.1", 7890);
  frameRate(10);
  colorMode(RGB, 100);
}

void draw()
{
  // Whole countdown time in milliseconds (default: 300000 = 5 minutes)
  float timer = 420000;
  // Amount of LEDs turned on
  float ledon;

  // Turn all LEDs off
  for (int i = 0; i < 56; i++) {
    opc.setPixel(i, color(0, 0, 0));
    opc.setPixel(i+64, color(0, 0, 0));
    opc.setPixel(i+128, color(0, 0, 0));
    opc.setPixel(i+192, color(0, 0, 0));
  }
  
  while (millis() < timer) {
    // Green phase (60% of time)
    if (millis() / timer < 0.6) {
      ledon = millis() / timer / 0.6 * 56;
      for (int i = 0; i < 56; i++) {
        if (i < ledon) {
          opc.setPixel(i, color(0, 100, 0));
          opc.setPixel(i+64, color(0, 100, 0));
          opc.setPixel(i+128, color(0, 100, 0));
          opc.setPixel(i+192, color(0, 100, 0));
        }
      }
    } else if (millis() / timer < 0.8) {
    // Yellow Phase (80% of time)
      ledon = (millis() - (timer * 0.6)) / timer / 0.2 * 56;
      for (int i = 0; i < 56; i++) {
        if (i < ledon) {
          opc.setPixel(i, color(100, 100, 0));
          opc.setPixel(i+64, color(100, 100, 0));
          opc.setPixel(i+128, color(100, 100, 0));
          opc.setPixel(i+192, color(100, 100, 0));
        }
      }
    } else {
    // Red Phase (80% of time)
      ledon = (millis() - (timer * 0.8)) / timer / 0.2 * 56;
      for (int i = 0; i < 56; i++) {
        if (i < ledon) {
          opc.setPixel(i, color(100, 0, 0));
          opc.setPixel(i+64, color(100, 0, 0));
          opc.setPixel(i+128, color(100, 0, 0));
          opc.setPixel(i+192, color(100, 0, 0));
        }
      }
    }
  
  // Write all LEDs
  opc.writePixels();
  }
}
