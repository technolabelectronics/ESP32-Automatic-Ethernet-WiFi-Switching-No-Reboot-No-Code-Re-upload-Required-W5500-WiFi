/*
  IOT Gateway V1.0
  Automatic Ethernet / Wi-Fi Switching

  Priority:
  1. Ethernet
  2. Wi-Fi

  Network status is checked every 5 seconds.

  LED behaviour:
  Ethernet active -> Ethernet LED ON
  Wi-Fi active    -> Wi-Fi LED ON
  No network      -> Both LEDs blink slowly
*/

#include <SPI.h>
#include <Ethernet.h>
#include <WiFi.h>

// =====================================================
// Wi-Fi credentials
// =====================================================

const char* WIFI_SSID     = "technolab";
const char* WIFI_PASSWORD = "12345678";

// =====================================================
// W5500 Ethernet pin configuration
// =====================================================

#define ETH_CS_PIN       5
#define ETH_SCLK_PIN    18
#define ETH_MISO_PIN    19
#define ETH_MOSI_PIN    23
#define ETH_INT_PIN     26
#define ETH_RESET_PIN   27

// =====================================================
// Status LEDs
// =====================================================

#define WIFI_LED_PIN     22
#define ETHERNET_LED_PIN 32

// HIGH turns the LEDs ON according to your schematic.
#define LED_ON  HIGH
#define LED_OFF LOW

// =====================================================
// Ethernet MAC address
// =====================================================

// Locally administered MAC address.
// Each gateway should ideally have a unique MAC address.
byte ethernetMac[] = {
  0x02, 0xAA, 0xBB, 0xCC, 0xDD, 0x01
};

// =====================================================
// Network selection
// =====================================================

enum NetworkType {
  NETWORK_NONE,
  NETWORK_ETHERNET,
  NETWORK_WIFI
};

NetworkType activeNetwork = NETWORK_NONE;

// Check network every 5 seconds
const unsigned long NETWORK_CHECK_INTERVAL = 5000;
unsigned long previousNetworkCheck = 0;

// Wi-Fi connection timeout
const unsigned long WIFI_TIMEOUT = 15000;

// Ethernet DHCP timeout is controlled internally
// by the Ethernet library.

// =====================================================
// Utility functions
// =====================================================

const char* networkName(NetworkType network) {
  switch (network) {
    case NETWORK_ETHERNET:
      return "Ethernet";

    case NETWORK_WIFI:
      return "Wi-Fi";

    default:
      return "No Network";
  }
}

bool hasValidIPAddress(IPAddress ip) {
  return !(ip[0] == 0 &&
           ip[1] == 0 &&
           ip[2] == 0 &&
           ip[3] == 0);
}

void printIPAddress(const char* label, IPAddress address) {
  Serial.print(label);
  Serial.println(address);
}

// =====================================================
// LED control
// =====================================================

void updateNetworkLEDs() {
  switch (activeNetwork) {
    case NETWORK_ETHERNET:
      digitalWrite(ETHERNET_LED_PIN, LED_ON);
      digitalWrite(WIFI_LED_PIN, LED_OFF);
      break;

    case NETWORK_WIFI:
      digitalWrite(ETHERNET_LED_PIN, LED_OFF);
      digitalWrite(WIFI_LED_PIN, LED_ON);
      break;

    default:
      digitalWrite(ETHERNET_LED_PIN, LED_OFF);
      digitalWrite(WIFI_LED_PIN, LED_OFF);
      break;
  }
}

void blinkNoNetworkLEDs() {
  static bool ledState = false;
  static unsigned long previousBlink = 0;

  if (millis() - previousBlink >= 500) {
    previousBlink = millis();
    ledState = !ledState;

    digitalWrite(ETHERNET_LED_PIN, ledState);
    digitalWrite(WIFI_LED_PIN, ledState);
  }
}

// =====================================================
// Ethernet functions
// =====================================================

