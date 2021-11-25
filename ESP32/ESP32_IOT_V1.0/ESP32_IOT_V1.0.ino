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
#include <Vector.h>
#include <Streaming.h>
#include <SD.h>
#include <Wire.h>  // para o Modulo RTC I2C
//#include "RTClib.h"

#define BLUE 25
#define GREEN 26
#define RED 27
#define PIN_LED 2
#define RELAY_PIN 13
#define RELAY_ON 1
#define RELAY_OFF 0
#define ARRAY_SIZE 60
#define TIME_INTERVAL 1000

//MAC ADDRESS ESP32 TESTE
//#define MAC_ADDRESS "7C:9E:BD:F5:60:B4"
//MAC ADDRESS ESP32 ÓTIMA
//#define MAC_ADDRESS "94:B9:7E:C4:DD:B8"
//MAC ADDRESS ESP32 TESTE2
//#define MAC_ADDRESS "B8:F0:09:CD:1B:28"
//MAC ADDRESS ESP32 AMARELO
#define MAC_ADDRESS "80:7D:3A:D5:1E:04"
#define FORMAT_LITTLEFS_IF_FAILED true

//const char *ssid = "ForSellEscritorio";
//const char *password = "forsell1010";

//const char *ssid = "LABORATORIO";
//const char *password = "";

const char * ssid = "Andrew";
const char * password = "teste123";

//const char * ssid = "Fabrica ";
//const char * password = "*35221023#2025";

//const char *ssid = "Andre Wifi";
//const char *password = "090519911327";

// As seguintes variáveis são unsigned long porque o tempo, medido em milissegundos,
// rapidamente se tornará um número maior do que pode ser armazenado em um int.
unsigned long lastTime = 0;
//timer de 1 minuto
unsigned long timerDelay = 60000;

//Armazena o valor (tempo) da ultima vez que o led foi aceso
unsigned long previous_Millis = 0;

hw_timer_t * timer = NULL; //faz o controle do temporizador (interrupção por tempo)

int vetor[60] = { 0 };

int sizeOfRecord = 80;

const char * remote_host = "www.google.com";

// Variável que guarda o último registro obtido
String lastRecord = "";

int relay_State = 0;
//variaveis que indicam o núcleo
static uint8_t taskCoreZero = 0;
static uint8_t taskCoreOne = 1;

//flags
int relay_Flag = 0;
int flag_ON_OFF = 0;
int flag_Send_Post = 0;
bool flag_SPIFFS = false;
bool flag_Valida_Internet = false;

//counts
int count_ON_OFF;
int count_Seconds;
int count_SPIFFS = 0;
int count_Data_On_SPIFFS_Sucess;
int count_Lines_True = 0;
int count_Lines = 0;

String myFilePath = "/esquadrejadeira.txt";
String mySDCardPath = "/data.txt";
// String que recebe as mensagens de erro
String errorMsg;

String fileName; // Nome do arquivo
File pFile; // Ponteiro do arquivo

//RTC_DS3231 rtc;

QueueHandle_t buffer;
Timestamps ts(3600); // instantiating object of class Timestamp with an time offset of 0 seconds 

long timezone = -3;
byte daysavetime = 0; // Daylight saving time (horario de verão)

void check_ON_OFF();

uint8_t Get_NTP(void);
void showFile();
void TASK_Check_Relay_Status(void * p);
void TASK_Send_POST(void * p);
void listAllFiles();
int count_Lines_SPIFFS();
void send_POST();
void printDirectory(File dir, int numTabs);


// Classe FS_File_Record e suas funções
class FS_File_Record {
   // Todas as funções desta lib são publicas, mais detalhes em FS_File_Record.cpp
   public:
   FS_File_Record(String, int);
   FS_File_Record(String);
   bool init();
   bool readFileLastRecord(String * , String * );
   bool destroyFile();
   String findRecord(int);
   bool rewind();
   bool writeFile(String, String * );
   bool seekFile(int);
   bool readFileNextRecord(String * , String * );
   String getFileName();
   void setFileName(String);
   int getSizeRecord();
   void setSizeRecord(int);
   void newFile();
   bool fileExists();
   bool availableSpace();
   int getTotalSpace();
   int getUsedSpace();
};

WiFiClient client;
WiFiServer server(80);
HTTPClient http;
FS_File_Record ObjFS(myFilePath, sizeOfRecord);

