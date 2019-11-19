
#include "ESP_SysLogger.h"
//#include "Arduino.h"

//Constructor
ESPSL::ESPSL() { }

//-------------------------------------------------------------------------------------
// begin object
boolean ESPSL::begin(uint8_t depth, uint8_t recLen) 
{
  if (Verbose1) Serial.printf("begin(%d, %d)..\n", depth, recLen);

  char buffer[255];
  int16_t fileSize;
  
  // --- check if the file exists ---
  if (!SPIFFS.exists(_sysLogFile)) {
    create(depth, recLen);
  }
  fileSize = sysLogFileSize();
  if (fileSize < ((_noLines + 2) * _recLen)) {
    Serial.printf("begin(a): sysLog file size [%d] .. bailing out!\r\n", fileSize);
    return false;
  }
  // --- check if the file exists and can be opened ---
  if (Verbose1) Serial.printf("open(%s, 'r+') ..\r\n", _sysLogFile);
  File logFile  = SPIFFS.open(_sysLogFile, "r+");    // open for reading and writing
  if (!logFile) {
    Serial.printf("begin(): Some error opening [%s] .. bailing out!\r\n", _sysLogFile);
    return false;
  } // if (!logFile)
  fileSize = sysLogFileSize();
  if (fileSize < ((_noLines + 2) * _recLen)) {
    Serial.printf("begin(r+): sysLog file size [%d] .. error!\r\n", fileSize);
  }

  if (logFile.available() > 0) {
    if (Verbose1) Serial.println("begin(): read record [0]");
    int l = logFile.readBytesUntil('\n', buffer, sizeof(buffer));
        buffer[l]   = '\0';
        if (Verbose1) Serial.printf("begin(): rec[0] [%s]\r\n", buffer);
        sscanf(buffer,"%d;%d;%d;", 
                                &_lineID,
                                &_noLines,
                                &_recLen);
        if (Verbose1) Serial.printf("begin(): rec[0] [%d][%d][%d]\r\n", _lineID, _noLines, _recLen);
        Serial.flush();
        if (_noLines < 10) _noLines = 10; 
        if (_recLen  < 50) _recLen  = 50; 
  } 
 
  logFile.close();
  //dumpLogFile();
  init();
  //_nextFree = 0;
  return true; //We're all setup!
  
} // begin()

//-------------------------------------------------------------------------------------
// begin object
boolean ESPSL::begin(uint8_t depth, uint8_t recLen, boolean mode) 
{
  if (Verbose1) Serial.printf("begin(%d, %d, %d)..\n", depth, recLen, mode);

  if (mode) {
    remove();
    create(depth, recLen);
  }
  return (begin(depth, recLen));
  
} // begin()

//-------------------------------------------------------------------------------------
// Create a SysLog file on SPIFFS
boolean ESPSL::create(uint8_t depth, uint8_t recLen) 
{
  if (Verbose1) Serial.printf("create(%d, %d)..\n", depth, recLen);

  int16_t bytesWritten;
  
  _noLines  = depth;
  _recLen   = recLen;
  if (_recLen > 100) _recLen = 100;
  //_nextFree = 0;
  cRec[0] = '\0';  
  // --- check if the file exists and can be opened ---
  File logFile  = SPIFFS.open(_sysLogFile, "a");    // open for appending!

  if (Verbose1) sprintf(cRec, "%08d;%d;%d; ", 0, _noLines, _recLen);
  fixRecLen(cRec, _recLen);
  bytesWritten = logFile.print(cRec);
  if (bytesWritten != _recLen) {
    Serial.printf("Create(): ERROR!! written [%d] bytes but should have been [%d] for record [0]\r\n"
                                            , bytesWritten, _recLen);

    logFile.close();
    return false;
  }
  
  int r;
  for (r=1; r <= _noLines; r++) {
    sprintf(cRec, "%08d;@!@!@!@!=== empty log regel (%d) ==================================", r, r);
    fixRecLen(cRec, recLen);
    bytesWritten = logFile.print(cRec);
    //Serial.printf("[%d] -> [%s]\r\n", 0, cRec);
    if (bytesWritten != _recLen) {
      Serial.printf("create(): ERROR!! written [%d] bytes but should have been [%d] for record [%d]\r\n"
                                            , bytesWritten, _recLen, r);
      logFile.close();
      return false;
    }
    
  } // for r ....

  sprintf(cRec, "%08d;==EOF==", r);
  if (Verbose1) Serial.printf("create() -> cRec[%s]\r\n", cRec);
  fixRecLen(cRec, _recLen);
  bytesWritten = logFile.print(cRec);
  if (bytesWritten != _recLen) {
    Serial.printf("Create(): ERROR!! written [%d] bytes but should have been [%d] for record [%d]\r\n"
                                            , bytesWritten, _recLen, (_noLines+1));

    logFile.close();
    return false;
  }
  
  logFile.close();
  //_nextFree = 0;
  
  return (true);
  
} // create()

