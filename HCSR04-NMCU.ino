/*
 * Servo Auto Feeder 
 * 
 * 
 * author: Mas Sagita
 */

#define BLYNK_PRINT Serial

//Library Wifi dan Blynk
#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
#include <SimpleTimer.h>

SimpleTimer timer;

//Library LCD
#include <LiquidCrystal_I2C.h>
LiquidCrystal_I2C lcd(0x27, 16, 2);

//Library RTC
#include <virtuabotixRTC.h>
virtuabotixRTC myRTC(14, 0, 2); //CLK, DT, RST //D5, D3, D4

//Library Servo
#include <Servo.h>
Servo myservo;

//Token, ssid dan password
char auth[ ] = "your auth token";
char ssid[ ] = "your wifi ssid";
char pass[ ] = "your password ssid";

//Pin Virtual
#define pinHCSR     V0
#define pinServo    V1

#define rtcJam   V2
#define rtcMenit V3
#define rtcDetik V4

//Pin di NodeMCU
#define servopin    15    //servo pin
const int led = 16;       //led indikator pin
uint8_t triggerPin = 12;  //sensor jarak trigger pin
uint8_t echoPin    = 13;  //sensor jarak echo pin

//Variabel untuk sensor jarak
unsigned long timerStart = 0;
int TIMER_TRIGGER_HIGH = 10;
int TIMER_LOW_HIGH = 2;
float timeDuration, distance;

//Kumpulan variabel untuk sensor jarak
enum SensorStates {
  TRIG_LOW,
  TRIG_HIGH,
  ECHO_HIGH
};

SensorStates _sensorState = TRIG_LOW;

//Fungsi timer untuk sensor jarak
void startTimer() {
  timerStart = millis();
}

//Fungsi untuk sensor jarak
bool isTimerReady(int mSec) {
  return (millis() - timerStart) < mSec;
}

//Variabel untuk blynk
int buttonStateBlynk;
int switchStateBlynk;
int setJam1, setJam2, setJam3;

//Variebel perhitungan makanan
int tWadah = 25;
const int pWadah = 15;
const int lWadah = 15;

int volWadah = 0;
int volMakanan = 0;

//Variabel menyimpan nilai jarak
float duration, jarak;
int setjarak = 10;

//Variabel set RTC
int setDetik, setMenit, setJam;

//Pin Virtual Blynk untuk LCD
WidgetLCD lcdblynk(V5);
//Pin Virtual Blynk untuk LED indikator di Blynk
WidgetLED ledServo(V8), ledMode(V9), ledIndikator(V10); //led virtual blynk

//Fungsi mengirim data ke Blynk
void sendUptime() {
  //write di blynk level makanan
  Blynk.virtualWrite(pinHCSR, volMakanan);
  //write di blynk dari RTC
  Blynk.virtualWrite(rtcJam, myRTC.hours);
  Blynk.virtualWrite(rtcMenit, myRTC.minutes);
  Blynk.virtualWrite(rtcDetik, myRTC.seconds);
}

//Fungsi untuk menerima data dari Blynk
BLYNK_WRITE(V6) { buttonStateBlynk = param.asInt(); } //Button
BLYNK_WRITE(V7) { switchStateBlynk = param.asInt(); } //Swith Auto / Manual
BLYNK_WRITE(V11) { setJam1 = param.asInt(); } //set Jam 1
BLYNK_WRITE(V12) { setJam2 = param.asInt(); } //set Jam 2
BLYNK_WRITE(V13) { setJam3 = param.asInt(); } //set Jam 3

int pos; //menyimpan nilai posisi servo
int refresh; //untuk refresh lcd

bool kondisi; //menyimpan kondisi manual atau otomatis

String statusServo = " "; //menyimpan status servo

//Inisialisasi
void setup() {
  Serial.begin (115200); //Serial dengan baud rate 115200
  Serial.println("RTC SERVO");

  pinMode(led, OUTPUT); //led sebagai output
  pinMode(triggerPin, OUTPUT);  //pin trigger HCSR04 output
  pinMode(echoPin, INPUT);      //pin echo HCSR04 input

  myservo.attach(servopin); //pin servo

  lcd.init(); //inisialisasi LCD

  for (int i = 0; i < 10; i++) { //led berkedip 8 kali. tanda masuk program
    digitalWrite(led, HIGH);
    delay(80);
    digitalWrite(led, LOW);
    delay(80);
  }

  lcd.backlight(); //Menghidupkan backlight pada lcd

  WiFi.begin(ssid, pass); //ssid dan password

  //mengetahui koneksi ke wifi
  while (WiFi.status() != WL_CONNECTED) {
    lcd.setCursor(0, 0);
    lcd.print("con to network");
    //membuat .... pada lcd
    if (++refresh >= 0) {
      lcd.setCursor(refresh, 1);
      lcd.print(".");
      delay(100);
      if (refresh == 16) {
        refresh = -1;
        lcd.clear();
      }
    }
    digitalWrite(led, digitalRead(led) ^ 1); //blink led
  }

  //memulai blynk
  Blynk.begin(auth, ssid, pass, "blynk-cloud.com", 8080);
  
  //interval sendUptime 1 detik
  timer.setInterval(1000, sendUptime);

  // Untuk setting jam interval upload program 56 seconds
  // seconds, minutes, hours, day of the week, day of the month, month, year
  myRTC.setDS1302Time(9, 27, 9, 7, 3, 12, 2021);

  myRTC.updateTime(); //update waktu RTC
  myservo.write(150); //set posisi awal servo

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("RTC SERVO");
  lcd.setCursor(0, 1);
  lcd.print("Tugas Akhir 1");

  //Untuk komunikasi ke excel
  Serial.println("CLEARSHEET");
  Serial.print("LABEL,ADate,ATime,RHours,RMinute,RSeconds,Servo,");
  Serial.println("Food(cm3),Food(%),SenJar");

  delay(1000);

  lcd.clear();
}

