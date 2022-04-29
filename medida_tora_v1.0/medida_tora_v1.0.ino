#include <Wire.h>
#include <WiFi.h>

//const char* ssid     = "Andre Wifi";
//const char* password = "090519911327";

const char* ssid     = "ForSellEscritorio";
const char* password = "forsell1010";

int statusWait = 0;
int count;
float medidaAnterior;
int ledVerde = 18;
int ledVermelho = 19;
int qntTora = 0;
float diferencaMedidas = 0;

float medidaLocal = 770;

WiFiServer server(80);


void SensorRead(unsigned char addr, unsigned char* datbuf, unsigned char cnt) {
  //unsigned short result=0;
  // step 1: instruct sensor to read echoes
  Wire.beginTransmission(82); // transmit to device #82 (0x52)
  // the address specified in the datasheet is 164 (0xa4)
  // but i2c adressing uses the high 7 bits so it's 82
  Wire.write(byte(addr));      // sets distance data address (addr)
  Wire.endTransmission();      // stop transmitting
  // step 2: wait for readings to happen
  delay(1);                   // datasheet suggests at least 30uS
  // step 3: request reading from sensor
  Wire.requestFrom(82, cnt);    // request cnt bytes from slave device #82 (0x52)
  // step 5: receive reading from sensor
  if (cnt <= Wire.available()) { // if two bytes were received
    *datbuf++ = Wire.read();  // receive high byte (overwrites previous reading)
    *datbuf++ = Wire.read(); // receive low byte as lower 8 bits
  }
}
float ReadDistance() {
  
  unsigned short lenth_val = 0;
  unsigned char i2c_rx_buf[16];
  unsigned char dirsend_flag = 0;  
    
  SensorRead(0x00, i2c_rx_buf, 2);
  lenth_val = i2c_rx_buf[0];
  lenth_val = lenth_val << 8;
  lenth_val |= i2c_rx_buf[1];
  delay(250);
    
  return lenth_val;
    
}

void setup() {
  Wire.begin();
  Serial.begin(115200);

  pinMode(ledVerde, OUTPUT);
  pinMode(ledVermelho, OUTPUT);  
  digitalWrite(ledVerde, LOW);
  digitalWrite(ledVermelho, LOW);

  Serial.println();
  Serial.println();
  Serial.print("Conectando ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
  delay(500);
  Serial.print(".");
  digitalWrite(ledVerde, HIGH);
  digitalWrite(ledVermelho, HIGH);
  delay(200);
  digitalWrite(ledVerde, LOW);
  digitalWrite(ledVermelho, LOW);
  delay(200);  
  }

  Serial.println("");
  Serial.println("WiFi conectado.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.println("");
  server.begin();
  
}

void piscaLed(int led, int interv_ms)
{
          if((int)(millis()/interv_ms)%2==0)
                  digitalWrite(led, HIGH);
         else
                  digitalWrite(led, LOW);
}

void cliente() {

 WiFiClient client = server.available();   // listen for incoming clients 

  if (client) {                             // if you get a client,
    
    Serial.println("New Client.");           // print a message out the serial port
    String currentLine = "";                // make a String to hold incoming data from the client
    while (client.connected()) {            // loop while the client's connected
      if (client.available()) {             // if there's bytes to read from the client,
        char c = client.read();             // read a byte, then
        Serial.write(c);                    // print it out the serial monitor
        if (c == '\n') {                    // if the byte is a newline character

          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
          if (currentLine.length() == 0) {
            // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
            // and a content-type so the client knows what's coming, then a blank line:
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println();
            client.print(medidaAnterior,1);
            client.print(" ");
            client.print("cm");  
//            client.print(" ");  
//            client.print(qntTora);
//            client.print(" ");                                
            // The HTTP response ends with another blank line:
            client.println();
            // break out of the while loop:
            break;
          } else {    // if you got a newline, then clear currentLine:
            currentLine = "";
          }
        } else if (c != '\r') {  // if you got anything else but a carriage return character,
          currentLine += c;      // add it to the end of the currentLine
        }

      }
    }
    // close the connection:
    //delay(2000);
    client.stop();
    Serial.println("Client Disconnected.");    
    Serial.println("Enviado com sucesso!");
     
  }
}

void loop() {

  medidaAnterior = 0;
  float medidaAtual = ReadDistance(); 
  float alturaLocal = medidaAtual / 10;
  
  diferencaMedidas = medidaLocal - medidaAtual;  
  
  float tora = diferencaMedidas / 10; 
 // delay(500);
  
 if (tora < 50) {

    Serial.println("Sem toras para medir!");
    digitalWrite(ledVerde, HIGH);
    delay(500);
    digitalWrite(ledVermelho, HIGH);
    
    //delay(2000);

  }

  else  if (tora >= 50) {

    Serial.println("Se preparando para medir a tora...");
    float packet = tora;
    
    while(packet >= 50) {

     float mA = ReadDistance(); 
     float aL = mA / 10;  
     float dM = medidaLocal - mA;    
     packet = dM / 10;

        if(packet >= medidaAnterior){
          medidaAnterior = packet;
        }      
    }
     Serial.println("Saiu do while");
     count = 1;
     qntTora++;
     //delay(2000);
   
  } 

 if(count == 1) {  

  Serial.println("Enviando medida para o web server...");
  cliente();
  delay(2000);
  digitalWrite(ledVerde, LOW);
  digitalWrite(ledVermelho, HIGH);
  delay(500);
  digitalWrite(ledVermelho, LOW);
  delay(500);
  digitalWrite(ledVermelho, HIGH);
  delay(500);
  digitalWrite(ledVermelho, LOW);
  delay(500);
  digitalWrite(ledVermelho, HIGH);
  delay(500);
  digitalWrite(ledVermelho, LOW);
  delay(1000);
  digitalWrite(ledVerde, HIGH);
  count = 0;
}
  Serial.println(" ");
  Serial.print("Altura local: ");
  Serial.print(alturaLocal,1);
  Serial.println(" cm"); 
  Serial.print("Altura do tora: ");
  Serial.print(medidaAnterior,1);
  Serial.print(" cm");
  Serial.println(" ");
  
} 