//-------------------------------------------------------------------------------------
// read SysLog file and find next line to write to
boolean ESPSL::init() 
{
  if (Verbose1) Serial.println("init()..");

  char logText[255];
  char buffer[255];
  int16_t recNr = 0;
  //_nextFree = 0;
      
  // --- check if the file exists and can be opened ---
  File logFile  = SPIFFS.open(_sysLogFile, "r");    // open for reading
  if (!logFile) {
    Serial.printf("init(): Some error opening [%s] .. bailing out!\r\n", _sysLogFile);
    return false;
  } // if (!logFile)

  uint32_t prevLineID = 0;
  _lineID             = 0;
  recNr               = 0;
  while (logFile.available() > 0) {
    recNr++;
    if (!logFile.seek((recNr) * _recLen, SeekSet)) {
      Serial.printf("init(): seek to position [%d] failed\r\n", recNr);
    }
    if (Verbose1) Serial.printf("init(): -> read record (recNr) [%d]\r\n", recNr);
    int l = logFile.readBytesUntil('\n', buffer, _recLen);
        buffer[l] = '\0';
        sscanf(buffer,"%d;%[^\0]", 
            &_lineID,
            logText);
        if (Verbose1) Serial.printf("init(): testing recNr[%2d] -> [%08d][%s]\r\n", recNr, _lineID, logText);
        if (_lineID < prevLineID) {
          _lineID = prevLineID + 1;
          if (Verbose1) Serial.printf("init(): -> recNr[%2d] has highest _lineID[%8d]\r\n"
                                                      , recNr
                                                      , _lineID);
          return true; 
        }
        else if (recNr >= _noLines) {
          //_lineID++;
          if (Verbose1) Serial.printf("init(): -> recNr[%2d] is EOF _lineID[%8d]\r\n"
                                                      , recNr
                                                      , _lineID);
          return true; 
        }
        prevLineID = _lineID;
        //_nextFree++;
  } // while ..

  return false;

} // init()


//-------------------------------------------------------------------------------------
boolean ESPSL::write(const char* logLine) 
{

} // write()


//-------------------------------------------------------------------------------------
boolean ESPSL::writef(const char *fmt, ...) 
{
  if (Verbose1) Serial.println("writef()..");

  int16_t bytesWritten;
  uint8_t seekToLine;
  char buffer[255];
  cRec[0] = '\0';  
  
  // --- check if the file exists and can be opened ---
  File logFile  = SPIFFS.open(_sysLogFile, "r+");    // open for reading and writing
  if (!logFile) {
    Serial.printf("writef(): Some error opening [%s] .. bailing out!\r\n", _sysLogFile);
    logFile.close();
    return false;
  } // if (!logFile)

  //char buffer[256];
  va_list args;
  va_start (args, fmt);
  vsnprintf (buffer, (_recLen + 10), fmt, args);
  va_end (args);

  sprintf(cRec, "%8d;%s ", _lineID, buffer);
  fixRecLen(cRec, _recLen);
  seekToLine = ((_lineID % _noLines) +1);
  if (!logFile.seek((seekToLine * _recLen), SeekSet)) {
    Serial.printf("writef(): seek to position [%d] failed\r\n", seekToLine);
  }
  bytesWritten = logFile.print(cRec);
  if (Verbose1) Serial.printf("writef() -> seek[%d] [%s]\r\n", seekToLine, cRec);
  if (bytesWritten != _recLen) {
      Serial.printf("writef(): ERROR!! written [%d] bytes but should have been\r\n"
                                       , bytesWritten, _recLen);
      logFile.close();
      return false;
  }

  _lineID++;
  int nextFree = (_lineID % _noLines) + 1;
  if (Verbose1) Serial.printf("writef(): nextFree is [%3d]\r\n", nextFree);
  
  logFile.close();

} // writef()


