/*
**  Program   : ESP_SysLogger.cpp
**
**  Version   : 1.6.3   (19-03-2020)
**
**  Copyright (c) 2020 Willem Aandewiel
**
**  TERMS OF USE: MIT License. See bottom of file.
***************************************************************************/

#include "ESP_SysLogger.h"

//Constructor
ESPSL::ESPSL() 
{ 
  _Serial   = NULL;
  _serialOn = false;
  _Stream   = NULL;
  _streamOn = false;
}

//-------------------------------------------------------------------------------------
// begin object
boolean ESPSL::begin(uint16_t depth, uint16_t lineWidth) 
{
#ifdef _DODEBUG
  if (_Debug(1)) printf("ESPSL::begin(%d, %d)..\n", depth, lineWidth);
#endif

  int16_t fileSize;

  if (lineWidth > _MAXLINEWIDTH) lineWidth = _MAXLINEWIDTH;
  globalBuff[0]    = '\0';
  
  // --- check if the file exists ---
  if (!SPIFFS.exists(_sysLogFile)) {
    create(depth, lineWidth);
  }
  
  // --- check if the file exists and can be opened ---
  _sysLog  = SPIFFS.open(_sysLogFile, "r+");    // open for reading and writing
  if (!_sysLog) {
    printf("ESPSL::begin(): Some error opening [%s] .. bailing out!\r\n", _sysLogFile);
    _sysLog.close();
    return false;
  } // if (!_sysLog)

  if (_sysLog.available() > 0) {
#ifdef _DODEBUG
    if (_Debug(3)) println("ESPSL::begin(): read record [0]");
#endif
    int l = _sysLog.readBytesUntil('\0', globalBuff, 50); // just for the first record
        globalBuff[l]   = '\0';
#ifdef _DODEBUG
        if (_Debug(4)) printf("ESPSL::begin(): rec[0] [%s]\r\n", globalBuff);
#endif
        sscanf(globalBuff,"%u;%d;%d;" 
                                , &_lastUsedLineID
                                , &_noLines
                                , &_lineWidth);
        Serial.flush();
        if (_noLines    < 10) _noLines    = 10; 
        if (_lineWidth  < 50) _lineWidth  = 50; 
        _recLength = _lineWidth + 9;
#ifdef _DODEBUG
        if (_Debug(4)) printf("ESPSL::begin(): rec[0] -> [%8d][%d][%d]\r\n", _lastUsedLineID, _noLines, _lineWidth);
#endif
  } 
  
  globalBuff[0]   = '\0';
  
  checkSysLogFileSize("begin():", (_noLines + 1) * _recLength);
  
  if (_noLines != depth) {
    printf("ESPSL::begin(): lines in file (%d) <> %d !!\r\n", _noLines, depth);
  }
  if (_lineWidth != lineWidth) {
    printf("ESPSL::begin(): lineWidth in file (%d) <> %d !!\r\n", _lineWidth, lineWidth);
  }

  init();

  return true; // We're all setup!
  
} // begin()

