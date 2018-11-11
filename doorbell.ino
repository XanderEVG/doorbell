#include "HAL.h"
#include "EERTOS.h"
#include <avr/pgmspace.h>

#include <SoftwareSerialWithoutPCINT2.h>   //Если используем  PCINT2(прерывания кнопок) то в либе SoftwareSerial нужно закоментить  прерывания PCINT2. при этом SoftwareSerial на этих пинах работать не будет
#include "DFRobotDFPlayerMini.h"
#include <EEPROM.h>


#define BLUETOOTH_BY_SOFT   //BLUETOOTH BY SoftwareSerial
//#define CHECK_TIME_SHOW_TIME  // В функции CHECK_TIME выводить в Serial текущее время
//#define EEPROM_NOT_UPDATE // не обновлять eeprom (на время отладки)
#define BT_DEBUG_OUTPUT //Выводить всякое Г


/* // TODO // 
  read_rtc
  write_rtc
  sync_pass or BLUETOOTH pin?
*/


//============================================================================
// Дефайны
//============================================================================

#define TIMER1_OCR_1MS 250   //Настройка таймера на 1мс
#define MS_IN_SEC_COUNT 997  // Поправочный коэффициент для 1 сек
//варианты:  250 1000 - за час отстает на 8 сек
//варианты:  249 1000 - за час спешат  на 4 сек
//варианты:  250 997 - за час отстает на ?


#define SYNC_PERIOD 50              // преиод вызова функции check_sync()
#define READ_MAX_VOLUME_DELAY 2000  //Частота опроса АЦП(максимальный уровень громкости)
#define RTC_READ_PERIOD 60000       // период вызова read_rtc(), мс
#define RTC_READ_DUMMY 60           // Запрашивать время 1 раз за RTC_READ_DUMMY вызовов read_rtc. зачем? хз

#define PLAY_AFTER_DELAY 10 //Задержка воспроизведения. Задержка нужна в случае если динамик отключатся релюхой(шумит)


/// STATEs ///
#define NO_ACTION 0
#define PLAYING_STOP 0
#define PLAYING 1

#define DFPLAER_STOP 512
#define DFPLAER_PLAY 513
#define DFPLAER_PAUSE 514

#define VOLUME_LIMITED 1     //Если звук ограничен через АЦП
#define VOLUME_NOT_LIMITED 0

#define BTN_NO_PRESS 0
#define BTN_PRESS 1

#define SILENCE_MODE_OFF 0   //Ночью включаем тихий режим
#define SILENCE_MODE_ON 1



/// PINS ///
//NANO and PRO MINI - порты одни и те же
#define TX_PIN 1
#define RX_PIN 0

#define STX_PIN 8  //Пин TX SoftwareSerial
#define SRX_PIN 9  //Пин RX SoftwareSerial

#define BT_TX_PIN 10
#define BT_RX_PIN 11

#define BTN_PIN 7
#define LED_PIN LED_BUILTIN //13

#define SDA_PIN 4 //A4
#define SCL_PIN 5 //A5

#define SOUND_ON_PIN 6
#define SOUND_LVL_PIN 3 //A3 

#define BTN_PORT 7
#define HARD_BTN_PORT PIND
#define HARD_BTN_PIN 7

#define SOUND_ON_PORT 6
#define HARD_SOUND_ON_PORT PORTD
#define HARD_SOUND_ON_PIN 6

#define MAX_VOLUME_PIN 1


/// EQVALIZER ///
#define EQ_LVL_NORMAL 0
#define EQ_LVL_POP 1
#define EQ_LVL_ROCK 2
#define EQ_LVL_JAZZ 3
#define EQ_LVL_CLASSIC 4
#define EQ_LVL_BASS 5



/// EEPROM адреса///
#define EEPROM_ADDR_SILENCE_OFF_TIME_H 0
#define EEPROM_ADDR_SILENCE_OFF_TIME_M 1
#define EEPROM_ADDR_SILENCE_ON_TIME_H 2
#define EEPROM_ADDR_SILENCE_ON_TIME_M 3
#define EEPROM_ADDR_SOUND_LVL_SILENCE_MODE 4
#define EEPROM_ADDR_SOUND_LVL_NORMAL_MODE 5
#define EEPROM_ADDR_EQ_LVL 6
#define EEPROM_ADDR_MELODY_ID 7 // 2 байта
#define EEPROM_ADDR_MAX_PLAYING_DURATION 11 // 2 байта


/// SYNC  адреса переменных в пакете///
#define SYNC_INDEX_METHOD 5
#define SYNC_INDEX_TIME_H 6
#define SYNC_INDEX_TIME_M 8
#define SYNC_INDEX_TIME_S 10
#define SYNC_INDEX_SILENCE_ON_H 12
#define SYNC_INDEX_SILENCE_ON_M 14
#define SYNC_INDEX_SILENCE_OFF_H 16
#define SYNC_INDEX_SILENCE_OFF_M 18
#define SYNC_INDEX_SILENCE_LVL 20
#define SYNC_INDEX_NORMAL_LVL 22
#define SYNC_INDEX_EQ_LVL 24
#define SYNC_INDEX_MANUAL_SILENCE 25
#define SYNC_INDEX_MELODY_COUNT 26
#define SYNC_INDEX_MELODY_COUNT_H 26
#define SYNC_INDEX_MELODY_COUNT_L 26 
#define SYNC_INDEX_MELODY_ID 29
#define SYNC_INDEX_MELODY_ID_H 29
#define SYNC_INDEX_MELODY_ID_L 29
#define SYNC_INDEX_MELODY_DURATION 32
#define SYNC_INDEX_MELODY_DURATION_H 32
#define SYNC_INDEX_MELODY_DURATION_L 32


