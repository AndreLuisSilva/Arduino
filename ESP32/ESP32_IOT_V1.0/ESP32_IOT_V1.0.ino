#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClient.h>
#include <Timestamps.h>
#include <sys/time.h>
#include <time.h>
#include <SPIFFS.h>
#include <FS.h>
#include <ESP32Ping.h>
#include "esp_task_wdt.h"
#define RELAY_PIN 18
#define RELAY_ON 1
#define RELAY_OFF 0
#define ARRAY_SIZE 60
#define TIME_INTERVAL 1000
#define MAC_ADDRESS "B8:27:EB:B0:21:80"
#define FORMAT_LITTLEFS_IF_FAILED true

//const char *ssid = "ForSellEscritorio";
//const char *password = "forsell1010";

const char *ssid = "AndreESP32";
const char *password = "teste123";

//const char *ssid = "Andre Wifi";
//const char *password = "090519911327";

//Seu nome de domínio com caminho de URL ou endereço IP com caminho
String serverName = "http://192.168.1.106:1880/update-sensor";

// As seguintes variáveis são unsigned long porque o tempo, medido em milissegundos,
// rapidamente se tornará um número maior do que pode ser armazenado em um int.
unsigned long lastTime = 0;
//timer de 1 minuto
unsigned long timerDelay = 60000;

//Armazena o valor (tempo) da ultima vez que o led foi aceso
unsigned long previous_Millis = 0;

int vector[60] = { 0 };

int relay_State = 0;
//variaveis que indicam o núcleo
static uint8_t taskCoreZero = 0;
static uint8_t taskCoreOne = 1;

//flags
int relay_Flag = 0;
int flag_ON_OFF = 0;
int flag_Send_Post = 0;
bool flag_SPIFFS = false;

//counts
int count_ON_OFF;
int count_Seconds;
int count_Data_On_SPIFFS_Sucess;
int count_SPIFFS;
int count_Lines_False = 0;
int count_Lines_True = 0;

String myFilePath = "/esquadrejadeira.txt";

File file;

QueueHandle_t buffer;
Timestamps ts(3600);  // instantiating object of class Timestamp with an time offset of 0 seconds 

long timezone = -3;
byte daysavetime = 0; // Daylight saving time (horario de verão)

int sizeOfRecord = 75;

void check_ON_OFF();

uint8_t Get_NTP(void);
void showFile();
void TASK_Check_Relay_Status(void *p);
void TASK_Send_POST(void *p);

void setup()
{
  Serial.begin(115200);

  pinMode(RELAY_PIN, INPUT_PULLUP);

  esp_task_wdt_init(30, true);
  esp_task_wdt_add(NULL);
  disableCore0WDT();

  WiFi.begin(ssid, password);
  Serial.println("");
  Serial.println("Conectando");
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("");
  Serial.print("Conectado à rede WiFi: ");
  Serial.println(ssid);
  Serial.print("Endereço IP: ");
  Serial.println(WiFi.localIP());
  Serial.print("MAC ADDRESS: ");
  Serial.println(WiFi.macAddress());
  Serial.println("");

  if (Get_NTP() == false)
  {
    // Get time from NTP
    Serial.println("Request timeout com servidor NTP");
  }

  // Se não foi possível iniciar o File System, exibimos erro e reiniciamos o ESP
  if (!SPIFFS.begin(true))
  {
    Serial.println("SPIFFS Mount Failed");
    return;
    ESP.restart();
  }

  File file = SPIFFS.open(myFilePath, FILE_WRITE);

  if (!file)
  {
    Serial.println("There was an error opening the file for writing");
    return;

  }
  else
  {
    // Exibimos mensagem
    Serial.println("Sistema de arquivos OK");
  }

  listAllFiles();
  Serial.println("");
  
  //SPIFFS.remove("/esquadrejadeira.bin");

  buffer = xQueueCreate(10, sizeof(uint32_t));  //Cria a queue *buffer* com 10 slots de 4 Bytes  
  

  xTaskCreatePinnedToCore(
    TASK_Check_Relay_Status, /*função que implementa a tarefa */
    "TASK_Check_Relay_Status", /*nome da tarefa */
    10000, /*número de palavras a serem alocadas para uso com a pilha da tarefa */
    NULL, /*parâmetro de entrada para a tarefa (pode ser NULL) */
    2, /*prioridade da tarefa (0 a N) */
    NULL, /*referência para a tarefa (pode ser NULL) */
    taskCoreZero); /*Núcleo que executará a tarefa */

  xTaskCreatePinnedToCore(
    TASK_Send_POST, /*função que implementa a tarefa */
    "TASK_Send_POST", /*nome da tarefa */
    10000, /*número de palavras a serem alocadas para uso com a pilha da tarefa */
    NULL, /*parâmetro de entrada para a tarefa (pode ser NULL) */
    2, /*prioridade da tarefa (0 a N) */
    NULL, /*referência para a tarefa (pode ser NULL) */
    taskCoreOne); /*Núcleo que executará a tarefa */

  xTaskCreatePinnedToCore(
    TASK_Send_Data_From_SPIFFS, /*função que implementa a tarefa */
    "TASK_Send_Data_From_SPIFFS", /*nome da tarefa */
    10000, /*número de palavras a serem alocadas para uso com a pilha da tarefa */
    NULL, /*parâmetro de entrada para a tarefa (pode ser NULL) */
    1, /*prioridade da tarefa (0 a N) */
    NULL, /*referência para a tarefa (pode ser NULL) */
    taskCoreOne); /*Núcleo que executará a tarefa */

  delay(500); //tempo para a tarefa iniciar

}