//-------------------------------------------------------------------------------------
// begin object
boolean ESPSL::begin(uint16_t depth, uint16_t lineWidth, boolean mode) 
{
#ifdef _DODEBUG
  if (_Debug(1)) printf("ESPSL::begin(%d, %d, %d)..\n", depth, lineWidth, mode);
#endif

  if (mode) {
    removeSysLog();
    create(depth, lineWidth);
  }
  return (begin(depth, lineWidth));
  
} // begin()
//-------------------------------------------------------------------------------------
// Create a SysLog file on SPIFFS
boolean ESPSL::create(uint16_t depth, uint16_t lineWidth) 
{
#ifdef _DODEBUG
  if (_Debug(1)) printf("ESPSL::create(%d, %d)..\n", depth, lineWidth);
#endif

  int32_t bytesWritten;
  
  _noLines  = depth;
  if (lineWidth > _MAXLINEWIDTH) 
        _lineWidth  = _MAXLINEWIDTH;
  else  _lineWidth  = lineWidth;
  _recLength = _lineWidth + 9;

  //_nextFree = 0;
  globalBuff[0] = '\0';  
  // --- check if the file exists and can be opened ---
  File _logFile  = SPIFFS.open(_sysLogFile, "a");    // open for reading and writing
  if (!_logFile) {
    printf("ESPSL::create(): Some error opening [%s] .. bailing out!\r\n", _sysLogFile);
    _logFile.close();
    return false;
  } // if (!_logFile)


  sprintf(globalBuff, "%08d;%d;%d; META DATA ESP_SysLogger", 0, _noLines, _lineWidth);
  fixLineWidth(globalBuff, _recLength);
  if (_Debug(1)) printf("create(): rec(0) [%s](%d bytes)\r\n", globalBuff, strlen(globalBuff));
  bytesWritten = _logFile.print(globalBuff);
  _logFile.flush();
  if (bytesWritten != _recLength) {
    printf("ESPSL::create(): ERROR!! written [%d] bytes but should have been [%d] for record [0]\r\n"
                                            , bytesWritten, _recLength);

    _logFile.close();
    return false;
  }
  
  int r;
  for (r=0; r < _noLines; r++) {
    yield();
    sprintf(globalBuff, "%08d;%s=== empty log regel (%d) ===" , (r+_noLines)
                                                              , _emptyID
                                                              , r);
    fixLineWidth(globalBuff, _recLength);
    bytesWritten = _logFile.print(globalBuff);
    //printf("[%d] -> [%s]\r\n", 0, _cRec);
    if (bytesWritten != _recLength) {
      printf("ESPSL::create(): ERROR!! written [%d] bytes but should have been [%d] for record [%d]\r\n"
                                            , bytesWritten, _recLength, r);
      _logFile.close();
      return false;
    }
    
  } // for r ....
  
  _logFile.close();
  _lastUsedLineID = _noLines;
  _oldestLineID   = 1;
  return (true);
  
} // create()

//-------------------------------------------------------------------------------------
// read SysLog file and find next line to write to
boolean ESPSL::init() 
{
#ifdef _DODEBUG
  if (_Debug(1)) println("ESPSL::init()..");
#endif

  char *logText = (char*)malloc( sizeof(char) * (_lineWidth + 15) );
  if (logText == NULL) 
  {
    println("init(): malloc(logText) error!");
    Serial.flush();
    return false;
  }
  logText[0]    = '\0';

  int16_t recNr = 0;
      
  // --- check if the file exists and can be opened ---
  File initFile  = SPIFFS.open(_sysLogFile, "r");    // open for reading and writing
  if (!initFile) {
    printf("ESPSL::init(): Some error opening [%s] .. bailing out!\r\n", _sysLogFile);
    initFile.close();
    free(logText);
    return false;
  } // if (!initFile)

  _oldestLineID   = 0;
  _lastUsedLineID = 0;
  recNr           = 0;
  while (initFile.available() > 0) {
    recNr++;
    if (!initFile.seek((recNr) * _recLength, SeekSet)) {
      printf("ESPSL::init(): seek to position [%d] failed\r\n", recNr);
    }
#ifdef _DODEBUG
    if (_Debug(4)) printf("ESPSL::init(): -> read record (recNr) [%d]\r\n", recNr);
#endif
    int l = initFile.readBytesUntil('\0', globalBuff, _recLength);
        globalBuff[l] = '\0';
        sscanf(globalBuff,"%u;%[^\0]", 
            &_oldestLineID,
            logText);
#ifdef _DODEBUG
        if (_Debug(4)) printf("ESPSL::init(): testing recNr[%2d] -> [%08d][%s]\r\n", recNr
                                                                                  , _oldestLineID
                                                                                  , logText);
#endif
        if (_oldestLineID < _lastUsedLineID) {
#ifdef _DODEBUG
          if (_Debug(4)) printf("ESPSL::init(): ----> recNr[%2d] _oldest[%8d] _lastUsed[%8d]\r\n"
                                                      , recNr
                                                      , _oldestLineID
                                                      , _lastUsedLineID);
#endif
          free(logText);
          return true; 
        }
        else if (recNr >= _noLines) {
          _lastUsedLineID++;
          _oldestLineID = (_lastUsedLineID - _noLines) +1;
#ifdef _DODEBUG
          if (_Debug(3)) printf("ESPSL::init(eof): -> recNr[%2d] _oldest[%8d] _lastUsed[%8d]\r\n"
                                                      , recNr
                                                      , _oldestLineID
                                                      , _lastUsedLineID);
#endif
          free(logText);
          return true; 
        }
        _lastUsedLineID = _oldestLineID;
        yield();
  } // while ..

  free(logText);
  return false;

} // init()


