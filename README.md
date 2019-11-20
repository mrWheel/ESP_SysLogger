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

... more to come
