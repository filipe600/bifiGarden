/********************************************************
   BiFi Garden
   Controlador para Sistema de Monitoramento e Irrigação Automática de Jardim/Horta


   Pode ser usado com hardware baseado nos microcontroladores ESP32 e ESP8266
   Instruções de montagem e placa dedicada estão disponíveis no diretório do projeto

   Código baseado no exemplo de Andre Michelon do canal Internet e Coisas
*/

/*********************************************************

   Esse steck deve ser carregado, depois deve ser feito o upload dos arquivos html da interface Web através do plugin SPIFFS
   É recomendado utilizar 2MB para a partição SPIFFS

   Para configurar a primeira inicilização o PINO 5 (GPIO14) deve ter colocado em GND e então as configurações podem
   ser feitas através da rede "Bifi Gardem - XXXXXXXX" (senha "bifigarden").
*/

#define ESP8266

#ifdef ESP8266
//ESP8266
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <FS.h>
#else
//ESP32
#include <WiFi.h>
#include <WebServer.h>
#include <SPIFFS.h>
#endif

#include "Arduino.h"
#include "Esp.h"
#include <DNSServer.h>
#include <TimeLib.h>
#include <ArduinoJson.h>

// WebServer Port
const byte WEBSERVER_PORT = 80;

// Web Server Headers
const char *WEBSERVER_HEADER_KEYS[] = {"User-Agent"};

// DNS Port
const byte DNSSERVER_PORT = 53;

// LED Pin
#ifdef ESP32
// ESP32
const byte LED_PIN = 2;
#else
// ESP8266
const byte LED_PIN = LED_BUILTIN;
#endif

//Reset Button
const byte RESET_PIN = 14;

// LED State
#ifdef ESP8266
const byte LED_ON = LOW;
const byte LED_OFF = HIGH;
#else
const byte LED_ON = HIGH;
const byte LED_OFF = LOW;
#endif

// JSON Size
const size_t JSON_SIZE = JSON_OBJECT_SIZE(15) + 530;

// Web Server
#ifdef ESP8266
ESP8266WebServer server(WEBSERVER_PORT);
#else
WebServer server(WEBSERVER_PORT);
#endif

// DNS Server
DNSServer dnsServer;

char id[30];      // Identificação do dispositivo
boolean wifiMode; // Modo do Wifi - 0 AP - 1 STA
char ssid[30];    // SSID Rede WiFi
char pw[30];      // Senha da Rede WiFi

boolean tSEnable;     // ThingSpeak habilitado
char tSChannelId[50]; // ThingSpeak Channel Id
char tSWriteKey[50];  // ThingSpeak WriteKey

boolean mqttEnable;        // MQTT Habilitado
char mqttName[30];         // Nome MQTT
char mqttServerAdress[50]; // Endereço do servidor MQTT
word mqttServerPort;       // Porta Servidor MQTT
char mqttUser[30];         // Usuário MQTT
char mqttPw[30];           // Senha MQTT
boolean mqttTLS;           // TLS MQTT
boolean mqttSSTLS;         // Self-Signed TLS MQTT

void log(String s)
{
  // Gera log na Serial
  Serial.println(s);
}

String softwareStr()
{
  // Retorna nome do software
  return String(__FILE__).substring(String(__FILE__).lastIndexOf("\\") + 1);
}

String longTimeStr(const time_t &t)
{
  // Retorna segundos como "d:hh:mm:ss"
  String s = String(t / SECS_PER_DAY) + "d ";
  if (hour(t) < 10)
  {
    s += '0';
  }
  s += String(hour(t)) + ':';
  if (minute(t) < 10)
  {
    s += '0';
  }
  s += String(minute(t)) + ':';
  if (second(t) < 10)
  {
    s += '0';
  }
  s += String(second(t));
  return s;
}

String hexStr(const unsigned long &h, const byte &l = 8)
{
  // Retorna valor em formato hexadecimal
  String s;
  s = String(h, HEX);
  s.toUpperCase();
  s = ("00000000" + s).substring(s.length() + 8 - l);
  return s;
}

