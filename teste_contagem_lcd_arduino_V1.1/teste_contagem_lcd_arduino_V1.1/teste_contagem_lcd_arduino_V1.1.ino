#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <Keypad.h>
#include <LiquidCrystal_I2C.h>
#include <SPIFFS.h>
#include <Timestamps.h>
#include <sys/time.h>
#include <time.h>
#include "RTClib.h"                         

#include "nvs_flash.h"  
#include "esp_task_wdt.h"

#define MAC_ADDRESS "B8:27:EB:B0:21:80"
#define SENSOR 18
#define RESET_BUTTON 5
#define CHAVE_NVS  ""

const char *ssid = "ForSellEscritorio";
const char *password = "forsell1010";

const byte ROWS = 4;  //quatro linhas
const byte COLS = 4;  //quatro colunas
byte rowPins[ROWS] = { 32, 33, 25, 26 };
byte colPins[COLS] = { 27, 14, 12, 13 };

char keys[ROWS][COLS] = {
    {'1', '2', '3', 'A' },
    {'4', '5', '6', 'B' },
    {'7', '8', '9', 'C' },
    {'*', '0', '#', 'D' }
    };

bool s_high = 0;
bool b_high = 0;
bool menu_return = false;
bool validar_Entrada_Manual = false;
int count = 0;
int x = 10, numero;

//Arrays para criação dos segmentos e customização dos números
byte LT[8]  = {B01111,  B11111,  B11111,  B11111,  B11111,  B11111,  B11111,  B11111};
byte UB[8]  = {B11111,  B11111,  B11111,  B00000,  B00000,  B00000,  B00000,  B00000};
byte RT[8]  = {B11110,  B11111,  B11111,  B11111,  B11111,  B11111,  B11111,  B11111};
byte LL[8]  = {B11111,  B11111,  B11111,  B11111,  B11111,  B11111,  B11111,  B01111};
byte LB[8]  = {B00000,  B00000,  B00000,  B00000,  B00000,  B11111,  B11111,  B11111};
byte LR[8]  = {B11111,  B11111,  B11111,  B11111,  B11111,  B11111,  B11111,  B11110};
byte UMB[8] = {B11111,  B11111,  B11111,  B00000,  B00000,  B00000,  B11111,  B11111};
byte LMB[8] = {B11111,  B00000,  B00000,  B00000,  B00000,  B11111,  B11111,  B11111};

int value;
int menu_num = 1;
int sub_menu = 1;

long time1;

uint16_t ano;
uint8_t mes;
uint8_t dia;
uint8_t hora;
uint8_t minuto;
uint8_t segundo;

String inputString;
long inputInt;

String myFilePath = "/count.txt";

long timezone = -3;
byte daysavetime = 0; // Daylight saving time (horario de verão)

//Protótipos
void get_Keypad_Buttons();
void count_Sensor();
void menu_Entrada_Manual();
void menu_Voltar();
void menu_Enviar();
void menu_Reset_LCD();
void invalida_Enviar_Serial();
void grava_dado_nvs(uint32_t dado);
uint32_t le_dado_nvs(void);
void IRAM_ATTR funcao_ISR();
void TASK_Check_Reset_Button(void *p);

//Objetos
LiquidCrystal_I2C lcd(0x27, 20, 4);
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);
WiFiClient client;
HTTPClient http;
File pFile;      // Ponteiro do arquivo
RTC_DS3231 rtc;

