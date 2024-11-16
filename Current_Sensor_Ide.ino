//biblioteci bluetooth
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

//biblioteci lcd
#include <Adafruit_GFX.h>      // Biblioteca grafică generală
#include <Adafruit_ST7735.h>   // Biblioteca specifică pentru ST7735

//biblioteca atenuare masuratoare
#include "esp_adc_cal.h" 

// Definirea pinilor pentru ESP32 conform conexiunilor
#define TFT_CS     5    
#define TFT_RST    4  
#define TFT_DC     2    
#define TFT_SCLK   18  
#define TFT_MOSI   23   

// Crearea display
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

// UUID-uri pentru serviciu și caracteristică
#define SERVICE_UUID           "12345678-1234-1234-1234-123456789012"
#define CHARACTERISTIC_UUID    "87654321-4321-4321-4321-210987654321"

// Declarații globale
BLECharacteristic *pCharacteristic;
bool deviceConnected = false;
unsigned long previousMillis = 0;
const unsigned long interval = 20;
//unghi sin
float angle = 0;
// Callback pentru gestionarea conexiunii
class MyServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
        deviceConnected = true;
    }

    void onDisconnect(BLEServer* pServer) {
        deviceConnected = false;
    }
};

const int pin = 34; //pin output op-amp
const int po=32; //pin offset
const int citiri=1000;//nr citiri
int peakValue=0;//val max
esp_adc_cal_characteristics_t *adc_chars;//

void setup(void) {
    Serial.begin(115200);
    BLEDevice::init("ESP32_SenzorCurent"); // Numele dispozitivului

    // Crearea serverului BLE
    BLEServer *pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks()); 

    // Crearea serviciului BLE
    BLEService *pService = pServer->createService(SERVICE_UUID);

    // Crearea caracteristicii BLE
    pCharacteristic = pService->createCharacteristic(
                        CHARACTERISTIC_UUID,
                        BLECharacteristic::PROPERTY_READ |
                        BLECharacteristic::PROPERTY_NOTIFY
                      );

    pCharacteristic->addDescriptor(new BLE2902());

    // Pornirea serviciului
    pService->start();

    // Activarea publicării
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->start();
    
    Serial.println("BLE Server is running...");

  // Activare display-ului
  tft.initR(INITR_BLACKTAB);   
  tft.setRotation(0);         

  // Umplere ecran
  tft.fillScreen(ST7735_BLACK);
  tft.setTextColor(ST7735_WHITE);
  tft.setTextSize(2);

  //mesajele fixe
  tft.setCursor(10, 10);      
  tft.println("Curent:");

  tft.setCursor(10, 70);
  tft.println("Putere:");
  
  //pregatire masuratoare cu atenuare
    adc_chars = (esp_adc_cal_characteristics_t *)calloc(1, sizeof(esp_adc_cal_characteristics_t));
    esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, 1100, adc_chars);
    analogSetAttenuation(ADC_11db); // Atenuare la 11dB pentru interval complet de măsurare 0-3.3V 
}

void afiseazaValoare(float valoare, int x, int y) {
  // Șterge zona anterioară a valorii pentru a evita suprapunerea
  tft.fillRect(x, y, 110, 20, ST77XX_BLACK); // Șterge un dreptunghi
  
  // Setează cursorul la poziția dorită
  tft.setCursor(x, y);
  
  tft.print(valoare, 1);
  tft.print(" W");
}

void loop() {

  tft.fillRect(10, 40, 100, 20, ST7735_BLACK);  // Șterge vechea valoare (cu un dreptunghi negru)
  tft.setCursor(10, 40);                        // Setează cursorul pentru textul următor
  tft.setTextColor(ST7735_WHITE);
  tft.setTextSize(2);

    // Citire tensiune pentru offset
    uint32_t rawValue2 = analogRead(po); 
    int32_t offset = esp_adc_cal_raw_to_voltage(rawValue2, adc_chars); 
    
    // Citire valoare brută de la senzor
    peakValue = 0;
    uint32_t rawValue;
    for(int i = 0; i < citiri; i++) 
    {
        rawValue = analogRead(pin);
        if(rawValue > peakValue) 
        {
            peakValue = rawValue;
        }
    }
    // Conversie tensiune
    int32_t voltage = esp_adc_cal_raw_to_voltage(peakValue, adc_chars); 
    float current = (float(voltage - offset) / 1000.0) / 1.41 * 30;  // Calcul curent
    if(current<1.2)
    {
      current=0;
    }
    tft.print(current, 2);
    tft.println(" A");
    
    float power = current * 230;  
    afiseazaValoare(power, 10, 100); 

    float peakCurrent = current*1.41;
    unsigned long currentMillis = millis();
    if (deviceConnected && (currentMillis - previousMillis >= interval)) {
        previousMillis = currentMillis;

        // Generare valoare sinusoidală
        float sineValue = peakCurrent * sin(angle);  // Calcul sinusoida
        angle += 0.7;  // Crește unghiul pentru sin
        if (angle >= 2 * PI) angle = 0;  // Resetează la 0 când ajunge la 2pi

        // Trimite valoarea
        pCharacteristic->setValue(sineValue);
        pCharacteristic->notify();
        
        Serial.println(sineValue);
    }
  delay(1000);