//============================================================================
// Глобальные переменные
//============================================================================

#if defined( BLUETOOTH_BY_SOFT )
  SoftwareSerial SerialBT(BT_RX_PIN, BT_TX_PIN); // RX, TX
  #define SERIAL_BAUDRATE 115200
  #define BT_SERIAL_BAUDRATE 9600
#else
  #define SerialBT Serial
  #define SERIAL_BAUDRATE 9600
#endif

SoftwareSerial Serial2(SRX_PIN, STX_PIN); // RX, TX
DFRobotDFPlayerMini myDFPlayer;

volatile word _time_ms;
volatile byte _time_s;
volatile byte _time_m;
volatile byte _time_h;
volatile word _days;
volatile byte _btn_state;

byte silence_off_time_h;
byte silence_off_time_m;
byte silence_on_time_h;
byte silence_on_time_m;

byte sound_lvl_silence_mode;
byte sound_lvl_normal_mode;
byte old_volume_lvl;

int melody_id;  //if 0 -> play random
int count_melody;
int max_playing_duration;

byte eq_lvl;
byte melody_state;

byte silence_mode;
byte manual_silence_mode; 
byte max_sound_lvl;

byte volume_limited;

byte rtc_read_dummy_counter;

char error_message[32] = "                               "; // длинна текста - 31 символ, последний байт - 0

//sync_pass="qpxdjYGw3";

//============================================================================
// Прерывания
//============================================================================
ISR(RTOS_TIMER_ISR)
{
  TimerService(); //Магия таймерной службы
}

//Таймер времени
ISR(TIMER1_COMPA_vect)
{
  _time_ms++;
  if(_time_ms==MS_IN_SEC_COUNT)
  {
    _time_ms=0;
    _time_s++;
    
    if(_time_s>59)
    {
      _time_s=0;
      _time_m++;
      if(_time_m>59)
      {
        _time_m=0;
        _time_h++;
        if(_time_h>23)
        {
          _time_h=0;
          _days++;
        }
      }
    }   
  }
  
}

//Прерывание опроса кнопки
ISR (PCINT2_vect) 
{
  byte buttonState = HARD_BTN_PORT & 1<<HARD_BTN_PIN;
  
  if(!buttonState)
  {
      if(_btn_state==BTN_NO_PRESS)
      {
        _btn_state=BTN_PRESS;
        SetTask(Btn_pressed);
      }
  }
}






//============================================================================
// Прототипы задач
//============================================================================

void BlinkON (void);
void BlinkOFF (void);
void Btn_pressed (void);
void Btn_release (void);

void Play (void);
void Stop (void);

void init_vars(void);
void write_eeprom(void);        //при первом включении звонка нужно забить еепром первичными данными. после этого - комментируем  вызов этой функции 
void sync_eeprom_update(void);  //обновляем 
void read_eeprom(void);

void first_setting_mp3(void);

void read_max_volume(void);
void check_time(void); //Проверка времени, переключение режимов тихий/нормальный
void set_silens_mode(byte mode);

void read_rtc(void);
void write_rtc(void);

void sync_send_data(void);  //Отправляем текущие параметры в bluetooth
void sync_send_ok(void);    //на все запросы отвечаем ok
void sync_send_error(void); // кроме случаев, когда мы нихрена не поняли
void sync_get_data(byte *data);  //с блютуза мы получаем 2 вида данных - полные настройки 
void sync_get_time(byte *data);  // и текущее время(для синхронизации)
void sync_play(void);            // а еще получаем команду воспроизвести текущую мелодию
void clear_serial(void);         // очищаем буфер

void after_sync(void);       //после приема пакета нужно сохранить настройки
void after_time_sync(void);  // в т ч время
void setting_mp3(void);      //и записать настройки в mp3 модуль 

void check_sync(void);       //проверяем, пришел ли пакет
void set_time_error_msg(void);  //в случае ошибок нужно сформировать текст ошибки и отправить с помощью sync_send_error
void set_data_error_msg(void);
void dymmy_send();  //для отладки

//============================================================================
// Область задач
//============================================================================

void BlinkON (void)
{
  digitalWrite(LED_BUILTIN, HIGH);
  SetTimerTask(BlinkOFF,500);
}
void BlinkOFF (void)
{
  digitalWrite(LED_BUILTIN, LOW);
  SetTimerTask(BlinkON,500);
}



