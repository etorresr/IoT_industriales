/*
 * Yaxcal Tec
 * Diseño de interfaz con una
 * ILI9341 de Adafruit
 * ESP32 WROOM
 * Arduino placa --> FireBeetle-ESP32
 * Mayo 2021
 */
#include <Adafruit_GFX.h>         // Core graphics library
#include <Adafruit_ILI9341.h>      // Hardware-specific library
#include <SdFat.h>                // SD card & FAT filesystem library
#include <Adafruit_SPIFlash.h>    // SPI / QSPI flash library
#include <Adafruit_ImageReader.h> // Image-reading functions
#include <stdint.h>
#include "SPI.h"                  //Libreria puerto SPI
#include "TouchScreen.h"          //Libreria para elemento touch
#define USE_SD_CARD

/*
 * Definicion de pines de conexion
 * #23MOSI, #19MISO, #18CLK
 */
#define TFT_RST -1
#define TFT_CS   22
#define TFT_DC   21
#define SD_CS   17
#define YP 15   //Pin en el breakout Y+ debe ser Analogico
#define XM 4   //Pin en el breakout X- debe ser analogico
#define YM 0    //Pin en el breakout Y- debe ser digital
#define XP 2    //Pin en el breakout X+ debe ser digital
#define cc 10    //puerto de calentador
#define vv 9    //puerto de ventilador
/*
 * Se conecta el módulo SPI del chip
 * PIN-CHIP------|-----HX8357
 * 18--SPI_CLK--->-----CLK
 * 19--SPI_MISO-->-----MISO
 * 23--SPI_MOSI-->-----MOSI
 */
//Proceso de configuracion SD
#if defined(USE_SD_CARD)
  SdFat                SD;         // SD card filesystem
  Adafruit_ImageReader reader(SD); // Image-reader object, pass in SD filesys
#else
  // SPI or QSPI flash filesystem (i.e. CIRCUITPY drive)
  #if defined(__SAMD51__) || defined(NRF52840_XXAA)
    Adafruit_FlashTransport_QSPI flashTransport(PIN_QSPI_SCK, PIN_QSPI_CS,
      PIN_QSPI_IO0, PIN_QSPI_IO1, PIN_QSPI_IO2, PIN_QSPI_IO3);
  #else
    #if (SPI_INTERFACES_COUNT == 1)
      Adafruit_FlashTransport_SPI flashTransport(SS, &SPI);
    #else
      Adafruit_FlashTransport_SPI flashTransport(SS1, &SPI1);
    #endif
  #endif
  Adafruit_SPIFlash    flash(&flashTransport);
  FatFileSystem        filesys;
  Adafruit_ImageReader reader(filesys); // Image-reader, pass in flash filesys
#endif

//Se define pÃ­nes para el core del LCD y se define variable para imagen
Adafruit_ILI9341        tft    = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST); //Se agrega RST
Adafruit_Image         img;        // An image loaded into RAM

//Se definen pines para el uso del TScreen. Ademas se pasa el valor resistivo
//entre X+ y X-
TouchScreen ts = TouchScreen(XP, YP, XM, YM, 30);
//*******************************************|-->pressureThreshhold
/*
 * banderas globales
 */
int xpos = 39;
int programa = 0;
int tiempo = 0;
String txt1,txt2;
int bandera = 1;
int color_select=0;
/*
 * son accesadas por las funciones
 * de operacion en funcion loop
 */
void letrero(String, String);
void inicio(void);
void secado(void);
void selec(int);
void pantalla_inicial(void);
void paro_emergencia(void);
/*
función de configuración principal
*/
void setup() {
  Serial.begin(9600);
  analogReadResolution(10);
  SPI.setFrequency(80000000);
  pinMode(cc,OUTPUT);
  pinMode(vv,OUTPUT);
  ImageReturnCode stat;
  tft.begin();

#if defined(USE_SD_CARD)
  // SD card is pretty straightforward, a single call...
  if(!SD.begin(SD_CS, SD_SCK_MHZ(25))) { // ESP32 requires 25 MHz limit
   // Serial.println(F("SD begin() failed"));
    for(;;); // Fatal error, do not continue
  }
#else
  // SPI or QSPI flash requires two steps, one to access the bare flash
  // memory itself, then the second to access the filesystem within...
  if(!flash.begin()) {

    for(;;);
  }
  if(!filesys.begin(&flash)) {

    for(;;);
  }
#endif

  tft.fillScreen(ILI9341_BLACK);
  stat = reader.drawBMP("/logo1.bmp", tft, 0, 50);
  reader.printStatus(stat);
  tft.fillRect(0,0,240,50,0xFFFF);
  tft.fillRect(300,0,320,50,0xFFFF);
  delay(4000);
  /*
   * Permanece por 4 segundos el logo 
   */
  tft.fillScreen(ILI9341_BLACK);
  stat = reader.drawBMP("/ball.bmp", tft, 0, 145);

  pantalla_inicial();

  txt1 = "--*Bienvenido*--";
  txt2 = "----|-----|-----";
  letrero(txt1,txt2);

}

