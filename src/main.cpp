#include <SPI.h>
#include <LoRa.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// SX1278 pins
#define SCK     5    // GPIO5  -- SX1278's SCK
#define MISO    19   // GPIO19 -- SX1278's MISO
#define MOSI    27   // GPIO27 -- SX1278's MOSI
#define SS      18   // GPIO18 -- SX1278's CS
#define RST     14   // GPIO14 -- SX1278's RESET
#define DI0     26   // GPIO26 -- SX1278's IRQ(Interrupt Request)
#define BAND    868E6 // Europe freq

// OLED pins
#define OLED_SDA 4
#define OLED_SCL 15
#define OLED_RST -1
#define WIDTH 128
#define HEIGHT 64

Adafruit_SSD1306 display(WIDTH, HEIGHT, &Wire, OLED_RST);

unsigned long lastAction = 0;
int actionInterval = 100;

uint8_t deviceAddress = 0xEE;
const uint8_t broadcastAddress = 0xFF; 

int msgCount = 0;

// prototypes
void initDisplay();
void initLora();
void onReceive(int packetSize);
void sendMessage(uint8_t to, uint8_t msgId, String msg);
void updateDisplay();
void printError(String msg);


void setup()
{
    // Sanity check delay
    delay(2000);

    Serial.begin(115200);
    while(!Serial);
    Serial.println();
    Serial.println("[init] Serial... Ok");

    initDisplay();
    initLora();

    LoRa.onReceive(onReceive);
    LoRa.receive();

    Serial.println("[init] done.");    
}

void loop()
{
    if(millis() - lastAction > actionInterval) {
        lastAction = millis();
        // int packetSize = LoRa.parsePacket();

        // if(packetSize)
        //     onReceive(packetSize);
    }
}

/**
 * 
 */
void initDisplay()
{
    Serial.print("[init] SSD1306 (Display)... ");
    if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3c)) {
        Serial.println("allocation failed");
        while(1);        
    }
    Serial.println("Ok");

    display.clearDisplay();
    display.setCursor(0, 0);
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.setTextWrap(true);
    display.println(F("LoRa32 Echo"));
    display.display();
}

/**
 * 
 */
void initLora()
{
    Serial.print("[init] LoRa... ");
    SPI.begin(SCK, MISO, MOSI, SS);
    LoRa.setPins(SS, RST, DI0);

    if (!LoRa.begin(BAND)) {
        Serial.println("failed!");
        while(1);
    }
    Serial.println("Ok");
}

/**
 * 
 */
void onReceive(int packetSize)
{
    if (packetSize == 0) return;          // if there's no packet, return

    // packet structure
    // from address
    // to address
    // message id
    // message length - required since cb is used
    // message

    // read incoming packet
    // header
    uint8_t toAdr = LoRa.read();
    
    // return as early as possible
    if (toAdr != deviceAddress && toAdr != broadcastAddress)
        return;

    uint8_t fromAdr = LoRa.read();
    uint8_t msgId = LoRa.read();
    uint8_t msgLength = LoRa.read();
    // payload
    String msg = "";
    while(LoRa.available()) {
        msg += (char)LoRa.read();
    }

    if(msgLength != msg.length())
        printError("message length does not match.");

    // if message is for this device, or broadcast, print details:
    Serial.println("Received from: 0x" + String(fromAdr, HEX));
    Serial.println("Sent to: 0x" + String(toAdr, HEX));
    Serial.println("Message ID: " + String(msgId));
    Serial.println("Message length: " + String(msgLength));
    Serial.println("Message: " + msg);
    Serial.println("RSSI: " + String(LoRa.packetRssi()));
    Serial.println("Snr: " + String(LoRa.packetSnr()));
    Serial.println();

    sendMessage(fromAdr, msgId, msg);

    msgCount++;
    updateDisplay();
}

/**
 * 
 */
void sendMessage(uint8_t to, uint8_t msgId, String msg)
{
    LoRa.beginPacket();         // start packet

    LoRa.write(to);             // add destination address
    LoRa.write(deviceAddress);  // add sender address
    LoRa.write(msgId);          // add message ID
    LoRa.write(msg.length());   // add payload length
    LoRa.print(msg);            // add payload
    
    LoRa.endPacket(true);       // finish packet and send it  
}

/**
 * 
 */
void updateDisplay()
{
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("LoRa32 Echo");
    display.println("Message Count:");
    display.println(msgCount);
    display.display();
}

/**
 * 
 */
void printError(String msg)
{
    Serial.println("[error] "+ msg);
}