//-------------------------------------------------------------------------------------
boolean ESPSL::write(const char* logLine) 
{
#ifdef _DODEBUG
  if (_Debug(3)) printf("ESPSL::write(%s)..\r\n", logLine);
#endif

  int32_t   bytesWritten;
  uint16_t  seekToLine;
  int       nextFree;

  // --- check if the file is opened ---
  if (!_sysLog) {
    printf("ESPSL::write(): Some error [%s] seems not to be open .. bailing out!\r\n", _sysLogFile);
    return false;
  } // if (!_sysLog)

  nextFree = (_lastUsedLineID % _noLines) + 1;
#ifdef _DODEBUG
  if (_Debug(4)) printf("ESPSL::write(s): oldest[%8d], last[%8d], nextFree Slot[%3d]\r\n"
                                                      , _oldestLineID
                                                      , _lastUsedLineID
                                                      , nextFree);
#endif
  
  char *lineBuf = (char*)malloc( sizeof(char) * (_recLength + 10) );
  if (lineBuf == NULL) 
  {
    println("write(): malloc(lineBuf) error!");
    Serial.flush();
    return false;
  }
  lineBuf[0]    = '\0';
  globalBuff[0] = '\0';
  strncat(globalBuff, logLine, _lineWidth);
  sprintf(lineBuf, "%8d;%s ", (_lastUsedLineID +1), globalBuff);
  fixLineWidth(lineBuf, _recLength); 
  seekToLine = (_oldestLineID % _noLines) +1; // always skip rec. 0 (status rec)
#ifdef _DODEBUG
  if (_Debug(4)) printf("ESPSL::write() -> seek[%d] [%s]\r\n", seekToLine, lineBuf);
#endif
  if (!_sysLog.seek((seekToLine * _recLength), SeekSet)) {
    printf("ESPSL::write(): seek to position [%d] failed\r\n", seekToLine);
  }
  bytesWritten = _sysLog.print(lineBuf);
  _sysLog.flush();
  if (bytesWritten != _recLength) {
      printf("ESPSL::write(): ERROR!! written [%d] bytes but should have been [%d]\r\n"
                                       , bytesWritten, _lineWidth);
      free(lineBuf);
      return false;
  }

  _lastUsedLineID++;
  _oldestLineID++;
  nextFree = (_lastUsedLineID % _noLines) + 1;  // always skip rec "0"
#ifdef _DODEBUG
  if (_Debug(4)) printf("ESPSL::write(e): oldest[%8d], last[%8d], nextFree Slot[%3d]\r\n"
                                                      , _oldestLineID
                                                      , _lastUsedLineID
                                                      , nextFree);
#endif

  free(lineBuf);

} // write()


