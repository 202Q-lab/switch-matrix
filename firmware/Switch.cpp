#include "Switch.h"
#include <Arduino.h>

/* SP6T */

SP6T::SP6T(unsigned char pp[6], unsigned char sp, unsigned char ep): Switch(sp, ep) {
    for (int i = 0; i < 6; i++) {
        port_pins[i] = pp[i];
    }
}

unsigned char SP6T::GetPortPin(char port) {
    return port_pins[port];
}

int SP6T::GetPortCount() {
    return 6;
}

/* SPDT */

SPDT::SPDT(unsigned char pin, unsigned char sp, unsigned char ep): Switch(sp, ep) {

    port_pin = pin;
}

unsigned char SPDT::GetPortPin(char port) {
    return port_pin;
}

int SPDT::GetPortCount() {
    return 1;
}

/* Switch base class */

Switch::Switch(unsigned char sp, unsigned char ep): select_pin(sp), enable_pin(ep), invert(false) {
    /* Constructor initializes pin and state variables. */
    pulse_len = 10;
    current_port = -1;
}; 

void Switch::Setup() {
    /* Prepares the pins used by the switch for output. */

    for (int i = 0; i < GetPortCount(); i++) {
        pinMode(GetPortPin(i), OUTPUT);
    }

    pinMode(select_pin, OUTPUT);
    pinMode(enable_pin, OUTPUT);
}

void Switch::SetPort(char port) {
    if (current_port == port) {
        return;
    }

    if (current_port != -1) {
        OpenThrow(current_port);
    }
    if (port != -1) {
        CloseThrow(port);
    }

    this->current_port = port;
}

void Switch::SetPulseLength(unsigned long len) {
    /* Sets the length of the pulse sent to the switches. */
    this->pulse_len = len;
}

void Switch::SetInverted(bool inverted) {
    /* Sets whether the pulse behaviour should be inverted or not. */
    invert = inverted;
}

void Switch::ChangeSelect(unsigned int value) {
    /* Change the select status to high or low side. 
    HIGH select means low side driver, LOW select is high side driver. */

    digitalWrite(enable_pin, LOW);
    digitalWrite(select_pin, value);
}

bool Switch::Reset() {
    /* Open all throws on the switch. */
    bool ret = true;
    for (int i = 0; i < GetPortCount(); i++) {
        OpenThrow(i);
    }
    current_port = -1;
    return ret;
}

long Switch::CloseThrow(unsigned char i_throw) {
    /* Closes the specified throw. */

    if (invert) {
        ChangeSelect(HIGH);
    } else {
        ChangeSelect(LOW);
    }

    digitalWrite(enable_pin, HIGH);
    long start = micros();
    digitalWrite(GetPortPin(i_throw), HIGH);
    delay(pulse_len);
    digitalWrite(GetPortPin(i_throw), LOW);
    long end = micros();
    digitalWrite(enable_pin, LOW);
    
    return end - start;
}

long Switch::OpenThrow(unsigned char i_throw) {
    /* Opens the specified throw. */

    if (invert) {
        ChangeSelect(LOW);
    } else {
        ChangeSelect(HIGH);
    }

    digitalWrite(enable_pin, HIGH);
    long start = micros();
    digitalWrite(GetPortPin(i_throw), HIGH);
    delay(pulse_len);
    digitalWrite(GetPortPin(i_throw), LOW);
    long end = micros();
    digitalWrite(enable_pin, LOW);
    
    return end - start;
}