String deviceID()
{
  // Retorna ID padrão
#ifdef ESP8266
  // ESP8266 utiliza função getChipId()
  return "Bifi Garden - " + hexStr(ESP.getChipId());
#else
  // ESP32 utiliza função getEfuseMac()
  return "Bifi Garden - " + hexStr(ESP.getEfuseMac());
#endif
}

String ipStr(const IPAddress &ip)
{
  // Retorna IPAddress em formato "n.n.n.n"
  String sFn = "";
  for (byte bFn = 0; bFn < 3; bFn++)
  {
    sFn += String((ip >> (8 * bFn)) & 0xFF) + ".";
  }
  sFn += String(((ip >> 8 * 3)) & 0xFF);
  return sFn;
}
void status()
{
  FSInfo fs_info;
  SPIFFS.info(fs_info);
  Serial.print("File System: ");
  Serial.print(fs_info.usedBytes / 1000);
  Serial.print(" kB Used / ");
  Serial.print(fs_info.totalBytes / 1000);
  Serial.println(" kB Total");
  Serial.print("Flash Size: ");
  Serial.print(ESP.getFlashChipSize() / 1000000);
  Serial.println("MB");
  Serial.print("Steck Size: ");
  Serial.print(ESP.getSketchSize() / 1000);
  Serial.println(" kB");
  Serial.print("Version: ");
#ifdef ESP8266
  Serial.println(ESP.getFullVersion());
#else
  Serial.println(ESP.getSdkVersion());
#endif
  Serial.print("CPU Frequency: ");
  Serial.print(ESP.getCpuFreqMHz());
  Serial.println(" MHz");
  Serial.print("Chip Id: ");
#ifdef ESP8266
  Serial.println(ESP.getChipId(), HEX);
#else
  Serial.println(ESP.getEfuseMac(), HEX);
#endif
}

// void ledSet() {
//   // Define estado do LED
//   digitalWrite(LED_PIN, ledOn ? LED_ON : LED_OFF);
// }

// Funções de Configuração ------------------------------
void configReset()
{
  // Define configuração padrão

  strlcpy(id, "bifi-garden", sizeof(id));
  wifiMode = false;
  strlcpy(ssid, "", sizeof(ssid));
  strlcpy(pw, "", sizeof(pw));
  tSEnable = false;
  strlcpy(tSChannelId, "", sizeof(tSChannelId));
  strlcpy(tSWriteKey, "", sizeof(tSWriteKey));
  mqttEnable = false;
  strlcpy(mqttName, "", sizeof(mqttName));
  strlcpy(mqttServerAdress, "", sizeof(mqttServerAdress));
  mqttServerPort = 0;
  strlcpy(mqttUser, "", sizeof(mqttUser));
  strlcpy(mqttPw, "", sizeof(mqttPw));
  mqttTLS = false;
  mqttSSTLS = false;
}

boolean configRead()
{
  
  // Lê configuração
  StaticJsonDocument<JSON_SIZE> jsonConfig;

  File file = SPIFFS.open(F("/config.json"), "r");
  if (deserializeJson(jsonConfig, file))
  {
    // Falha na leitura, assume valores padrão
    configReset();
    log(F("Falha lendo CONFIG, assumindo valores padrão."));
    return false;
  }
  else
  {
    // Sucesso na leitura
    strlcpy(id, jsonConfig["id"] | "", sizeof(id));
    wifiMode = jsonConfig["wifiMode"] | false;
    strlcpy(ssid, jsonConfig["ssid"] | "", sizeof(ssid));
    strlcpy(pw, jsonConfig["pw"] | "", sizeof(pw));
    tSEnable = jsonConfig["tSEnable"] | false;
    strlcpy(tSChannelId, jsonConfig["tSChannelId"] | "", sizeof(tSChannelId));
    strlcpy(tSWriteKey, jsonConfig["tSWriteKey"] | "", sizeof(tSWriteKey));
    mqttEnable = jsonConfig["mqttEnable"] | false;
    strlcpy(mqttName, jsonConfig["mqttName"] | "", sizeof(mqttName));
    strlcpy(mqttServerAdress, jsonConfig["mqttServerAdress"] | "", sizeof(mqttServerAdress));
    mqttServerPort = jsonConfig["mqttServerPort"] | 0;
    strlcpy(mqttUser, jsonConfig["mqttUser"] | "", sizeof(mqttUser));
    strlcpy(mqttPw, jsonConfig["mqttPw"] | "", sizeof(mqttPw));
    mqttTLS = jsonConfig["mqttTLS"] | false;
    mqttSSTLS = jsonConfig["mqttSSTLS"] | false;

    file.close();

    log(F("\nLendo config:"));
    serializeJsonPretty(jsonConfig, Serial);
    log("");

    return true;
  }
}