void setup()
{
  Serial.begin(9600); 
  //Wire.setClock(10000);

  time1 = millis();
  
//  le_dado_nvs();
//  lcd.init();
//  lcd.backlight();  
//  split_Number(count);  
//  lcd.createChar(0, LT);
//  lcd.createChar(1, UB);
//  lcd.createChar(2, RT);
//  lcd.createChar(3, LL);
//  lcd.createChar(4, LB);
//  lcd.createChar(5, LR);
//  lcd.createChar(6, UMB);
//  lcd.createChar(7, LMB);
  
  pinMode(SENSOR, INPUT_PULLUP);
  pinMode(RESET_BUTTON, INPUT);     

  //check_wifi();
  //check_RTC();

   esp_task_wdt_init(10, true);
   //esp_task_wdt_add(NULL);
   disableCore0WDT();

  xTaskCreatePinnedToCore(
      TASK_Check_Reset_Button,   /*função que implementa a tarefa */
      "TASK_Check_Reset_Button", /*nome da tarefa */
      10000,                     /*número de palavras a serem alocadas para uso com a pilha da tarefa */
      NULL,                      /*parâmetro de entrada para a tarefa (pode ser NULL) */
      1,                         /*prioridade da tarefa (0 a N) */
      NULL,                      /*referência para a tarefa (pode ser NULL) */
      1);                        /*Núcleo que executará a tarefa */

  xTaskCreatePinnedToCore(
      TASK_LCD_Begin,            /*função que implementa a tarefa */
      "TASK_LCD_Begin",          /*nome da tarefa */
      10000,                     /*número de palavras a serem alocadas para uso com a pilha da tarefa */
      NULL,                      /*parâmetro de entrada para a tarefa (pode ser NULL) */
      2,                         /*prioridade da tarefa (0 a N) */
      NULL,                      /*referência para a tarefa (pode ser NULL) */
      1);      

}

void TASK_Check_Reset_Button(void *p)
{
  /* Botão de reset do sistema */
  for (;;)
  {    
    if(digitalRead(RESET_BUTTON)== HIGH)
    {
        ESP.restart();  
    }
    
    vTaskDelay(100 / portTICK_PERIOD_MS);
    
  }
}

void TASK_LCD_Begin(void *p)
{
  /* Reseta o LCD */
  for (;;)
  {    
    if(millis() - time1 > 1000)
    {
      time1 = millis();
      //Wire.setClock(10000);
      lcd.init();
      lcd.backlight(); 
      lcd.clear(); 
      split_Number(count);  
      lcd.createChar(0, LT);
      lcd.createChar(1, UB);
      lcd.createChar(2, RT);
      lcd.createChar(3, LL);
      lcd.createChar(4, LB);
      lcd.createChar(5, LR);
      lcd.createChar(6, UMB);
      lcd.createChar(7, LMB);
    } 
       
    vTaskDelay(250 / portTICK_PERIOD_MS);
    
  }
}

void loop()
{
  get_Keypad_Buttons();  

 if(validar_Entrada_Manual == false)
 {
  if (value == HIGH)
  {
    b_high = true;
  }

  count_Sensor();
 }
}

void menu_Entrada_Manual()
{

  if(menu_return == true)
    {  
      //Wire.setClock(10000); 
      lcd.clear();   
      lcd.setCursor(0, 1);
      lcd.print("ENTRADA MANUAL: ");
      lcd.setCursor(15, 1);
      lcd.print(inputString);
      validar_Entrada_Manual = true;           
    }
              
    else if(menu_return == false)
    {
      if(inputString != "")
      {
        //menu_Enviar("");
      }
      
      else
      {
      //Wire.setClock(10000);
      lcd.clear();
      split_Number(count);
      validar_Entrada_Manual = false;
      inputString = "";
      }      
    }
}
  
void menu_Voltar()
{ 
  //Wire.setClock(10000);
  lcd.clear();
  inputString = "";
  count = 0;
  split_Number(count);
  menu_Entrada_Manual(); 
   
}

void menu_Enviar(String value)
{
  if (validar_Entrada_Manual && inputString != "")
  {
    //Wire.setClock(10000);
    lcd.setCursor(0, 2);
    lcd.print("");
    //Serial.print("M");
    Serial.print(inputString);    
    Serial.println(value);
    b_high = false;    
    lcd.clear();    
    split_Number(count);      
    //send_POST('M', inputString.toInt());
    count = 0;
    inputString = "";
    grava_dado_nvs(count);  
    validar_Entrada_Manual = false;  
    menu_Entrada_Manual();
    delay(150);  
  }
  else if (count > 0 && !validar_Entrada_Manual)
  {
    //Wire.setClock(10000);
    lcd.setCursor(0, 2);
    lcd.print("");    
    //Serial.print("S");    
    Serial.print(count);
    Serial.println(value);
    b_high = false;    
    //send_POST('S', count);
    count = 0;
    inputString = "";
    grava_dado_nvs(count);
    lcd.clear();    
    split_Number(count);
    //validar_Entrada_Manual = true;      
    delay(150);    
  }
  else
  {
    //menu_Reset_LCD();
  }
}

