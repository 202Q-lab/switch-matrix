#ifndef SWITCH_H
#define SWITCH_H

class Switch {
    public:
        Switch(unsigned char select_pin, unsigned char enable_pin);

        void Setup();
        bool Reset();

        // Port methods
        void SetPort(char i_port);
        char GetPort() { return current_port; };

        virtual int GetPortCount() = 0;
        virtual unsigned char GetPortPin(char i_port) = 0;

        // Inverted
        bool IsInverted() { return invert; };
        void SetInverted(bool inverted);

        unsigned long GetPulseLength() { return pulse_len; };
        void SetPulseLength(unsigned long len);

    private:
        const static int TIME_TOLERANCE = 10; // 10 us tolerance (end pulse if less than 10 us left)
        
        long CloseThrow(unsigned char i_throw);
        long OpenThrow(unsigned char i_throw);
        void ChangeSelect(unsigned int value);
        
        char current_port;
        unsigned char select_pin;
        unsigned char enable_pin;
        bool invert;
        unsigned long pulse_len;
};

class SP6T : public Switch {
    public:
        SP6T(unsigned char pp[6], unsigned char select_pin, unsigned char enable_pin);
        
        int GetPortCount();
        unsigned char GetPortPin(char port);

    private:
        unsigned char port_pins[6];
};

class SPDT : public Switch {
    public:
        SPDT(unsigned char pin, unsigned char select_pin, unsigned char enable_pin);
        
        int GetPortCount();
        unsigned char GetPortPin(char port);

    private:
        unsigned char port_pin;
};

#endif