void init_vars()
{
    _time_ms=0;
    _time_s=0;
    _time_m=0;
    _time_h=0;
    _days=0;
    
    _btn_state=BTN_NO_PRESS;

    silence_on_time_h=22;
    silence_on_time_m=00;
    silence_off_time_h=7;
    silence_off_time_m=00;

    
    sound_lvl_silence_mode=5;
    sound_lvl_normal_mode=15;
    old_volume_lvl=15;
    
    melody_id=12;//0;  //if 0 -> play random
    count_melody=1;
    max_playing_duration=6000;
    
    eq_lvl=EQ_LVL_NORMAL;
    melody_state=PLAYING_STOP;
    
    silence_mode=SILENCE_MODE_OFF;
    manual_silence_mode=SILENCE_MODE_OFF; 
    max_sound_lvl=30;
    volume_limited=VOLUME_NOT_LIMITED;
    rtc_read_dummy_counter=0;
}

void write_eeprom()
{
  EEPROM.write(EEPROM_ADDR_SILENCE_OFF_TIME_H, silence_off_time_h);
  EEPROM.write(EEPROM_ADDR_SILENCE_OFF_TIME_M, silence_off_time_h);
  EEPROM.write(EEPROM_ADDR_SILENCE_ON_TIME_H, silence_on_time_h);
  EEPROM.write(EEPROM_ADDR_SILENCE_ON_TIME_M, silence_on_time_m);
  EEPROM.write(EEPROM_ADDR_SOUND_LVL_SILENCE_MODE, sound_lvl_silence_mode);
  EEPROM.write(EEPROM_ADDR_SOUND_LVL_NORMAL_MODE, sound_lvl_normal_mode);
  EEPROM.put(EEPROM_ADDR_MELODY_ID, melody_id);  //многобайтовые переменные сохраняем с помощью put
  EEPROM.write(EEPROM_ADDR_EQ_LVL, eq_lvl);
  EEPROM.put(EEPROM_ADDR_MAX_PLAYING_DURATION, max_playing_duration); 
}

void sync_eeprom_update()
{
  Serial.println("EEPROM UPDATE");
  EEPROM.update(EEPROM_ADDR_SILENCE_OFF_TIME_H, silence_off_time_h);
  EEPROM.update(EEPROM_ADDR_SILENCE_OFF_TIME_M, silence_off_time_h);
  EEPROM.update(EEPROM_ADDR_SILENCE_ON_TIME_H, silence_on_time_h);
  EEPROM.update(EEPROM_ADDR_SILENCE_ON_TIME_M, silence_on_time_m);
  EEPROM.update(EEPROM_ADDR_SOUND_LVL_SILENCE_MODE, sound_lvl_silence_mode);
  EEPROM.update(EEPROM_ADDR_SOUND_LVL_NORMAL_MODE, sound_lvl_normal_mode);
  EEPROM.put(EEPROM_ADDR_MELODY_ID, melody_id); // put использует метод update(т е не перезаписывает значение если оно не меняется) 
  EEPROM.update(EEPROM_ADDR_EQ_LVL, eq_lvl);
  EEPROM.put(EEPROM_ADDR_MAX_PLAYING_DURATION, max_playing_duration); 
  
}

void read_eeprom()
{
  silence_off_time_h = EEPROM.read(EEPROM_ADDR_SILENCE_OFF_TIME_H);
  silence_off_time_m = EEPROM.read(EEPROM_ADDR_SILENCE_OFF_TIME_M);
  silence_on_time_h = EEPROM.read(EEPROM_ADDR_SILENCE_ON_TIME_H);
  silence_on_time_m = EEPROM.read(EEPROM_ADDR_SILENCE_ON_TIME_M);
  sound_lvl_silence_mode = EEPROM.read(EEPROM_ADDR_SOUND_LVL_SILENCE_MODE);
  sound_lvl_normal_mode = EEPROM.read(EEPROM_ADDR_SOUND_LVL_NORMAL_MODE);
  EEPROM.get(EEPROM_ADDR_MELODY_ID, melody_id);  //многобайтовые переменные
  eq_lvl = EEPROM.read(EEPROM_ADDR_EQ_LVL);
  EEPROM.get(EEPROM_ADDR_MAX_PLAYING_DURATION, max_playing_duration);
}

void first_setting_mp3()
{
  #if defined( BLUETOOTH_BY_SOFT )
    Serial2.listen();
  #endif
  byte sound_lvl = sound_lvl_normal_mode;
  if(sound_lvl>max_sound_lvl)
  {
    sound_lvl=max_sound_lvl;
  }
  old_volume_lvl=sound_lvl;
  myDFPlayer.volume(sound_lvl); 
  myDFPlayer.EQ(eq_lvl);

  count_melody=myDFPlayer.readFileCounts();
  #if defined( BLUETOOTH_BY_SOFT )
    SerialBT.listen(); //listen - для переключения прослушки порта, если используется два софтверных сериала
  #endif
}