void menu_Reset_LCD()
{
  count = 0;
  inputString = "";
  //validar_Entrada_Manual = false;
  grava_dado_nvs(count);
  lcd.clear();  // Limpa o display  
  split_Number(count);
}

void count_Sensor()
{
  if (digitalRead(SENSOR))
  {
    s_high = true;
  }

  if (!digitalRead(SENSOR) && s_high == true)
  {
    //Wire.setClock(10000);
    s_high = false;
    count++;
    lcd.clear();  // Limpa o display   
    split_Number(count);
    grava_dado_nvs(count);      
    delay(150);
  }
}

void armazena_Digito_Manual(char digit)
{
  if (validar_Entrada_Manual)
  {
    if (inputString.length() <= 3)
    {
      inputString += digit;
      menu_Entrada_Manual();
    }
    else
    {
      menu_Entrada_Manual();
    }
  }
}

void get_Keypad_Buttons()
{
  char key = keypad.getKey();

  if (key)
  {
    switch (key)
    {
      case 'A':
      if(count != 0 || inputString != "")
      {
        menu_Enviar("CAPA");
      }        
        break;

      case 'B':
      if(count != 0 || inputString != "")
      {
        menu_Enviar("ENCHIMENTO");
      }      
        break;
      case 'C':
      if(count != 0 || inputString != "")
      {
        menu_Enviar("MIOLO");
      }      
        break;

      case 'D':
        //menu_Enviar();
        break;

      case '0':
        armazena_Digito_Manual('0');
        break;

      case '1':
        armazena_Digito_Manual('1');
        break;

      case '2':
        armazena_Digito_Manual('2');
        break;

      case '3':
        armazena_Digito_Manual('3');
        break;

      case '4':
        armazena_Digito_Manual('4');
        break;

      case '5':
        armazena_Digito_Manual('5');
        break;

      case '6':
        armazena_Digito_Manual('6');
        break;

      case '7':
        armazena_Digito_Manual('7');
        break;

      case '8':
        armazena_Digito_Manual('8');
        break;

      case '9':
        armazena_Digito_Manual('9');
        break;
        
      case '*':
      menu_return = !menu_return;
      menu_Entrada_Manual();
      break;

      case '#':
      menu_Voltar();
      //menu_Reset_LCD();
      break;
    }
  }
}

void grava_dado_nvs(uint32_t dado)
{
    nvs_handle handler_particao_nvs;
    esp_err_t err;
    
    err = nvs_flash_init_partition("nvs");
     
    if (err != ESP_OK)
    {
        Serial.println("[ERRO] Falha ao iniciar partição NVS.");           
        return;
    }
 
    err = nvs_open_from_partition("nvs", "ns_nvs", NVS_READWRITE, &handler_particao_nvs);
    if (err != ESP_OK)
    {
        Serial.println("[ERRO] Falha ao abrir NVS como escrita/leitura"); 
        return;
    }
 
    /* Atualiza valor do count */
    err = nvs_set_u32(handler_particao_nvs, CHAVE_NVS, dado);
 
    if (err != ESP_OK)
    {
        Serial.println("[ERRO] Erro ao gravar na memória interna");                   
        nvs_close(handler_particao_nvs);
        return;
    }
    else
    {
        //Serial.println("Dado gravado com sucesso!");     
        nvs_commit(handler_particao_nvs);    
        nvs_close(handler_particao_nvs);      
    }
}

