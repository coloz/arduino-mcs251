#include <SD.h>

void setup() {
  Serial.begin(115200);
  if (!SD.begin(PIN_SPI_SS) || !SD.mkdir("LOGS/DAY1")) return;
  File a = SD.open("LOGS/DAY1/A.TXT", FILE_WRITE);
  File b = SD.open("LOGS/DAY1/B.TXT", FILE_WRITE);
  if (a && b) { a.println("first file"); b.println("second file"); }
  a.close(); b.close();
  File directory = SD.open("LOGS/DAY1");
  while (directory) {
    File entry = directory.openNextFile();
    if (!entry) break;
    Serial.println(entry.name());
    entry.close();
  }
  directory.close();
}
void loop() {}