void setting_mp3()
{
  
  #if defined( BLUETOOTH_BY_SOFT )
    Serial.println("MP3 SETTING");
  #endif
  
  byte sound_lvl = sound_lvl_normal_mode;
  if(silence_mode==SILENCE_MODE_ON)
  {
    sound_lvl=sound_lvl_silence_mode;
  }

  if(sound_lvl>max_sound_lvl)
  {
    sound_lvl = max_sound_lvl;
  }
  
  old_volume_lvl=sound_lvl;

  #if defined( BLUETOOTH_BY_SOFT )
    Serial2.listen();
  #endif
  
  myDFPlayer.EQ(eq_lvl);
  myDFPlayer.volume(sound_lvl);

  #if defined( BLUETOOTH_BY_SOFT )
    SerialBT.listen();
  #endif
}


void Btn_pressed() 
{
      Play();
      SetTimerTask(Stop, max_playing_duration);
      #if defined( BLUETOOTH_BY_SOFT )
        Serial.println("BTN PRESS"); 
      #endif
      SetTimerTask(Btn_release,300); 
            
}

void Btn_release() 
{        
      cli(); //нафига, операция же атомарная?!
      _btn_state=BTN_NO_PRESS;
      sei();       
}

void Play ()
{
    digitalWrite(SOUND_ON_PIN, HIGH);
    delay(PLAY_AFTER_DELAY);

    #if defined( BLUETOOTH_BY_SOFT )
      Serial.println("PLAY");
    #endif
    
    #if defined( BLUETOOTH_BY_SOFT )
      Serial2.listen();
    #endif
    
    melody_state=PLAYING;
    if(myDFPlayer.readState()!=DFPLAER_PLAY)
    {
      if(melody_id==0)
      {
         int rand_id = random(count_melody-1) + 1;
         myDFPlayer.play(rand_id);
      }
      else
      {
        myDFPlayer.play(melody_id); 
      } 
    } 
    
    #if defined( BLUETOOTH_BY_SOFT )
      SerialBT.listen();
    #endif 
}

void Stop()
{
  #if defined( BLUETOOTH_BY_SOFT )
      Serial.println("STOP");
  #endif
    
  #if defined( BLUETOOTH_BY_SOFT )
      Serial2.listen();
  #endif

  int dfp_state = myDFPlayer.readState();    // WTF? why returning -1?
  if(dfp_state==DFPLAER_PLAY || dfp_state==-1) 
  {
    myDFPlayer.stop();
    melody_state=PLAYING_STOP;
  }
  
  
  
  digitalWrite(SOUND_ON_PIN, LOW);
  #if defined( BLUETOOTH_BY_SOFT )
      SerialBT.listen();
  #endif 
}


void read_max_volume()
{
  int val;
  val = analogRead(MAX_VOLUME_PIN);
  val = map(val, 0, 1023, 0, 30);
  val = constrain(val, 0, 30);
  max_sound_lvl=val;
  SetTimerTask(read_max_volume, READ_MAX_VOLUME_DELAY); 
  if(melody_state!=PLAYING)
  {

    byte sound_lvl = sound_lvl_normal_mode;
    if(silence_mode==SILENCE_MODE_ON)
    {
      sound_lvl=sound_lvl_silence_mode;
    }
        
    if(sound_lvl>max_sound_lvl)
    {
      if(old_volume_lvl!=max_sound_lvl)
      {
        old_volume_lvl=max_sound_lvl;
        #if defined( BLUETOOTH_BY_SOFT )
            Serial2.listen();
        #endif
        myDFPlayer.volume(max_sound_lvl);
        #if defined( BLUETOOTH_BY_SOFT )
          SerialBT.listen();
        #endif 
        volume_limited=VOLUME_LIMITED;
      }
    }
    else
    {
      if(volume_limited==VOLUME_LIMITED)
      {
        if(old_volume_lvl!=sound_lvl)
        {
          volume_limited=VOLUME_NOT_LIMITED;
          old_volume_lvl=sound_lvl;
          #if defined( BLUETOOTH_BY_SOFT )
            Serial2.listen();
          #endif
          myDFPlayer.volume(sound_lvl);
          #if defined( BLUETOOTH_BY_SOFT )
            SerialBT.listen();
          #endif 
        }
      }
    }
  }
}

void check_time()
{
  cli();
  byte time_h=_time_h;
  byte time_m=_time_m;
  byte time_s=_time_s;
  sei();
  
  #if defined( CHECK_TIME_SHOW_TIME ) && defined( BLUETOOTH_BY_SOFT )
    Serial.print(time_h, DEC);
    Serial.print(":");
    Serial.print(time_m, DEC);
    Serial.print(":");
    Serial.println(time_s, DEC);
  #endif
  
  byte revers_check=0; 
  //Если время включения режима тишины меньше времени выключения - сравниваем время наоборот(см далее)
  if(silence_on_time_h<silence_off_time_h)
  {
    revers_check=1;
  }
  else if(silence_on_time_h<silence_off_time_h)
  {
    if(silence_on_time_m<silence_off_time_m)
    {
      revers_check=1;
    }
  }

  int cur_mins = time_h*60 + time_m;
  int on_mins = silence_on_time_h*60 + silence_on_time_m;
  int off_mins = silence_off_time_h*60 + silence_off_time_m;


  //что бы понять этот говнокод нужно нарисовать диаграмму срабатывания
  if(revers_check==0)
  {
    //Нормальное сравнение
    if(cur_mins>off_mins && cur_mins<on_mins)
    {
      if(silence_mode==SILENCE_MODE_ON)
      {
        set_silens_mode(SILENCE_MODE_OFF);  
      }
    }
    else
    {
      if(silence_mode==SILENCE_MODE_OFF)
      {
        set_silens_mode(SILENCE_MODE_ON);  
      }
    }
    
  }
  else
  {
    //Обратное сравнение
    if(cur_mins>on_mins && cur_mins<off_mins)
    {
      if(silence_mode==SILENCE_MODE_OFF)
      {
        set_silens_mode(SILENCE_MODE_ON);  
      }
    }
    else
    {
      if(silence_mode==SILENCE_MODE_ON)
      {
        set_silens_mode(SILENCE_MODE_OFF);  
      }
    }
    
  }
  
  SetTimerTask(check_time, 1000); 
}