uint32_t le_dado_nvs()
{
    nvs_handle handler_particao_nvs;
    esp_err_t err;
    uint32_t dado_lido;
     
    err = nvs_flash_init_partition("nvs");
     
    if (err != ESP_OK)
    {
        Serial.println("[ERRO] Falha ao iniciar partição NVS.");         
        return 0;
    }
 
    err = nvs_open_from_partition("nvs", "ns_nvs", NVS_READWRITE, &handler_particao_nvs);
    if (err != ESP_OK)
    {
        Serial.println("[ERRO] Falha ao abrir NVS como escrita/leitura");         
        return 0;
    }
 
    /* Faz a leitura do dado associado a chave definida em CHAVE_NVS */
    err = nvs_get_u32(handler_particao_nvs, CHAVE_NVS, &dado_lido);
     
    if (err != ESP_OK)
    {
        Serial.println("[ERRO] Falha ao fazer leitura do dado");         
        return 0;
    }
    else
    {
        count = dado_lido;
        //Serial.println("Dado lido com sucesso!");  
        nvs_close(handler_particao_nvs);   
        return dado_lido;
    }
}

void custom0()//Seleciona os segmentos para formar o numero 0
{
  lcd.setCursor(x, 1); //Seleciona a linha superior
  lcd.write((byte)0);  //Segmento 0 selecionado
  lcd.write(1);  //Segmento 1 selecionado
  lcd.write(2);
  lcd.setCursor(x, 2); //Seleciona a linha inferior
  lcd.write(3);
  lcd.write(4);
  lcd.write(5);
  
}
void custom1() //Seleciona os segmentos para formar o numero 1
{
  lcd.setCursor(x, 1);
  lcd.write(1);
  lcd.write(2);
  lcd.setCursor(x + 1, 2);
  lcd.write(5);
}
void custom2() //Seleciona os segmentos para formar o numero 2
{  
  lcd.setCursor(x, 1);
  lcd.write(6);
  lcd.write(6);
  lcd.write(2);
  lcd.setCursor(x, 2);
  lcd.write(3);
  lcd.write(7);
  lcd.write(7);
}
void custom3()  //Seleciona os segmentos para formar o numero 3
{
  lcd.setCursor(x, 1);
  lcd.write(6);
  lcd.write(6);
  lcd.write(2);
  lcd.setCursor(x, 2);
  lcd.write(7);
  lcd.write(7);
  lcd.write(5);
  
}
void custom4()  //Seleciona os segmentos para formar o numero 4
{
  lcd.setCursor(x, 1);
  lcd.write(3);
  lcd.write(4);
  lcd.write(2);
  lcd.setCursor(x + 2, 2);
  lcd.write(5);
}
void custom5()  //Seleciona os segmentos para formar o numero 5
{
  lcd.setCursor(x, 1);
  lcd.write((byte)0);
  lcd.write(6);
  lcd.write(6);
  lcd.setCursor(x, 2);
  lcd.write(7);
  lcd.write(7);
  lcd.write(5);
}
void custom6()  //Seleciona os segmentos para formar o numero 6
{
  lcd.setCursor(x, 1);
  lcd.write((byte)0);
  lcd.write(6);
  lcd.write(6);
  lcd.setCursor(x, 2);
  lcd.write(3);
  lcd.write(7);
  lcd.write(5);
}
void custom7() //Seleciona os segmentos para formar o numero 7
{
  lcd.setCursor(x, 1);
  lcd.write(1);
  lcd.write(1);
  lcd.write(2);
  lcd.setCursor(x + 1, 2);
  lcd.write((byte)0);
}
void custom8()  //Seleciona os segmentos para formar o numero 8
{
  lcd.setCursor(x, 1);
  lcd.write((byte)0);
  lcd.write((byte)6);
  lcd.write(2);
  lcd.setCursor(x, 2);
  lcd.write(3);
  lcd.write(7);
  lcd.write(5);
}
void custom9()  //Seleciona os segmentos para formar o numero 9
{
  lcd.setCursor(x, 1);
  lcd.write((byte)0);
  lcd.write((byte)6);
  lcd.write((byte)2);
  lcd.setCursor(x + 2, 2);
  lcd.write((byte)5);
}
void mostranumero() //Mostra o numero na posicao definida por "X"
{
  switch (numero)
  {
    case 0: custom0();
      break;
    case 1: custom1();
      break;
    case 2: custom2();
      break;
    case 3: custom3();
      break;
    case 4: custom4();
      break;
    case 5: custom5();
      break;
    case 6: custom6();
      break;
    case 7: custom7();
      break;
    case 8: custom8();
      break;
    case 9: custom9();
      break;
  }
}

