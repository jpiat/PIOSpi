#include "PioSPI.h" // necessary library


unsigned char test_data [] = {0, 1, 2, 3, 4, 5, 6};
unsigned char rx_buffer [7] ;

PioSPI spiBus(18, 21, 19, 5, 0, 0, 1000000);

void setup()
{
  Serial.begin(115200);
  spiBus.begin(); // wake up the SPI bus.
}

void testLoopBack(int value)
{

  spiBus.beginTransaction();
  spiBus.transfer(0); // send command byte
  spiBus.transfer(value); // send value (0~255)
  spiBus.endTransaction();
}

void loop()
{
  spiBus.transfer(test_data, rx_buffer, sizeof(test_data));
  for(int i = 0; i <  sizeof(test_data) ; i ++){
    if(rx_buffer[i] != test_data[i]){
      Serial.print("Error at byte : ");
      Serial.print(i);
      Serial.println(" !");
      break ;
    }
  }
  Serial.print("Test succeeded ! ");
  delay(100);
}
