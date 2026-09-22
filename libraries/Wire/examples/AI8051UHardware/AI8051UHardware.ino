#include <Arduino.h>
#include <Wire.h>

// AI8051U-34K64: C++ / MCS251 / 40 MHz. UART1 115200 8N1.
// SSD1306: VCC=3V3, GND=GND, SDA=P3.2, SCL=P3.3, with pull-ups.
// LF-terminated commands: E <text>, S (status), I (probe), O64, O32, X.
// I2C stays inactive until commanded. P5.2 toggles every 100 ms.
static unsigned long lastLed, lastReport, toggles, sequence;
static uint8_t ledState = HIGH;
static char inputLine[80];
static uint8_t inputLength;
static bool inputOverflow, wireStarted, displayActive;
static uint8_t oledAddress, oledPages = 8, oledPage, oledColumn;
static unsigned long frameCount, i2cTransactions, i2cErrors;
static unsigned long rxOverflows;
static const char glyphChars[] = "AI8051U COKFRME234679";
static const uint8_t glyphData[][5] = {
    {0x7E,0x11,0x11,0x11,0x7E}, {0,0x41,0x7F,0x41,0},
    {0x36,0x49,0x49,0x49,0x36}, {0x3E,0x51,0x49,0x45,0x3E},
    {0x27,0x45,0x45,0x45,0x39}, {0,0x42,0x7F,0x40,0},
    {0x3F,0x40,0x40,0x40,0x3F}, {0,0,0,0,0},
    {0x3E,0x41,0x41,0x41,0x22}, {0x3E,0x41,0x41,0x41,0x3E},
    {0x7F,0x08,0x14,0x22,0x41}, {0x7F,0x09,0x09,0x09,0x01},
    {0x7F,0x09,0x19,0x29,0x46}, {0x7F,0x02,0x0C,0x02,0x7F},
    {0x7F,0x49,0x49,0x49,0x41}, {0x42,0x61,0x51,0x49,0x46},
    {0x21,0x41,0x45,0x4B,0x31}, {0x18,0x14,0x12,0x7F,0x10},
    {0x3C,0x4A,0x49,0x49,0x30}, {0x01,0x71,0x09,0x05,0x03},
    {0x06,0x49,0x49,0x29,0x1E}
};

static void serviceLed()
{
    unsigned long now = millis();
    if ((unsigned long)(now - lastLed) >= 100UL) {
        lastLed += 100UL;
        ledState = ledState == HIGH ? LOW : HIGH;
        digitalWrite(P5_2, ledState);
        ++toggles;
    }
}

static void status()
{
    Serial.print("AI8051U SEQ="); Serial.print(++sequence);
    Serial.print(" MS="); Serial.print(millis());
    Serial.print(" TOGGLES="); Serial.print(toggles);
    Serial.print(" PIN="); Serial.print(digitalRead(P5_2));
    Serial.print(" OLED="); Serial.print(displayActive ? 1 : 0);
    Serial.print(" FRAMES="); Serial.print(frameCount);
    Serial.print(" TX="); Serial.print(i2cTransactions);
    Serial.print(" ERR="); Serial.print(i2cErrors);
    Serial.print(" RX_OVF="); Serial.println(rxOverflows);
}

static void beginWire()
{
    if (!wireStarted) {
        Wire.setPins(P3_2, P3_3);
        Wire.setClock(100000UL); // Software GPIO overhead reduces actual rate.
        Wire.setWireTimeout(25000UL, true);
        Wire.begin();
        wireStarted = true;
    }
}

static bool finishTransmission()
{
    uint8_t result = Wire.endTransmission();
    ++i2cTransactions;
    if (result != 0) {
        ++i2cErrors;
        displayActive = false;
        Serial.print("I2C ERROR="); Serial.println((unsigned int)result);
        return false;
    }
    return true;
}

static uint8_t probe()
{
    beginWire();
    uint8_t found = 0;
    for (uint8_t address = 0x3C; address <= 0x3D; ++address) {
        Wire.beginTransmission(address);
        uint8_t result = Wire.endTransmission();
        Serial.print("I2C ADDRESS="); Serial.print((unsigned int)address, HEX);
        Serial.print(" STATUS="); Serial.println((unsigned int)result);
        if (result == 0 && found == 0) found = address;
        serviceLed();
    }
    return found;
}