void split_Number(int value)
{
  if(value < 100)
  {
    //Primeiro digito 
    x = 7;
    numero = value / 10;
    mostranumero();
    
    //Segundo digito 
    x = 11;
    numero = value % 10;    
    mostranumero();
   
   }
   else if(value >= 100 && value < 999)  
   {
    //Primeiro digito 
    x = 4;
    numero = value / 100;
    mostranumero();    
    
    //Segundo digito 
    x = 8;
    numero = (value / 10) % 10;    
    mostranumero();    

    //Terceiro digito 
    x = 12;
    numero = value % 10;    
    mostranumero();
    
   }
   else if(value > 999 && value <= 9999)  
   {
    //Primeiro digito 
    x = 2;
    numero = value / 1000;
    mostranumero();    
       
    //Segundo digito 
    x = 6;
    numero = (value / 100) % 10;    
    mostranumero();       

    //Terceiro digito 
    x = 10;
    numero = (value / 10) % 10;     
    mostranumero();   
    
    //Quarto digito 
    x = 14;
    numero = value % 10;    
    mostranumero();    
  
   }
   else if(value > 9999)  
   {
    //Primeiro digito 
    x = 2;
    numero = 9999 / 1000;
    mostranumero();    
        
    //Segundo digito 
    x = 6;
    numero = (9999 / 100) % 10;    
    mostranumero();    
    
    //Terceiro digito 
    x = 10;
    numero = (9999 / 10) % 10;     
    mostranumero();    
      
    //Quarto digito 
    x = 14;
    numero = 9999 % 10;    
    mostranumero(); 
    count = 9999;   
    
   }
  
}

void send_POST(char tipo_entrada, int count)
{
  //Serial.println("Iniciando o envio do POST.");

  char logdata[150];
  //Verifique o status da conexão WiFi
  if (WiFi.status() == WL_CONNECTED)  {
    
    HTTPClient http;
    
    // Especifique o destino para a solicitação HTTP
    http.begin("http://192.168.1.127/php/query_insert_count.php");
    http.addHeader("Content-Type", "application/x-www-form-urlencoded"); 
    //http.addHeader("Connection", "close");    
    DateTime now = rtc.now();      
    dia = now.day();
    mes = now.month();
    ano = now.year();
    hora = now.hour();
    minuto = now.minute();
    segundo = now.second();
    long randNumber = random(999999);
    char myData[150];
    sprintf(myData, "%04d-%02d-%02d %02d:%02d:%02d.%ld", ano, mes, dia, hora, minuto, segundo, randNumber);
    char *myItem = (char *)malloc(strlen(myData) + 1);
    strcpy(myItem, myData);   
    sprintf(logdata, "on_off=1&mac_address=%s&tipo_entrada=%c&data_hora=%s&contagem=%d\n", MAC_ADDRESS, tipo_entrada, myItem, count);
    
    
    int httpResponseCode = http.POST(logdata); //publica o post
    //Serial.print("http code: ");
    //Serial.println(httpResponseCode);
    
    //verifica se foi possível fazer o insert com post
    if (httpResponseCode > 0)
    {
      String response = http.getString(); //Obtém a resposta do request
      Serial.println(response);

      if (response.toInt() != 1) {
               Serial.println("Insert não inserido no banco com sucesso =(");
               Serial.println("Salvando dados na memória flash...");
               pFile = SPIFFS.open(myFilePath, FILE_APPEND);
               if (pFile.print(logdata)) {
                  Serial.println("");
                  Serial.println("Mensagem anexada com sucesso!!!");
               } else {
                  Serial.println("");
                  Serial.print("Falha ao anexar!");
               }

               pFile.close();
            } else {               
               Serial.println("Insert realizado com sucesso!");
            }
      //Se o INSERT no banco não retornar um "OK", salva na memória flash.      
    }    
    
  }
  Serial.println("");
}