boolean configSave()
{
  // Grava configuração
  StaticJsonDocument<JSON_SIZE> jsonConfig;

  File file = SPIFFS.open(F("/config.json"), "w+");
  if (file)
  {
    // Atribui valores ao JSON e grava
    jsonConfig["id"] = id;
    jsonConfig["wifiMode"] = wifiMode;
    jsonConfig["ssid"] = ssid;
    jsonConfig["pw"] = pw;
    jsonConfig["tSEnable"] = tSEnable;
    jsonConfig["tSChannelId"] = tSChannelId;
    jsonConfig["tSWriteKey"] = tSWriteKey;
    jsonConfig["mqttEnable"] = mqttEnable;

    jsonConfig["mqttName"] = mqttName;
    jsonConfig["mqttServerAdress"] = mqttServerAdress;
    jsonConfig["mqttServerPort"] = mqttServerPort;
    jsonConfig["mqttUser"] = mqttUser;
    jsonConfig["mqttPw"] = mqttPw;
    jsonConfig["mqttTLS"] = mqttTLS;
    jsonConfig["mqttSSTLS"] = mqttSSTLS;


    serializeJsonPretty(jsonConfig, file);
    file.close();

    log(F("\nGravando config:"));
    serializeJsonPretty(jsonConfig, Serial);
    log("");

    return true;
  }
  return false;
}

void handleHome()
{
  // Home
  File file = SPIFFS.open(F("/home.htm"), "r");
  if (file)
  {
  file.setTimeout(100);
  String s = file.readString();
  file.close();

  // Atualiza conteúdo dinâmico
  s.replace(F("#host#"), WiFi.hostname());
  s.replace(F("#flashSize#"), String(ESP.getFlashChipSize() / 1000000));
  s.replace(F("#sketchSize#"), String(ESP.getSketchSize() / 1000));
  FSInfo fs_info;
  SPIFFS.info(fs_info);
  s.replace(F("#usedBytes#"), String(fs_info.usedBytes / 1000));
  s.replace(F("#totalBytes#"), String(fs_info.totalBytes / 1000));
  s.replace(F("#cpuFreq#"), String(ESP.getCpuFreqMHz()));
  #ifdef ESP8266
    s.replace(F("#id#"), hexStr(ESP.getChipId()));
  #else
    s.replace(F("#id#"), hexStr(ESP.getEfuseMac()));
  #endif
  s.replace(F("#mac#"), WiFi.macAddress());
    #ifdef ESP8266
  s.replace(F("#version#"), (ESP.getFullVersion()));
  #else
    s.replace(F("#version#"), (ESP.getSdkVersion()));
  #endif
  s.replace(F("#wifiMode#"), wifiMode ? F("STA Mode") : F("AP Mode"));
  s.replace(F("#ssid#"), ssid);
  s.replace(F("#sysIp#"), ipStr(WiFi.status() == WL_CONNECTED ? WiFi.localIP() : WiFi.softAPIP()));
  s.replace(F("#activeTime#"), longTimeStr(millis() / 1000));

  // Envia dados
  server.send(200, F("text/html"), s);
  log("Home - Cliente: " + ipStr(server.client().remoteIP()) +
    (server.uri() != "/" ? " [" + server.uri() + "]" : ""));
  }
  else
  {
    server.send(500, F("text/plain"), F("Home - ERROR 500"));
    log(F("Home - ERRO lendo arquivo"));
  }
}
void handleSystem()
{
  // Config
  File file = SPIFFS.open(F("/system.htm"), "r");
  if (file)
  {
    file.setTimeout(100);
    String s = file.readString();
    file.close();

    s.replace(F("#id#"), id);
    s.replace(F("#APMode#"), wifiMode ? "" : " checked ");
    s.replace(F("#STAMode#"), wifiMode ? " checked" : "");
    s.replace(F("#ssid#"), ssid);
    s.replace(F("#mqttEnable#"), mqttEnable ? "checked" : "");
    s.replace(F("#mqttName#"), mqttName);
    s.replace(F("#mqttServerAdress#"), mqttServerAdress);
    s.replace(F("#mqttServerPort#"), String(mqttServerPort));
    s.replace(F("#mqttUser#"), mqttUser);
    s.replace(F("#mqttTLS#"), mqttTLS ? "checked" : "");
    s.replace(F("#mqttSSTLS#"), mqttSSTLS ? "checked" : "");
    s.replace(F("#tSEnable#"), tSEnable ? "checked" : "");
    s.replace(F("#tSChannelId#"), tSChannelId);
    s.replace(F("#tSWriteKey#"), tSWriteKey);

    // Send data
    server.send(200, F("text/html"), s);
    log("Config - Cliente: " + ipStr(server.client().remoteIP()));
  }
  else
  {
    server.send(500, F("text/plain"), F("Config - ERROR 500"));
    log(F("Config - ERRO lendo arquivo"));
  }
}

