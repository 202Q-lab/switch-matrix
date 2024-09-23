#include <Vrekrer_scpi_parser.h>
#include <SPI.h>
#include <Ethernet.h>
#include <EEPROM.h>
#include "Switch.h"

#define DEBUG

/*
DATA TO SET WHEN FLASHING A NEW DEVICE
*/

// the media access control (MAC) address for the Ethernet shield. This is given
// on the shield itself (assuming Arduino Ethernet Shield 2).
// Can be set to any 6 bytes, but should be unique on the network.
byte mac[] = { 0xA8, 0x61, 0x0A, 0xAE, 0xAA, 0x3B };

// Pins 50-53 is used for SPI with the Ethernet shield and should not be used.
// Pin  10 and 4 is used as SPI SS and should not be used. 
// It is recommended to first and formost use pins 22-49, however other pins can be used.

/* 3 SP6T CONFIGURATION */
/*<--- UNCOMMENT FROM HERE
const unsigned int SWITCH_COUNT = 3;
Switch* switches[SWITCH_COUNT] = {
   new SP6T(new char[6]{22, 23, 24, 25, 26, 27}, 28, 29),
   new SP6T(new char[6]{30, 31, 32, 33, 34, 35}, 36, 37),
   new SP6T(new char[6]{38, 39, 40, 41, 42, 43}, 44, 45),
};
  TO HERE --->*/

/* 1 SP6T + 12 SPDT CONFIGURATION */
/*<--- UNCOMMENT FROM HERE
const unsigned int SWITCH_COUNT = 13;
Switch* switches[SWITCH_COUNT] = {
  new SP6T(new char[6]{22, 23, 24, 25, 26, 27}, 28, 29),

  new SPDT(30, 36, 37), new SPDT(31, 36, 37), new SPDT(32, 36, 37), // Note that these share select and enable pins
  new SPDT(33, 36, 37), new SPDT(34, 36, 37), new SPDT(35, 36, 37), // Since they are on the same circuit on the PCB
  
  new SPDT(38, 44, 45), new SPDT(39, 44, 45), new SPDT(40, 44, 45), // Note that these share select and enable pins
  new SPDT(41, 44, 45), new SPDT(42, 44, 45), new SPDT(43, 44, 45), // Since they are on the same circuit on the PCB
};
  TO HERE --->*/

/* 18 SPDT CONFIGURATION */
/*<--- UNCOMMENT FROM HERE
const unsigned int SWITCH_COUNT = 18;
Switch* switches[SWITCH_COUNT] = {
  new SPDT(22, 28, 29), new SPDT(23, 28, 29), new SPDT(24, 28, 29), // Note that these share select and enable pins
  new SPDT(25, 28, 29), new SPDT(26, 28, 29), new SPDT(27, 28, 29), // Since they are on the same circuit on the PCB

  new SPDT(30, 36, 37), new SPDT(31, 36, 37), new SPDT(32, 36, 37), // Note that these share select and enable pins
  new SPDT(33, 36, 37), new SPDT(34, 36, 37), new SPDT(35, 36, 37), // Since they are on the same circuit on the PCB

  new SPDT(38, 44, 45), new SPDT(39, 44, 45), new SPDT(40, 44, 45), // Note that these share select and enable pins
  new SPDT(41, 44, 45), new SPDT(42, 44, 45), new SPDT(43, 44, 45), // Since they are on the same circuit on the PCB
};
  TO HERE --->*/

// END OF VARIABLES TO CHANGE

// 5025 is default SCPI-raw port, and is recommended.
int port = 5025;

IPAddress initialIp(192,168,0,2);
IPAddress initialDns(192,168,0,1);
IPAddress initialGateway(192,168,0,1);
IPAddress initialMask(255,255,255,0);

// The static IP to use.
IPAddress ip;
IPAddress dns;
IPAddress gateway;
IPAddress mask;

int ip_eeprom_addr = 1;
int dns_eeprom_addr = ip_eeprom_addr + 4;
int gateway_eeprom_addr = dns_eeprom_addr + 4;
int mask_eeprom_addr = gateway_eeprom_addr + 4;