void set_silens_mode(byte mode)
{
  #if defined( BLUETOOTH_BY_SOFT )
      Serial.println("CHANGE SILENCE MODE");
  #endif
  
  silence_mode=mode;
    
  byte sound_lvl = sound_lvl_normal_mode;
  if(silence_mode==SILENCE_MODE_ON)
  {
    sound_lvl=sound_lvl_silence_mode;
  }
      
  if(sound_lvl>max_sound_lvl)
  {
    sound_lvl = max_sound_lvl;
  }

  
  #if defined( BLUETOOTH_BY_SOFT )
      Serial2.listen();
  #endif
    myDFPlayer.volume(sound_lvl);
  #if defined( BLUETOOTH_BY_SOFT )
    SerialBT.listen();
  #endif
  
}


void read_rtc()
{
  rtc_read_dummy_counter++;
  if(rtc_read_dummy_counter==RTC_READ_DUMMY)
  {
    //Опрашиваем реже чем позволяет таймерная служба, поэтому применяем лишний счетчик
    rtc_read_dummy_counter=0;

    //TODO READ RTC

    #if defined( BLUETOOTH_BY_SOFT )
        Serial.println("READ RTC");
    #endif
    
  }

  SetTimerTask(read_rtc, RTC_READ_PERIOD);
}

void write_rtc()
{
  #if defined( BLUETOOTH_BY_SOFT )
        Serial.println("WRITE RTC");
  #endif
}


void sync_send_data()
{
  #if defined( BLUETOOTH_BY_SOFT )
    Serial.println("SEND DATA");
  #endif

  // Пакет состоит из 4 частей: 
  // "SYNC_" - начало пакета, 4 байта
  // Method - тип сообщения, 1 байт. 
  // DATA - данные в виде строки, около 31 байт
  // "END\n" - конец пакета
  
  //SerialBT.println("SYNC_D0000000000000000000000000000000END");
  //return; 
  
  SerialBT.print("SYNC_D");
  cli();
  byte time_h=_time_h;
  byte time_m=_time_m;
  byte time_s=_time_s;
  sei();

  // Все передаем в ASCII, т к при передаче нуля затыкалась прога на андроиде, считала это концом строки
  // что бы длинна пакета была постоянной - заполняем нулями
  // TODO говнокод, переписать 


  if(time_h<10) SerialBT.print("0"); 
  SerialBT.print(time_h);
  
  if(time_m<10) SerialBT.print("0");  
  SerialBT.print(time_m); 
  
  if(time_s<10) SerialBT.print("0"); 
  SerialBT.print(time_s); 

  if(silence_on_time_h<10) SerialBT.print("0"); 
  SerialBT.print(silence_on_time_h);

  if(silence_on_time_m<10) SerialBT.print("0"); 
  SerialBT.print(silence_on_time_m); 

  if(silence_off_time_h<10) SerialBT.print("0"); 
  SerialBT.print(silence_off_time_h); 

  if(silence_off_time_m<10) SerialBT.print("0"); 
  SerialBT.print(silence_off_time_m); 

  if(sound_lvl_silence_mode<10) SerialBT.print("0"); 
  SerialBT.print(sound_lvl_silence_mode); 
  
  if(sound_lvl_normal_mode<10) SerialBT.print("0"); 
  SerialBT.print(sound_lvl_normal_mode); 


  SerialBT.print(eq_lvl);   
  SerialBT.print(manual_silence_mode); 


  if(count_melody<10) SerialBT.print("0"); 
  if(count_melody<100) SerialBT.print("0"); 
  SerialBT.print(count_melody); 

  if(melody_id<10) SerialBT.print("0"); 
  if(melody_id<100) SerialBT.print("0"); 
  SerialBT.print(melody_id); 

  if(max_playing_duration<10) SerialBT.print("0"); 
  if(max_playing_duration<100) SerialBT.print("0"); 
  if(max_playing_duration<1000) SerialBT.print("0"); 
  if(max_playing_duration<10000) SerialBT.print("0"); 
  SerialBT.print(max_playing_duration); 


  SerialBT.print("END\n");
}

void sync_send_ok()
{
  #if defined( BLUETOOTH_BY_SOFT )
    Serial.println("SEND OK");
  #endif
  
  SerialBT.println("SYNC_OK000000000000000000000000000000END\n");
}

