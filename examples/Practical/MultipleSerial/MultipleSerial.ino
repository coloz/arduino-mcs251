// UART numbers match the chip manual. Serial may instead be USB CDC.
void setup()
{
#if STC_FLASH_BYTES > 16384
    // The 16 KB device keeps room for the UART3/UART4 forwarding loop.
    Serial1.begin(115200);
#endif
#if STC_CORE_UART_AVAILABLE_MASK & 2
    Serial2.begin(9600);
#endif
    Serial3.begin(19200);
    Serial4.begin(38400);
#if STC_CORE_UART_COUNT > 4
    Serial5.begin(9600);
    Serial6.begin(19200);
    Serial7.begin(38400);
    Serial8.begin(57600);
#endif
}

void loop()
{
    while (Serial3.available()) Serial4.write(Serial3.read());
    while (Serial4.available()) Serial3.write(Serial4.read());
}
