#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <WiFiClientSecure.h>
#include <time.h>
#include "ModuloRele.hpp"

/* Libera prints para debug */
//#define DEBUG

/* Pinos GPIOs */
#define BOMBA               36
#define SENSOR_DE_FLUXO     39
#define SENSOR_NIVEL_BAIXO  27
#define SENSOR_NIVEL_ALTO   14
#define RELE_BOMBA          2
#define RELE_CAIXA          15
#define RELE_SETOR_1        5
#define RELE_SETOR_2        4
#define BEEP                18 
#define UMIDADE             13

/* Variáveis */
const int RELAYS[N_RELES] = {RELE_BOMBA, RELE_CAIXA, RELE_SETOR_1, RELE_SETOR_2};
bool flagLigaCaixa = 0;
bool flagLigaSetor1 = 0;
bool flagLigaSetor2 = 0;
bool flagLigaBomba = 0;
bool flagOcupado = 0;
int flagSelecao = 0;

/* Estruturas*/
struct irrigacaoConf
{
  int horaInicioSetor1 = 17;                      //Hora de início da irrigação
  int minutoInicioSetor1 = 0;                     //minutos de início da irrigação
  int horaInicioSetor2 = 17;                      //Hora de início da irrigação
  int minutoInicioSetor2 = 35;                    //minutos de início da irrigação
  int duracao = 30;                               //30 minutos
  int diaDaSemana[7] = {1, 1, 1, 1, 1, 1, 1};     //Habilitar dia da semana dom ... sab
};

struct tempoAtual
{
  int hInicioSetor1 = 0;                      //Hora real de início da irrigação
  int mInicioSetor1 = 0;                     //minutos real de início da irrigação
  int hFimSetor1 = 0;
  int mFimSetor1 = 0;
  int hInicioSetor2 = 0;                      //Hora real de início da irrigação
  int mInicioSetor2 = 0;                    //minutos real de início da irrigação
  int hFimSetor2 = 0;
  int mFimSetor2 = 0;
};

//---- WiFi settings
const char* ssid = "Lu e Deza";
const char* password = "liukin1208";

// Dados para acesso ao MQTT
const char* mqtt_server = "503847782e204ff99743e99127691fe7.s1.eu.hivemq.cloud";    //Host do broker
const int porta_TLS = 8883;                                                         //Porta
const char* mqtt_usuario = "Aquabox2";                                               //Usuário
const char* mqtt_senha = "Liukin@0804";                                             //Senha do usuário
const char* topico_tx = "Aquabox/tx";                                               //Tópico para transmitir dados
const char* topico_rx = "Aquabox/rx";                                               //Tópico para receber dados

WiFiClientSecure espClient;  
PubSubClient client(espClient);
unsigned long lastMsg = 0;
#define MSG_BUFFER_SIZE (500)
char msg[MSG_BUFFER_SIZE];
int value = 0;