uint8_t Get_NTP(void) 
{
   struct tm timeinfo;

   timeval epoch = {946684800, 0}; // timeval is a struct: {tv_sec, tv_usec}. Old data for detect no replay from NTP. 1/1/2000    
   settimeofday( & epoch, NULL); // Set internal ESP32 RTC

   Serial.println("Entrando em contato com o servidor NTP");
   configTime(3600 * timezone, daysavetime * 3600, "a.st1.ntp.br", "a.ntp.br", "gps.ntp.br"); // initialize the NTP client 
   // using configTime() function to get date and time from an NTP server.

   if (getLocalTime( & timeinfo, 1000) == 0) // transmits a request packet to a NTP server and parse the received 
   {
      // time stamp packet into to a readable format. It takes time structure
      // as a parameter. Second parameter is server replay timeout
      Serial.println("Sem conexão com servidor NTP");
      return false; // Something went wrong
      ESP.restart();
   }

   Serial.println("Resposta do servidor NTP");
   Serial.printf("Agora: %02d-%02d-%04d %02d:%02d:%02d\n", timeinfo.tm_mday, timeinfo.tm_mon + 1, timeinfo.tm_year + 1900, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
   //Serial.println("-------------------------");
   return true; // All OK and go away
}

void check_RTC()
{
  if (!rtc.begin()) {
    Serial.println("Não foi possivel encontrar o RTC!");
    lcd.clear();
    lcd.setCursor(0, 2);
    lcd.print("Falha ao ler RTC!");
    delay(3000);
    ESP.restart();
  }

  if (Get_NTP() == false) {
      // Get time from NTP
      Serial.println("Timeout na conexão com servidor NTP");      
      //ESP.restart();
      Serial.println("Utilizando horario armazenado no RTC: ");      

      //rtc.setClockMode(false);
      DateTime now = rtc.now();      

      dia = now.day();
      mes = now.month();
      ano = now.year();
      hora = now.hour();
      minuto = now.minute();
      segundo = now.second();
      
      Serial.printf("Agora: %02d-%02d-%04d %02d:%02d:%02d\n", dia, mes, ano, hora, minuto, segundo);
      Serial.println("----------------------------------------"); 
      
   } else {
      struct tm timeinfo;
      getLocalTime( & timeinfo);  

      dia = timeinfo.tm_mday;
      mes = timeinfo.tm_mon + 1;
      ano = timeinfo.tm_year + 1900;
      hora = timeinfo.tm_hour;
      minuto = timeinfo.tm_min;
      segundo = timeinfo.tm_sec;     
    
      Serial.println("RTC atualizado pelo servidor NTP.");     
       
      rtc.adjust(DateTime(ano, mes, dia, hora, minuto, segundo));      
      
      Serial.printf("Agora: %02d-%02d-%04d %02d:%02d:%02d\n", dia, mes, ano, hora, minuto, segundo);
      Serial.println("----------------------------------------"); 
   }     

    return_Temp();

    Serial.println("----------------------------------------"); 
}

void return_Temp() 
{
   float tempC = rtc.getTemp1();
   float temp = rtc.getTemperature();
   Serial.print("Temperatura atual: ");    
   Serial.print(tempC, 1);
   Serial.println("ºC");       
}

void check_wifi()
{
  Serial.println(""); 
   Serial.println(""); 
   WiFi.begin(ssid, password);
   Serial.println("----------------------------------------");  
   Serial.print("Conectando a rede ");
   Serial.println(ssid);
   while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
   }

   Serial.println("");
   Serial.println("----------------------------------------");  
   Serial.print("Conectado à rede WiFi: ");
   Serial.println(ssid);
   Serial.print("Endereço IP: ");
   Serial.println(WiFi.localIP());
   Serial.print("MAC ADDRESS: ");
   Serial.println(WiFi.macAddress());   
   Serial.println("----------------------------------------"); 
}
