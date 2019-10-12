#include <RTClib.h>
#include "FS.h"
#include "SD.h"
#include "SPI.h"
#include <TinyGPS.h>
#include <Adafruit_HMC5883_U.h>
#include <MapFloat.h>
#include <Adafruit_BMP085.h>
#include <Wire.h>
#include <SSD1306.h>

#define pinINT   27 

//Variaveis rpm
volatile int conta_RPM = 0;
int copyconta_RPM = 0;
float RPM = 0;
float contaAtualMillis = 0;
float antigoMillis = 0;
float subMillis = 0;
float minutos = 0;

//Variaveis tempo
float Tin  = 0;
RTC_DS1307 rtc;

//Variaveis gps
#define RXD2 16
#define TXD2 17
TinyGPS gps1;
float Altitude_soloLocal = 547.00;
int WOW = 0;

//Variaveis mag
Adafruit_HMC5883_Unified mag = Adafruit_HMC5883_Unified(12345);
float MagBow = 0;

//Variaveis pots
int Pot1 = 36;
int Pot2 = 39;
int Pot3 = 32;

//Variaveis bmps
int S0 = 0;
int S1 = 2;
float HP = 0;
Adafruit_BMP085 bmp_1;
Adafruit_BMP085 bmp_2;

//Variaveis mpu
const int MPU_addr=0x69;
int16_t AcX,AcY,AcZ,Tmp,GyX,GyY,GyZ;
int minVal=265;
int maxVal=402;
float x, y, z;
int iteracoes = 50, contagemFiltro;
float pitch, roll, pitchTotal, rollTotal;

//Objeto display
SSD1306 screen(0x3c, 21, 22);

//Funcao interrupcao
void IRAM_ATTR ContaInterrupt() {
  conta_RPM = conta_RPM + 1; //incrementa o contador de interrupções
}

//Funcoes microSD
void listDir(fs::FS &fs, const char * dirname, uint8_t levels){
    Serial.printf("Listing directory: %s\n", dirname);

    File root = fs.open(dirname);
    if(!root){
        Serial.println("Failed to open directory");
        return;
    }
    if(!root.isDirectory()){
        Serial.println("Not a directory");
        return;
    }

    File file = root.openNextFile();
    while(file){
        if(file.isDirectory()){
            Serial.print("  DIR : ");
            Serial.println(file.name());
            if(levels){
                listDir(fs, file.name(), levels -1);
            }
        } else {
            Serial.print("  FILE: ");
            Serial.print(file.name());
            Serial.print("  SIZE: ");
            Serial.println(file.size());
        }
        file = root.openNextFile();
    }
}

void createDir(fs::FS &fs, const char * path){
    Serial.printf("Creating Dir: %s\n", path);
    if(fs.mkdir(path)){
        Serial.println("Dir created");
    } else {
        Serial.println("mkdir failed");
    }
}

void removeDir(fs::FS &fs, const char * path){
    Serial.printf("Removing Dir: %s\n", path);
    if(fs.rmdir(path)){
        Serial.println("Dir removed");
    } else {
        Serial.println("rmdir failed");
    }
}

void readFile(fs::FS &fs, const char * path){
    Serial.printf("Reading file: %s\n", path);

    File file = fs.open(path);
    if(!file){
        Serial.println("Failed to open file for reading");
        return;
    }

    Serial.print("Read from file: ");
    while(file.available()){
        Serial.write(file.read());
    }
    file.close();
}

void writeFile(fs::FS &fs, const char * path, const char * message){
    Serial.printf("Writing file: %s\n", path);

    File file = fs.open(path, FILE_WRITE);
    if(!file){
        Serial.println("Failed to open file for writing");
        return;
    }
    if(file.print(message)){
        Serial.println("File written");
    } else {
        Serial.println("Write failed");
    }
    file.close();
}

void appendFile(fs::FS &fs, const char * path, const char * message){
    Serial.printf("Appending to file: %s\n", path);

    File file = fs.open(path, FILE_APPEND);
    if(!file){
        Serial.println("Failed to open file for appending");
        return;
    }
    if(file.print(message)){
        Serial.println("Message appended");
    } else {
        Serial.println("Append failed");
    }
    file.close();
}