void loop() {
  hitungJarak(); //memanggil fungsi menghitung jarak
  myRTC.updateTime(); //memanggil fungsi update waktu pada rtc

  testOpenFeeder(); //fungsi untuk membuka servo

  Blynk.run(); //run blynk
  timer.run(); //run timer blynk

  //merefresh lcd
  if (++refresh > 5) {
    lcd.clear();
    refresh = 0;
  }

  //konversi jarak ke persentase makanan
  volMakanan = map(jarak, 0, 28, 100, 0);
  //konversi jarak ke volume wadah
  volWadah = map(jarak, 0, 28, 5400, 0);

  Serial.print(jarak); Serial.print("\t");
  Serial.print(volMakanan); Serial.print("\t");
  Serial.print(volWadah); Serial.println("\t");

  //tampilan pada LCD
  lcd.setCursor(0, 0);
  lcd.print("F:"); lcd.print(volMakanan); lcd.print("%");

  lcd.setCursor(6, 0);
  lcd.print("V:"); lcd.print(volWadah); lcd.print("cm3");

  lcd.setCursor(0, 1);
  lcd.print(myRTC.hours); lcd.print(":");
  lcd.print(myRTC.minutes); lcd.print(":");
  lcd.print(myRTC.seconds);
  lcd.print(" "); lcd.print(setJam1);
  lcd.print(" "); lcd.print(setJam2);
  lcd.print(" "); lcd.print(setJam3);

  //blink led untuk mengetahui apakah program looping
  digitalWrite(led, digitalRead(led) ^ 1);
}

//fungsi menghitung sensor jarak
void hitungJarak() {
  switch (_sensorState) {
    /* Start with LOW pulse to ensure a clean HIGH pulse*/
    case TRIG_LOW: {
        digitalWrite(triggerPin, LOW);
        startTimer();
        if (isTimerReady(TIMER_LOW_HIGH)) {
          _sensorState = TRIG_HIGH;
        }
      } break;

    /*Triggered a HIGH pulse of 10 microseconds*/
    case TRIG_HIGH: {
        digitalWrite(triggerPin, HIGH);
        startTimer();
        if (isTimerReady(TIMER_TRIGGER_HIGH)) {
          _sensorState = ECHO_HIGH;
        }
      } break;

    /*Measures the time that ping took to return to the receiver.*/
    case ECHO_HIGH: {
        digitalWrite(triggerPin, LOW);
        timeDuration = pulseIn(echoPin, HIGH);
        /*
           distance = time * speed of sound
           speed of sound is 340 m/s => 0.034 cm/us
        */
        jarak = timeDuration * 0.034 / 2;
        _sensorState = TRIG_LOW;
      } break;
  }
}

int i, tanda;

//fungsi buka servo berdasarkan settingan RTC
void testOpenFeeder() {
  tanda = 0;
  
  //jika switch auto
  if (switchStateBlynk == 0) {
    ledMode.on(); //hidupkan led mode di app blynk
    
    //Jika kondisi waktu terpenuhi
    if (myRTC.hours == setJam1 || myRTC.hours == setJam2 || myRTC.hours == setJam3 && myRTC.minutes == 0) {
      
      //membuka servo jika kondisi detik terpenuhi
      if (myRTC.seconds >= 20 && myRTC.seconds <= 21) {
        statusServo = "BUKA OTO"; //status servo buka
        //Mengirim data ke excel
        Serial.println((String) "DATA,DATE,TIME," + myRTC.hours + "," + myRTC.minutes
                       + "," + myRTC.seconds + "," + statusServo + "," + volWadah
                       + "," + volMakanan + "," + jarak);
        lcd.clear();
        lcd.print("SERVO OPEN");
        ledServo.on(); //hidupkan led servo di blynk
        //membuka servo menggunakan perulangan for
        for (pos = 150; pos >= 80; pos -= 1) {
          myservo.write(pos);
          delay(25);
        }
      }
      //menutup servo jika kondisi detik terpenuhi
      if (myRTC.seconds >= 22 && myRTC.seconds <= 23) {
        statusServo = "TUTUP OTO"; //status servo tertutup
        //Mengirim data ke Excel
        Serial.println((String) "DATA,DATE,TIME," + myRTC.hours + "," + myRTC.minutes
                       + "," + myRTC.seconds + "," + statusServo + "," + volWadah
                       + "," + volMakanan + "," + jarak);
        lcd.clear();
        lcd.print("SERVO CLOSE");
        ledServo.off(); // Mematikan led indikator servo
        //menutup servo menggunakan perulangan for
        for (pos = 80; pos <= 150; pos += 1) {
          myservo.write(pos);
          delay(25);
        }
        tanda == 1;
      }
      //Mengirim ulang volume makanan
      if (tanda == 1) {
        Serial.println((String) "DATA,DATE,TIME, , , , ," + volWadah
                       + "," + volMakanan + "," + jarak);
      }
    }
  }
  
  //Switch state pada bkynk
  if (switchStateBlynk == 1) {
    ledMode.off(); //mematikan led indikator Mode
    openServoManual(); //memanggil fungsi manual untuk membuka servo
  }
}

//fungsi buka servo manual
void openServoManual() {
  //jika switch pada blynk 0 buka servo
  if (buttonStateBlynk == 0) {
    statusServo = "BUKA MAN";
    ledServo.on();
    myservo.write(45); // posisi buka servo
  }
  //jika switch pada blynk 1 buka servo
  if (buttonStateBlynk == 1) {
    statusServo = "TUTUP MAN";
    ledServo.off();
    myservo.write(160); // posisi tutup servo
  }
}

