#include <SSD1306.h>

#define pinINT   27 //Se não mudar para o pino 10 e continuar utilizando o 11, o core 0 entra em panico!!

//Variaveis rpm
volatile int conta_RPM = 0;
int copyconta_RPM = 0;
float RPM = 0;
float contaAtualMillis = 0;
float antigoMillis = 0;
float subMillis = 0;
float minutos = 0;

//Objeto display
SSD1306 screen(0x3c, 21, 22);

//Funcao interrupcao
void IRAM_ATTR ContaInterrupt() {
  conta_RPM = conta_RPM + 1; //incrementa o contador de interrupções
}


void setup() {
  Serial.begin(115200);
  
  pinMode(pinINT,INPUT_PULLUP);
  attachInterrupt(pinINT,ContaInterrupt, RISING);
  
  screen.init();
  screen.setFont(ArialMT_Plain_16);
  
  Serial.println("  RPM         copyconta_RPM");
  
}

void loop() {
  delayMicroseconds(1000000);
  
  copyconta_RPM = conta_RPM;
  contaAtualMillis = millis(); 
  subMillis = contaAtualMillis - antigoMillis;
  minutos = (subMillis/(60000));
  RPM = copyconta_RPM/(minutos);
  antigoMillis = contaAtualMillis;
  conta_RPM = 0;
  
  Serial.print(" ");
  Serial.print(RPM);
  Serial.print("                ");
  Serial.println(copyconta_RPM);
  //jorge
  screen.clear();
  screen.drawString(20,  0, "RPM: "+String(RPM));
  screen.display();
  
}