void resetEthernetModule() {
  Serial.println("Resetting Ethernet module...");

  digitalWrite(ETH_RESET_PIN, LOW);
  delay(100);

  digitalWrite(ETH_RESET_PIN, HIGH);
  delay(500);
}

bool ethernetCableConnected() {
  EthernetLinkStatus linkStatus = Ethernet.linkStatus();

  return linkStatus == LinkON;
}

bool ethernetHasValidIP() {
  return hasValidIPAddress(Ethernet.localIP());
}

bool connectEthernet() {
  Serial.println();
  Serial.println("--------------------------------");
  Serial.println("Trying Ethernet...");
  Serial.println("--------------------------------");

  if (!ethernetCableConnected()) {
    Serial.println("Ethernet cable is not connected.");
    return false;
  }

  /*
    Ethernet.begin() requests an IP address through DHCP.
    It returns 1 when DHCP succeeds and 0 when DHCP fails.
  */
  int dhcpResult = Ethernet.begin(ethernetMac);

  if (dhcpResult == 0) {
    Serial.println("Ethernet DHCP failed.");
    return false;
  }

  delay(300);

  if (!ethernetCableConnected()) {
    Serial.println("Ethernet link was lost.");
    return false;
  }

  if (!ethernetHasValidIP()) {
    Serial.println("Ethernet did not receive a valid IP.");
    return false;
  }

  Serial.println("Ethernet connected successfully!");

  printIPAddress("IP Address : ", Ethernet.localIP());
  printIPAddress("Gateway    : ", Ethernet.gatewayIP());
  printIPAddress("Subnet     : ", Ethernet.subnetMask());
  printIPAddress("DNS        : ", Ethernet.dnsServerIP());

  return true;
}

// =====================================================
// Wi-Fi functions
// =====================================================
bool connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) {
    return true;
  }

  Serial.println();
  Serial.println("--------------------------------");
  Serial.println("Trying Wi-Fi...");
  Serial.println("--------------------------------");

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long connectionStart = millis();
  unsigned long previousBlink = 0;

  bool ledState = false;

  while (WiFi.status() != WL_CONNECTED &&
         millis() - connectionStart < WIFI_TIMEOUT) {

    // Blink both LEDs every 500 ms while no network is available
    if (millis() - previousBlink >= 500) {
      previousBlink = millis();
      ledState = !ledState;

      digitalWrite(WIFI_LED_PIN, ledState);
      digitalWrite(ETHERNET_LED_PIN, ledState);

      Serial.print(".");
    }

    delay(10);
  }

  Serial.println();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Wi-Fi connection failed.");

    // Turn both OFF before NETWORK_NONE blinking starts
    digitalWrite(WIFI_LED_PIN, LED_OFF);
    digitalWrite(ETHERNET_LED_PIN, LED_OFF);

    WiFi.disconnect();

    return false;
  }

  Serial.println("Wi-Fi connected successfully!");

  Serial.print("SSID       : ");
  Serial.println(WiFi.SSID());

  printIPAddress("IP Address : ", WiFi.localIP());
  printIPAddress("Gateway    : ", WiFi.gatewayIP());
  printIPAddress("Subnet     : ", WiFi.subnetMask());

  Serial.print("RSSI       : ");
  Serial.print(WiFi.RSSI());
  Serial.println(" dBm");

  // Wi-Fi connected: only Wi-Fi LED ON
  digitalWrite(WIFI_LED_PIN, LED_ON);
  digitalWrite(ETHERNET_LED_PIN, LED_OFF);

  return true;
}

void disconnectWiFi() {
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Disconnecting Wi-Fi...");
  }

  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
}

// =====================================================
// Network switching
// =====================================================

void changeActiveNetwork(NetworkType newNetwork) {
  if (activeNetwork == newNetwork) {
    return;
  }

  Serial.println();
  Serial.print("Network changed: ");
  Serial.print(networkName(activeNetwork));
  Serial.print(" -> ");
  Serial.println(networkName(newNetwork));

  activeNetwork = newNetwork;
  updateNetworkLEDs();
}

