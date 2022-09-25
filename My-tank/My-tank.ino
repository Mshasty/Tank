#include <Servo.h>
#include "pitches.h"

#define TEST_MODE // State for test mode

#define Trig 7 // Триггер дальномера
#define Echo 2 // Датчик эха дальномера
#define ledPin 13

// The PWM pins are: D03, D05, D06, D09, D10, and D11.
#define motorRgtFwd 5
#define motorRgtBwd 3
#define motorLftFwd 6
#define motorLftBwd 10
#define speaker 11

const int danger = 12; // критическое расстояние
const unsigned int rotate45 = 600; // время поворота еа 45 градусов

Servo myservo;

unsigned int leftdist;
unsigned int rightdist;
unsigned int impulseTime=0; 
unsigned int distance_sm=0; 
unsigned int distanceFwd;
unsigned int tempdist;
unsigned int angleleft;
unsigned int angleright;

void setup() {
  pinMode(Trig, OUTPUT); //инициируем как выход 
  pinMode(Echo, INPUT); //инициируем как вход 
  pinMode(ledPin, OUTPUT); 
  pinMode(motorRgtFwd, OUTPUT);
  pinMode(motorRgtBwd, OUTPUT);
  pinMode(motorLftFwd, OUTPUT);
  pinMode(motorLftBwd, OUTPUT);
  pinMode(speaker, OUTPUT);
  digitalWrite(speaker, HIGH);
  myservo.attach(8);
  tone(speaker, NOTE_G5, 250);
  delay(300);
  tone(speaker, NOTE_B5, 125);
  delay(150);
  tone(speaker, NOTE_D6, 125);
  delay(300);
  digitalWrite(speaker, HIGH);
  headrotate();
#ifdef TEST_MODE
  Serial.begin(9600);
#endif
}

void loop() {
  distanceFwd = ping();
  if (distanceFwd>danger) {
    digitalWrite(motorRgtFwd, LOW);
    digitalWrite(motorRgtBwd, HIGH);
    digitalWrite(motorLftFwd, LOW);
    digitalWrite(motorLftBwd, HIGH);
  } else { // Тормозим и крутим дальномером
#ifdef TEST_MODE
    Serial.println("Стоп");
#endif
    digitalWrite(motorRgtFwd, LOW);
    digitalWrite(motorRgtBwd, LOW);
    digitalWrite(motorLftFwd, LOW);
    digitalWrite(motorLftBwd, LOW);
    tone(speaker, NOTE_D6, 63);
    delay(85);
    tone(speaker, NOTE_D5, 125);
    delay(300);
    digitalWrite(speaker, HIGH);
    myservo.write(45); // вправо 45 град.
    delay(300);
    rightdist = ping();
    myservo.write(0); // вправо
    delay(300);
    tempdist = ping();
#ifdef TEST_MODE
    Serial.print("Дистанция справа 45 - ");
    Serial.print(rightdist);
    Serial.print(", 0  -  ");
    Serial.println(tempdist);
#endif
    if (rightdist > tempdist) {
      angleright = 1;
    } else {
      angleright = 2;
      rightdist = tempdist;
    }
    myservo.write(135); // влево 45 град.
    delay(500);
    leftdist= ping();
    myservo.write(180); // влево
    delay(300);
    tempdist = ping();
#ifdef TEST_MODE
    Serial.print("Дистанция слева 135 - ");
    Serial.print(leftdist);
    Serial.print(", 180 - ");
    Serial.println(tempdist);
#endif
    if (leftdist > tempdist) {
      angleleft = 1;
    } else {
      angleleft = 2;
      leftdist = tempdist;
    }
#ifdef TEST_MODE
    Serial.print("Впереди - ^ ");
    Serial.print(distanceFwd);
    Serial.print(", ");
    Serial.print("Слева - < "); 
    Serial.print(leftdist);
    Serial.print(", ");
    Serial.print("Справа - > ");
    Serial.println(rightdist);
#endif
    //myservo.write(90);
    delay(100);
    compareDistance();
  }
}

void compareDistance(){
  if (leftdist > rightdist) { // поворот влево
    if (leftdist > danger * 2) {
      myservo.write(90 + 35 * angleleft);
#ifdef TEST_MODE
      if (angleleft > 1) {
        Serial.print("Разворот влево на  ");
      } else {
        Serial.print("Поворот влево на   ");
      }
      Serial.print(angleleft * 45);
      Serial.println(" градусов");
#endif
      digitalWrite(motorRgtFwd, LOW);
      digitalWrite(motorRgtBwd, HIGH);
      digitalWrite(motorLftFwd, HIGH);
      digitalWrite(motorLftBwd, LOW);
      delay(rotate45*angleleft);
    } else moveback();
  } else { // поворот вправо
    if (rightdist > danger * 2) {
      myservo.write(90 - 35 * angleright);
#ifdef TEST_MODE
      if (angleright > 1) {
        Serial.print("Разворот вправо на ");
      } else {
        Serial.print("Поворот вправо на  ");
      }
      Serial.print(angleright * 45);
      Serial.println(" градусов");
#endif
      digitalWrite(motorRgtFwd, HIGH);
      digitalWrite(motorRgtBwd, LOW);
      digitalWrite(motorLftFwd, LOW);
      digitalWrite(motorLftBwd, HIGH);
      delay(rotate45*angleright);
    } else moveback();
  }
  myservo.write(90);
#ifdef TEST_MODE
  Serial.println("Вперёд");
#endif
  delay(200);
}

void moveback() {
  myservo.write(90);
#ifdef TEST_MODE
  Serial.println("Задний ход");
#endif
  digitalWrite(motorRgtFwd, HIGH);
  digitalWrite(motorRgtBwd, LOW);
  digitalWrite(motorLftFwd, HIGH);
  digitalWrite(motorLftBwd, LOW);
  tone(speaker, NOTE_C6, 125);
  delay(rotate45*1);
  tone(speaker, NOTE_C6, 125);
  delay(rotate45*1);
  tone(speaker, NOTE_C6, 125);
  delay(rotate45*1);
  digitalWrite(speaker, HIGH);
  myservo.write(10);
#ifdef TEST_MODE
  Serial.println("Разворот вправо на 180 градусов");
#endif
  digitalWrite(motorRgtFwd, HIGH);
  digitalWrite(motorRgtBwd, LOW);
  digitalWrite(motorLftFwd, LOW);
  digitalWrite(motorLftBwd, HIGH);
  delay(rotate45*4);
}

long ping() { // измеряем расстояние до препятствия
  digitalWrite(Trig, HIGH); 
  /* Подаем импульс на вход trig дальномера */
  delayMicroseconds(10); // равный 10 микросекундам 
  digitalWrite(Trig, LOW); // Отключаем 
  impulseTime=pulseIn(Echo, HIGH); // Замеряем длину импульса 
  distance_sm=impulseTime / 58; // Пересчитываем в сантиметры  
  if (distance_sm < danger) { // Если расстояние менее задуманного  {     
    digitalWrite(ledPin, HIGH); // Светодиод горит 
  } else {   
    digitalWrite(ledPin, LOW); // иначе не горит 
  }   
  delay(100); 
  return distance_sm;
}

void headrotate() {
  myservo.write(30);
  delay(250);
  for(byte i = 0; i < 24; i++) {
    myservo.write(30 + 5 * i);
    delay(20);
  }
  myservo.write(90);
  delay(200);
}