SCPI_Parser instr;
EthernetServer server = EthernetServer(port);

void setup() {
  /* Create SCPI command tree, configure Arduino pins and setup communication. */

  Serial.begin(9600);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }

  Serial.print("Setting up Switch Matrix firmware with ");
  Serial.print(SWITCH_COUNT);
  Serial.println(" switches.");
  Serial.println("Their port counts are:");

  for (auto sw : switches) {
    sw->Setup();
    Serial.println(sw->GetPortCount());
  }

  pinMode(LED_BUILTIN, OUTPUT);

  // Create the SCPI command tree
  instr.RegisterCommand(F("*IDN?"), &Identify);
  instr.RegisterCommand(F("*RST"), &ResetAll);
  instr.RegisterCommand(F("IP?"), &GetIp);
  instr.RegisterCommand(F("IP"), &SetIp);
  instr.RegisterCommand(F("DNS?"), &GetDns);
  instr.RegisterCommand(F("DNS"), &SetDns);
  instr.RegisterCommand(F("GATEWAY?"), &GetGateway);
  instr.RegisterCommand(F("GATEWAY"), &SetGateway);
  instr.RegisterCommand(F("SUBNET?"), &GetSubnet);
  instr.RegisterCommand(F("SUBNET"), &SetSubnet);

  instr.SetCommandTreeBase(F("SWitch#"));
  instr.RegisterCommand(F("*RST"), &Reset);
  instr.RegisterCommand(F("PORT?"), &GetPort);
  instr.RegisterCommand(F("PORT"), &SetPort);
  instr.RegisterCommand(F("PULSe?"), &GetPulseLen);
  instr.RegisterCommand(F("PULSe"), &SetPulseLen);
  instr.RegisterCommand(F("INVert?"), &GetInvert);
  instr.RegisterCommand(F("INVert"), &SetInvert);

  LoadData();

  SetupServer();
}

void LoadData() {
  char val = EEPROM.read(0);
  if (val == 255) { // EEPROM has never been written to before. Set Ethernet settings to the initial and write to EEPROM.
    ip = initialIp;
    dns = initialDns;
    gateway = initialGateway;
    mask = initialMask;
    UpdateData();
    EEPROM.write(0, 0);
  }
  else {
    uint32_t addr;
    EEPROM.get(ip_eeprom_addr, addr);
    ip = IPAddress(addr);
    EEPROM.get(dns_eeprom_addr, addr);
    dns = IPAddress(addr);
    EEPROM.get(gateway_eeprom_addr, addr);
    gateway = IPAddress(addr);
    EEPROM.get(mask_eeprom_addr, addr);
    mask = IPAddress(addr);
  }
}

void UpdateData() {
  EEPROM.put(ip_eeprom_addr, (uint32_t)ip);
  EEPROM.put(dns_eeprom_addr, (uint32_t)dns);
  EEPROM.put(gateway_eeprom_addr, (uint32_t)gateway);
  EEPROM.put(mask_eeprom_addr, (uint32_t)mask);
}

void loop() {
  instr.ProcessInput(Serial, "\n");

  Ethernet.maintain();
  EthernetClient client = server.available();

  if (client) {
    String msgstr = String(instr.GetMessage(client, "\n"));
    msgstr.trim();
    Serial.println(msgstr);
    instr.Execute(msgstr.c_str(), client);
  }
}

