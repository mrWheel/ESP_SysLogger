#ifndef _ESP_SYSLOGGER_H
#define _ESP_SYSLOGGER_H

#if defined(ESP8266)
  #include <FS.h>
#else
  #include <SPIFFS.h>
#endif

#define Verbose1 false

class ESPSL {
  
public:
  ESPSL();

  boolean begin(uint8_t depth, uint8_t recLen);
  boolean begin(uint8_t depth, uint8_t recLen, boolean mode);
  boolean create(uint8_t depth, uint8_t recLen);
  boolean init();
  boolean status();
  boolean write(const char*);
  boolean writef(const char *fmt, ...);
  boolean startReading(uint8_t startLine, uint8_t numLines);    // Returns last line read
  String  readNextLine();
  String  dumpLogFile();
  boolean remove();
  
private:

  const char* _sysLogFile = "/sysLog.dat";
  uint32_t    _lineID;
  int32_t     _noLines;
  int32_t     _recLen;
  uint32_t     _readPointer;
  int16_t     _readEnd;
  char        cRec[255];
  void        fixRecLen(char *record, int len);
  int16_t     sysLogFileSize();

};

#endif