//-------------------------------------------------------------------------------------
boolean ESPSL::writef(const char *fmt, ...) 
{
#ifdef _DODEBUG
  if (_Debug(3)) printf("ESPSL::writef(%s)..\r\n", fmt);
#endif

  int32_t   bytesWritten;
  uint16_t  seekToLine;
  int       nextFree;
  
  // --- check if the file exists is open ---
  if (!_sysLog) {
    return false;
  } // if (!_sysLog)

  nextFree = (_lastUsedLineID % _noLines) + 1;
#ifdef _DODEBUG
  if (_Debug(4)) printf("ESPSL::writef(s): oldest[%8d], last[%8d], nextFree Slot[%3d]\r\n"
                                                      , _oldestLineID
                                                      , _lastUsedLineID
                                                      , nextFree);
#endif

  char *lineBuf = (char*)malloc( sizeof(char) * (_MAXLINEWIDTH + 10) );
  if (lineBuf == NULL) 
  {
    println("writef(): malloc(lineBuf) error!");
    Serial.flush();
    return false;
  }
  lineBuf[0]    = '\0';

  va_list args;
  va_start (args, fmt);
  vsnprintf (lineBuf, (_MAXLINEWIDTH), fmt, args);
  va_end (args);

  bool retVal = write(lineBuf);
  free(lineBuf);

  return retVal;

} // writef()


//-------------------------------------------------------------------------------------
boolean ESPSL::writeDbg(const char *dbg, const char *fmt, ...) 
{
#ifdef _DODEBUG
  if (_Debug(3)) printf("ESPSL::writeDbg(%s, %s)..\r\n", dbg, fmt);
#endif

  int       nextFree;
  char *dbgStr = (char*)malloc( sizeof(char) * (_lineWidth + 5) );
  if (dbgStr == NULL) 
  {
    println("writeDbg(): malloc(dbgStr) error!");
    Serial.flush();
    return false;
  }
  char *totBuf = (char*)malloc( sizeof(char) * (_lineWidth + 5) );
  if (totBuf == NULL) 
  {
    println("writeDbg(): malloc(totBuf) error!");
    Serial.flush();
    free(dbgStr);
    return false;
  }
  dbgStr[0]    = '\0';
  totBuf[0]    = '\0';

  sprintf(dbgStr, "%s", dbg);

  va_list args;
  va_start (args, fmt);
  vsnprintf (totBuf, (_MAXLINEWIDTH), fmt, args);
  va_end (args);
  
  while ((strlen(totBuf) > 0) && (strlen(dbgStr) + strlen(totBuf)) > (_lineWidth -1)) {
    totBuf[strlen(totBuf)-1] = '\0';
    yield();
  }
  if ((strlen(dbgStr) + strlen(totBuf)) < _lineWidth) {
    strncat(dbgStr, totBuf, _lineWidth);
  }

  free(totBuf);
  bool retVal = write(dbgStr);
  free(dbgStr);
  
  return retVal;

} // writeDbg()


//-------------------------------------------------------------------------------------
char *ESPSL::buildD(const char *fmt, ...) 
{
#ifdef _DODEBUG
  if (_Debug(3)) printf("ESPSL::buildD(%s)..\r\n", fmt);
#endif
  globalBuff[0] = '\0';
  
  va_list args;
  va_start (args, fmt);
  vsnprintf (globalBuff, (_MAXLINEWIDTH), fmt, args);
  va_end (args);

  return globalBuff;
  
} // buildD()


//-------------------------------------------------------------------------------------
// set pointer to startLine
boolean ESPSL::startReading(int16_t startLine, int16_t numLines) 
{
#ifdef _DODEBUG
  if (_Debug(1)) printf("ESPSL::startReading([%d], [%d])..\r\n", startLine, numLines);
#endif

  if (startLine < 0) {
    _readPointer     = _lastUsedLineID + startLine;
  }
  else if (startLine == 0) 
        _readPointer = _oldestLineID;
  else  _readPointer = _oldestLineID + startLine;
  if (_readPointer > _lastUsedLineID) _readPointer = _oldestLineID;
  if (numLines == 0)
        _readEnd     = _lastUsedLineID;
  else  _readEnd     = (_readPointer + numLines) -1;
  if (_readEnd > _lastUsedLineID) _readEnd = _lastUsedLineID; 
#ifdef _DODEBUG
  if (_Debug(3)) printf("ESPSL::startReading(): _oldest[%d], _last[%d], Start[%d], End[%d]\r\n"
                                              , _oldestLineID
                                              , _lastUsedLineID
                                              , _readPointer, _readEnd);
#endif

} // startReading()