void SetupServer() {
  // Establish an Ethernet connection through the Ethernet shield.

  Ethernet.begin(mac, ip);

  #if defined(DEBUG)
  if (Ethernet.hardwareStatus() == EthernetNoHardware) {
    Serial.println(F("NO ETHERNET HARDWARE!"));
  } else {
    Serial.println(F("Ethernet hardware detected!"));
  }
  int linkStatus = Ethernet.linkStatus();
  if (linkStatus == Unknown) {
    Serial.println(F("Link Status: Unknown"));
  }
  else if (linkStatus == LinkON) {
    Serial.println(F("Link Status: On"));
  }
  else if (linkStatus == LinkOFF) {
    Serial.println(F("Link Status: Off"));
  }
  Serial.print("Local IP: ");
  Serial.println(Ethernet.localIP());
  Serial.print("Gateway IP: ");
  Serial.println(Ethernet.gatewayIP());
  Serial.print("DNS Server IP: ");
  Serial.println(Ethernet.dnsServerIP());
  Serial.print("Subnet Mask: ");
  Serial.println(Ethernet.subnetMask());
  #endif

  // Start the server to listen for connections
  server.begin();
}

void Identify(SCPI_C commands, SCPI_P parameters, Stream& interface) {
  // SCPI Command: *IDN?
  // Responds with the ID of this instrument.
  interface.println(F("202Q-lab,Switch Controller,001,0.0.1"));
}

void ResetAll(SCPI_C commands, SCPI_P parameters, Stream& interface) {
  // SCPI Command: *RST
  // Opens all throws connected to the device.


  digitalWrite(LED_BUILTIN, HIGH);
  for (auto sw : switches) {
    sw->Reset();
  }
  digitalWrite(LED_BUILTIN, LOW);
}

inline int FindSuffix(String command, String header) {
  header.toUpperCase();
  command.toUpperCase();
  int suffix = -1;
  sscanf(header.c_str(), String("%*[" + command + "]%u").c_str(), &suffix);
  return suffix - 1;
}

inline bool IsIndex(int ind) {
  /* Method for checking if an index is a valid switch (0-5) */
  return (ind >= 0) && (ind < SWITCH_COUNT);
}

void Reset(SCPI_C commands, SCPI_P parameters, Stream& interface) {
  // SCPI Command: SWitch#:*RST?
  // Opens all throws for the specified switch.

  String header = String(commands.First());
  int sw_i = FindSuffix("SWITCH", header);

  digitalWrite(LED_BUILTIN, HIGH);

  // If the switch index is valid, try to reset that switch
  if (IsIndex(sw_i)) {
    Switch* sw = switches[sw_i];
    if (sw->Reset()) {
      interface.println("TRUE");
    } else {
      interface.println("FALSE");
    }
  }

  digitalWrite(LED_BUILTIN, LOW);
}

void GetPort(SCPI_C commands, SCPI_P parameters, Stream& interface) {
  // SCPI Command: SWitch#:PORT?
  // Gets the status of the specified throw in the switch.

  String header = String(commands.First());
  int sw_i = FindSuffix("SWITCH", header);

  if (IsIndex(sw_i)) {
    Switch* sw = switches[sw_i];
    char status = sw->GetPort() + 1;
    interface.println(String((int)status));
  }
}

void SetPort(SCPI_C commands, SCPI_P parameters, Stream& interface) {
  // SCPI Command: SWitch#:PORT 0-6
  // Switches to the given port. (0 to leave all open)

  String header = String(commands.First());
  int sw_i = FindSuffix("SWITCH", header);

  String command = String(parameters.First());
  char port = (char)(command.toInt() - 1);

  digitalWrite(LED_BUILTIN, HIGH);

  if (IsIndex(sw_i)) {
    Switch* sw = switches[sw_i];
    if (port < sw->GetPortCount()) {
      sw->SetPort(port);
    }
  }

  digitalWrite(LED_BUILTIN, LOW);
}

void GetPulseLen(SCPI_C commands, SCPI_P parameters, Stream& interface) {
  // SCPI Command: SWitch#:PULSe?
  // Returns the pulse length of the given switch.

  String header = String(commands.First());
  int sw_i = FindSuffix("SWITCH", header);

  if (IsIndex(sw_i)) {
    Switch* sw = switches[sw_i];
    interface.println(String(sw->GetPulseLength()));
  }
}