void sync_send_error()
{
  #if defined( BLUETOOTH_BY_SOFT )
    Serial.println("SEND ERROR");
  #endif
  
  SerialBT.print("SYNC_E");
  for(int i=0; i<31; i++)
  {
    SerialBT.print(error_message[i]);
  }
  SerialBT.println("END\n");
}

void sync_get_data(byte *data)
{
  #if defined( BLUETOOTH_BY_SOFT )
    Serial.println("GET DATA");
  #endif

  bool success=true;

  //Читаем и переводим из ASCII. встроенная функция тупая и жирная
  byte time_h = (data[SYNC_INDEX_TIME_H] - '0') * 10 + data[SYNC_INDEX_TIME_H+1] - '0';
  byte time_m = (data[SYNC_INDEX_TIME_M] - '0') * 10 + data[SYNC_INDEX_TIME_M+1] - '0';
  byte time_s = (data[SYNC_INDEX_TIME_S] - '0') * 10 + data[SYNC_INDEX_TIME_S+1] - '0';
  
  if(time_h>23 || time_m>59 || time_s>59 )
  {
    set_time_error_msg();
    SetTask(sync_send_error);
    return;  
  }
  
  cli();
  _time_ms=0;
  _time_h=time_h;
  _time_m=time_m;
  _time_s=time_s;
  sei();

  silence_on_time_h = (data[SYNC_INDEX_SILENCE_ON_H] - '0') * 10 + data[SYNC_INDEX_SILENCE_ON_H+1] - '0';
  silence_on_time_m = (data[SYNC_INDEX_SILENCE_ON_H] - '0') * 10 + data[SYNC_INDEX_SILENCE_ON_H+1] - '0';

  silence_off_time_h = (data[SYNC_INDEX_SILENCE_OFF_H] - '0') * 10 + data[SYNC_INDEX_SILENCE_OFF_H+1] - '0';
  silence_off_time_m = (data[SYNC_INDEX_SILENCE_OFF_M] - '0') * 10 + data[SYNC_INDEX_SILENCE_OFF_M+1] - '0';         


  sound_lvl_silence_mode = (data[SYNC_INDEX_SILENCE_LVL] - '0') * 10 + data[SYNC_INDEX_SILENCE_LVL+1] - '0';
  sound_lvl_normal_mode = (data[SYNC_INDEX_NORMAL_LVL] - '0') * 10 + data[SYNC_INDEX_NORMAL_LVL+1] - '0';
  eq_lvl = data[SYNC_INDEX_EQ_LVL] - '0';
  manual_silence_mode = data[SYNC_INDEX_MANUAL_SILENCE] - '0';


  melody_id = (data[SYNC_INDEX_MELODY_ID] - '0') * 100 + (data[SYNC_INDEX_MELODY_ID+1] - '0') * 10 + data[SYNC_INDEX_MELODY_ID+2] - '0';
  max_playing_duration =  (data[SYNC_INDEX_MELODY_DURATION  ] - '0') * 10000 + 
                          (data[SYNC_INDEX_MELODY_DURATION+1] - '0') * 1000 +
                          (data[SYNC_INDEX_MELODY_DURATION+2] - '0') * 100 +  
                          (data[SYNC_INDEX_MELODY_DURATION+3] - '0') * 10 + 
                           data[SYNC_INDEX_MELODY_DURATION+4] - '0';

  
  if(silence_on_time_h>23) { silence_on_time_h=22;  success=false; }
  if(silence_on_time_m>59) { silence_on_time_m=00;  success=false; }

  if(silence_off_time_h>23) { silence_off_time_h=07;  success=false; }
  if(silence_off_time_m>59) { silence_off_time_m=00;  success=false; }

  if(sound_lvl_silence_mode>30) { sound_lvl_silence_mode=5;  success=false; }
  if(sound_lvl_normal_mode>30) { sound_lvl_normal_mode=15;  success=false; }
  if(eq_lvl>EQ_LVL_BASS) { eq_lvl=EQ_LVL_NORMAL;  success=false; }

  if(melody_id>999) { melody_id=1;  success=false; }
  if(max_playing_duration>60000) { max_playing_duration=3000;  success=false; }
  
  #if defined( BT_DEBUG_OUTPUT )
    Serial.print("time: "); Serial.print(time_h); Serial.print(":"); Serial.print(time_m); Serial.print(":"); Serial.println(time_s); 
    Serial.print("silence_on: "); Serial.print(silence_on_time_h); Serial.print(":"); Serial.println(silence_on_time_m); 
    Serial.print("silence_off: "); Serial.print(silence_off_time_h); Serial.print(":"); Serial.println(silence_off_time_m);
    Serial.print("sound_lvl: "); Serial.print(sound_lvl_normal_mode);Serial.print("(");Serial.print(sound_lvl_silence_mode);Serial.println(")"); 
    Serial.print("eq_lvl: "); Serial.println(eq_lvl); 
    Serial.print("manual_silence_mode: "); Serial.println(manual_silence_mode); 
    Serial.print("melody_id: "); Serial.println(melody_id); 
    Serial.print("max_playing_duration: "); Serial.println(max_playing_duration); 
    Serial.println();
  #endif
  
  
  if(success==false)
  {
    set_data_error_msg();
    SetTask(sync_send_error);
    return;  
  }
  else
  {
    SetTask(after_sync);
  }
  
}