void handleSystemSave()
{
  // Grava Config
  String s;

  // Grava id
  
  s = server.arg("id");
  s.trim();
  if (s == "")
  {
    s = deviceID();
  }
  strlcpy(id, s.c_str(), sizeof(id));
  log("Id:"+s);
  // Grava modo wifi
  s = server.arg("WifiMode");
  s.trim();
  if (s == "WifiAP")
  {
    wifiMode = false;
  }
  else if (s == "WifiSTA")
  {
    wifiMode = true;
  }
  log("WifiMode:"+s);
  // Grava ssid
  s = server.arg("ssid");
  s.trim();
  strlcpy(ssid, s.c_str(), sizeof(ssid));
  log("SSID:"+s);

  // Grava pw
  s = server.arg("pw");
  s.trim();
  if (s != "")
  {
    // Atualiza senha, se informada
    strlcpy(pw, s.c_str(), sizeof(pw));
  }
  log("PW:"+s);

  // Grava estado mqtt
  if (server.arg("mqttEnable") !="")
  {
    s = server.arg("mqttEnable");
    log("mqttEnable:"+s);
    mqttEnable = true;
    // Grava nome mqtt
    s = server.arg("mqttName");
    s.trim();
    strlcpy(mqttName, s.c_str(), sizeof(mqttName));
    log("mqttName:"+s);

    // Grava endereco mqtt
    s = server.arg("mqttServerAdress");
    s.trim();
    strlcpy(mqttServerAdress, s.c_str(), sizeof(mqttServerAdress));
    log("mqttAdress:"+s);

    // Grava porta mqtt
    s = server.arg("mqttServerPort");
    mqttServerPort = s.toInt();
    log("mqttPort:"+s);

    // Grava usuario mqtt
    s = server.arg("mqttUser");
    s.trim();
    strlcpy(mqttUser, s.c_str(), sizeof(mqttUser));
    log("mqttUser:"+s);

    // Grava senha mqtt
    s = server.arg("mqttPw");
    s.trim();
    strlcpy(mqttPw, s.c_str(), sizeof(mqttPw));
    log("mqttPw:"+s);

    // Grava estado TLS mqtt
    if(server.arg("mqttTLS")!=""){
      mqttTLS = true;
    }
    else
    {
      mqttTLS = false;
    }

    // Grava estado Self Signed TLS mqtt
    if(server.arg("mqttSSTLS")!=""){
      mqttSSTLS = true;
    }
    else
    {
      mqttSSTLS = false;
    }
  }
  else
  {
    mqttEnable = false;
  }

  // Grava estado ThingSpeak
    if(server.arg("TSEnable")!=""){
      tSEnable = true;
      // Grava CAnal TS
      s = server.arg("tSChannelId");
      s.trim();
      strlcpy(tSChannelId, s.c_str(), sizeof(tSChannelId));

      // Grava chave TS
      s = server.arg("tSWriteKey");
      s.trim();
      strlcpy(tSWriteKey, s.c_str(), sizeof(tSWriteKey));
    }
    else{
    tSEnable = false;
    }
    

    

    // Grava configuração
    if (configSave()) {
    server.send(200, F("text/html"), F("<html><meta charset='UTF-8'><script>alert('Configuração salva.'); window.location = \"/system\"</script></html>"));
    log("ConfigSave - Cliente: " + ipStr(server.client().remoteIP()));
    } else {
    server.send(200, F("text/html"), F("<html><meta charset='UTF-8'><script>alert('Falha salvando configuração.');history.back()</script></html>"));
    log(F("ConfigSave - ERRO salvando Config"));
    }
}