void SetPulseLen(SCPI_C commands, SCPI_P parameters, Stream& interface) {
  // SCPI Command: SWitch#:PULSe time_ms
  // Sets the pulse length of the given switch to the given time in microseconds.

  String header = String(commands.First());
  int sw_i = FindSuffix("SWITCH", header);
  

  String param = String(parameters.First());
  unsigned long timems = (unsigned long)param.toInt();

  if (IsIndex(sw_i)) {
    Switch* sw = switches[sw_i];
    sw->SetPulseLength(timems);
  }
}

void GetInvert(SCPI_C commands, SCPI_P parameters, Stream& interface) {
  // SCPI Command: SWitch#:INVert?
  // Returns whether or not the specified switch is in invert mode.

  String header = String(commands.First());
  int sw_i = FindSuffix("SWITCH", header);

  if (IsIndex(sw_i)) {
    Switch* sw = switches[sw_i];
    
    interface.println(sw->IsInverted());
  }
}

void SetInvert(SCPI_C commands, SCPI_P parameters, Stream& interface) {
  // SCPI Command: SWitch#:INVert 1|0
  // Enables or disables invert mode for the specified switch.

  String header = String(commands.First());
  int sw_i = FindSuffix("SWITCH", header);

  String param = String(parameters.First());
  int status = param.toInt();

  if (IsIndex(sw_i)) {
    Switch* sw = switches[sw_i];
    if (status == 1) {
      sw->SetInverted(true);
    } else if (status == 0) {
      sw->SetInverted(false);
    }
  }
}

void GetIp(SCPI_C commands, SCPI_P parameters, Stream& interface) {
  // SCPI Command: IP?
  // Returns the IP of the Arduino
  interface.println(Ethernet.localIP());
}

void SetIp(SCPI_C commands, SCPI_P parameters, Stream& interface) {
  // SCPI Command: IP <addr>
  // Sets the IP of the Arduino
  String param = String(parameters.First());
  IPAddress newIp;
  if (newIp.fromString(param.c_str())) {
    Ethernet.setLocalIP(newIp);
    ip = newIp;
    UpdateData();
  }
  else {
    interface.println("Invalid IP");
  }
}

void GetDns(SCPI_C commands, SCPI_P parameters, Stream& interface) {
  // SCPI Command: DNS?
  // Returns the DNS server IP of the Arduino
  interface.println(Ethernet.dnsServerIP());
}

void SetDns(SCPI_C commands, SCPI_P parameters, Stream& interface) {
  // SCPI Command: DNS <addr>
  // Sets the DNS server IP of the Arduino
  String param = String(parameters.First());
  IPAddress newIp;
  if (newIp.fromString(param.c_str())) {
    Ethernet.setDnsServerIP(newIp);
    dns = newIp;
    UpdateData();
  }
  else {
    interface.println("Invalid DNS IP");
  }
}

void GetGateway(SCPI_C commands, SCPI_P parameters, Stream& interface) {
  // SCPI Command: GATEWAY?
  // Returns the Gateway IP of the Arduino
  interface.println(Ethernet.gatewayIP());
}

void SetGateway(SCPI_C commands, SCPI_P parameters, Stream& interface) {
  // SCPI Command: GATEWAY <addr>
  // Sets the Gateway IP of the Arduino
  String param = String(parameters.First());
  IPAddress newIp;
  if (newIp.fromString(param.c_str())) {
    Ethernet.setGatewayIP(newIp);
    gateway = newIp;
    UpdateData();
  }
  else {
    interface.println("Invalid gateway IP");
  }
}

void GetSubnet(SCPI_C commands, SCPI_P parameters, Stream& interface) {
  // SCPI Command: SUBNET?
  // Returns the subnet mask of the Arduino
  interface.println(Ethernet.subnetMask());
}

void SetSubnet(SCPI_C commands, SCPI_P parameters, Stream& interface) {
  // SCPI Command: SUBNET <addr>
  // Sets the subnet mask of the Arduino
  String param = String(parameters.First());
  IPAddress newIp;
  if (newIp.fromString(param.c_str())) {
    Ethernet.setSubnetMask(newIp);
    mask = newIp;
    UpdateData();
  }
  else {
    interface.println("Invalid subnet mask");
  }
}
