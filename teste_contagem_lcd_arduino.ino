#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <Keypad.h>
#include <LiquidCrystal_I2C.h>
#include <Bounce2.h>
#include "nvs_flash.h"  
#include "esp_task_wdt.h"

#define SENSOR 18
#define RESET_BUTTON 5
#define CHAVE_NVS  ""
#define RXD2 16
#define TXD2 17

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

String inputString;
long inputInt;

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
Bounce debouncer = Bounce();
LiquidCrystal_I2C lcd(0x27, 20, 4);
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);
WiFiClient client;
HTTPClient http;

void setup()
{
  Serial.begin(115200); 
  le_dado_nvs();
  lcd.init();
  lcd.backlight();
  //lcd.setCursor(0, 0);
  //lcd.print("COUNT: ");
  //lcd.setCursor(7, 0);
  //lcd.print(count);

  split_Number(count);
  
  lcd.createChar(0, LT);
  lcd.createChar(1, UB);
  lcd.createChar(2, RT);
  lcd.createChar(3, LL);
  lcd.createChar(4, LB);
  lcd.createChar(5, LR);
  lcd.createChar(6, UMB);
  lcd.createChar(7, LMB);
  
  pinMode(SENSOR, INPUT_PULLUP);
  pinMode(RESET_BUTTON, INPUT);
  //attachInterrupt(RESET_BUTTON, funcao_ISR, RISING);

  //debouncer.attach(RESET_BUTTON); // Informa que o tratamento de debouce será feito no pino 4;
  //debouncer.interval(100); // Seta o intervalo de trepidação;

  Serial.println("");
  Serial.println("");  

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
      1);             /*Núcleo que executará a tarefa */


}

void TASK_Check_Reset_Button(void *p)
{
  /* kick watchdog every 1 second */
  for (;;)
  {
    vTaskDelay(100 / portTICK_PERIOD_MS);
    if(digitalRead(RESET_BUTTON)== HIGH)
    {
        ESP.restart();  
    }
  }
}

void loop()
{
  get_Keypad_Buttons();

  debouncer.update(); // Executa o algorítimo de tratamento;
  value = debouncer.read(); // Lê o valor tratado do botão;  

  if (value == HIGH)
  {
    b_high = true;
  }

  count_Sensor();

}

void menu_Entrada_Manual()
{
    menu_return = !menu_return;  
  
    lcd.setCursor(0, 2);
    lcd.print("Entrada Manual: ");
    lcd.setCursor(15, 2);
    lcd.print(inputString);
    validar_Entrada_Manual = true;
    menu_return = false;

    if(menu_return)
    {
      lcd.clear();
      split_Number(count);
      validar_Entrada_Manual = false;
      inputString = "";

    }
    
  }
  
void menu_Voltar()
{
  lcd.clear();
  //lcd.setCursor(0, 0);
  //lcd.print("COUNT: ");
  //lcd.setCursor(7, 0);
  //lcd.print(count);
  split_Number(count);
  validar_Entrada_Manual = false;
  inputString = "";

}

void menu_Enviar(String value)
{
  //if ((value == LOW) && (count > 0) && (b_high == true))
  //if (count > 0)
  //{ 

  if (validar_Entrada_Manual && inputString != "")
  {
    lcd.setCursor(0, 2);
    lcd.print("");
    
    //Serial.print("COUNT: ");
    Serial.print("M");
    Serial.print(inputString);    
    Serial.println(value);
    b_high = false;
    //validate_Serial_Send = false;
    //Serial.println(count);
    count = 0;
    inputString = "";
    grava_dado_nvs(count);
    lcd.clear();
    //lcd.setCursor(0, 0);
    //lcd.print("COUNT: ");
    //lcd.setCursor(7, 0);
    //lcd.print(count);
    split_Number(count);
    delay(150);

  }
  else if (count > 0 && !validar_Entrada_Manual)
  {
    lcd.setCursor(0, 2);
    lcd.print("");
   
    //Serial.print("COUNT: ");
    Serial.print("S");    
    Serial.print(count);
    Serial.println(value);
    b_high = false;
    //validate_Serial_Send = false;
    //Serial.println(count);
    count = 0;
    inputString = "";
    grava_dado_nvs(count);
    lcd.clear();
    //lcd.setCursor(0, 0);
    //lcd.print("COUNT: ");
    //lcd.setCursor(7, 0);
    //lcd.print(count);
    split_Number(count);
    delay(150);
  }
  else
  {
    invalida_Enviar_Serial();
  }

  validar_Entrada_Manual = false;
  inputString = "";

}