void TASK_Send_Data_From_SPIFFS(void *p)
{
  
  while (true)
  {
    if (count_Lines_SPIFFS() > 0) {
      
    //se tiver conexão com WiFi
    if (WiFi.status() == WL_CONNECTED)
    {    
      //se existir o arquivo
      if (SPIFFS.exists(myFilePath))
      {
        //abre o arquivo para leitura
        file = SPIFFS.open(myFilePath, "r");
        //caso aconteça algum erro ao abrir o arquivo
        if (!file)
        {
          Serial.println("Failed to open file for reading");
          return;
        }

        //se não...
        else
        {
          //enquanto o arquivo estiver disponivel para leitura
          while (file.available())
          {                      
            String line = file.readStringUntil('\n');
            //Serial.println(line);
            WiFiClient client;
            HTTPClient http;
            // Especifique o destino para a solicitação HTTP
            http.begin("http://54.207.230.239/site/query_insert_postgres_conexao_madeira.php");
            http.addHeader("Content-Type", "application/x-www-form-urlencoded");

            int httpResponseCode = http.POST(line); //publica o post 

            //verifica se foi possivel fazer o insert com post
            if (httpResponseCode > 0)
            {
              String response = http.getString(); //Obtém a resposta do request
              Serial.println("Conexão para reenviar os dados para o servidor bem sucedida."); //Printa o código do retorno
              Serial.println("");

              //Se o INSERT no banco não retornar um "OK", salva na memória flash
              if (response != "OK")
              {
                Serial.println("Não foi possível reenviar o dado da SPIFF para o banco de dados");
                Serial.println("");
                count_Lines_False++;
              }
              else
              {
                Serial.println("Insert realizado com sucesso!");
                Serial.println("");
                count_Lines_True++;
              }
            }
            else
            {
              //Se acontecer algum outro tipo de erro ao enviar o POST
              Serial.println("");
              Serial.print("Erro ao reenviar POST: ");
              Serial.println(httpResponseCode);
              Serial.println("");
              count_Lines_False++;              
            } 
            flag_SPIFFS = true;            
          }          
          file.close();                        
          }
        }
      }   
      else
      {
      Serial.println("Não foi possivel se reconectar ao servidor!");
    }
    if (count_Lines_True == count_Lines_SPIFFS()) {
                     
              Serial.println("Dados serão removidos das SPIFFS.");
              Serial.println("\n\n---BEFORE REMOVING---");
              listAllFiles();
               
              SPIFFS.remove("/esquadrejadeira.txt");
               
              Serial.println("\n\n---AFTER REMOVING---");
              listAllFiles();
              count_Lines_True = 0;
                     
            }                     
   }   
  }

  vTaskDelay(pdMS_TO_TICKS(50));
}

