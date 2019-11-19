/*
 * 
 */

//#define Verbose1 false

#include "ESP_SysLogger.h"

ESPSL sysLog; //Create instance of this object

uint32_t statusTimer;

void listSPIFFS(void)
{
  Serial.println("===============================================================");
#if defined(ESP8266)
  {
  Dir dir = SPIFFS.openDir("/");         // List files on SPIFFS
  while (dir.next())  {
          String fileName = dir.fileName();
          size_t fileSize = dir.fileSize();
          Serial.printf("FS File: %s, size: %d\n", fileName.c_str(), fileSize);
      }
  }
#else
  {
      File root = SPIFFS.open("/");
      File file = root.openNextFile();
      while(file){
          String fileName = file.name();
          size_t fileSize = file.size();
          Serial.printf("FS File: %s, size: %s\n", fileName.c_str(), formatBytes(fileSize).c_str());
          file = root.openNextFile();
      }
  }
#endif
  Serial.println("===============================================================");
} // listSPIFFS()

//-------------------------------------------------------------------------
void setup() 
{
  Serial.begin(115200);
  Serial.println("\nESP8266: Start ESP System Logger ....\n");

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  Serial.println(ESP.getResetReason());
  if (   ESP.getResetReason() == "Exception" 
      || ESP.getResetReason() == "Software Watchdog"
      || ESP.getResetReason() == "Soft WDT reset"
      ) {
      while (1) {
        delay(500);
        digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
      }
  }
#if defined(ESP8266)
  {
    SPIFFS.begin();
  }
#else
  {
    SPIFFS.begin(true);
  }
#endif
   listSPIFFS();

  if (!sysLog.begin(21, 50)) {
  //if (!sysLog.begin(21, 50, true)) {
    sysLog.dumpLogFile();
  }
  //sysLog.init();
  //sysLog.dumpLogFile();
  sysLog.writef("Reset Reason [%s]\n", ESP.getResetReason().c_str());
  sysLog.status();
  
  listSPIFFS();

  Serial.println("\n=====================================================");
  sysLog.dumpLogFile();
  Serial.println("\n=====================================================");
  sysLog.startReading(0, 0);
  String lLine;
  while( (lLine = sysLog.readNextLine()) && !(lLine == "EOF")) {
    Serial.println(lLine);
  }

  Serial.println("\n=====================================================");
  sysLog.startReading(10, 5);
  while( (lLine = sysLog.readNextLine()) && !(lLine == "EOF")) {
    Serial.println(lLine);
  }
  
  Serial.println("\n=====================================================");
  sysLog.startReading(5, 5);
  while( (lLine = sysLog.readNextLine()) && !(lLine == "EOF")) {
    Serial.println(lLine);
  }
  
  Serial.println("\n=====================================================");
  sysLog.startReading(5, 0);
  while( (lLine = sysLog.readNextLine()) && !(lLine == "EOF")) {
    Serial.println(lLine);
  }
  Serial.println("\nsetup() done .. \n");
  
} // setup()


//-------------------------------------------------------------------------
void loop() 
{
  static uint8_t lineCount = 0;
  
  sysLog.writef("-0-234 Regel [%2d] 78=3=2345678=4=2345678=5=2345678=6=", ++lineCount);
  sysLog.writef("       Regel [%2d] [%d]", ++lineCount, millis());

  if (lineCount > 7) {
    listSPIFFS();
    sysLog.dumpLogFile();
    Serial.println("==========================================");
    sysLog.readNextLine();
    sysLog.status();
  }
  while (lineCount > 7) {
    delay(1000);
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
  }
  
} // loop()