void checkNetwork() {
  Serial.println();
  Serial.println("===== Network Status Check =====");

  EthernetLinkStatus linkStatus = Ethernet.linkStatus();

  Serial.print("Ethernet link: ");

  if (linkStatus == LinkON) {
    Serial.println("Connected");
  } else if (linkStatus == LinkOFF) {
    Serial.println("Disconnected");
  } else {
    Serial.println("Unknown");
  }

  /*
    Ethernet always has first priority.
  */

  if (linkStatus == LinkON) {

    if (activeNetwork == NETWORK_ETHERNET &&
        ethernetHasValidIP()) {

      // Renew or maintain the DHCP lease.
      Ethernet.maintain();

      Serial.println("Ethernet remains active.");
      updateNetworkLEDs();
      return;
    }

    /*
      Cable is connected, but Ethernet is not currently
      active. Attempt DHCP and switch to Ethernet.
    */

    if (connectEthernet()) {
      disconnectWiFi();
      changeActiveNetwork(NETWORK_ETHERNET);

      Serial.println("Using Ethernet.");
      return;
    }

    Serial.println("Ethernet unavailable despite cable connection.");
  }

  /*
    Ethernet unavailable: use Wi-Fi as fallback.
  */

  if (WiFi.status() == WL_CONNECTED) {
    changeActiveNetwork(NETWORK_WIFI);

    Serial.println("Wi-Fi remains active.");
    return;
  }

  if (connectWiFi()) {
    changeActiveNetwork(NETWORK_WIFI);

    Serial.println("Using Wi-Fi.");
    return;
  }

  /*
    Neither network is available.
  */

  changeActiveNetwork(NETWORK_NONE);
  Serial.println("No network connection available.");
}

// =====================================================
// Setup
// =====================================================

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(WIFI_LED_PIN, OUTPUT);
  pinMode(ETHERNET_LED_PIN, OUTPUT);

  digitalWrite(WIFI_LED_PIN, LED_OFF);
  digitalWrite(ETHERNET_LED_PIN, LED_OFF);

  pinMode(ETH_RESET_PIN, OUTPUT);
  pinMode(ETH_INT_PIN, INPUT_PULLUP);

  Serial.println();
  Serial.println("================================");
  Serial.println(" IOT Gateway V1.0");
  Serial.println(" Ethernet / Wi-Fi Network Manager");
  Serial.println(" Priority: Ethernet > Wi-Fi");
  Serial.println("================================");

  /*
    Initialize the ESP32 SPI bus:
    SCLK = GPIO18
    MISO = GPIO19
    MOSI = GPIO23
    CS   = GPIO5
  */

  SPI.begin(
    ETH_SCLK_PIN,
    ETH_MISO_PIN,
    ETH_MOSI_PIN,
    ETH_CS_PIN
  );

  Ethernet.init(ETH_CS_PIN);

  resetEthernetModule();

  /*
    Initialize W5500 once so linkStatus() can report
    cable state correctly.
  */

  Serial.println("Initializing Ethernet...");
  Ethernet.begin(ethernetMac);
  delay(500);

  checkNetwork();

  previousNetworkCheck = millis();
}

// =====================================================
// Main loop
// =====================================================

void loop() {
  /*
    Check Ethernet and Wi-Fi every 5 seconds.
  */

  if (millis() - previousNetworkCheck >=
      NETWORK_CHECK_INTERVAL) {

    previousNetworkCheck = millis();
    checkNetwork();
  }

  /*
    Blink both network LEDs when neither Ethernet
    nor Wi-Fi is available.
  */

  if (activeNetwork == NETWORK_NONE) {
    blinkNoNetworkLEDs();
  }

  /*
    Add MQTT, HTTP, Modbus or cloud code here.

    You can check the active interface like this:

    if (activeNetwork == NETWORK_ETHERNET) {
      // Use Ethernet client
    }
    else if (activeNetwork == NETWORK_WIFI) {
      // Use Wi-Fi client
    }
  */
}