//-------------------------------------------------------------------------------------
// set pointer to startLine
boolean ESPSL::startReading(uint8_t startLine, uint8_t numLines) 
{
  Serial.printf("startReading([%d], [%d])..\r\n", startLine, numLines);
  //if (numLines == 0 ) numLines = _noLines;
  /*
  if (numLines > (_noLines - startLine)) {
    numLines = _noLines - startLine;
  }
  Serial.printf("startReading: startLine[%d], numLines[%d] ..\r\n", startLine, numLines);
  */
  if (startLine == 0) {
    _readEnd      = _lineID - 1;
    if (numLines == 0)
          _readPointer  = _lineID - _noLines;
    else  _readPointer  = _lineID - numLines;
  }
  else {
    if (numLines == 0) {
      _readEnd      = _lineID - 1;
      _readPointer  = (_lineID - startLine);
    }
    else {
      _readEnd      = (_lineID - startLine) -1;
      _readPointer  = (_lineID - startLine) - numLines;
    }
  }
  Serial.printf("startReading(): _lineID[%d], Pointer[%d], End[%d]\r\n", _lineID, _readPointer, _readEnd);

} // startReading


//-------------------------------------------------------------------------------------
// start reading from startLine
String ESPSL::readNextLine()
{
  if (Verbose1) Serial.printf("readNextLine(%d)..", _lineID);
  int16_t recNr = _readPointer;
  uint8_t seekToLine;
  char buffer[_recLen + 2];
  char logText[_recLen];

  if (_readPointer > _readEnd) return("EOF");
  // --- check if the file exists and can be opened ---
  File logFile  = SPIFFS.open(_sysLogFile, "r");    // open for reading
  if (!logFile) {
    Serial.printf("readNextLine(): Some error opening [%s] .. bailing out!\r\n", _sysLogFile);
    return "EOF";
  } // if (!logFile)
  int16_t fileSize = sysLogFileSize();
  if (fileSize < ((_noLines + 2) * _recLen)) {
    Serial.printf("readNextLine(): sysLog file size [%d] .. error!\r\n", fileSize);
  }

  //for (recNr = 0; recNr < _noLines; recNr++) {
    seekToLine = (_readPointer % _noLines) +1;
    if (!logFile.seek((seekToLine * _recLen), SeekSet)) {
      Serial.printf("readNextLine(): seek to position [%d] failed\r\n", seekToLine);
    }

    uint32_t  lineID;
    int l = logFile.readBytesUntil('\n', buffer, sizeof(buffer));
    buffer[l] = '\0';
    //Serial.printf("init(): [%d] -> [%s]\r\n", l, buffer);
    sscanf(buffer,"%d;%[^\0]", 
            &lineID,
            logText);

    if (Verbose1) Serial.printf("readNextLine(): [%2d]->recNr[%2d][%8d]-> [%s]\r\n", seekToLine, recNr, lineID, logText);
    else {
      _readPointer++;
      logFile.close();
      return String(buffer);
      return String(logText);
    }

  //} // for ..

  logFile.close();
  return String("EOF");

} //  readNextLine