static void startOled(uint8_t pages)
{
    displayActive = false;
    oledAddress = probe();
    if (!oledAddress) { Serial.println("OLED NOT_FOUND"); return; }
    oledPages = pages;
    const uint8_t init[] = {0xAE,0xD5,0x80,0xA8,(uint8_t)(pages*8-1),
        0xD3,0x00,0x40,0x8D,0x14,0x20,0x02,0xA1,0xC8,0xDA,
        (uint8_t)(pages==8 ? 0x12 : 0x02),0x81,0x5F,0xD9,0xF1,
        0xDB,0x40,0xA4,0xA6,0x2E,0xAF};
    Wire.beginTransmission(oledAddress);
    Wire.write((uint8_t)0x00);
    for (uint8_t i=0; i<sizeof(init); ++i) Wire.write(init[i]);
    if (!finishTransmission()) return;
    oledPage = oledColumn = 0;
    frameCount = 0;
    displayActive = true;
    Serial.println("OLED STARTED");
}

static uint8_t glyph(char ch, uint8_t column)
{
    if (column == 5) return 0;
    for (uint8_t i=0; glyphChars[i]; ++i)
        if (glyphChars[i] == ch) return glyphData[i][column];
    return 0;
}

static uint8_t pixelColumn(uint8_t page, uint8_t x)
{
    if (page == 0 && x < 16*6)
        return glyph("AI8051U IIC OK   "[x/6], x%6);
    if (page == 1 && x < 12*6) {
        uint8_t cell = x/6;
        if (cell < 6) return glyph("FRAME "[cell], x%6);
        unsigned long divisor = 1;
        for (uint8_t i=cell; i<11; ++i) divisor *= 10;
        return glyph((char)('0' + (frameCount/divisor)%10), x%6);
    }
    if (x == 0 || x == 127) return 0xFF;
    uint8_t value = ((x + (uint8_t)frameCount*4)%32 < 8) ? 0x7E : 0;
    if (page == 2) value |= 0x01;
    if (page == oledPages-1) value |= 0x80;
    return value;
}

static void serviceDisplay()
{
    if (!displayActive) return;
    if (oledColumn == 0) {
        Wire.beginTransmission(oledAddress);
        Wire.write((uint8_t)0x00); Wire.write((uint8_t)(0xB0|oledPage));
        Wire.write((uint8_t)0x00); Wire.write((uint8_t)0x10);
        if (!finishTransmission()) return;
    }
    Wire.beginTransmission(oledAddress);
    Wire.write((uint8_t)0x40);
    for (uint8_t i=0; i<16; ++i) Wire.write(pixelColumn(oledPage, oledColumn+i));
    if (!finishTransmission()) return;
    oledColumn += 16;
    if (oledColumn == 128) {
        oledColumn = 0;
        if (++oledPage == oledPages) { oledPage=0; ++frameCount; }
    }
}

static void command()
{
    inputLine[inputLength] = 0;
    if (inputOverflow) Serial.println("INPUT OVERFLOW");
    else if (inputLength >= 2 && inputLine[0]=='E' && inputLine[1]==' ') {
        Serial.print("ECHO "); Serial.println(inputLine+2);
    } else if (inputLength==1 && inputLine[0]=='S') status();
    else if (inputLength==1 && inputLine[0]=='I') { displayActive=false; probe(); }
    else if (inputLength==3 && inputLine[0]=='O' && inputLine[1]=='6' && inputLine[2]=='4') startOled(8);
    else if (inputLength==3 && inputLine[0]=='O' && inputLine[1]=='3' && inputLine[2]=='2') startOled(4);
    else if (inputLength==1 && inputLine[0]=='X') { displayActive=false; Serial.println("OLED STOPPED"); }
    else if (inputLength) Serial.println("COMMAND UNKNOWN");
    inputLength=0;
    inputOverflow=false;
}

void setup()
{
    pinMode(P5_2, OUTPUT);
    digitalWrite(P5_2, HIGH);
    Serial.begin(115200);
    Serial.println("AI8051U HARDWARE MCS251 40MHZ UART=115200 LED=P5.2/100MS OLED=WAIT");
}

void loop()
{
    serviceLed();
    if (Serial.overflow()) { ++rxOverflows; inputOverflow=true; }
    while (Serial.available()) {
        int ch=Serial.read();
        if (ch=='\n') command();
        else if (ch!='\r') {
            if (inputLength < sizeof(inputLine)-1) inputLine[inputLength++]=(char)ch;
            else inputOverflow=true;
        }
        serviceLed();
    }
    serviceDisplay();
    serviceLed();
    unsigned long now=millis();
    if ((unsigned long)(now-lastReport)>=1000UL) { lastReport=now; status(); }
}