void handleConfig()
{
  // Config
  File file = SPIFFS.open(F("/config.htm"), "r");
  if (file)
  {
    file.setTimeout(100);
    String s = file.readString();
    file.close();

    // Atualiza conteúdo dinâmico
    s.replace(F("#id#"), id);
    //s.replace(F("#ledOn#")  ,  ledOn ? " checked" : "");
    //s.replace(F("#ledOff#") , !ledOn ? " checked" : "");
    s.replace(F("#ssid#"), ssid);
    // Send data
    server.send(200, F("text/html"), s);
    log("Config - Cliente: " + ipStr(server.client().remoteIP()));
  }
  else
  {
    server.send(500, F("text/plain"), F("Config - ERROR 500"));
    log(F("Config - ERRO lendo arquivo"));
  }
}

void handleConfigSave()
{
  // Grava Config
  // Verifica número de campos recebidos
#ifdef ESP8266
  // ESP8266 gera o campo adicional "plain" via post
  if (server.args() == 5)
  {
#else
  // ESP32 envia apenas os 4 campos
  if (server.args() == 4)
  {
#endif
    String s;

    // Grava id
    s = server.arg("id");
    s.trim();
    if (s == "")
    {
      s = deviceID();
    }
    strlcpy(id, s.c_str(), sizeof(id));

    // Grava ssid
    s = server.arg("ssid");
    s.trim();
    strlcpy(ssid, s.c_str(), sizeof(ssid));

    // Grava pw
    s = server.arg("pw");
    s.trim();
    if (s != "")
    {
      // Atualiza senha, se informada
      strlcpy(pw, s.c_str(), sizeof(pw));
    }

    // Grava ledOn
    //ledOn = server.arg("led").toInt();

    // Grava configuração
    if (configSave())
    {
      server.send(200, F("text/html"), F("<html><meta charset='UTF-8'><script>alert('Configuração salva.');history.back()</script></html>"));
      log("ConfigSave - Cliente: " + ipStr(server.client().remoteIP()));
    }
    else
    {
      server.send(200, F("text/html"), F("<html><meta charset='UTF-8'><script>alert('Falha salvando configuração.');history.back()</script></html>"));
      log(F("ConfigSave - ERRO salvando Config"));
    }
  }
  else
  {
    server.send(200, F("text/html"), F("<html><meta charset='UTF-8'><script>alert('Erro de parâmetros.'); window.location = \"/\";</script></html>"));
  }
}
// void handleReconfig() {
//   // Reinicia Config
//   configReset();

//   // Atualiza LED
//   ledSet();

//   // Grava configuração
//   if (configSave()) {
//     server.send(200, F("text/html"), F("<html><meta charset='UTF-8'><script>alert('Configuração reiniciada.');window.location = '/'</script></html>"));
//     log("Reconfig - Cliente: " + ipStr(server.client().remoteIP()));
//   } else {
//     server.send(200, F("text/html"), F("<html><meta charset='UTF-8'><script>alert('Falha reiniciando configuração.');history.back()</script></html>"));
//     log(F("Reconfig - ERRO reiniciando Config"));
//   }
// }

void handleReboot()
{
  // Reboot
  File file = SPIFFS.open(F("/reboot.htm"), "r");
  if (file)
  {
    server.streamFile(file, F("text/html"));
    file.close();
    log("Reboot - Cliente: " + ipStr(server.client().remoteIP()));
    delay(100);
    ESP.restart();
  }
  else
  {
    server.send(500, F("text/plain"), F("Reboot - ERROR 500"));
    log(F("Reboot - ERRO lendo arquivo"));
  }
}

void handleCSS()
{
  // Arquivo CSS
  File file = SPIFFS.open(F("/style.css"), "r");
  if (file)
  {
    // Define cache para 3 dias
    server.sendHeader(F("Cache-Control"), F("public, max-age=172800"));
    server.streamFile(file, F("text/css"));
    file.close();
    log("CSS - Cliente: " + ipStr(server.client().remoteIP()));
  }
  else
  {
    server.send(500, F("text/plain"), F("CSS - ERROR 500"));
    log(F("CSS - ERRO lendo arquivo"));
  }
}

void setup()
{
#ifdef ESP8266
  // Velocidade para ESP8266
  Serial.begin(74880);
#else
  // Velocidade para ESP32
  Serial.begin(115200);
#endif

  // SPIFFS
  if (!SPIFFS.begin())
  {
    log(F("SPIFFS ERRO"));
    while (true)
      ;
  }

  // Lê configuração
  configRead();
  pinMode(LED_PIN,OUTPUT);

  pinMode(RESET_PIN,INPUT_PULLUP);

  digitalWrite(LED_PIN,LED_OFF);

  //Reset Config
  if(!digitalRead(RESET_PIN)){
    wifiMode = false;
    String s = deviceID();
    strlcpy(ssid, s.c_str(), sizeof(ssid));
    s = "bifigarden";
    strlcpy(pw, s.c_str(), sizeof(pw));
    digitalWrite(LED_PIN,LED_ON);
  }

  //Wifi STA
  if(wifiMode){  
    WiFi.softAPdisconnect (true);
    WiFi.hostname(id);
    WiFi.begin(ssid, pw);
    log("Conectando WiFi " + String(ssid));
    byte b = 0;
    while (WiFi.status() != WL_CONNECTED && b < 60)
    {
      b++;
      Serial.print(".");
      delay(500);
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED)
    {
      // WiFi Station conectado
      log("WiFi conectado (" + String(WiFi.RSSI()) + ") IP " + ipStr(WiFi.localIP()));
    }
    else
    {
      log(F("WiFi não conectado"));
    }
  }

  //Wifi AP
  else{
    
    #ifdef ESP8266
      // Configura WiFi para ESP8266
      WiFi.hostname(id);
      WiFi.softAP(ssid, pw);
    #else
      // Configura WiFi para ESP32
      WiFi.setHostname(id.c_str());
      WiFi.softAP(ssid.c_str(), pw.c_str());
    #endif
    log("WiFi AP -> " +String(ssid) + " - IP " + ipStr(WiFi.softAPIP()));

    // Habilita roteamento DNS
    dnsServer.setErrorReplyCode(DNSReplyCode::ServerFailure);
    dnsServer.start(DNSSERVER_PORT, "*", WiFi.softAPIP());

  }

  // WebServer
  server.on(F("/config"), handleConfig);
  server.on(F("/configSave"), handleConfigSave);
  server.on(F("/system"), handleSystem);
  server.on(F("/systemSave"), handleSystemSave);
  server.on(F("/reboot"), handleReboot);
  server.on(F("/style.css"), handleCSS);
  server.onNotFound(handleHome);
  server.collectHeaders(WEBSERVER_HEADER_KEYS, 1);
  server.begin();

  status();

  // Pronto
  log(F("Pronto"));
}

void loop()
{

  yield();
  dnsServer.processNextRequest();
  server.handleClient();
}