void invalida_Enviar_Serial()
{
  //Serial.println("ERRO, string vazia!");
  lcd.clear();
  lcd.setCursor(2, 0);
  //lcd.print("FALHA AO ENVIAR!");
  lcd.setCursor(3, 1);
  //lcd.print("Nao e possivel");
  lcd.setCursor(5, 2);
  //lcd.print("enviar uma");
  lcd.setCursor(4, 3);
  //lcd.print("string vazia!");
  //delay(2000);
  lcd.clear();
  menu_Reset_LCD();
}

void menu_Reset_LCD()
{
  count = 0;
  inputString = "";
  validar_Entrada_Manual = false;
  grava_dado_nvs(count);
  lcd.clear();  // Limpa o display
  //lcd.setCursor(0, 0);
  //lcd.print("COUNT: ");
  //lcd.setCursor(7, 0);
  //lcd.print(count);
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
    s_high = false;
    count++;
    //Serial.println(count);       

    lcd.clear();  // Limpa o display
    //lcd.setCursor(0, 0);
    //lcd.print("COUNT: ");
    //lcd.setCursor(7, 0);
    //lcd.print(count);
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
        menu_Enviar("Capa");
        break;

      case 'B':
        menu_Enviar("Miolo");
        break;

      case 'C':
        menu_Enviar("Retalho");
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
      menu_Entrada_Manual();
      break;

      case '#':
      menu_Voltar();
      menu_Reset_LCD();
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
  lcd.setCursor(x, 0); //Seleciona a linha superior
  lcd.write((byte)0);  //Segmento 0 selecionado
  lcd.write(1);  //Segmento 1 selecionado
  lcd.write(2);
  lcd.setCursor(x, 1); //Seleciona a linha inferior
  lcd.write(3);
  lcd.write(4);
  lcd.write(5);
}
void custom1() //Seleciona os segmentos para formar o numero 1
{
  lcd.setCursor(x, 0);
  lcd.write(1);
  lcd.write(2);
  lcd.setCursor(x + 1, 1);
  lcd.write(5);
}
void custom2() //Seleciona os segmentos para formar o numero 2
{
  lcd.setCursor(x, 0);
  lcd.write(6);
  lcd.write(6);
  lcd.write(2);
  lcd.setCursor(x, 1);
  lcd.write(3);
  lcd.write(7);
  lcd.write(7);
}
void custom3()  //Seleciona os segmentos para formar o numero 3
{
  lcd.setCursor(x, 0);
  lcd.write(6);
  lcd.write(6);
  lcd.write(2);
  lcd.setCursor(x, 1);
  lcd.write(7);
  lcd.write(7);
  lcd.write(5);
}
void custom4()  //Seleciona os segmentos para formar o numero 4
{
  lcd.setCursor(x, 0);
  lcd.write(3);
  lcd.write(4);
  lcd.write(2);
  lcd.setCursor(x + 2, 1);
  lcd.write(5);
}
void custom5()  //Seleciona os segmentos para formar o numero 5
{
  lcd.setCursor(x, 0);
  lcd.write((byte)0);
  lcd.write(6);
  lcd.write(6);
  lcd.setCursor(x, 1);
  lcd.write(7);
  lcd.write(7);
  lcd.write(5);
}
void custom6()  //Seleciona os segmentos para formar o numero 6
{
  lcd.setCursor(x, 0);
  lcd.write((byte)0);
  lcd.write(6);
  lcd.write(6);
  lcd.setCursor(x, 1);
  lcd.write(3);
  lcd.write(7);
  lcd.write(5);
}
void custom7() //Seleciona os segmentos para formar o numero 7
{
  lcd.setCursor(x, 0);
  lcd.write(1);
  lcd.write(1);
  lcd.write(2);
  lcd.setCursor(x + 1, 1);
  lcd.write((byte)0);
}
void custom8()  //Seleciona os segmentos para formar o numero 8
{
  lcd.setCursor(x, 0);
  lcd.write((byte)0);
  lcd.write((byte)6);
  lcd.write(2);
  lcd.setCursor(x, 1);
  lcd.write(3);
  lcd.write(7);
  lcd.write(5);
}
void custom9()  //Seleciona os segmentos para formar o numero 9
{
  lcd.setCursor(x, 0);
  lcd.write((byte)0);
  lcd.write((byte)6);
  lcd.write((byte)2);
  lcd.setCursor(x + 2, 1);
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

/* Função ISR (chamada quando há geração da
interrupção) */
void IRAM_ATTR funcao_ISR()
{
        delayMicroseconds(1000000);
        ESP.restart();  
}