void loop() {
   TSPoint p = ts.getPoint();

  if (p.z > ts.pressureThreshhold) {
    //Boton Start
    if(p.x>273 && p.x<324 && p.y>651 && p.y<687)
    {
      txt1 = "Inicia proceso de";
      txt2 = "esterilizacion";
      bandera = 0;
      letrero(txt1,txt2);
      inicio();
    }
    //Boton Stop
    if(p.x>441 && p.x<563 && p.y>635 && p.y<693)
    {
    paro_emergencia();
    }
    //Boton Select
    if(p.x>266 && p.x<332 && p.y>767 && p.y<851)
    {
        color_select++;
      if (color_select == 4){
        color_select = 0;
      }

      selec(color_select);
    }
    //Boton Dry
    if(p.x>405 && p.x<559 && p.y>778 && p.y<841)
    {
      secado();
  }
  }

  delay(100);

}

void letrero(String txt1, String txt2){
  tft.fillRect(60,30,235,70,0x03EF);
  tft.fillRect(65,35,240,70,0xFFFF);
  tft.setCursor(85,50); tft.setTextColor(0x001F); tft.setTextSize(1);
  tft.println(txt1);
  tft.setCursor(95,80); tft.setTextColor(0x001F); tft.setTextSize(1);
  tft.println(txt2);
}
void secado (void){
  int llenado = 0;
  pantalla_inicial();
  tft.setCursor(2,110); tft.setTextColor(0x0000); tft.setTextSize(1);
  tft.println("Secado en progreso");
  txt1 = "Se lleva a cabo";
  txt2 = "programa secado";
  Serial.println(txt2);
  letrero(txt1,txt2);

  while (llenado < 240){
    if (llenado < 80){
    tft.fillRect(1,131,llenado,14,0xF800);
    }
    if (llenado > 80 && llenado < 160){
    tft.fillRect(1,131,llenado,14,0xFD20);
    }
    if (llenado > 160){
    tft.fillRect(1,131,llenado,14,0x001F);
    }
    digitalWrite(vv,HIGH);
    llenado++;
    delay(10);
  }

  tft.setCursor(2,110); tft.setTextColor(0x0000); tft.setTextSize(1);
  tft.println("");
  txt1 = "Programa secado";
  txt2 = "  finalizado";
  letrero(txt1,txt2);
  delay(60000);
  pantalla_inicial();

  txt1 = "--*Bienvenido*--";
  txt2 = "----|-----|-----";
  letrero(txt1,txt2);
}
void inicio (void){
  int llenado = 0;
  int cuenta = 0;
  float valory = 0;
  pantalla_inicial();
  tft.setCursor(42,110); tft.setTextColor(0x0000); tft.setTextSize(1);
  tft.println("Calentando camara");
  delay(100);
  Serial.println(txt2);

  while (llenado < 240){
    if (llenado < 100){
    tft.fillRect(1,131,llenado,14,0x001F);
    }
    if (llenado > 100 && llenado < 200){
    tft.fillRect(1,131,llenado,14,0xFD20);
    }
    if (llenado > 200){
    tft.fillRect(1,131,llenado,14,0xF800);
    }
    //tft.fillRect(llenado+2,131,llenado+240,10,0x7BEF);
    digitalWrite(cc,HIGH);
    llenado++;
  }
  tft.fillRect(30,0,240,120,0x45D5);
  tft.fillRect(0,130,240,14,0x45D5);
  tft.drawLine(0,130,240,130,0x001F);
  tft.drawLine(0,145,240,145,0x001F);
  tft.setCursor(42,110); tft.setTextColor(0x0000); tft.setTextSize(1);
  tft.println("Esterilizacion en progreso");
  tft.setCursor(42,0); tft.setTextColor(0x0000); tft.setTextSize(1);
  if (programa == 1) tft.println("Programa 1");
  if (programa == 2) tft.println("Programa 2");
  if (programa == 3) tft.println("Programa 3");
  tft.fillRect(llenado+2,144,llenado+240,14,0x7BEF);
  while (xpos < 240){
    if (cuenta > 20){
      xpos++;
      cuenta = 0;
    }
    valory = 50*log10(xpos-38);
    valory = (int)valory;
    delay(tiempo);
    tft.fillRect(xpos,115-valory,2,2,ILI9341_BLUE);
    tft.fillRect(1,131,xpos+20,14,0x07E0);
    cuenta++;


  }
  xpos = 39;
  digitalWrite(cc,LOW);
  txt1 = "Puede iniciar el";
  txt2 = "proceso de secado";
  letrero(txt1,txt2);

}
 void selec (int color_select){
  pantalla_inicial();
  Serial.println("seleccion");

  if (color_select == 1){
    txt1 = "Programa 1";
    txt2 = "134 C @ 4 min";
    letrero(txt1,txt2);
    programa = 1;
    tiempo = 10;
  }
  if (color_select == 2){
    txt1 = "Programa 2";
    txt2 = "135 C @ 10 min";
    letrero(txt1,txt2);
    programa = 2;
    tiempo = 20;
  }
  if (color_select == 3){
    txt1 = "Programa 3";
    txt2 = "121 C @ 20 min";
    letrero(txt1,txt2);
    programa = 3;
    tiempo = 30;
  }
 }
 void paro_emergencia(void){
  tft.fillRect(0,0,240,145,0xF800);
  txt1 = "Paro de emergencia";
  txt2 = "5 minutos para abrir puerta";
  letrero(txt1,txt2);
  delay(1000);
  pantalla_inicial();

  txt1 = "--*Bienvenido*--";
  txt2 = "----|-----|-----";
  letrero(txt1,txt2);

 }
 void pantalla_inicial(void){
  tft.fillRect(0,0,240,145,0x45D5);
  tft.drawLine(0,130,240,130,0x001F);
  tft.drawLine(0,145,240,145,0x001F);
  tft.setCursor(5,5); tft.setTextColor(ILI9341_WHITE); tft.setTextSize(1);
  tft.println("200");
  tft.setCursor(5,34); tft.setTextColor(ILI9341_WHITE); tft.setTextSize(1);
  tft.println("150");
  tft.setCursor(5,62); tft.setTextColor(ILI9341_WHITE); tft.setTextSize(1);
  tft.println("100");
  tft.setCursor(5,91); tft.setTextColor(ILI9341_WHITE); tft.setTextSize(1);
  tft.println("50");
  tft.setCursor(5,120); tft.setTextColor(ILI9341_WHITE); tft.setTextSize(1);
  tft.println("0");


  tft.setCursor(40,120); tft.setTextColor(ILI9341_WHITE); tft.setTextSize(1);
  tft.println("20");
  tft.setCursor(65,120); tft.setTextColor(ILI9341_WHITE); tft.setTextSize(1);
  tft.println("40");
  tft.setCursor(90,120); tft.setTextColor(ILI9341_WHITE); tft.setTextSize(1);
  tft.println("60");
  tft.setCursor(115,120); tft.setTextColor(ILI9341_WHITE); tft.setTextSize(1);
  tft.println("80");
  tft.setCursor(140,120); tft.setTextColor(ILI9341_WHITE); tft.setTextSize(1);
  tft.println("100");
  tft.setCursor(165,120); tft.setTextColor(ILI9341_WHITE); tft.setTextSize(1);
  tft.println("120");
  tft.setCursor(190,120); tft.setTextColor(ILI9341_WHITE); tft.setTextSize(1);
  tft.println("140");
  tft.setCursor(215,120); tft.setTextColor(ILI9341_WHITE); tft.setTextSize(1);
  tft.println("160");
 }
