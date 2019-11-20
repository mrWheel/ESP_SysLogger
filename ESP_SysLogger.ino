/*
**  Program   : ESP_SysLogger
*/
#define _FW_VERSION "v0.1.0 (20-11-2019)"

/*
**  Copyright (c) 2019 Willem Aandewiel / Erik Meinders
**
**  TERMS OF USE: MIT License. See bottom of file.
***************************************************************************/

#include "ESP_SysLogger.h"

#if defined(ESP32) 
  #define LED_BUILTIN 2
#endif

ESPSL sysLog; // Create instance of this object

uint32_t statusTimer;


//-------------------------------------------------------------------------
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
          Serial.printf("FS File: %s, size: %d\n", fileName.c_str(), fileSize);
          file = root.openNextFile();
      }
  }
#endif
  Serial.println("===============================================================");
} // listSPIFFS()


//-------------------------------------------------------------------------
void testReadnext() 
{
  String lLine;
  Serial.println("\n=====================================================");
  sysLog.dumpLogFile();

  Serial.println("\n=====from oldest to end==============================");
  sysLog.startReading(0, 0);  
  while( (lLine = sysLog.readNextLine()) && !(lLine == "EOF")) {
    Serial.printf("==>> [%s]\r\n", lLine.c_str());
  }
  
  Serial.println("\n=====from oldest for 9 lines=========================");
  sysLog.startReading(0, 9);
  while( (lLine = sysLog.readNextLine()) && !(lLine == "EOF")) {
    Serial.printf("==>> [%s]\r\n", lLine.c_str());
  }
  
  Serial.println("\n=====from 20e for 6 lines============================");
  sysLog.startReading(20, 6);
  while( (lLine = sysLog.readNextLine()) && !(lLine == "EOF")) {
    Serial.printf("==>> [%s]\r\n", lLine.c_str());
  }

  Serial.println("\n=====last 12 lines===================================");
  sysLog.startReading(-12);
  while( (lLine = sysLog.readNextLine()) && !(lLine == "EOF")) {
    Serial.printf("==>> [%s]\r\n", lLine.c_str());
  }
  sysLog.setDebugLvl(0);
  Serial.println("\n=====last 12e for 5 lines=============================");
  sysLog.startReading(-12, 5);
  while( (lLine = sysLog.readNextLine()) && !(lLine == "EOF")) {
    Serial.printf("==>> [%s]\r\n", lLine.c_str());
  }
/**  
  Serial.println("\n=====from oldest to 100nt============================");
  sysLog.startReading(0, 100);
  while( (lLine = sysLog.readNextLine()) && !(lLine == "EOF")) {
    Serial.printf("==>> [%s]\r\n", lLine.c_str());
  }
  
  Serial.println("\n=====from 60e to 100nt===============================");
  sysLog.startReading(60, 100);
  while( (lLine = sysLog.readNextLine()) && !(lLine == "EOF")) {
    Serial.printf("==>> [%s]\r\n", lLine.c_str());
  }
**/
  Serial.println("\nDone testing readNext()\n");

} // testReadNext()


//-------------------------------------------------------------------------
void setup() 
{
  Serial.begin(115200);
#if defined(ESP8266)
  Serial.println("\nESP8266: Start ESP System Logger ....\n");
#elif defined(ESP32)
  Serial.println("\nESP32: Start ESP System Logger ....\n");
#else
  Serial.println("\nDon't know: Start ESP System Logger ....\n");
#endif

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

#if !defined(ESP32)
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
#endif

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

  if (!sysLog.begin(95, 60)) {
  //if (!sysLog.begin(75, 70, true)) {
    Serial.println("Error opening sysLog!");
    delay(10000);
  }
  sysLog.setDebugLvl(2);
  sysLog.status();

#if defined(ESP32)
  Serial.println("just started ..");
  sysLog.writef("Just Started [%d]", (sysLog.getLastLineID() +1));
#else
  Serial.printf("Reset Reason [%s]\r\n", ESP.getResetReason().c_str());
  sysLog.writef("Reset Reason [%s]", ESP.getResetReason().c_str());
#endif

  sysLog.setDebugLvl(0);
  sysLog.status();
  
  //listSPIFFS();

  Serial.println("\nsetup() done .. \n");
  delay(5000);
  
} // setup()


//-------------------------------------------------------------------------
void loop() 
{
  static uint8_t lineCount = 0;

  lineCount++;
  String dLine = "***********************************************************************************";
  String sLine;
  sLine = dLine.substring(0, lineCount);   
  sysLog.writef("Regel [@%d] %s", (sysLog.getLastLineID() +1), sLine.c_str());
  sysLog.writef("Regel [@%d]",    (sysLog.getLastLineID() +1));

  if (lineCount > 20) {
    Serial.println("\n");
    listSPIFFS();
    Serial.println("==========================================");
    sysLog.status();
    testReadnext();
  }
  while (lineCount > 20) {
    delay(1000);
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
  }
  
} // loop()

/***************************************************************************
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the
* "Software"), to deal in the Software without restriction, including
* without limitation the rights to use, copy, modify, merge, publish,
* distribute, sublicense, and/or sell copies of the Software, and to permit
* persons to whom the Software is furnished to do so, subject to the
* following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
* OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT
* OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR
* THE USE OR OTHER DEALINGS IN THE SOFTWARE.
* 
***************************************************************************/