//-------------------------------------------------------------------------------------
// start reading from startLine
String ESPSL::dumpLogFile() 
{
  if (Verbose1) Serial.println("dumpLogFile()..");

  char    buffer[_recLen + 1];
  char    logText[_recLen];
  int16_t recNr;
      
  // --- check if the file exists and can be opened ---
  File logFile  = SPIFFS.open(_sysLogFile, "r");    // open for reading
  if (!logFile) {
    Serial.printf("dumpLogFile(): Some error opening [%s] .. bailing out!\r\n", _sysLogFile);
    return "Error!";
  } // if (!logFile)
  int16_t fileSize = sysLogFileSize();
  if (fileSize < ((_noLines + 2) * _recLen)) {
    Serial.printf("dumpLogFile(): sysLog file size [%d] .. error!\r\n", fileSize);
  }

  recNr = 0;
  while (logFile.available() > 0) {
    uint32_t  lineID;
    int l = logFile.readBytesUntil('\n', buffer, sizeof(buffer));
    buffer[l] = '\0';
    //Serial.printf("init(): [%d] -> [%s]\r\n", l, buffer);
    sscanf(buffer,"%d;%[^\0]", 
            &lineID,
            logText);

    //Serial.printf("dumpLogFile(): -> [%s]\r\n", xRec.c_str());
    Serial.printf("dumpLogFile(): recNr[%2d][%8d]-> [%s]\r\n", recNr, lineID, logText);
    recNr++;
  } // while ..

  logFile.close();
  return "EOF";

} // dumpLogFile

//-------------------------------------------------------------------------------------
// erase SysLog file from SPIFFS
boolean ESPSL::remove() 
{
  if (Verbose1) Serial.println("remove()..");
  SPIFFS.remove(_sysLogFile);
  return true;
} // erase()

//-------------------------------------------------------------------------------------
// returns SysLog file info
boolean ESPSL::status() 
{
  Serial.printf("status():       _recLen[%3d]\r\n", _recLen);
  Serial.printf("status():      _noLines[%3d]\r\n", _noLines);
  Serial.printf("status(): nextFree Slot[%3d]\r\n", (_lineID % _noLines));
  Serial.printf("status():  _lineID[%8d]\r\n", _lineID);
  return true;
}


//===========================================================================================
void ESPSL::fixRecLen(char *record, int len) 
{
  if (Verbose1) Serial.printf("fixRecLen([%s], %d) ..\r\n", record, len);
  char tmpRec[255];
  tmpRec[0] = '\0';
  
  sprintf(tmpRec, "%s", record);
  record[0] = '\0';
  //Serial.printf("record[%s] fix to [%d] bytes\r\n", tmpRec, len);
  int8_t s = 0, l = 0;
  while (tmpRec[s] != '\0' && tmpRec[s]  != '\n' && s < (_recLen -1)) {s++;}
  //Serial.printf("Length of record is [%d] bytes\r\n", s);
  for (l = s; l < (len - 1); l++) {
    tmpRec[l] = ' ';
  }
  tmpRec[l]   = '\n';
  tmpRec[len] = '\0';

  while (tmpRec[l] != '\0') {l++;}
  //strncat(record, tmpRec, len);
  strcat(record, tmpRec);
  if (Verbose1) Serial.printf("Length of record is now [%d] bytes\r\n", l);
  
} // fixRecLen()

//===========================================================================================
int16_t  ESPSL::sysLogFileSize()
{
#if defined(ESP8266)
  {
    Dir dir = SPIFFS.openDir("/");         // List files on SPIFFS
    while (dir.next())  {
          String fileName = dir.fileName();
          size_t fileSize = dir.fileSize();
          //Serial.printf("sysLogFileSize(): File: %s, size: %d\n", fileName.c_str(), fileSize);
          if (fileName == _sysLogFile) {
            if (Verbose1) Serial.printf("sysLogFileSize(): fileSize[%d]\r\n", fileSize);
            return((int)fileSize);
          }
    }
  return 0;
  }
#else
  {
      File root = SPIFFS.open("/");
      File file = root.openNextFile();
      while(file){
          String fileName = file.name();
          size_t fileSize = file.size();
          if (Verbose1) Serial.printf("FS File: %s, size: %s\n", fileName.c_str(), formatBytes(fileSize).c_str());
          if (fileName == _sysLogFile) {
            return((int)fileSize);
          }
          file = root.openNextFile();
      }
      return 0;
  }
#endif
} // sysLogFileSize()