// HiveMQ Cloud Let's Encrypt CA certificate
static const char *root_ca PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----
MIIFazCCA1OgAwIBAgIRAIIQz7DSQONZRGPgu2OCiwAwDQYJKoZIhvcNAQELBQAw
TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh
cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMTUwNjA0MTEwNDM4
WhcNMzUwNjA0MTEwNDM4WjBPMQswCQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJu
ZXQgU2VjdXJpdHkgUmVzZWFyY2ggR3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBY
MTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAK3oJHP0FDfzm54rVygc
h77ct984kIxuPOZXoHj3dcKi/vVqbvYATyjb3miGbESTtrFj/RQSa78f0uoxmyF+
0TM8ukj13Xnfs7j/EvEhmkvBioZxaUpmZmyPfjxwv60pIgbz5MDmgK7iS4+3mX6U
A5/TR5d8mUgjU+g4rk8Kb4Mu0UlXjIB0ttov0DiNewNwIRt18jA8+o+u3dpjq+sW
T8KOEUt+zwvo/7V3LvSye0rgTBIlDHCNAymg4VMk7BPZ7hm/ELNKjD+Jo2FR3qyH
B5T0Y3HsLuJvW5iB4YlcNHlsdu87kGJ55tukmi8mxdAQ4Q7e2RCOFvu396j3x+UC
B5iPNgiV5+I3lg02dZ77DnKxHZu8A/lJBdiB3QW0KtZB6awBdpUKD9jf1b0SHzUv
KBds0pjBqAlkd25HN7rOrFleaJ1/ctaJxQZBKT5ZPt0m9STJEadao0xAH0ahmbWn
OlFuhjuefXKnEgV4We0+UXgVCwOPjdAvBbI+e0ocS3MFEvzG6uBQE3xDk3SzynTn
jh8BCNAw1FtxNrQHusEwMFxIt4I7mKZ9YIqioymCzLq9gwQbooMDQaHWBfEbwrbw
qHyGO0aoSCqI3Haadr8faqU9GY/rOPNk3sgrDQoo//fb4hVC1CLQJ13hef4Y53CI
rU7m2Ys6xt0nUW7/vGT1M0NPAgMBAAGjQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNV
HRMBAf8EBTADAQH/MB0GA1UdDgQWBBR5tFnme7bl5AFzgAiIyBpY9umbbjANBgkq
hkiG9w0BAQsFAAOCAgEAVR9YqbyyqFDQDLHYGmkgJykIrGF1XIpu+ILlaS/V9lZL
ubhzEFnTIZd+50xx+7LSYK05qAvqFyFWhfFQDlnrzuBZ6brJFe+GnY+EgPbk6ZGQ
3BebYhtF8GaV0nxvwuo77x/Py9auJ/GpsMiu/X1+mvoiBOv/2X/qkSsisRcOj/KK
NFtY2PwByVS5uCbMiogziUwthDyC3+6WVwW6LLv3xLfHTjuCvjHIInNzktHCgKQ5
ORAzI4JMPJ+GslWYHb4phowim57iaztXOoJwTdwJx4nLCgdNbOhdjsnvzqvHu7Ur
TkXWStAmzOVyyghqpZXjFaH3pO3JLF+l+/+sKAIuvtd7u+Nxe5AW0wdeRlN8NwdC
jNPElpzVmbUq4JUagEiuTDkHzsxHpFKVK7q4+63SM1N95R1NbdWhscdCb+ZAJzVc
oyi3B43njTOQ5yOf+1CceWxG1bQVs5ZufpsMljq4Ui0/1lvh+wjChP4kqKOJ2qxq
4RgqsahDYVvTH9w7jXbyLeiNdd8XM2w9U/t7y0Ff/9yi0GE44Za4rF2LN9d11TPA
mRGunUHBcnWEvgJBQl9nJEiU0Zsnvgc/ubhPgXRR4Xq37Z0j4r7g1SgEEzwxA57d
emyPxgcYxn/eR44/KJ4EBs+lVDR3veyJm+kXQ99b21/+jh5Xos1AnX5iItreGCc=
-----END CERTIFICATE-----
)EOF";

//Relógio
/* Variáveis para o relógio*/
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = -14400; //GMT Time Brazil
const int   daylightOffset_sec = 3600;

//Inicializar Classes - Instanciar
ModuloRele reles(RELAYS[0], RELAYS[1], RELAYS[2], RELAYS[3], true);

//Inicializar Estruturas
struct tm timeinfo;
struct irrigacaoConf conf_Irriga;
struct tempoAtual tempo_Irriga;

// Protótipo de funções
void setup_wifi(void);
void callback(char* topic, byte* payload, unsigned int length);
void reconnect(void);
void Relogio(void);
void monitoraFlags (void);
void initSensores(void);
void checarSensores(void);
void encherCaixa(void);
void ligaBomba(void);
void ligaSetor1(void);
void ligaSetor2(void);
void calculoTempo(int hInicio, int mInicio, int setor);


void setup() 
{
  delay(500);
  // When opening the Serial Monitor, select 9600 Baud
  Serial.begin(9600);
  delay(500);

  setup_wifi();

  espClient.setCACert(root_ca);
  client.setServer(mqtt_server, porta_TLS);
  client.setCallback(callback);

  // Inicializa o relógio
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  //Inicializa os Relés
  
  reles.offAll();

  //Inicializa Sensores
  initSensores();

}

void loop() 
{
  if (!client.connected()) 
  {
    reconnect();
  }
  client.loop();

  Relogio();
  checarSensores();
  monitoraFlags();
}



// Funções
//-----------------------------------------------------------------------------

