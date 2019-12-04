# ESP_SysLogger
A system Logger Library for the ESP micro controller chips

## setup your code

You need to include "ESP_SysLogger.h"
```
#include "ESP_SysLogger.h"
```

Then you need to create the ESPSL object 
```
ESPSL sysLog;
```
In `setup()` add the following code to create or open a log file of 100 lines, 80 chars/line
```
   SPIFFS.begin();
   .
   .
   if (!sysLog.begin(100, 80)) {
     Serial.println("sysLog.begin() error!");
     delay(10000);
   }
```
Adding an entry to the system log 
```
   sysLog.write("This is a line of text");
```
or
```
   sysLog.writef("This is line [%d] of [%s]", __LINE__, __FUNCTION__);
```
To display the sytem log file you first have to tell the ESP_SysLogger where to start
and how many lines you want to see
```
   Serial.println("\n=====from oldest to end==============================");
   sysLog.startReading(0, 0);
   while( (lLine = sysLog.readNextLine()) && !(lLine == "EOF")) {
     Serial.printf("==>> [%s]\r\n", lLine.c_str());
   }
```
or just the last 15 lines
```
   Serial.println("\n=====last 15 lines==============================");
   sysLog.startReading(-15);
   while( (lLine = sysLog.readNextLine()) && !(lLine == "EOF")) {
     Serial.printf("==>> [%s]\r\n", lLine.c_str());
   }
```

With a simple macro you can add Debug info to your log-lines
```
#if defined(_Time_h)    // _Time_h is defined by #include <TimeLib.h>
  #define writeToSysLog(...) ({ sysLog.writeD(hour(), minute(), second(), __FUNCTION__, __LINE__, __VA_ARGS__); })
#else
  #define writeToSysLog(...) ({ sysLog.writeD(__FUNCTION__, __LINE__, __VA_ARGS__); })
#endif
```
This Logline:
```
     writeToSysLog("Reset Reason [%s]", ESP.getResetReason().c_str());
```
looks like this (the first line):
```
   [12:30:22][  46120| 45952] [setup       ( 179)] Reset Reason [External System] 
   [19:51:03][  45616| 44912] [showBareLogF(  62)] Dump logFile [sysLog.dumpLogFile()]
```
the macro add's the time, free heap, the larges block of heap, the name of the function in which this
log line was written and the line number in that function:
```
   [12:30:22][  46120| 45952] [setup       ( 179)]
```

... more to come
