/*
  Galaxy
  Networked RFID readers based on Arduino Unos.
 */

// libraries
#include <Adafruit_CC3000.h> // wifi
#include <ccspi.h> // wifi
#include <SPI.h> // wifi
#include <Wire.h> // rfid
#include <Adafruit_PN532.h> // rfid
#include <SoftwareSerial.h> // lcd.

//tabs

// external vars
// extern const char STUFF;

// configuration

#include "config.h"

#define NODE_ID 001 // uid for the arduino build to send to the server

/* CC3K definitions ----------------------------------------------------- */
#define ADAFRUIT_CC3000_IRQ   3
#define ADAFRUIT_CC3000_VBAT  5
#define ADAFRUIT_CC3000_CS    10
Adafruit_CC3000 cc3000 = Adafruit_CC3000(ADAFRUIT_CC3000_CS, ADAFRUIT_CC3000_IRQ, ADAFRUIT_CC3000_VBAT,
                                         SPI_CLOCK_DIV2);
#define WLAN_SECURITY WLAN_SEC_WPA2 // can be WLAN_SEC_UNSEC, WLAN_SEC_WEP, WLAN_SEC_WPA or WLAN_SEC_WPA2
#define IDLE_TIMEOUT_MS  3000
// What page to grab!

/* NFC definitions  ----------------------------------------------------- */

#define IRQ   (2)
#define RESET (3)  // Not connected by default on the NFC Shield

Adafruit_PN532 nfc(IRQ, RESET);

//global vars
uint32_t ip;
SoftwareSerial lcd = SoftwareSerial(0,9);


/* Setup ----------------------------------------------------- */
// the setup routine runs once when you press reset:
void setup(void) {
  Serial.begin(115200);

  // nfc
  nfc.begin();
  uint32_t versiondata = nfc.getFirmwareVersion();
  nfc.SAMConfig();// configure board to read RFID tags

  // lcd
  lcd.begin(9600);
  // set the size of the display if it isn't 16x2 (you only have to do this once)
  lcd.write(0xFE);
  lcd.write(0xD1);
  lcd.write(16);  // 16 columns
  lcd.write(2);   // 2 rows

  screenClear();
  screenOn();

  lcd.println(); // @todo figure out why we need this to properly displ.
  lcd.println("Connecting...");
  // CC3K
  clientConnect();

  lcd.println("Success!");

  clientSend(SERVER, ENDPOINT, "HELLO");

  screenClear();
  screenOff();
}

/* Loop ------------------------------------------------------ */
// the loop routine runs over and over again forever:
void loop() {
  uint8_t success;
  uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 };  // Buffer to store the returned UID
  uint8_t uidLength;                        // Length of the UID (4 or 7 bytes depending on ISO14443A card type)
  String uidstr;
  boolean response;
  // Wait for an ISO14443A type cards (Mifare, etc.).  When one is found
  // 'uid' will be populated with the UID, and uidLength will indicate
  // if the uid is 4 bytes (Mifare Classic) or 7 bytes (Mifare Ultralight)
  success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength);

  if (success)
  {
    // Found an ISO14443A card
    // UID Value
    screenOn();
    delay(10);
    lcd.println(); // @todo figure out why we need this.
    lcd.print("Hello ");
    char uidtmp[(uidLength*2)+1];
    for (int i = 0; i < uidLength; i++)
    {
      lcd.print(String(uid[i], HEX));
      uidstr = uidstr + String(uid[i], HEX);
    }
    uidstr.toCharArray(uidtmp,(uidLength*2)+1);
    lcd.println();
    Serial.println(uidstr);

    response = clientSend(SERVER, ENDPOINT, uidtmp);
    Serial.println(response);
    if (response)
    {
      lcd.print("Connected!");
      delay(5000);
      screenClear();
      screenOff();
    }
    else
    {
      lcd.print("Oops!");
      delay(5000);
      screenClear();
      screenOff();
    }
  }


}

/* Functions ------------------------------------------------------ */
// @todo move these to tabs

// lcd
void screenOn() {
  lcd.write(0xFE);
  lcd.write(0x42);
  delay(100);
}

void screenOff() {
  lcd.write(0xFE);
  lcd.write(0x46);
  delay(10);
}

void screenClear() {
  lcd.write(0xFE);
  lcd.write(0x58);
  delay(10);
  // go 'home'
  lcd.write(0xFE);
  lcd.write(0x48);
  delay(10);
}


// web clients!
void clientConnect() {
  uint8_t macAddress[6];
  String macAddressStr;

  Serial.println("Getting MAC address");

  cc3000.getMacAddress(macAddress);

  for (int i = 0; i < sizeof(macAddress); i++) {
    macAddressStr = macAddressStr + String(macAddress[i], HEX);
    macAddressStr = macAddressStr + ".";
  }

  Serial.print("MAC address: ");
  Serial.println(macAddressStr);

  /* Initialise the module */
  if (!cc3000.begin() || !cc3000.connectToAP(WLAN_SSID, WLAN_PASS, WLAN_SECURITY))
  {
    Serial.println(F("Couldn't connect to access point!"));
  }
  Serial.println(F("Connected!"));

  /* Wait for DHCP to complete */
  while (!cc3000.checkDHCP())
  {
    delay(100); // ToDo: Insert a DHCP timeout!
  }
}

boolean clientSend(char *domain, char *endpoint, char *args) {
  /* Display the IP address DNS, Gateway, etc. */
  ip = 0;
  // Try looking up the SERVER's IP address
  Serial.print("Looking up ");
  Serial.println(domain);

  while (ip == 0)
  {
    if (! cc3000.getHostByName(domain, &ip))
    {
      Serial.println(F("Couldn't resolve!"));
      return false;
    }
    delay(500);
  }

  /* Try connecting to the SERVER.
     Note: HTTP/1.1 protocol is used to keep the server from closing the connection before all data is read.
  */
  Adafruit_CC3000_Client www = cc3000.connectTCP(ip, PORT);
  if (www.connected())
  {
    www.fastrprint(F("GET "));
    www.fastrprint(endpoint);
    www.fastrprint(args);
    www.fastrprint(F(" HTTP/1.1\r\n"));
    www.fastrprint(F("Host: ")); www.fastrprint(domain); www.fastrprint(F("\r\n"));
    www.fastrprint(F("\r\n"));
    www.println();
  }
  else
  {
    Serial.println(F("Connection failed"));
    return false;
  }

  /* Read data until either the connection is closed, or the idle timeout is reached. */
  // this is the important stuff. the c variable is the http response.
  unsigned long lastRead = millis();
  while (www.connected() && (millis() - lastRead < IDLE_TIMEOUT_MS))
  {
    while (www.available()) {
      char c = www.read();
      Serial.print(c);
      lastRead = millis();
    }
  }
  www.close();

  /* You need to make sure to clean up after yourself or the CC3000 can freak out */
  //cc3000.disconnect();
  return true;
}