void sync_get_time(byte *data)
{
  #if defined( BLUETOOTH_BY_SOFT )
    Serial.println("GET TIME DATA");
  #endif


  byte time_h = (data[SYNC_INDEX_TIME_H] - '0') * 10 + data[SYNC_INDEX_TIME_H+1] - '0';
  byte time_m = (data[SYNC_INDEX_TIME_M] - '0') * 10 + data[SYNC_INDEX_TIME_M+1] - '0';
  byte time_s = (data[SYNC_INDEX_TIME_S] - '0') * 10 + data[SYNC_INDEX_TIME_S+1] - '0';
  
  if(time_h>23 || time_m>59 || time_s>59 )
  {
    set_time_error_msg();
    SetTask(sync_send_error);
    return;  
  }
  

  cli();
  _time_ms=0;
  _time_h=time_h;
  _time_m=time_m;
  _time_s=time_s;
  sei();

  SetTask(after_time_sync);
}


void sync_play()
{
  #if defined( BLUETOOTH_BY_SOFT )
    Serial.println("PLAY BY ADMIN");
  #endif

  Play();
  SetTimerTask(Stop, max_playing_duration);
  SetTask(sync_send_ok);
   
}

void clear_serial()
{
    while(SerialBT.available())
    {
      SerialBT.read();
    }
}

void after_sync() 
{
  SetTask(write_rtc);
  #if !defined( EEPROM_NOT_UPDATE )
    SetTask(sync_eeprom_update);
  #endif
  SetTask(write_rtc);
  SetTask(setting_mp3);
  SetTask(sync_send_ok);
}

void after_time_sync() 
{
  SetTask(write_rtc);
  SetTask(sync_send_ok);
}

void check_sync()
{
   // Проверка пришло ли сообщение от блютуза
   // Ждем пока пакет придет целиком
   if (SerialBT.available() > 40) {  

        byte correct_string = true;
        byte data_array[41];
        byte start_str[6]="SYNC_";
        byte end_str[5]="END\n";
        
        for(byte i=0; i<41; i++)
        {
          data_array[i] =SerialBT.read();
        }

        //Если включен BT_DEBUG_OUTPUT - вываливаем все в консоль
        #if defined( BT_DEBUG_OUTPUT )
          for(byte i=0; i<41; i++)
          {
            Serial.write(data_array[i]);
          }
          Serial.println();
        #endif


        //Проверяем начало пакета, ищем "SYNC_", если не нашли это в начале пакета - пакет битый
        for(byte i=0; i<5; i++)
        {
          if(data_array[i]!=start_str[i])
          {
            error_message[0] = 'P';
            error_message[1] = 'A';
            error_message[2] = 'R';
            error_message[3] = 'S';
            error_message[4] = 'E';
            error_message[5] = ' ';
            error_message[6] = 'E';
            error_message[7] = 'R';
            error_message[8] = 'R';
            error_message[9] = 'O';
            error_message[10] = 'R';
            error_message[11] = ' ';
            error_message[12] = 'S';
            error_message[13] = 'T';
            error_message[14] = 'A';
            error_message[15] = 'R';
            error_message[16] = 'T'; 
            for(byte i=17; i<32; i++)
            {
              error_message[i]=' ';
            }
            correct_string=false;
            break;
          }
        }


        // Если нашли начало пакета - ищем конец пакета. если не нашли - ну ты понел
        if(correct_string!=false)
        {
          byte end_str_index=3;
          for(byte i=40; i>36; i--)
          {
            if(data_array[i]!=end_str[end_str_index])
            {
              error_message[0] = 'P';
              error_message[1] = 'A';
              error_message[2] = 'R';
              error_message[3] = 'S';
              error_message[4] = 'E';
              error_message[5] = ' ';
              error_message[6] = 'E';
              error_message[7] = 'R';
              error_message[8] = 'R';
              error_message[9] = 'O';
              error_message[10] = 'R';
              error_message[11] = ' ';
              error_message[12] = 'E';
              error_message[13] = 'N';
              error_message[14] = 'D';
              error_message[15] = ' ';
              error_message[16] = ' ';
              for(byte i=17; i<32; i++)
              {
                error_message[i]=' ';
              }
              correct_string=false;
              break;
            }
            end_str_index--;
          }
        }

        //Если начало и конец пакета найдены - смотрим какой метод пришел
        if(correct_string!=false)
        {
            byte method = data_array[SYNC_INDEX_METHOD];
            if(method=='R') // - запрос анных, отправляем
            {
              SetTask(sync_send_data);
            }
            else if(method=='W') //Пришли новые данные - читаем и сохраняем
            {
                sync_get_data(data_array);
            }
            else if(method=='T') //Пришло время
            {
                sync_get_time(data_array);
            }
            else if(method=='P') //Пришла команда воспроизвести мелодию
            {
                sync_play();
            }
            else
            {
                // Если метод не распознан - пакет битый
                error_message[0] = 'M';
                error_message[1] = 'E';
                error_message[2] = 'T';
                error_message[3] = 'H';
                error_message[4] = 'O';
                error_message[5] = 'D';
                error_message[6] = ' ';
                error_message[7] = 'E';
                error_message[8] = 'R';
                error_message[9] = 'R';
                error_message[10] = 'O';
                error_message[11] = 'R';
                error_message[12] = ' ';
                error_message[13] = method;
                error_message[14] = ' ';
                error_message[15] = ' ';
                error_message[16] = ' ';
                for(byte i=17; i<32; i++)
                {
                  error_message[i]=' ';
                }
                
                SetTask(sync_send_error);
            }
        }
        else
        { 
            SetTask(sync_send_error);
        }
        clear_serial();
  } 
  SetTimerTask(check_sync, SYNC_PERIOD);
}


