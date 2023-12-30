#include <SPI.h>
#include <FS.h>
// #include <SD.h>
#include <SD_MMC.h>
#include <LoRa.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// LoRa
// most LoRa pins are already defined in "pins_arduino.h" 
#define BAND    868E6 // Europe freq

// OLED pins
#define OLED_SDA 4
#define OLED_SCL 15
#define OLED_RST -1
#define WIDTH 128
#define HEIGHT 64

Adafruit_SSD1306 display(WIDTH, HEIGHT, &Wire, OLED_RST);

unsigned long lastAction = 0;
int actionInterval = 1;

uint8_t deviceAddress = 0xEE;
const uint8_t broadcastAddress = 0xFF;
uint8_t lastReceivedAddress = 0x00;
int lastRSSI = 0;

int msgCount = 0;

// prototypes
void initDisplay();
void initLora();
void initStorage();
void onReceive(int packetSize);
void sendMessage(uint8_t to, uint8_t msgId, String msg);
void updateDisplay();
void appendFile(const char *path, const char *message);
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
    initStorage();

    // LoRa.onReceive(onReceive);
    LoRa.receive();

    Serial.println("[init] done.");    
}

void loop()
{
    if(millis() - lastAction > actionInterval) {
        lastAction = millis();
        int packetSize = LoRa.parsePacket();

        if(packetSize)
            onReceive(packetSize);
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
    Serial.print("[init] LoRa (SX1276)... ");
    SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_CS);
    LoRa.setPins(LORA_CS, LORA_RST, LORA_IRQ);

    if (!LoRa.begin(BAND)) {
        Serial.println("failed!");
        while(1);
    }
    Serial.println("Ok");
}

/**
 * 
 */
void initStorage() 
{
    Serial.print("[init] Storage (SD MMC)... ");
    if(!SD_MMC.begin()){
        Serial.println("card mount failed");
        while(1);
    }
    Serial.println("Ok");

    Serial.println("[init] Storage info: ");

    Serial.print("[init] Card type: ");
    switch(SD_MMC.cardType()) {
        case CARD_NONE: Serial.println("No SD_MMC card attached. Try another TF/SD Card"); while(1);
        case CARD_MMC: Serial.println("mmc"); break;
        case CARD_SD: Serial.println("sdsc"); break;
        case CARD_SDHC: Serial.println("sdhc"); break;
        default: Serial.println("unkown"); break;
    }

    Serial.printf("[init] Card Size: %lluMB\n", SD_MMC.cardSize() / (1024 * 1024));
    Serial.printf("[init] Card total space: %lluMB\n", SD_MMC.totalBytes() / (1024 * 1024));
    Serial.printf("[init] Card used space: %lluMB\n", SD_MMC.usedBytes() / (1024 * 1024));

    if(!SD_MMC.exists("/data_lora"))
        SD_MMC.mkdir("/data_lora");

    if(!SD_MMC.exists("/data_lora/data.csv")) {
        File f = SD_MMC.open("/data_lora/data.csv", "w+");
        f.print("Longitude;Latitude;Value");
        f.close();
    }
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
    // location lat (from latitude)
    // location lng (from longitude)
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
    
    double latitude, longitude;
    LoRa.readBytes((uint8_t*)&latitude, sizeof(latitude));
    LoRa.readBytes((uint8_t*)&longitude, sizeof(longitude));

    uint8_t msgId = LoRa.read();
    uint8_t msgLength = LoRa.read();
    // payload
    String msg = "";
    while(LoRa.available()) {
        msg += (char)LoRa.read();
    }

    if(msgLength != msg.length())
        printError("message length does not match.");

    lastReceivedAddress = fromAdr;
    lastRSSI = LoRa.packetRssi();

    // if message is for this device, or broadcast, print details:
    Serial.println("Received from: 0x" + String(fromAdr, HEX));
    Serial.println("Sent to: 0x" + String(toAdr, HEX));
    Serial.println("Latitude: "+ String(latitude, 7));
    Serial.println("Longitude: "+ String(longitude, 7));
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

    LoRa.endPacket();       // finish packet and send it  
}

void fill(String &str, uint8_t totalLength, const char* chr = "0")
{
    for(uint8_t i = str.length(); i < totalLength; i++)
        str = chr + str;
}

/**
 * 
 */
void updateDisplay()
{
    String numReplies = String(msgCount);
    fill(numReplies, 7);
    
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("LoRa32 Echo");
    display.setCursor(0, display.getCursorY() + 10);
    display.println("Total replies:");
    display.setCursor(0, display.getCursorY() + 5);
    display.println(numReplies);
    display.println("last from: 0x"+ String(lastReceivedAddress, HEX));
    display.println("last RSSI: "+ String(lastRSSI));
    display.display();
}

/**
 * 
 */
void storeData(double latitude, double longitude, int rssi)
{
    const char *data = "";
    appendFile("/data_lora/data.csv", data);
}

/**
 * 
 */
void appendFile(const char *path, const char *message)
{
    File file = SD_MMC.open(path, "a");
    if(!file) {
        Serial.println("Failed to open file for appending");
        return;
    }

    if(file.print(message)) {
        Serial.println("Message appended");
        return;
    }

    Serial.println("Append failed");
}

/**
 * 
 */
void printError(String msg)
{
    Serial.println("[error] "+ msg);
}