//Conecta Wifi
void setup_wifi(void) 
{
  delay(10);
  // Conectar a rede wifi
  Serial.println();
  Serial.print("Conectando a ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  randomSeed(micros());

  Serial.println("");
  Serial.println("WiFi conectado");
  Serial.print("Endereço IP: ");
  Serial.println(WiFi.localIP());
}

// Callback do Mqtt
void callback(char* topic, byte* payload, unsigned int length) 
{
  Serial.print("A Mensagem Chegou [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
}

//Reconecta Mqtt
void reconnect(void) 
{
  // Loop até que estejamos reconectados
  while (!client.connected()) {
    Serial.print("Tentando conexão MQTT… ");
    String clientId = "ESP32Client";
    // Tentar conectar
    if (client.connect(clientId.c_str(), mqtt_usuario, mqtt_senha)) {
      Serial.println("Mqtt conectado!");
      // Uma vez conectado, publique um anúncio…
      //client.publish("testTopic", "Hello World!");
      // … e resubscribe
      //client.subscribe("testTopic");
    } else {
      Serial.print("Falha, rc = ");
      Serial.print(client.state());
      Serial.println(" tente novamente em 5 segundos");
      // Aguarde 5 segundos antes de tentar novamente
      delay(5000);
    }
  }
}

//Atualiza relógio pela Internet
void Relogio(void)
{
  /* Estrutura de dados da struct tm                       */
   /*tm_sec;           segundo, faixa 0 to 59               */
   /*tm_min;           minutos, faixa 0 a 59                */
   /*tm_hora;          horas, intervalo de 0 a 23           */
   /*tm_mdia;          dia do mês, faixa 1 to 31            */
   /*tm_mon;           mês, intervalo de 0 a 11             */
   /*tm_ano;           O número de anos desde 1900          */
   /*tm_wday;          dia da semana, intervalo de 0 a 6    */
   /*tm_yday;          dia do ano, intervalo de 0 a 365     */
   /*tm_isdst;         horário de verão                     */

  

    if(!getLocalTime(&timeinfo))
    {
        
      Serial.println("Falha ao obter o relógio");
       
    }
    else
    {
      //Serial.println("Relógio Inicializado");
    }
}

void monitoraFlags (void)
{
  switch (flagSelecao)
  {
  case 0:
    /* code */
    break;
  
  case 1:
    encherCaixa();
    break;
  
  case 2:
    ligaBomba();
    break;
  
  case 3:
    ligaSetor1();
    break;

  case 4:
    ligaSetor2();
    break;

  default:
    break;
  }
}

void initSensores(void)
{
  pinMode(SENSOR_NIVEL_BAIXO, INPUT);
  pinMode(SENSOR_NIVEL_ALTO, INPUT);
  pinMode(BOMBA, INPUT);
}

void checarSensores(void)
{
  int nivel;

  //Checa as condições dos sensores e horários
  nivel = digitalRead(SENSOR_NIVEL_BAIXO);
  delay(20);
  if(!nivel)
  {
    //Serial.println("Caixa Vazia");
    flagLigaCaixa = 1;
    //flagSelecao = 1;
  }

  nivel = digitalRead(SENSOR_NIVEL_ALTO);
  delay(20);
  if(!nivel)
  {
    //Serial.println("Caixa Cheia");
    flagLigaCaixa = 0;
  }

  nivel = digitalRead(BOMBA);
  delay(20);
  if(!nivel)
  {
    flagLigaBomba = 1;
    //flagSelecao = 2;
  }
  else
  {
    flagLigaBomba = 0;
  }

  if((timeinfo.tm_hour == conf_Irriga.horaInicioSetor1) && (timeinfo.tm_min == conf_Irriga.minutoInicioSetor1) && (conf_Irriga.diaDaSemana[timeinfo.tm_wday]))
  {
    flagLigaSetor1 = 1;
  }

  if((timeinfo.tm_hour == conf_Irriga.horaInicioSetor2) && (timeinfo.tm_min == conf_Irriga.minutoInicioSetor2) && (conf_Irriga.diaDaSemana[timeinfo.tm_wday]))
  {
    flagLigaSetor2 = 1;
  }

  if((timeinfo.tm_hour == tempo_Irriga.hFimSetor1) && (timeinfo.tm_min == tempo_Irriga.mFimSetor1))
  {
    flagLigaSetor1 = 0;
  }

  if((timeinfo.tm_hour == tempo_Irriga.hFimSetor2) && (timeinfo.tm_min == tempo_Irriga.mFimSetor2))
  {
    flagLigaSetor2 = 0;
  }


  //Define a seleção
  if((flagLigaCaixa) && (!flagOcupado))
  {
    flagSelecao = 1;
  }
  else
  {
    if((flagLigaBomba) && (!flagOcupado))
    {
      flagSelecao = 2;
    }
    else
    {
      if((flagLigaSetor1) && (!flagOcupado))
      {
        flagSelecao = 3;
        //pega tempo que começou a irrigar setor 1
        tempo_Irriga.hInicioSetor1 = timeinfo.tm_hour;
        tempo_Irriga.mInicioSetor1 = timeinfo.tm_min;
        calculoTempo(tempo_Irriga.hInicioSetor1, tempo_Irriga.mInicioSetor1, 1);
      }
      else
      {
        if((flagLigaSetor2) && (!flagOcupado))
      {
        flagSelecao = 4;
        //pega tempo que começou a irrigar setor 2
        tempo_Irriga.hInicioSetor2 = timeinfo.tm_hour;
        tempo_Irriga.mInicioSetor2 = timeinfo.tm_min;
        calculoTempo(tempo_Irriga.hInicioSetor2, tempo_Irriga.mInicioSetor2, 2);
      }
      }
    }
  }
}

void calculoTempo(int hInicio, int mInicio, int setor)
{
  int totalMinutos = 0;
  
  if(setor == 1)
  {
    totalMinutos = (hInicio * 60) + mInicio + conf_Irriga.duracao;
    tempo_Irriga.hFimSetor1 = ((totalMinutos/60) % 24);
    tempo_Irriga.mFimSetor1 = totalMinutos % 60;
  }
  
  if(setor == 2)
  {
    totalMinutos = (hInicio * 60) + mInicio + conf_Irriga.duracao;
    tempo_Irriga.hFimSetor2 = ((totalMinutos/60) % 24);
    tempo_Irriga.mFimSetor2 = totalMinutos % 60;
  }
}

void encherCaixa(void)
{
  if((flagLigaCaixa) && (!flagOcupado))
  {
    flagOcupado = 1;
    reles.offAll();
    //delay(2000);
    reles.on(1);
    delay(5000);
    reles.on(0);
    Serial.println("Enchendo a Caixa");
  }
  
  if((!flagLigaCaixa) && (flagOcupado))
  {
    reles.off(0);
    delay(5000);
    reles.off(1);
    flagOcupado = 0;
    flagSelecao = 0;
    Serial.println("Caixa cheia");
  }
}

void ligaBomba(void)
{
  if((flagLigaBomba)&& (!flagOcupado))
  {
    reles.offAll();
    //delay(2000);
    flagOcupado = 1;
    reles.on(0);
    Serial.println("Bomba Ligada");
  }

  if((!flagLigaBomba) && (flagOcupado))
  {
    reles.off(0);
    flagOcupado = 0;
    flagSelecao = 0;
    Serial.println("Bomba desligada");
  }
}

void ligaSetor1(void)
{
  if((flagLigaSetor1) && (!flagOcupado))
  {
    flagOcupado = 1;
    reles.offAll();
    //delay(2000);
    reles.on(2);
    delay(5000);
    reles.on(0);
    Serial.println("Irrigando Setor 1");
  }
  
  if((!flagLigaSetor1) && (flagOcupado))
  {
    reles.off(0);
    delay(5000);
    reles.off(2);
    flagOcupado = 0;
    flagSelecao = 0;
    Serial.println("Desligado Setor 1");
  }
}

void ligaSetor2(void)
{
  if((flagLigaSetor2) && (!flagOcupado))
  {
    flagOcupado = 1;
    reles.offAll();
    //delay(2000);
    reles.on(3);
    delay(5000);
    reles.on(0);
    Serial.println("Irrigando Setor 2");
  }
  
  if((!flagLigaSetor2) && (flagOcupado))
  {
    reles.off(0);
    delay(5000);
    reles.off(3);
    flagOcupado = 0;
    flagSelecao = 0;
    Serial.println("Desligado Setor 2");
  }
}