void renameFile(fs::FS &fs, const char * path1, const char * path2){
    Serial.printf("Renaming file %s to %s\n", path1, path2);
    if (fs.rename(path1, path2)) {
        Serial.println("File renamed");
    } else {
        Serial.println("Rename failed");
    }
}

void deleteFile(fs::FS &fs, const char * path){
    Serial.printf("Deleting file: %s\n", path);
    if(fs.remove(path)){
        Serial.println("File deleted");
    } else {
        Serial.println("Delete failed");
    }
}

void testFileIO(fs::FS &fs, const char * path){
    File file = fs.open(path);
    static uint8_t buf[512];
    size_t len = 0;
    uint32_t start = millis();
    uint32_t end = start;
    if(file){
        len = file.size();
        size_t flen = len;
        start = millis();
        while(len){
            size_t toRead = len;
            if(toRead > 512){
                toRead = 512;
            }
            file.read(buf, toRead);
            len -= toRead;
        }
        end = millis() - start;
        Serial.printf("%u bytes read for %u ms\n", flen, end);
        file.close();
    } else {
        Serial.println("Failed to open file for reading");
    }


    file = fs.open(path, FILE_WRITE);
    if(!file){
        Serial.println("Failed to open file for writing");
        return;
    }

    size_t i;
    start = millis();
    for(i=0; i<2048; i++){
        file.write(buf, 512);
    }
    end = millis() - start;
    Serial.printf("%u bytes written for %u ms\n", 2048 * 512, end);
    file.close();
}

void setup() {
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);
  
  pinMode(pinINT,INPUT_PULLUP);
  attachInterrupt(pinINT,ContaInterrupt, RISING);

  while (!Serial);
  if (! rtc.begin()) {
    Serial.println("Couldn't find RTC");
    while (1);
  }
  //rtc.adjust(DateTime(2019, 10, 9, 10, 19, 0));  // (Ano,mês,dia,hora,minuto,segundo)

  if(!mag.begin())
  {
    Serial.println("Ooops, no HMC5883 detected ... Check your wiring!");
    while(1);
  }

  pinMode(Pot1,INPUT);
  pinMode(Pot2,INPUT);
  pinMode(Pot3,INPUT);

  pinMode(S0,OUTPUT);
  pinMode(S1,OUTPUT);
  digitalWrite(S0,LOW);
  digitalWrite(S1,LOW);
  if(!bmp_1.begin() ){           //Verifico se o sensor BMP180 está funcionando
    Serial.println("Sensor BMP1 não encontrado!");
    while(1){}
  } 
  digitalWrite(S0,HIGH);
  digitalWrite(S1,LOW); 
  if(!bmp_2.begin() ){           //Verifico se o sensor BMP180 está funcionando
    Serial.println("Sensor BMP2 não encontrado!");
    while(1){}
  }

  Wire.begin();
  Wire.beginTransmission(MPU_addr);
  Wire.write(0x6B);
  Wire.write(0);
  Wire.endTransmission(true);
  
  screen.init();
  screen.setFont(ArialMT_Plain_16);

  if(!SD.begin()){
    Serial.println("Card Mount Failed");
    return;
  }
  uint8_t cardType = SD.cardType();
  if(cardType == CARD_NONE){
    Serial.println("No SD card attached");
    return;
  }
  listDir   (SD, "/", 2);    
  writeFile (SD, "/Canarinho.txt", "");
  appendFile(SD, "/Canarinho.txt", "Tempo ");
  appendFile(SD, "/Canarinho.txt", "RPM ");
  appendFile(SD, "/Canarinho.txt", "XGPS  ");
  appendFile(SD, "/Canarinho.txt", "YGPS  ");
  appendFile(SD, "/Canarinho.txt", "ZGPS  ");
  appendFile(SD, "/Canarinho.txt", "WOW  ");
  appendFile(SD, "/Canarinho.txt", "VCAS  ");
  appendFile(SD, "/Canarinho.txt", "MagHead ");
  appendFile(SD, "/Canarinho.txt", "ELEV  ");
  appendFile(SD, "/Canarinho.txt", "AIL  ");
  appendFile(SD, "/Canarinho.txt", "RUD  ");
  appendFile(SD, "/Canarinho.txt", "HP  ");
  appendFile(SD, "/Canarinho.txt", "NZ  ");
  appendFile(SD, "/Canarinho.txt", "THETA ");
  appendFile(SD, "/Canarinho.txt", "PHI \r\n");
  
  appendFile(SD, "/Canarinho.txt", "[segundos] ");
  appendFile(SD, "/Canarinho.txt", "[RPM] ");
  appendFile(SD, "/Canarinho.txt", "[m] ");
  appendFile(SD, "/Canarinho.txt", "[m] ");
  appendFile(SD, "/Canarinho.txt", "[m] ");
  appendFile(SD, "/Canarinho.txt", "[bit] ");
  appendFile(SD, "/Canarinho.txt", "[m/s] ");
  appendFile(SD, "/Canarinho.txt", "[deg] ");
  appendFile(SD, "/Canarinho.txt", "[deg] ");
  appendFile(SD, "/Canarinho.txt", "[deg] ");
  appendFile(SD, "/Canarinho.txt", "[deg] ");
  appendFile(SD, "/Canarinho.txt", "[ft] ");
  appendFile(SD, "/Canarinho.txt", "[g] ");
  appendFile(SD, "/Canarinho.txt", "[deg] ");
  appendFile(SD, "/Canarinho.txt", "[deg] \r\n");
  
  //Serial.println("  RPM         copyconta_RPM");
  
}

