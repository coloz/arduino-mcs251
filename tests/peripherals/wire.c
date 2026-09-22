#include "Arduino.h"
#include "hal/stc_c_hal.h"
#include "stc_peripheral_irqs.h"
static uint8_t regs[2][10], mux[2], gate, modes[256];
static uint8_t failed_gate, timeout_bus = 255u, nack_bus = 255u;
static unsigned long ticks;
STC_IRQ_DATA stc_peripheral_service_t stc_wire_slave_service;
STC_IRQ_DATA stc_peripheral_service_t stc_wire1_slave_service;
void pinMode(uint8_t pin, uint8_t mode) { modes[pin] = mode; }
void digitalWrite(uint8_t pin, uint8_t value) { (void)pin; (void)value; }
int digitalRead(uint8_t pin) { (void)pin; return HIGH; }
int digitalPinIsValid(uint8_t pin) { return pin < 0xb8u && (pin & 15u) < 8u; }
void delayMicroseconds(unsigned int delay) { ticks += delay; }
unsigned long micros(void) { return ++ticks; }
uint8_t stc_wire_hw_gate_read(void) { return gate; }
void stc_wire_hw_gate_write(uint8_t value) { gate = value; }
static uint8_t rd(uint8_t bus, uint8_t reg) { if (!(gate&128)) failed_gate=1; return regs[bus][reg]; }
static void wr(uint8_t bus, uint8_t reg, uint8_t value)
{
    if (!(gate&128)) failed_gate=1;
    regs[bus][reg]=value;
    if (reg == 1 && value && bus != timeout_bus) {
        /* MSACKI is bit 1; preserve the master's outgoing ACK at bit 0. */
        regs[bus][2] = (regs[bus][2] & 1u) | 0x40u | (bus == nack_bus ? 2u : 0u);
        if (value == 4) regs[bus][7] = 0x60u + bus;
    }
}
uint8_t stc_wire_hw_read(uint8_t reg) { return rd(0,reg); }
void stc_wire_hw_write(uint8_t reg,uint8_t value) { wr(0,reg,value); }
uint8_t stc_wire_hw_mux_read(void) { return mux[0]; }
void stc_wire_hw_mux_write(uint8_t value) { mux[0]=value; }
uint8_t stc_wire1_hw_read(uint8_t reg) { return rd(1,reg); }
void stc_wire1_hw_write(uint8_t reg,uint8_t value) { wr(1,reg,value); }
uint8_t stc_wire1_hw_mux_read(void) { if (!(gate&128)) failed_gate=1; return mux[1]; }
void stc_wire1_hw_mux_write(uint8_t value) { if (!(gate&128)) failed_gate=1; mux[1]=value; }
static uint8_t received[2];
static void receive0(int count) { if(count==1) received[0]=(uint8_t)Wire_read(); }
static void receive1(int count) { if(count==1) received[1]=(uint8_t)Wire1_read(); }
static void request0(void) { Wire_write(0xa0); }
static void request1(void) { Wire1_write(0xb1); }
#define CHECK(condition) do { if (!(condition)) return __LINE__; } while (0)
int run_tests(void)
{
    uint8_t bus;
    mux[0]=0x09; mux[1]=0x35;
    Wire_begin(); Wire1_begin();
    CHECK(mux[0]==0x39 && mux[1]==0x35 && gate==0);
    CHECK((regs[0][0]&0xc0)==0xc0 && (regs[1][0]&0xc0)==0xc0);
    Wire_setClock(1000); CHECK(regs[0][9] != 0 && regs[1][9] == 0);
    Wire_setClock(100000); CHECK(regs[0][9] == 0);
    Wire_beginTransmission(0x50); Wire_write(0xaa);
    Wire1_beginTransmission(0x51); Wire1_write(0xbb);
    CHECK(Wire_endTransmissionStop(0)==0);
    CHECK(Wire1_endTransmissionStop(1)==0);
    CHECK(Wire_requestFromStop(0x50,2,1)==2);
    CHECK(Wire1_requestFromStop(0x51,1,1)==1);
    CHECK(Wire_read()==0x60 && Wire1_read()==0x61 && Wire_read()==0x60);
    /* A completed read leaves ACKO=1 after its final NACK. That outgoing
     * NACK must not turn the next slave's incoming ACK into a write error. */
    CHECK((regs[0][2]&1u) && (regs[1][2]&1u));
    Wire_beginTransmission(0x50); Wire_write(0xa5);
    CHECK(Wire_endTransmission()==0);
    Wire1_beginTransmission(0x51); Wire1_write(0x5a);
    CHECK(Wire1_endTransmission()==0);
    regs[1][2] &= (uint8_t)~1u; /* Incoming NACK is still visible with ACKO=0. */
    nack_bus=1; Wire1_beginTransmission(0x51);
    CHECK(Wire1_endTransmission()==2); nack_bus=255;
    Wire1_setWireTimeout(2,1); timeout_bus=1;
    Wire1_beginTransmission(0x51); CHECK(Wire1_endTransmission()==5);
    CHECK(Wire1_getWireTimeoutFlag() && !Wire_getWireTimeoutFlag()); timeout_bus=255;
    CHECK(Wire1_setPinsChecked(P6_0,P6_1)==0); CHECK(mux[1]==0xf5 && mux[0]==0x39);
    Wire1_end(); CHECK(regs[1][0]==0 && regs[0][0]!=0 && mux[1]==0x35);
    Wire_end(); CHECK(mux[0]==0x09);
    Wire_onReceive(receive0); Wire1_onReceive(receive1);
    Wire_onRequest(request0); Wire1_onRequest(request1);
    Wire_beginSlave(0x30); Wire1_beginSlave(0x31);
    CHECK(!Wire_configurationError() && !Wire1_configurationError());
    for(bus=0;bus<2;bus++) {
        stc_peripheral_service_t service = bus ? stc_wire1_slave_service : stc_wire_slave_service;
        CHECK(service != 0);
        regs[bus][4]=0x40; service(); /* START */
        regs[bus][7]=(0x30+bus)<<1; regs[bus][4]=0x20; service(); /* address/write */
        regs[bus][7]=0x70+bus; regs[bus][4]=0x20; service(); /* payload */
        regs[bus][4]=8; service(); /* STOP */
        CHECK(received[bus]==0x70+bus);
        regs[bus][4]=0x40; service();
        regs[bus][7]=((0x30+bus)<<1)|1; regs[bus][4]=0x20; service();
        CHECK(regs[bus][6]==(bus ? 0xb1 : 0xa0));
    }
    CHECK(Wire1_setPinsChecked(P3_3,P3_2)==4); /* belongs to I2C1 */
    Wire1_end(); CHECK(stc_wire1_slave_service==0 && stc_wire_slave_service!=0);
    CHECK(regs[0][0]==0x80 && regs[1][0]==0);
    Wire_end(); CHECK(!failed_gate && gate==0 && mux[0]==0x09 && mux[1]==0x35);
    return 0;
}