void TASK_Send_POST(void *p)
{
  esp_task_wdt_add(NULL);
  uint32_t rcv = 0;
  while (true)
  {
    if (buffer == NULL) return;

    if (xQueueReceive(buffer, &rcv, portMAX_DELAY) == true) //Se recebeu o valor dentro de 1seg (timeout), mostrara na tela
    {
      send_POST();
    }
  }
  esp_task_wdt_reset();
  //vTaskDelay(pdMS_TO_TICKS(50));
}

void TASK_Check_Relay_Status(void *p)
{
  while (true)
  {
    unsigned long current_Millis = millis();

    //Verifica se o intervalo já foi atingido
    if (current_Millis - previous_Millis >= TIME_INTERVAL)
    {
      //Armazena o valor da ultima vez que o led foi aceso
      previous_Millis = current_Millis;
      count_Seconds++;
      check_ON_OFF();
    }
  }

  vTaskDelay(pdMS_TO_TICKS(50));
}

bool check_Ping() {
  bool success = Ping.ping("www.google.com", 3);
 
  if(!success){
    Serial.println("Ping failed");
    return false;
  }
  return true;
  Serial.println("Ping succesful.");
}

void wifi_Reconnect()
{
  // Caso retorno da função WiFi.status() seja diferente de WL_CONNECTED
  // entrara na condição de desconexão
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("Não foi possivel conectar ao servidor WEB ou banco de dados");
    // Função que reconectará ao WIFI caso esteja disponível novamente.
    WiFi.reconnect();
    // Se o status de WiFi.status() continuar diferente de WL_CONNECTED
    // vai aguardar 1 segundo até a proxima verificação.
  }
}