void loop() {
  delayMicroseconds(10000);
  
  copyconta_RPM = conta_RPM;
  contaAtualMillis = millis(); 
  subMillis = contaAtualMillis - antigoMillis;
  minutos = (subMillis/(60000));
  RPM = copyconta_RPM/(minutos);
  antigoMillis = contaAtualMillis;
  conta_RPM = 0;
  
  //Serial.print(" ");
  //Serial.print(RPM);
  //Serial.print("                ");
  //Serial.println(copyconta_RPM);

  screen.clear();
  screen.drawString(20,  0, "RPM: "+String(RPM));

  DateTime now = rtc.now();
  Tin = ((now.hour()*3600)+(now.minute()*60)+(now.second())); 
  
  screen.drawString(20,  20, "Tempo: "+String(Tin));
  screen.display();

  bool recebido = false;
  while (Serial2.available()) {
    char cIn = Serial2.read();
    recebido = (gps1.encode(cIn) || recebido);
  }
  float latitude, longitude;
  unsigned long idadeInfo;
  gps1.f_get_position(&latitude, &longitude, &idadeInfo);
  float altitudeGPS;
  float altitudeResult;
  altitudeGPS = gps1.f_altitude();
  if ((altitudeGPS != TinyGPS::GPS_INVALID_ALTITUDE) && (altitudeGPS != 1000000)) {
    altitudeResult = (altitudeGPS - Altitude_soloLocal);
  if(altitudeResult > 3){
    WOW = 1;
  }
  if(altitudeResult < 3){
    WOW = 0;
  }
  altitudeResult = 0;
  }
  float velocidademps; float velocidadekmph;
  velocidademps = gps1.f_speed_mps();

  sensors_event_t event;
  mag.getEvent(&event);
  float heading = atan2(event.magnetic.y, event.magnetic.x);
  float declinationAngle = 5.94;
  heading += declinationAngle;
  if(heading < 0)
    heading += 2*PI;
  if(heading > 2*PI)
    heading -= 2*PI;
  float headingDegrees = heading * 180/M_PI;
  if(headingDegrees <= 208.00){
    MagBow = mapFloat(headingDegrees,208.00, 0.00,0.00, 208.00);
  }
  if(headingDegrees > 208.00){
    MagBow = mapFloat(headingDegrees,360.00 , 207.99, 208.01, 360.00);
  }

  int valuePot_Prof  = analogRead(Pot3); 
  if(valuePot_Prof <= 2047){
    valuePot_Prof  =  map(valuePot_Prof,0,2047,90,0);
  }
  else{
    valuePot_Prof  =  map(valuePot_Prof,2048,4095,0,-90);
  }
  int valuePot_Aileron  = analogRead(Pot1);
  if(valuePot_Aileron <= 2047){
    valuePot_Aileron  =  map(valuePot_Aileron,0,2047,90,0);
  }
  else{
    valuePot_Aileron  =  map(valuePot_Aileron,2048,4095,0,-90);
  }
  int valuePot_Leme  = analogRead(Pot2);
  if(valuePot_Leme <= 2047){
    valuePot_Leme  =  map(valuePot_Leme,0,2047,90,0);
  }
  else{
    valuePot_Leme  =  map(valuePot_Leme,2048,4095,0,-90);
  }

  digitalWrite(S0,HIGH);
  digitalWrite(S1,LOW);
  HP = bmp_2.readAltitude(101325);

  roll = 0;
  pitch = 0;
  Wire.beginTransmission(MPU_addr);
  Wire.write(0x3B);
  Wire.endTransmission(false);
  Wire.requestFrom(MPU_addr,14,true); 
  AcX=Wire.read()<<8|Wire.read();
  AcY=Wire.read()<<8|Wire.read();
  AcZ=Wire.read()<<8|Wire.read();
  int xAng = map(AcX,minVal,maxVal,-90,90);
  int yAng = map(AcY,minVal,maxVal,-90,90);
  int zAng = map(AcZ,minVal,maxVal,-90,90);

  x= RAD_TO_DEG * (atan2(-yAng, -zAng)+PI);
  y= RAD_TO_DEG * (atan2(-xAng, -zAng)+PI);
  z= RAD_TO_DEG * (atan2(-yAng, -xAng)+PI);
  pitch = x;
  roll = y;
  if(pitch > 180) pitch -= 360;
  if(roll > 180) roll -= 360;

  //Tirar comentario para testar o horizonte
  /*if(contagemFiltro < iteracoes) {
    contagemFiltro++;
    pitchTotal += pitch;
    rollTotal += roll;
    return;
  }
  contagemFiltro = 0;
  pitch = pitchTotal / iteracoes;
  roll = rollTotal / iteracoes;
  pitchTotal = 0;
  rollTotal = 0;
  */
  
  appendFile(SD, "/Canarinho.txt", String(Tin).c_str());
  appendFile(SD, "/Canarinho.txt", " ");
  appendFile(SD, "/Canarinho.txt", String(RPM).c_str());
  if (latitude != TinyGPS::GPS_INVALID_F_ANGLE) {
    appendFile(SD, "/Canarinho.txt", "  ");
    appendFile(SD, "/Canarinho.txt", String(latitude,6).c_str());
  }
  if (longitude != TinyGPS::GPS_INVALID_F_ANGLE) {
    appendFile(SD, "/Canarinho.txt", "  ");
    appendFile(SD, "/Canarinho.txt", String(longitude,6).c_str());
  }
  if ((altitudeGPS != TinyGPS::GPS_INVALID_ALTITUDE) && (altitudeGPS != 1000000)) {
    appendFile(SD, "/Canarinho.txt", "  ");
    appendFile(SD, "/Canarinho.txt", String(altitudeGPS).c_str());
    appendFile(SD, "/Canarinho.txt", "  ");
    appendFile(SD, "/Canarinho.txt", String(WOW).c_str());
  }
  if (velocidademps != TinyGPS::GPS_INVALID_F_SPEED){
    appendFile(SD, "/Canarinho.txt","  ");
    appendFile(SD, "/Canarinho.txt", String(velocidademps).c_str());
  }
  appendFile(SD, "/Canarinho.txt", "  ");
  appendFile(SD, "/Canarinho.txt", String(MagBow).c_str());
  appendFile(SD, "/Canarinho.txt", "  ");
  appendFile(SD, "/Canarinho.txt", String(valuePot_Prof).c_str());
  appendFile(SD, "/Canarinho.txt", "  ");
  appendFile(SD, "/Canarinho.txt", String(valuePot_Aileron).c_str());
  appendFile(SD, "/Canarinho.txt", "  ");
  appendFile(SD, "/Canarinho.txt", String(valuePot_Leme).c_str());
  appendFile(SD, "/Canarinho.txt", "  ");
  appendFile(SD, "/Canarinho.txt", String(HP).c_str());
  appendFile(SD, "/Canarinho.txt", " ");
  appendFile(SD, "/Canarinho.txt", String(AcZ/16384.0).c_str());
  appendFile(SD, "/Canarinho.txt", " ");
  appendFile(SD, "/Canarinho.txt", String(pitch).c_str());
  appendFile(SD, "/Canarinho.txt", " ");
  appendFile(SD, "/Canarinho.txt", String(roll).c_str());
  appendFile(SD, "/Canarinho.txt", "\r\n");
  
}
