#include "PioSPI.h" // necessary library
unsigned char test_data [7] ;
unsigned char rx_buffer [7] ;
PioSPI spiBus(18, 19, 5, 22, SPI_MODE0, 25000000); //MOSI, MISO, SCK, CS, CPHA, CPOL, FREQUENCY

void setup()
{
  Serial.begin(115200);
  spiBus.begin(); // wake up the SPI bus.
}

void loop()
{ 
  for(int i = 0; i <  sizeof(test_data) ; i ++){
    test_data[i] = random(0, 256);
  }
  spiBus.beginTransaction(SPISettings(25000000, MSBFIRST, SPI_MODE0));
  spiBus.transfer(test_data, rx_buffer, sizeof(test_data));
  spiBus.endTransaction();
  for(int i = 0; i <  sizeof(test_data) ; i ++){
    if(rx_buffer[i] != test_data[i]){
      Serial.print("Error at byte : ");
      Serial.print(i);
      Serial.print(" : ");
      Serial.print(test_data[i]);
      Serial.print(" != ");
      Serial.print(rx_buffer[i]);
      Serial.println("  ");
    }
  }
  Serial.println("Test Done ");
  delay(1000);
}