void set_time_error_msg()
{
    error_message[0] = 'T';
    error_message[1] = 'I';
    error_message[2] = 'M';
    error_message[3] = 'E';
    error_message[4] = ' ';
    error_message[5] = 'S';
    error_message[6] = 'E';
    error_message[7] = 'T';
    error_message[8] = 'T';
    error_message[9] = 'I';
    error_message[10] = 'N';
    error_message[11] = 'G';
    error_message[12] = ' ';
    error_message[13] = 'E';
    error_message[14] = 'R';
    error_message[15] = 'R';
    error_message[16] = 'O';
    error_message[17] = 'R';

    for(byte i=18; i<32; i++)
    {
      error_message[i]=' ';
    }
}


void set_data_error_msg()
{
    error_message[0] = 'D';
    error_message[1] = 'A';
    error_message[2] = 'T';
    error_message[3] = 'A';
    error_message[4] = ' ';
    error_message[5] = 'I';
    error_message[6] = 'N';
    error_message[7] = 'C';
    error_message[8] = 'O';
    error_message[9] = 'R';
    error_message[10] = 'R';
    error_message[11] = 'E';
    error_message[12] = 'C';
    error_message[13] = 'T';
    error_message[14] = ' ';
    error_message[15] = ' ';
    error_message[16] = ' ';
    error_message[17] = ' ';

    for(byte i=18; i<32; i++)
    {
      error_message[i]=' ';
    }
}


void dymmy_send()
{
  SerialBT.println("Fuck you!!!");
  SetTimerTask(dymmy_send, 1000);
}
//********************************************************************************************************************************************************
// ************************* Главный цикл //**************************************************************************************************************
//********************************************************************************************************************************************************
void setup() {

  Serial.begin(SERIAL_BAUDRATE);
  while (!Serial) {;}
  Serial.println("DOOR BELL v0");


  
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(SOUND_ON_PIN, OUTPUT);
  pinMode(SRX_PIN, INPUT); 
  pinMode(STX_PIN, OUTPUT);


  
  Serial2.begin(9600);
  if (!myDFPlayer.begin(Serial2)) {
      //Serial.println("DFPlayer error"); //срабатывает из за нестандартной либы softwareserial, сам не понял почему. игнорим
      //while(1){;}  
  }
  randomSeed(analogRead(0));

  //INIT SOFTWARE FOR BLUETOOTH 
  #if defined(BLUETOOTH_BY_SOFT)
    pinMode(BT_RX_PIN, INPUT);
    pinMode(BT_TX_PIN, OUTPUT);
  
    SerialBT.begin(BT_SERIAL_BAUDRATE);
    SerialBT.listen();
    //SerialBT.println("DOOR BELL v0");
  #endif
  
  

  InitAll();      // Инициализируем периферию
  InitRTOS();     // Инициализируем ядро 
   


  init_vars();
  //write_eeprom();
  read_eeprom();
  
  
  //Прерывание кнопки
  pinMode(BTN_PIN,INPUT);   
  digitalWrite(BTN_PIN,HIGH);
  PCICR |= 1<<PCIE2;    //Разрешаем прерывание PCIN2
  PCMSK2 |= 1<<PCINT23; //Делаем маску, прерывание только на PCINT23

    
  //Настройка таймера Т1 на ~1мс  
  TCCR1A=0;
  TCCR1B=0;
  TCNT1H=0;
  TCNT1L=0;
  OCR1AH = highByte(TIMER1_OCR_1MS);  //1 ms
  OCR1AL = lowByte(TIMER1_OCR_1MS);
  
  //MODE: CTC; CLOCK: 1024
  TCCR1B |= (1<<WGM12);
  TCCR1B |= (1<<CS11);
  TCCR1B |= (1<<CS10);
  TIMSK1 |= (1 << OCIE1A);


  //DFPlaer settings
  first_setting_mp3();
 
           

  // Запуск фоновых задач.
  RunRTOS();        // Старт ядра. 
  SetTimerTask(read_max_volume, READ_MAX_VOLUME_DELAY);
  SetTimerTask(check_time, 1000);
  SetTimerTask(read_rtc, RTC_READ_PERIOD);
  SetTimerTask(check_sync, SYNC_PERIOD);
  SetTimerTask(BlinkOFF,500);
  sei();
}

void loop() {
  TaskManager();   
}