//-------------------------------------------------------------------------------------
// set pointer to startLine
boolean ESPSL::startReading(int16_t startLine) 
{
#ifdef _DODEBUG
  if (_Debug(1)) printf("ESPSL::startReading([%d])..\r\n", startLine);
#endif
  startReading(startLine, 0);

} // startReading()

//-------------------------------------------------------------------------------------
// start reading from startLine
String ESPSL::readNextLine()
{
#ifdef _DODEBUG
  if (_Debug(1)) printf("ESPSL::readNextLine(%d)\r\n", _readPointer);
#endif
  globalBuff[0] = '\0';

  int32_t recNr = _readPointer;
  uint16_t seekToLine;

  if (_readPointer > _readEnd) return("EOF");
  // --- check if the file is open ---
  if (!_sysLog) {
    return "EOF";
  } // if (!_sysLog)

  for (recNr = _readPointer; recNr <= _readEnd; recNr++) {
    seekToLine = (_readPointer % _noLines) +1;
    if (!_sysLog.seek((seekToLine * _recLength), SeekSet)) {
      printf("ESPSL::readNextLine(): seek to position [%d] failed\r\n", seekToLine);
    }

    uint32_t  lineID;
    int l = _sysLog.readBytesUntil('\0', globalBuff, _recLength);
    globalBuff[l] = '\0';
    //printf("readNext(): rec[%s] (%d bytes)\r\n", globalBuff, strlen(globalBuff));
    sscanf(globalBuff,"%u;%[^\0]", 
            &lineID,
            globalBuff);

#ifdef _DODEBUG
    if (_Debug(4)) printf("ESPSL::readNextLine(a): [%5d]->recNr[%5d][%10d]-> [%s]\r\n"
                                                            , seekToLine
                                                            , recNr
                                                            , lineID
                                                            , globalBuff);
#endif
    _readPointer++;
    if (String(globalBuff).indexOf(String(_emptyID)) == -1) {
      return String(rtrim(globalBuff));
      //if (_Debug(5)) return String(rtrim(globalBuff));
      //else           return String(rtrim(globalBuff));
#ifdef _DODEBUG
    } else {
      if (_Debug(5)) printf("ESPSL::readNextLine(b): SKIP[%s]\r\n", globalBuff);
#endif
    }
    
  } // for ..

  return String("EOF");

} //  readNextLine()