//função que o temporizador irá chamar, para reiniciar o ESP32
void IRAM_ATTR resetModule() {
   ets_printf("(watchdog) reiniciar\n"); //imprime no log
   esp_restart(); //reinicia o chip
}

void setup() {
   Serial.begin(115200);

   pinMode(RELAY_PIN, INPUT_PULLUP);
   pinMode(PIN_LED, OUTPUT);
   pinMode(BLUE, OUTPUT);
   pinMode(GREEN, OUTPUT);
   pinMode(RED, OUTPUT);
   digitalWrite(PIN_LED, LOW);
   digitalWrite(BLUE, LOW);
   digitalWrite(GREEN, LOW);
   digitalWrite(RED, LOW);

   esp_task_wdt_init(80, true);
   esp_task_wdt_add(NULL);
   disableCore0WDT();   

   WiFi.begin(ssid, password);
   Serial.println("");
   Serial.print("Conectando a rede ");
   Serial.println(ssid);
   while (WiFi.status() != WL_CONNECTED) {
      vTaskDelay(pdMS_TO_TICKS(500));
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

   if (Get_NTP() == false) {
      // Get time from NTP
      Serial.println("Timeout na conexão com servidor NTP");
      delay(500);
      for (int i = 0; i < 3; i++) {
         digitalWrite(RED, HIGH);
         delay(250);
         digitalWrite(RED, LOW);
         delay(250);
      }
      delay(500);
      ESP.restart();

   }

   init_SD_Card();

   // Se não foi possível iniciar o File System, exibimos erro e reiniciamos o ESP
   if (!ObjFS.init()) {
      Serial.println("Erro no sistema de arquivos");
      delay(500);
      for (int i = 0; i < 3; i++) {
         digitalWrite(BLUE, HIGH);
         digitalWrite(RED, HIGH);
         delay(250);
         digitalWrite(BLUE, LOW);
         digitalWrite(RED, LOW);
         delay(250);
      }
      delay(500);
      ESP.restart();
   }

   // Exibimos mensagem
   Serial.println("Sistema de arquivos ok");

   // Se o arquivo não existe, criamos o arquivo
   if (!ObjFS.fileExists()) {
      delay(500);
      for (int i = 0; i < 3; i++) {
         digitalWrite(GREEN, HIGH);
         delay(250);
         digitalWrite(GREEN, LOW);
         delay(250);
      }
      delay(500);
      Serial.println("Novo arquvo: ");
      ObjFS.newFile(); // Cria o arquivo  

   }

   //readFile(myFilePath);
   digitalWrite(BLUE, HIGH);
   delay(500);
   listAllFiles();
   Serial.println("");
   Serial.println("Iniciando a leitura...");
   Serial.println("");
   digitalWrite(BLUE, LOW);

   //SPIFFS.remove("/esquadrejadeira.txt");  
   //SD.remove("/data.txt");  

   //if (!rtc.begin()) {
   //  Serial.println("Não foi possível encontrar RTC");
   //  while (1);
  //}
  //rtc.adjust(DateTime(__DATE__, __TIME__));



   buffer = xQueueCreate(10, sizeof(uint32_t)); //Cria a queue *buffer* com 10 slots de 4 Bytes  

   xTaskCreatePinnedToCore(
      TASK_Check_Internet, //Função que será executada
      "TASK_Check_Internet", //Nome da tarefa
      10000, //Tamanho da pilha
      NULL, //Parâmetro da tarefa (no caso não usamos)
      1, //Prioridade da tarefa
      NULL, //Caso queria manter uma referência para a tarefa que vai ser criada (no caso não precisamos)
      taskCoreZero); //Número do core que será executada a tarefa (usamos o core 0 para o loop ficar 

   xTaskCreatePinnedToCore(
      TASK_Check_Relay_Status, 
      "TASK_Check_Relay_Status",
      10000,
      NULL, 
      2, 
      NULL, 
      taskCoreZero); 

   xTaskCreatePinnedToCore(
      TASK_Check_Wifi_Status, 
      "TASK_Check_Wifi_Status", 
      10000,
      NULL, 
      1, 
      NULL, 
      taskCoreOne); 

   xTaskCreatePinnedToCore(
      TASK_Send_POST, 
      "TASK_Send_POST", 
      10000, 
      NULL, 
      2, 
      NULL, 
      taskCoreOne); 

   xTaskCreatePinnedToCore(
      TASK_Send_Data_From_SPIFFS, 
      "TASK_Send_Data_From_SPIFFS", 
      10000, 
      NULL, 
      1, 
      NULL, 
      taskCoreOne); 

   delay(500); //tempo para a tarefa iniciar 
}

void TASK_Check_Internet(void * p) {

   //Delay de 5 segundos da tarefa. É feita em ticks. Para executar em millis dividimos pela constante portTICK_PERIOD_MS
   TickType_t taskDelay = 1000 / portTICK_PERIOD_MS;

   while (true) {
      if (count_Seconds == 50) {
         //Se tem internet
         IPAddress ip(8, 8, 8, 8);
         bool connected = Ping.ping(ip, 3);
         if (connected) {
            Serial.println("");
            Serial.println("Conectado com sucesso a internet!!");
            Serial.println("");
            flag_Valida_Internet = true;
         } else {
            Serial.println("");
            Serial.println("Erro ao tentar se conectar a internet :(");
            Serial.println("");
            flag_Valida_Internet = false;
         }
      }
      //Aplica o delay
      vTaskDelay(taskDelay);
   }

}

void TASK_Check_Wifi_Status(void * p) {
   esp_task_wdt_delete(NULL);
   //esp_task_wdt_add(NULL); //Habilita o monitoramento do Task WDT nesta tarefa
   while (true) {
      check_Wifi_Connection();
      ESP_LOGI("TASK_Check_Wifi_Status", "OK");
      //esp_task_wdt_reset();
      vTaskDelay(pdMS_TO_TICKS(1000));
   }
}

void TASK_Send_Data_From_SPIFFS(void * p) {
   esp_task_wdt_delete(NULL);
   //esp_task_wdt_add(NULL); //Habilita o monitoramento do Task WDT nesta tarefa
   while (true) {
      send_POST_Again();
      ESP_LOGI("TASK_Send_Data_From_SPIFFS", "OK");
      //esp_task_wdt_reset();
      vTaskDelay(pdMS_TO_TICKS(10));
   }
}

void TASK_Send_POST(void * p) {
   //esp_task_wdt_delete(NULL);
   esp_task_wdt_add(NULL); //Habilita o monitoramento do Task WDT nesta tarefa 
   uint32_t rcv = 0;
   while (true) {
      if (buffer == NULL) {
         return;
      }

      if (xQueueReceive(buffer, & rcv, portMAX_DELAY) == true) //Se recebeu o valor dentro de 1seg (timeout), mostrara na tela
      {
         digitalWrite(PIN_LED, HIGH);
         send_POST();
         esp_task_wdt_reset();
         vTaskDelay(pdMS_TO_TICKS(10));
         digitalWrite(PIN_LED, LOW);

      }
   }
}

void TASK_Check_Relay_Status(void * p) {
   esp_task_wdt_delete(NULL);
   //esp_task_wdt_add(NULL);
   while (true) {
      unsigned long current_Millis = millis();
      //Verifica se o intervalo já foi atingido
      if (current_Millis - previous_Millis >= TIME_INTERVAL) {
         //Armazena o valor da ultima vez que o led foi aceso
         previous_Millis = current_Millis;
         count_Seconds++;
         check_ON_OFF();
      }
      //esp_task_wdt_reset();
      vTaskDelay(pdMS_TO_TICKS(10));
   }

}

void check_Wifi_Connection() {

   if (WiFi.status() == WL_CONNECTED) {
      // WiFi is UP,  do what ever
   } else {
      // wifi down, reconnect here
      WiFi.begin(ssid, password);
      while (WiFi.status() != WL_CONNECTED) {
         if (WiFi.status() == WL_CONNECTED) {
            break;
         } else {
            //Serial.print("Tentando se reconectar a rede Wifi: ");
            //Serial.println(ssid);
            digitalWrite(GREEN, HIGH);
            digitalWrite(BLUE, HIGH);
            vTaskDelay(pdMS_TO_TICKS(250));
            digitalWrite(GREEN, LOW);
            digitalWrite(BLUE, LOW);
            vTaskDelay(pdMS_TO_TICKS(250));
         }
      }
   }
}

bool validate_Send_POST() {
   //Se tem internet
   IPAddress ip(8, 8, 8, 8);
   bool connected = Ping.ping(ip, 3);
   if (connected) {
      return true;
   } else {
      return false;
   }
   vTaskDelay(pdMS_TO_TICKS(1000));
}

void send_POST_Again() {
   //verifica se existe algum arquivo criado na memoria flash
   if (ObjFS.fileExists()) {
      //verifica se existe conexão com o wifi
      if (WiFi.status() == WL_CONNECTED) {
         //verifica se existe conexão com a internet pingando o servidor do google
         if (validate_Send_POST()) {
            //abre o arquivo dentro da SPIFFS para leitura
            File file = SPIFFS.open(myFilePath, FILE_READ);

            String line = "";            

            while (file.available()) {

               int validate_Break_While = 0;
               line = file.readStringUntil('\n');
               validate_Break_While = re_Send_POST(line);

               if (validate_Break_While == (count_Lines_SPIFFS() + 1)) {
                  count_SPIFFS = 0;
                  listAllFiles();
                  SPIFFS.remove(myFilePath);
                  break;
               }
            }
         }
      }
   }
}

void init_SD_Card() {

    Serial.println("\nInicializando o cartão SD...");
 
  // we'll use the initialization code from the utility libraries
  // since we're just testing if the card is working!
  if (!SD.begin(SS)) {
    Serial.println("Falha na inicialização. Verifique se...");
    Serial.println("* tem um cartão inserido no slot?");
    Serial.println("* sua fiação está correta?");
    Serial.println("* você mudou o pino do chip? Selecione o pino correto para funcionar corretamente com o seu shield ou módulo!");
    while (1);
  } else {
    Serial.println("A fiação está OK e um cartão está inserido no slot corretamente.");
  }

   File file = SD.open("/data.txt");
  if(!file) {
    Serial.println("Arquivo ainda não existe");
    Serial.println("Criando arquivo...");   

   file = SD.open("/data.txt", FILE_APPEND);

   // Se foi possível abrir
   if (file) {
      // Escreve registro
      file.println("on_off, mac_address, data_hora \r\n");           
   }
  }
  else {
    Serial.println("Arquivo já existe.");  
  }
  
 
  // print the type of card
  Serial.println();
  Serial.print("Modelo de card: ");
  switch (SD.cardType()) {
    case CARD_NONE:
      Serial.println("Nenhum");
      break;
    case CARD_MMC:
      Serial.println("MMC");
      break;
    case CARD_SD:
      Serial.println("SD");
      break;
    case CARD_SDHC:
      Serial.println("SDHC");
      break;
    default:
      Serial.println("Desconhecido");
  }  
 
  Serial.print("Tamanho do cartão:  ");
  Serial.println((float)SD.cardSize()/1000);
 
  Serial.print("Total em bytes: ");
  Serial.println(SD.totalBytes());
 
  Serial.print("Bytes utilizados: ");
  Serial.println(SD.usedBytes()); 

  file.close();
}

/*--- LEITURA DO ARQUIVO ---*/
String readFile(String pathFile) {
   Serial.println("- Reading file: " + pathFile);
   SPIFFS.begin(true);
   File rFile = SPIFFS.open(pathFile, "r");
   String values;
   if (!rFile) {
      Serial.println("- Failed to open file.");
   } else {
      while (rFile.available()) {
         values += rFile.readString();
      }

      Serial.println("- File values: " + values);
   }

   rFile.close();
   return values;
}

void grava_Dados_SPIFFS(String values) {
   // Se não houver memória disponível, exibe e reinicia o ESP
   if (!ObjFS.availableSpace()) {
      Serial.println("Memory is full!");
      vTaskDelay(pdMS_TO_TICKS(10000));
      return;
   }

   // Escrevemos no arquivo e exibimos erro ou sucesso na serial para debug
   if (values != "" && !ObjFS.writeFile(values, & errorMsg))
      Serial.println(errorMsg);
   else
      Serial.println("Write ok");

   // Atribui para a variável global a última amostra
   lastRecord = values;
   // Exibe a última amostra no display
   showLastRecord();
   // Exibimos o espaço total, usado e disponível no display, de tempo em tempo
   showAvailableSpace(values);
}

// Construtor que seta somente o nome do arquivo, deixando o tamanho de registro default 3
FS_File_Record::FS_File_Record(String _fileName) {
   fileName = _fileName;
}

// Construtor que seta nome do arquivo e tamanho de registro +2 (\r\n)
FS_File_Record::FS_File_Record(String _fileName, int _sizeOfRecord) {
   fileName = _fileName;
   sizeOfRecord = _sizeOfRecord + 2;
}

// Inicializa SPIFFS
bool FS_File_Record::init() {
   return SPIFFS.begin(true);
}

// Posiciona o ponteiro do arquivo no início
bool FS_File_Record::rewind() {
   if (pFile)
      return pFile.seek(0);

   return false;
}

// Lê a próxima linha do arquivo
bool FS_File_Record::readFileNextRecord(String * line, String * errorMsg) {
   * errorMsg = "";
   * line = "";
   // Se o ponteiro estiver nulo  
   if (!pFile) {
      // Abre arquivo para leitura
      pFile = SPIFFS.open(fileName.c_str(), FILE_READ);

      // Se aconteceu algum erro 
      if (!pFile) {
         // Guarda msg de erro *errorMsg = "Failed to open the file";
         // Retorna falso
         return false;
      }
   }

   // Se for possível ler o arquivo
   if (pFile.available()) {
      // Lê arquivo *line = pFile.readStringUntil('\n');
      // Retorna true
      return true;
   }

   // Se o arquivo estiver vazio retorna true
   if (pFile.size() <= 0)
      return true;

   // Se não for possível ler o arquivo retorna falso    
   return false;
}

//Posiciona ponteiro do arquivo na posição "pos"
bool FS_File_Record::seekFile(int pos) {
   // Se o ponteiro estiver nulo  
   if (pFile)
      pFile.close();

   pFile = SPIFFS.open(fileName.c_str(), FILE_READ); // Abre o arquivo para leitura

   // Posiciona o ponteiro na posição multiplicando pelo tamanho do registro
   return pFile.seek(sizeOfRecord * pos);
}

// Escreve no arquivo
bool FS_File_Record::writeFile(String line, String * errorMsg) {
   if (pFile)
      pFile.close();

   pFile = SPIFFS.open(myFilePath, FILE_APPEND);

   // Se foi possível abrir
   if (pFile) {
      // Escreve registro
      pFile.println(line);
      // Fecha arquivo
      pFile.close();
      // Retorna true
      return true;
   }

   // Se não foi possível abrir guarda mensagem de erro e retorna false *errorMsg = "Failed to open the file: " + String(fileName);
   return false;
}

// Lê o último registro do arquivo
bool FS_File_Record::readFileLastRecord(String * line, String * errorMsg) {
   // Variável que guardará o tamanho do arquivo
   int sizeArq;

   // Limpa string *errorMsg = "";

   // Se o arquivo está aberto, fecha
   if (pFile)
      pFile.close();

   // Abre o arquivo para leitura
   pFile = SPIFFS.open(fileName.c_str(), FILE_READ);

   // Se não foi possível abrir o arquivo
   if (!pFile) {
      // Guarda mensagem de erro e retorna false *errorMsg = "Failed to open the file: " + String(fileName);
      return false;
   }

   // Obtém o tamanho do arquivo
   sizeArq = pFile.size();
   Serial.println("Size: " + String(sizeArq));

   // Se existe ao menos um registro
   if (sizeArq >= sizeOfRecord) {
      pFile.seek(sizeArq - sizeOfRecord); // Posiciona o ponteiro no final do arquivo menos o tamanho de um registro (sizeOfRecord)
      * line = pFile.readStringUntil('\n');
      pFile.close();
   }

   return true;
}

// Exclui arquivo
bool FS_File_Record::destroyFile() {
   // Se o arquivo estiver aberto, fecha
   if (pFile)
      pFile.close();

   // Exclui arquivo e retorna o resultado da função "remove"  
   return SPIFFS.remove((char * ) fileName.c_str());
}

// Função que busca um registro
// "pos" é a posição referente ao registro buscado
String FS_File_Record::findRecord(int pos) {
   // Linha que receberá o valor do registro buscado
   String line = "", errorMsg = "";

   // Posiciona na posição desejada
   // Obs. A posição se inicia com zero "0" 
   if (!seekFile(pos))
      return "Seek error";

   // Lê o registro
   if (!readFileNextRecord( & line, & errorMsg))
      return errorMsg;

   return line;
}

// Verifica se o arquivo existe
bool FS_File_Record::fileExists() {
   return SPIFFS.exists((char * ) fileName.c_str());
}

// Cria um novo arquivo, se já existir um arquivo de mesmo nome, ele será removido antes
void FS_File_Record::newFile() {
   if (pFile)
      pFile.close();

   SPIFFS.remove((char * ) fileName.c_str());
   pFile = SPIFFS.open(fileName.c_str(), FILE_WRITE);
   pFile.close();
}

// Obtém o nome do arquivo
String FS_File_Record::getFileName() {
   return fileName;
}

// Seta o nome do arquivo
void FS_File_Record::setFileName(String _fileName) {
   fileName = _fileName;
}

// Obtém o tamanho do registro
int FS_File_Record::getSizeRecord() {
   return sizeOfRecord - 2;
}

// Seta o tamanho do registro
void FS_File_Record::setSizeRecord(int _sizeOfRecord) {
   sizeOfRecord = _sizeOfRecord + 2;
}

// Verifica se existe espaço suficiente na memória flash
bool FS_File_Record::availableSpace() {
   return getUsedSpace() + sizeOfRecord <= getTotalSpace();
   // ou também pode ser:
   //return getUsedSpace()+(sizeof(char)*sizeOfRecord) <= getTotalSpace();   // sizeof(char) = 1
}

// Retorna o tamanho em bytes total da memória flash
int FS_File_Record::getTotalSpace() {
   return SPIFFS.totalBytes();
}

// Retorna a quantidade usada de memória flash
int FS_File_Record::getUsedSpace() {
   return SPIFFS.usedBytes();
}

void wifi_Reconnect() {
   // Caso retorno da função WiFi.status() seja diferente de WL_CONNECTED
   // entrara na condição de desconexão
   if (WiFi.status() != WL_CONNECTED) {
      Serial.println("Não foi possivel conectar ao servidor WEB ou banco de dados");
      // Função que reconectará ao WIFI caso esteja disponível novamente.
      WiFi.reconnect();
      // Se o status de WiFi.status() continuar diferente de WL_CONNECTED
      // vai aguardar 1 segundo até a proxima verificação.
   }
}

uint8_t Get_NTP(void) {
   struct tm timeinfo;

   timeval epoch = {
      946684800,
      0
   }; // timeval is a struct: {tv_sec, tv_usec}. Old data for detect no replay from NTP. 1/1/2000    
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
   Serial.println("-------------------------");
   return true; // All OK and go away
}

int count_Lines_SPIFFS() {
   int count = 0;
   String line = "";
   pFile = SPIFFS.open(myFilePath, FILE_READ);
   while (pFile.available()) {
      // we could open the file, so loop through it to find the record we require
      count++;
      //Serial.println(count);  // show line number of SPIFFS file
      line = pFile.readStringUntil('\n'); // Read line by line from the file        
   }

   return count;
}

//Envia POST novamente
int re_Send_POST(String post) { 
   
      // Especifique o destino para a solicitação HTTP
      http.begin("http://54.207.230.239/site/query_insert_postgres_conexao_madeira_teste.php");      
      http.addHeader("Content-Type", "application/x-www-form-urlencoded");

      int httpResponseCode = http.POST(post); //publica o post

      //verifica se foi possivel fazer o insert com post
      if (httpResponseCode > 0) {

         String response = http.getString(); //Obtém a resposta do request         

         //Afim de garantir que todos os inserts no banco de dados ocorram de forma correta, é iniciado a primeira vez o loop de inserção
         //nesse primeiro momento, as inserções serão somente realizadas e não serão contabilizadas
         //isso significa que desse modo o sistema vai passar pelo loop ao menos duas vezes para garantir a integridade da inserção de todos os dados
         //terminando esse loop, ele sai do while e não deleta o arquivo que contém os dados coletados de forma offline 
         //em seguida ele irá entrar novamente no while, mas dessa vez irá começar a contabilizar as linhas afetadas no banco de dados
         //cada linha que retornar um 0 significa que já foi inserida no banco e assim incrementasse o contador "count_SPIFFS"
         //validando que esse dado realmente já foi inserido no banco de dados ao menos uma vez
         //a SQL de inserção não permite a inserção de dados duplicados e nos retorna o numero de linhas afetadas
         //se existir alguma linha que ainda não tenha sido inserida, ele irá inserir novamente no banco de dados         
         //se isso ocorrer ao finalizar o loop ele irá realizar todo o processo novamente, não excluindo o arquivo e entrando no loop novamente
         //caso o contador de linhas armazenadas na memória flash seja igual ao count_SPIFFS
         //todas as linhas da SPIFFS já foram inseridas ao menos uma vez no banco e assim validando a exclusão do arquivo
         
         if (response.toInt() == 0) {         
            
            count_SPIFFS++;

         } 

      } else {
         //Se acontecer algum outro tipo de erro ao enviar o POST, salva na memória flash.
         Serial.println("");
         Serial.print("Erro ao Reenviar POST: ");
         Serial.println(httpResponseCode);
         Serial.println("");
      }

      http.end();
   
   //Serial.println("");
   return count_SPIFFS;
}

void send_POST() {
   Serial.println("Iniciando o envio do POST.");
   Serial.println("");
   struct tm timeinfo;
   getLocalTime( & timeinfo); // Get local time
   //aumentar o tamanho do char conforme necessário para o POST
   char logdata[80];
   char csv[80];
   //Verifique o status da conexão WiFi
   if (WiFi.status() == WL_CONNECTED) {

      if (flag_Valida_Internet) {         

         long randNumber = random(999999);
         // Especifique o destino para a solicitação HTTP
         http.begin("http://54.207.230.239/site/query_insert_postgres_conexao_madeira_teste.php");
         //http.begin("http://54.207.230.239/site/query_insert_postgres_conexao_madeira.php");
         //http.begin("http://54.207.230.239/site/query_insert_CMDados_paradas.php");
         http.addHeader("Content-Type", "application/x-www-form-urlencoded");
         sprintf(logdata, "on_off=%d&mac_address=%s&data_hora=%04d-%02d-%02d %02d:%02d:%02d.%ld\n", flag_ON_OFF, MAC_ADDRESS, timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec, randNumber);
         sprintf(csv,"%d,%s,%04d-%02d-%02d %02d:%02d:%02d.%ld\n", flag_ON_OFF, MAC_ADDRESS, timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec, randNumber);
         Serial.println(csv);
         File file = SD.open(mySDCardPath, FILE_APPEND);
         if (file.print(csv)) 
               {                  
                  Serial.println("Mensagem anexada com sucesso no cartão SD!!!");
               } else {                  
                  Serial.print("Falha ao anexar no cartão SD!");
               }
         file.close();

         int httpResponseCode = http.POST(logdata); //publica o post

         //verifica se foi possivel fazer o insert com post
         if (httpResponseCode > 0) {
            String response = http.getString(); //Obtém a resposta do request
            //Serial.println(httpResponseCode); //Printa o código do retorno
            //Serial.println("");
            //Serial.println(response); //Printa a resposta do request
            //Serial.println("");

            //Se o INSERT no banco não retornar um "OK", salva na memória flash.
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
               Serial.println("");
               Serial.println("Insert realizado com sucesso!");

            }
         } else {
            //Se acontecer algum outro tipo de erro ao enviar o POST, salva na memória flash.
            Serial.print("Erro ao enviar POST: ");
            Serial.println(httpResponseCode);
            Serial.println("Salvando dados na memória flash...");
            pFile = SPIFFS.open(myFilePath, FILE_APPEND);
            if (pFile.print(logdata)) {
               Serial.println("Mensagem anexada com sucesso!!!");

            } else {
               Serial.print("Falha ao anexar!");
            }

            pFile.close();
         }
         //flag_Valida_Internet = false;
      } else {
         long randNumber = random(999999);
         //Se não tiver conexão com o WiFi salva na memória flash
         Serial.println("Sem conexão com a internet!");
         Serial.println("Salvando dados na memória flash...");
         Serial.println("");
         sprintf(logdata, "on_off=%d&mac_address=%s&data_hora=%04d-%02d-%02d %02d:%02d:%02d.%ld\n", flag_ON_OFF, MAC_ADDRESS, timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec, randNumber);
         pFile = SPIFFS.open(myFilePath, FILE_APPEND);
         if (pFile.print(logdata)) {
            Serial.println("Mensagem anexada com sucesso!!!");

         } else {
            Serial.print("Falha ao anexar!");
         }

         pFile.close();
      }
      
   } else {
      long randNumber = random(999999);
         //Se não tiver conexão com o WiFi salva na memória flash
         Serial.println("Sem conexão com a rede!");
         Serial.println("Salvando dados na memória flash...");
         Serial.println("");
         sprintf(logdata, "on_off=%d&mac_address=%s&data_hora=%04d-%02d-%02d %02d:%02d:%02d.%ld\n", flag_ON_OFF, MAC_ADDRESS, timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec, randNumber);
         pFile = SPIFFS.open(myFilePath, FILE_APPEND);
         if (pFile.print(logdata)) {
            Serial.println("Mensagem anexada com sucesso!!!");

         } else {
            Serial.print("Falha ao anexar!");
         }

         pFile.close();
      }

      Serial.print("Linhas armazenadas: ");
      Serial.println(count_Lines_SPIFFS());
      Serial.println("");
      vTaskDelay(pdMS_TO_TICKS(100));
      //timerWrite(timer, 0); //reseta o temporizador (alimenta o watchdog) 
   }


