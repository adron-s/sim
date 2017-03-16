/* этот код не претеднует на быстродействие! для нормльной скорости его нужно переписать на работу с регистрами процессора вместо отдельных пинов!
   это просто наглядная демонстрация как писать и читать 4-х битные блоки в микросхему памяти используемую на SIM планках памяти.
   Сво микросхему я взял из видеокарты Realtek.  http://www.datasheets360.com/pdf/-2995955055621689720
   Я основывал свой код на проекте https://github.com/zrafa/30pin-simm-ram-arduino но переделал его под работу с выходами вместо регистров.
   Операция чтения так же выполняет и рефрешинг блоков памяти которые она читает. Говоря по простому - если перестать читать данные то через некоторое
   время они в памяти испортятся! Даже при наличии питания. Это время я не заменял но видимо это 2-3 минуты что довольно много. Получается что
   перезарядка элементов хранящих данные в памяти делается только при рефрешинге! Сама по себе она не делается а так как это конденсаторы
   то через ток утечки они со временем разряжаются и данные теряются! В датащите на микросхему описаны различные способы выполнять рефрешинг.
   Грубо говоря рефрешинг это когда ты по адресной шине выбираешь ячейку указывая сначала ras а затем cas в LOW а потом переводя их в HIGH. Этого
   уже достаточно! Собственно в нагрузку тебе на DQ0-3 вернутся значения этой 4-х битной ячейки. Зачем нужен OE я не разбирался. Автор 
   30pin-simm-ram-arduino его не использует! А вот we нужен! Он используется при записи данных! В 30pin-simm-ram-arduino про рефрешинг непонятно тоже.
   Видимо у автора сначала была идея делать его через прерывание от таймера кажные 60ms но в коде это не реализовано...В общем как то так...
*/

const int a0 = 2;
const int a1 = 3;
const int a2 = 4;
const int a3 = 5;
const int a4 = 6;
const int a5 = 7;
const int a6 = 8;
const int a7 = 9;
const int a8 = 10;

const int ras = 11;
const int cas = 12;

const int dq0 = 14;
const int dq1 = 15;
const int dq2 = 16;
const int dq3 = 17;

const int we = 18;
//oe не используется!
const int oe = 19;

unsigned long count = 0;
unsigned long last_mi = millis();

void sim_pin_init(char pin){
  pinMode(pin, OUTPUT);
  digitalWrite(pin, LOW);
}

#define MESSAGE_LEN 15
#define COUNT_OFFSET 40

void setup() {       
  sim_pin_init(a0);
  sim_pin_init(a1);
  sim_pin_init(a2);
  sim_pin_init(a3);
  sim_pin_init(a4);
  sim_pin_init(a5);
  sim_pin_init(a6);
  sim_pin_init(a7);
  sim_pin_init(a8);
  
  sim_pin_init(ras);
  sim_pin_init(cas);
  
  sim_pin_init(dq0);
  sim_pin_init(dq1);
  sim_pin_init(dq2);
  sim_pin_init(dq3);

  sim_pin_init(we);
  sim_pin_init(oe);

  //led L  
  pinMode(13, OUTPUT);
  digitalWrite(13, LOW);
    
  Serial.begin(9600);

  //ram_init
  digitalWrite(we, HIGH);
  digitalWrite(ras, HIGH);
  digitalWrite(cas, HIGH);
    
  write_count_to_ram();
}

void ram_write(unsigned int row, unsigned int col, unsigned char val){
  unsigned char pin;
  //переводим выходы DQ0-3 в режим передачи
  pinMode(dq0, OUTPUT);
  pinMode(dq1, OUTPUT);
  pinMode(dq2, OUTPUT);
  pinMode(dq3, OUTPUT);
  //передаем row
  for(pin = a0; pin <= a8; pin++, row >>= 1)
    digitalWrite(pin, row & 0x1);
  digitalWrite(ras, LOW);
  //set data bits on DQ0-3 pins
  for(pin = dq0; pin <= dq3; pin++, val >>= 1)
    digitalWrite(pin, val & 0x1);   
  //write it(data)
  digitalWrite(we, LOW);
  //передаем col
  for(pin = a0; pin <= a8; pin++, col >>= 1)
    digitalWrite(pin, col & 0x1);
  digitalWrite(cas, LOW);
  digitalWrite(cas, HIGH);
  digitalWrite(we, HIGH);
  digitalWrite(ras, HIGH);  
  //переводим выходы DQ0-3 в режим приема
  pinMode(dq0, INPUT);
  pinMode(dq1, INPUT);
  pinMode(dq2, INPUT);
  pinMode(dq3, INPUT);
}

unsigned char ram_read(unsigned int row, unsigned int col){
  unsigned char res = 0;
  unsigned char pin;
  //передаем row
  for(pin = a0; pin <= a8; pin++, row >>= 1)
    digitalWrite(pin, row & 0x1);
  digitalWrite(ras, LOW);
  //передаем col
  for(pin = a0; pin <= a8; pin++, col >>= 1)
    digitalWrite(pin, col & 0x1);
  digitalWrite(cas, LOW);
  digitalWrite(ras, HIGH);  
  digitalWrite(cas, HIGH);
  //read data bits from DQ0-3 pins
  for(pin = dq3; pin >= dq0; pin--){
    res |= digitalRead(pin) & 0x1;
    if(pin > dq0)
      res <<= 1;
  }  
  return res;
}

void write_count_to_ram(){
  unsigned long _count = count;
  unsigned char a = 0;
  //пишем блоками по 4 бита
  for(a = 0; a < sizeof(count) * 2; a++){
    ram_write(COUNT_OFFSET, a, _count & 0xF);
    _count >>= 4;    
  }    
}

void read_count_from_ram(){
  unsigned long _count = 0;
  char a = 0;
  //читаем блоками по 4 бита
  for(a = sizeof(count) * 2 - 1; a >= 0; a--){
    _count |= ram_read(COUNT_OFFSET, a);
    if(a > 0)
      _count <<= 4;    
  }
  count = _count;
}

void load_from_serial_to_ram(){
  unsigned int a = 0;
  while(Serial.available() > 0 && a < 512 / 2){
    char l = Serial.read();
    ram_write(0, 2 * a, l & 0xF);
    ram_write(0, 2 * a + 1, (l >> 4) & 0xF);
    a++;
  }
  ram_write(0, 2 * a, 0);
  ram_write(0, 2 * a + 1, 0);
  Serial.print("To Sim RAM stored ");
  Serial.print(a);
  Serial.println(" bytes");
}

#define PRINT_PERIOD 2000

void loop() {
  unsigned int a;
  if(millis() > last_mi + PRINT_PERIOD){
    last_mi = millis();
    Serial.print("message: ");
    read_count_from_ram();
    //считаем данные из sim ОЗУ(0..511 это 9-ть единиц!. читаем блоками по 4 бита!)
    for(a = 0; a < 512 / 2; a++){
      char l = 0;
      l = ram_read(0, 2 * a + 1);
      l <<= 4;
      l |= ram_read(0, 2 * a) & 0xf;
      //пока не встретим нулевой символ!
      if(l == 0)
        break;
      Serial.print(l);
    }  
    Serial.println();
    Serial.print("count: ");
    Serial.println(count);
    count++;
    write_count_to_ram();
    count = 0; //для наглядности   
  }
  if(Serial.available()){
    delay(100);
    load_from_serial_to_ram();
  }
}
