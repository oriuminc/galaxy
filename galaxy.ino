/*
  Galaxy
  Networked RFID readers based on Arduino Unos.
 */
 
// libraries
#include <Adafruit_CC3000.h> // wifi
#include <ccspi.h> // wifi
#include <SPI.h> // wifi

//#include <Adafruit_NFCShield_I2C.h> //rfid

//tabs

// external vars
// extern const char STUFF;
// These are the interrupt and control pins
#define ADAFRUIT_CC3000_IRQ   3 
#define ADAFRUIT_CC3000_VBAT  5
#define ADAFRUIT_CC3000_CS    10
Adafruit_CC3000 cc3000 = Adafruit_CC3000(ADAFRUIT_CC3000_CS, ADAFRUIT_CC3000_IRQ, ADAFRUIT_CC3000_VBAT,
                                         SPI_CLOCK_DIV2); 
#define WLAN_SSID    "Battlegrounds"
#define WLAN_PASS    "ding0.slugs"
#define WLAN_SECURITY WLAN_SEC_WPA2 // can be WLAN_SEC_UNSEC, WLAN_SEC_WEP, WLAN_SEC_WPA or WLAN_SEC_WPA2
#define IDLE_TIMEOUT_MS  3000 
// What page to grab!
#define WEBSITE      "adafruit.com"
#define WEBPAGE      "/testwifi/index.html"

//internal vars
uint32_t ip;

/* Setup ----------------------------------------------------- */
// the setup routine runs once when you press reset:
void setup(void) {               
  Serial.begin(115200);
  boolean RESPONSE;
  char domain[] = "adafruit.com";
  char endpoint[] = "/testwifi/index.html";
  RESPONSE = webclientSend(WEBSITE,  WEBPAGE);
  Serial.println(RESPONSE);
 
}

/* Loop ------------------------------------------------------ */
// the loop routine runs over and over again forever:
void loop() {
  delay(1000);
}

/* Functions ------------------------------------------------------ */
// @todo move these to tabs

boolean webclientSend(char *domain, char *endpoint) {
   
  /* Initialise the module */  
  if (!cc3000.begin() || !cc3000.connectToAP(WLAN_SSID, WLAN_PASS, WLAN_SECURITY))
  {
    Serial.println(F("Couldn't connect! Check your wiring?"));
    return false;
    while(1);
  }
  Serial.println(F("Connected!"));
  
  /* Wait for DHCP to complete */
  while (!cc3000.checkDHCP())
  {
    delay(100); // ToDo: Insert a DHCP timeout!
  }  

  /* Display the IP address DNS, Gateway, etc. */  
  ip = 0;
  // Try looking up the website's IP address
  while (ip == 0) {
    if (! cc3000.getHostByName(domain, &ip)) {
      Serial.println(F("Couldn't resolve!"));
      return false;
    }
    delay(500);
  }

  /* Try connecting to the website.
     Note: HTTP/1.1 protocol is used to keep the server from closing the connection before all data is read.
  */
  Adafruit_CC3000_Client www = cc3000.connectTCP(ip, 80);
  if (www.connected()) {
    www.fastrprint(F("GET "));
    www.fastrprint(endpoint);
    www.fastrprint(F(" HTTP/1.1\r\n"));
    www.fastrprint(F("Host: ")); www.fastrprint(domain); www.fastrprint(F("\r\n"));
    www.fastrprint(F("\r\n"));
    www.println();
  } else {
    Serial.println(F("Connection failed"));
    return false;
  }
  
  /* Read data until either the connection is closed, or the idle timeout is reached. */ 
  // this is the important stuff. the c variable is the http response. 
  unsigned long lastRead = millis();
  while (www.connected() && (millis() - lastRead < IDLE_TIMEOUT_MS)) {
    while (www.available()) {
      char c = www.read();
      Serial.print(c);
      lastRead = millis();
    }
  }
  www.close();
  
  /* You need to make sure to clean up after yourself or the CC3000 can freak out */
  cc3000.disconnect();
  return true;
}