//-------------------------------------------------------------------------------------
// start reading from startLine
String ESPSL::dumpLogFile() 
{
#ifdef _DODEBUG
  if (_Debug(1)) println("ESPSL::dumpLogFile()..");
#endif

  int16_t       recNr;
  uint16_t      seekToLine;
  globalBuff[0] = '\0';
      
  // --- check if the file is open ---
  if (!_sysLog) {
    return "Error!";
  } // if (!_sysLog)

  checkSysLogFileSize("dumpLogFile():", (_noLines + 1) * _recLength);

  recNr = 0;
  for (recNr = 0; recNr < _noLines; recNr++) {
    seekToLine = (recNr % _noLines)+1;
    if (!_sysLog.seek((seekToLine * _recLength), SeekSet)) {
      printf("ESPSL::dumpLogFile(): seek to position [%d] failed\r\n", seekToLine);
    }
    uint32_t  lineID;
    int l = _sysLog.readBytesUntil('\0', globalBuff, _recLength);
    globalBuff[l] = '\0';
    if (_Debug(5)) printf("ESPSL::dumpLogFile(-):  >>>>> [%d] -> [%s]\r\n", l, globalBuff);
    sscanf(globalBuff,"%u;%[^\0]", 
            &lineID,
            globalBuff);

    if (lineID == _lastUsedLineID) {
            printf("dumpLogFile::(L): recNr[%4d]ID[%8d]->[%s]\r\n", seekToLine, lineID, globalBuff);

            println(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>");
    }
    else if (lineID == _oldestLineID && recNr < _noLines)
            printf("dumpLogFile::(O): recNr[%4d]ID[%8d]->[%s]\r\n", seekToLine, lineID, globalBuff);
    else if (String(globalBuff).indexOf(String(_emptyID)) == -1) 
            printf("dumpLogFile::( ): recNr[%4d]ID[%8d]->[%s]\r\n", seekToLine, lineID, globalBuff);

  } // while ..

  return "EOF";

} // dumpLogFile

//-------------------------------------------------------------------------------------
// erase SysLog file from SPIFFS
boolean ESPSL::removeSysLog() 
{
#ifdef _DODEBUG
  if (_Debug(1)) println("ESPSL::removeSysLog()..");
#endif
  SPIFFS.remove(_sysLogFile);
  return true;
  
} // removeSysLog()

//-------------------------------------------------------------------------------------
// returns ESPSL status info
void ESPSL::status() 
{
  printf("ESPSL::status():        _noLines[%8d]\r\n", _noLines);
  printf("ESPSL::status():      _lineWidth[%8d]\r\n", _lineWidth);
  if (_noLines > 0) {
    printf("ESPSL::status():   _oldestLineID[%8d] (%2d)\r\n", _oldestLineID
                                                           , (_oldestLineID % _noLines)+1);
    printf("ESPSL::status(): _lastUsedLineID[%8d] (%2d)\r\n", _lastUsedLineID
                                                           , (_lastUsedLineID % _noLines)+1);
  }
  printf("ESPSL::status():       _debugLvl[%8d]\r\n", _debugLvl);
  
} // status()
//-------------------------------------------------------------------------------------
void ESPSL::setOutput(HardwareSerial *serIn, int baud)
{
  _Serial = serIn;
  _Serial->begin(baud);
  _Serial->println("Serial Output Ready..");
  _serialOn = true;
  
} // setOutput(hw, baud)

//-------------------------------------------------------------------------------------
void ESPSL::setOutput(Stream *serIn)
{
  _Stream = serIn;
  _Stream->println("Stream Output Ready..");
  _streamOn = true;
  
} // setOutput(Stream)

//-------------------------------------------------------------------------------------
void ESPSL::print(const char *line)
{
  if (_streamOn)  _Stream->print(line);
  if (_serialOn)  _Serial->print(line);
  
} // print()

//-------------------------------------------------------------------------------------
void ESPSL::println(const char *line)
{
  if (_streamOn)  _Stream->println(line);
  if (_serialOn)  _Serial->println(line);
  
} // println()

//-------------------------------------------------------------------------------------
void ESPSL::printf(const char *fmt, ...)
{
  char *lineBuff = (char*)malloc( sizeof(char) * (_MAXLINEWIDTH + 51) );
  if (lineBuff == NULL) 
  {
    println("printf(): malloc(lineBufff) error!");
    flush();
    return;
  }
  lineBuff[0]    = '\0';

  va_list args;
  va_start (args, fmt);
  vsnprintf(lineBuff, (_MAXLINEWIDTH +50), fmt, args);
  va_end (args);

  if (_streamOn)  _Stream->print(lineBuff);
  if (_serialOn)  _Serial->print(lineBuff);
  free(lineBuff);

} // printf()

//-------------------------------------------------------------------------------------
void ESPSL::flush()
{
  if (_streamOn)  _Stream->flush();
  if (_serialOn)  _Serial->flush();
  
} // flush()
  
//-------------------------------------------------------------------------------------
// check fileSize of _sysLogFile
boolean ESPSL::checkSysLogFileSize(const char* func, int32_t cSize)
{
#ifdef _DODEBUG
  if (_Debug(4)) printf("ESPSL::checkSysLogFileSize(%d)..\r\n", cSize);
#endif
  int32_t fileSize = sysLogFileSize();
  if (fileSize != cSize) {
    printf("ESPSL::%s -> [%s] size is [%d] but should be [%d] .. error!\r\n"
                                                          , func
                                                          , _sysLogFile
                                                          , fileSize, cSize);
    return false;
  }
  return true;

} // checkSysLogSize()


//-------------------------------------------------------------------------------------
// returns lastUsed LineID
uint32_t ESPSL::getLastLineID()
{
  return _lastUsedLineID;
  
} // getLastLineID()

//-------------------------------------------------------------------------------------
// set Debug Level
void ESPSL::setDebugLvl(int8_t debugLvl)
{
  if (debugLvl >= 0 && debugLvl <= 9)
        _debugLvl = debugLvl;
  else  _debugLvl = 1;
  
} // setDebugLvl

//-------------------------------------------------------------------------------------
// returns debugLvl
int8_t ESPSL::getDebugLvl()
{
  return _debugLvl;
  
} // getDebugLvl


//-------------------------------------------------------------------------------------
// returns debugLvl
boolean ESPSL::_Debug(int8_t Lvl)
{
  if (getDebugLvl() > 0 && Lvl <= getDebugLvl()) 
        return true;
  else  return false;
  
} // _Debug()


//===========================================================================================
const char* ESPSL::rtrim(char *aChr)
{
  for(int p=strlen(aChr); p>0; p--)
  {
    if (aChr[p] < '!' || aChr[p] > '~') 
    {
      aChr[p] = '\0';
    }
    else
      break;
  }
  return aChr;
} // rtrim()

//===========================================================================================
void ESPSL::fixLineWidth(char *inLine, int len) 
{
#ifdef _DODEBUG
  if (_Debug(4)) printf("ESPSL::fixLineWidth([%s], %d) ..\r\n", inLine, len);
#endif
  char *fixLine = (char*)malloc( sizeof(char) * (len + 10) );
  if (fixLine == NULL) 
  {
    println("fixLineWidth(): malloc(fixLine) error!");
    Serial.flush();
    return;
  }
  fixLine[0]    = '\0';

  strncat(fixLine, inLine, len);
  // remove all non printable chars ...
  for (int p=0; p < (strlen(fixLine) -1); p++)  // skip last '\n' and '\0'
  {
    if (fixLine[p] < ' ' || fixLine[p] > '~') 
    {
      fixLine[p] = '*';
      yield();
    }
  }
  fixLine[len] = '\0';
  
  int16_t s = 0, l = 0;
  while (fixLine[s] != '\0' && fixLine[s] != '\n' && s < (len -1)) {yield(); s++;}
  for (l = s; l < len; l++) 
  {
    yield();
    fixLine[l] = ' ';
  }
  fixLine[len] = '\0';

  inLine[0] = '\0';
  strncat(inLine, fixLine, len);
#ifdef _DODEBUG
  if (_Debug(4)) printf("ESPSL::fixLineWidth(): Length of inLine is [%d] bytes\r\n", strlen(inLine));
#endif

  free(fixLine);
  
} // fixLineWidth()

//===========================================================================================
int32_t  ESPSL::sysLogFileSize()
{
#if defined(ESP8266)
  {
    Dir dir = SPIFFS.openDir("/");         // List files on SPIFFS
    while (dir.next())  {
          yield();
          String fileName = dir.fileName();
          size_t fileSize = dir.fileSize();
#ifdef _DODEBUG
          if (_Debug(6)) printf("ESPSL::sysLogFileSize(): found: %s, size: %d\n"
                                                                             , fileName.c_str()
                                                                             , fileSize);
#endif
          if (fileName == _sysLogFile) {
#ifdef _DODEBUG
            if (_Debug(6)) printf("ESPSL::sysLogFileSize(): fileSize[%d]\r\n", fileSize);
#endif
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
#ifdef _DODEBUG
          if (_Debug(6)) printf("ESPSL::FS Found: %s, size: %d\n", fileName.c_str(), fileSize);
#endif
          if (fileName == _sysLogFile) {
#ifdef _DODEBUG
            if (_Debug(6)) printf("ESPSL::sysLogFileSize(): fileSize[%d]\r\n", fileSize);
#endif
            return((int)fileSize);
          }
          file = root.openNextFile();
      }
      return 0;
  }
#endif
} // sysLogFileSize()


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
