# LED-Lampe
Frei programmierbare LED-Lampe, die z.B. für den Countdown bei Lightning Talks verwendet werden kann.

![LED-Lampe](https://pbs.twimg.com/media/Dro9xYtWoAAWNO7?format=jpg&name=large)

## Software

Standalone-Firmware für **Adafruit Feather 32u4 Bluefruit LE** mit **Bluetooth LE (UART in Bluefruit Connect)** und **WS2812B/NeoPixel**.

- Sketch: `feather32u4_timer_lamp/feather32u4_timer_lamp.ino`
- LED-Datenleitung: **Pin 3 (SCL)**
- Befehle über BLE UART:
  - `s` = Start
  - `r` = Reset (alle LEDs aus)
  - `tNN` = Zeit in Minuten setzen, z.B. `t10`

Timer-Logik:
- Bis **letzte Minute**: LEDs werden von unten nach oben **grün** proportional zur Zeit.
- **Letzte 50s**: Grün wird von unten nach oben mit **gelb** überschrieben.
- **Letzte 10s**: Gelb wird von unten nach oben mit **rot** überschrieben.
- Nach Ablauf: **rot blinkend** (rot/aus/rot/aus …)

Benötigte Arduino Libraries:
- `Adafruit NeoPixel`
- `Adafruit BluefruitLE nRF51`

## Materialliste

* LED-Streifen W2812B (4x56 STÜCK) ([Datenblatt](https://www.seeedstudio.com/document/pdf/WS2812B%20Datasheet.pdf))
* LED-Trafo 5V, 12A
* Vierkantholz 1x1x100 cm zur Montage der LED-Streifen
* Kabelbinder zur zusätzlichen Fixierung der LED-Streifen auf dem Vierkantholz
* Kunststoff-Röhre 100 cm (Aussendirchmesser 40 mm, Innendurchmesser 35 mm)
* Beilagscheiben (Aussendurchmesser 35 mm, Bohrung 13 mm) zur Fixierung des Vierkantholzes in der Röhre
* USB-Kabel
