/* Composite USB CDC + keyboard + mouse. No UART on the USB data pins. */
#include <USB.h>
#include <Keyboard.h>
#include <Mouse.h>
void setup() {
  pinMode(P3_2, INPUT_PULLUP);
  USBSerial.begin(115200);
  Keyboard.begin();
  Mouse.begin();
}
void loop() {
  static bool wasPressed;
  bool pressed = digitalRead(P3_2) == LOW;
  if (pressed && !wasPressed && USBDevice.configured()) {
    Keyboard.println("Hello from STC!");
    Mouse.move(10, 0);
    USBSerial.println("Button pressed");
  }
  wasPressed = pressed;
  while (USBSerial.available()) USBSerial.write((uint8_t)USBSerial.read());
  delay(10);
}