/*---------------------------------------------------------
 *Try to communicate with the NTP server. If OK,
 *update internal RTC. 
 *@return TRUE if ok, FALSE, if failed. (^_^)
---------------------------------------------------------*/
uint8_t Get_NTP(void)
{
  struct tm timeinfo;

  timeval epoch = { 946684800, 0 }; // timeval is a struct: {tv_sec, tv_usec}. Old data for detect no replay from NTP. 1/1/2000    
  settimeofday(&epoch, NULL); // Set internal ESP32 RTC

  Serial.println("Entrando em contato com o servidor NTP");
  configTime(3600 *timezone, daysavetime *3600, "a.st1.ntp.br", "a.ntp.br", "gps.ntp.br");  // initialize the NTP client 
  // using configTime() function to get date and time from an NTP server.

  if (getLocalTime(&timeinfo, 1000) == 0) // transmits a request packet to a NTP server and parse the received 
  {
    // time stamp packet into to a readable format. It takes time structure
    // as a parameter. Second parameter is server replay timeout
    Serial.println("Sem conexão com servidor NTP");

    return false; // Something went wrong
    ESP.restart();  
  }

  Serial.println("NTP server reply");
  Serial.printf("Now: %02d-%02d-%04d %02d:%02d:%02d\n", timeinfo.tm_mday, timeinfo.tm_mon + 1, timeinfo.tm_year + 1900, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
  Serial.println("-------------------------");

  return true;  // All OK and go away
}

int count_Lines_SPIFFS()
{
  int count = 0;
  String line = "";
  file = SPIFFS.open(myFilePath, FILE_READ);
  while (file.available())
  {
    // we could open the file, so loop through it to find the record we require
    count++;
    //Serial.println(count);  // show line number of SPIFFS file
    line = file.readStringUntil('\n');  // Read line by line from the file        
  }

  return count;
}

void send_POST()
{
  Serial.println("");
  Serial.println("Iniciando o envio do POST.");
  Serial.println("");
  struct tm timeinfo;
  getLocalTime(&timeinfo);  // Get local time
  char logdata[150];
  //Verifique o status da conexão WiFi
  if (WiFi.status() == WL_CONNECTED)
  {
    WiFiClient client;
    HTTPClient http;
    // Especifique o destino para a solicitação HTTP
    http.begin("http://54.207.230.239/site/query_insert_postgres_conexao_madeira.php");
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    sprintf(logdata, "on_off=%d&mac_address=%s&data_hora=%04d-%02d-%02d %02d:%02d:%02d\n", flag_ON_OFF, MAC_ADDRESS, timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);

    int httpResponseCode = http.POST(logdata);  //publica o post
    Serial.println("");
    //verifica se foi possivel fazer o insert com post
    if (httpResponseCode > 0)
    {
      String response = http.getString(); //Obtém a resposta do request
      //Serial.println(httpResponseCode); //Printa o código do retorno
      Serial.println("");
      Serial.println(response); //Printa a resposta do request
      Serial.println("");

      //Se o INSERT no banco não retornar um "OK", salva na memória flash.
      if (response != "OK")
      {
        Serial.println("Insert não inserido no banco com sucesso =(");
        Serial.println("Salvando dados na memória flash...");
        file = SPIFFS.open(myFilePath, FILE_APPEND);
        if (file.print(logdata))
        {
          Serial.println("Mensagem anexada com sucesso!");
          Serial.println("");          
        }
        else
        {
          Serial.print("Falha ao anexar!");
        }

        file.close();
      }
    }
    else
    {
      //Se acontecer algum outro tipo de erro ao enviar o POST, salva na memória flash.
      Serial.print("Erro ao enviar POST: ");
      Serial.println(httpResponseCode);
      Serial.println("Salvando dados na memória flash...");
      file = SPIFFS.open(myFilePath, FILE_APPEND);
      if (file.print(logdata))
      {
        Serial.println("Mensagem anexada com sucesso!!!");
        Serial.println("");       
      }
      else
      {
        Serial.print("Falha ao anexar!");
      }

      file.close();
    }
  }
  else
  {
    //Se não tiver conexão com o WiFi salva na memória flash
    Serial.println("Sem conexão com a rede Wireless!");
    Serial.println("Salvando dados na memória flash...");
    Serial.println("");
    sprintf(logdata, "on_off=%d&mac_address=%s&data_hora=%04d-%02d-%02d %02d:%02d:%02d\n", flag_ON_OFF, MAC_ADDRESS, timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    //Serial.println(logdata);
    
  }
}

int return_Relay_State()
{
  if (
    digitalRead(RELAY_PIN) == 1)
  {
    relay_State = RELAY_ON;
  }
  else if (digitalRead(RELAY_PIN) == 0)
  {
    relay_State = RELAY_OFF;
  }

  delay(50);
  return relay_State;

}

boolean return_Array_State()
{
  for (int i = 0; i < ARRAY_SIZE; i++)
  {
    if (vector[i] == 1)
    {
      count_ON_OFF++;
    }
  }

  if (count_ON_OFF > 0)
  {
    return true;
  }
  else
  {
    return false;
  }
}

void listAllFiles()
{
  File root = SPIFFS.open("/");

  File file = root.openNextFile();

  while (file)
  {
    Serial.print("FILE: ");
    Serial.println(file.name());

    file = root.openNextFile();
  }
}

void check_ON_OFF()
{
  uint32_t snd = 0;

  if (count_Seconds <= ARRAY_SIZE)
  {
    Serial.print(count_Seconds);
    Serial.print("ª leitura = ");
    Serial.println(return_Relay_State());
    vector[count_Seconds] = return_Relay_State();

  }
  else if (count_Seconds > ARRAY_SIZE)
  {
    count_Seconds = 0;

    Serial.println("");
    Serial.print("Estado do rele ");
    if (return_Array_State() == true)
    {
      Serial.println("Ligado");
      Serial.print("Tempo: ");
      Serial.print(count_ON_OFF + 1);
      Serial.println(" seg.");
      flag_ON_OFF = 1;

    }
    else if (return_Array_State() == false)
    {
      Serial.println("Desligado");
      Serial.print("Tempo: ");
      Serial.print(ARRAY_SIZE - count_ON_OFF);
      Serial.println(" seg.");
      flag_ON_OFF = 0;
    }

    //zerar contadores de tempo
    count_ON_OFF = 0;
    Serial.print("Linhas: ");
    Serial.println(count_Lines_SPIFFS());

    //enviar POST
    //send_POST();
    xQueueSend(buffer, &snd, pdMS_TO_TICKS(0));
    //Serial.println("");
  }
}

void loop() {}