int return_Relay_State() {
   if (digitalRead(RELAY_PIN) == 1) {
      relay_State = RELAY_ON;
   } else if (digitalRead(RELAY_PIN) == 0) {
      relay_State = RELAY_OFF;
   }

   vTaskDelay(pdMS_TO_TICKS(50));
   return relay_State;
}

boolean return_Array_State() {
   for (int i = 0; i < ARRAY_SIZE; i++) {
      if (vetor[i] == 1) {
         count_ON_OFF++;
      }
   }

   if (count_ON_OFF > 0) {
      return true;
   } else {
      return false;
   }
}

void listAllFiles() {
   String linhas = "";
   int count = 0;
   // Read file content
   File file = SPIFFS.open(myFilePath, FILE_READ);
   Serial.println("");
   Serial.println("                     *********Conteúdo armazenado*********");
   while (file.available()) {
      linhas = file.readStringUntil('\n');
      Serial.println(linhas);
      count++;
      //Serial.write(file.read());
   }

   Serial.println("");
   Serial.print("Quantidade de linhas: ");
   Serial.println(count);
   // Check file size
   Serial.print("Tamanho do arquivo: ");
   Serial.println(file.size());
   file.close();
   Serial.println("");
}

void check_ON_OFF() {
   uint32_t snd = 0;
   if (count_Seconds <= ARRAY_SIZE) {
      //  if (count_Seconds == 1) {
      //    Serial.println("");
      //  }
      Serial.print(count_Seconds);
      Serial.print("ª leitura = ");
      Serial.println(return_Relay_State());
      vetor[count_Seconds] = return_Relay_State();
   } else if (count_Seconds > ARRAY_SIZE) {
      count_Seconds = 0;

      Serial.println("");
      Serial.print("Estado do rele ");
      if (return_Array_State() == true) {
         Serial.println("Ligado");
         Serial.print("Tempo: ");
         Serial.print(count_ON_OFF + 1);
         Serial.println(" seg.");
         flag_ON_OFF = 1;
      } else if (return_Array_State() == false) {
         Serial.println("Desligado");
         Serial.print("Tempo: ");
         Serial.print(ARRAY_SIZE - count_ON_OFF);
         Serial.println(" seg.");
         flag_ON_OFF = 0;
      }

      //zerar contadores de tempo
      count_ON_OFF = 0;
      //enviar POST
      //send_POST();
      xQueueSend(buffer, & snd, pdMS_TO_TICKS(0));
      //Serial.println("");
   }
}

// Exibe última amostra de temperatura e umidade obtida
void showLastRecord() {
   Serial.println("Last record:");
   Serial.println(lastRecord);
}

// Exibe o espaço total, usado e disponível no display
void showAvailableSpace(String values) {
   Serial.println("Space: " + String(ObjFS.getTotalSpace()) + " Bytes");
   Serial.println("Used: " + String(ObjFS.getUsedSpace()) + " Bytes");
} //fim da função

void loop() {
   vTaskDelay(pdMS_TO_TICKS(10));
   esp_task_wdt_delete(NULL);
}
