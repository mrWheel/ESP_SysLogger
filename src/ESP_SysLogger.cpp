/*
**  Program   : ESP_SysLogger.cpp
**
**  Copyright (c) 2019 Willem Aandewiel
**
**  TERMS OF USE: MIT License. See bottom of file.
***************************************************************************/

#include "ESP_SysLogger.h"

//Constructor
ESPSL::ESPSL() { }

//-------------------------------------------------------------------------------------
// begin object
boolean ESPSL::begin(uint16_t depth, uint16_t lineWidth) 
{
  if (_Debug(1)) Serial.printf("ESPSL::begin(%d, %d)..\n", depth, lineWidth);

  char lineBuf[_MAXLINEWIDTH + 10];
  int16_t fileSize;

  if (lineWidth > _MAXLINEWIDTH) lineWidth = _MAXLINEWIDTH;
  
  // --- check if the file exists ---
  if (!SPIFFS.exists(_sysLogFile)) {
    create(depth, lineWidth);
  }
  
  // --- check if the file exists and can be opened ---
  File _logFile  = SPIFFS.open(_sysLogFile, "r+");    // open for reading and writing
  if (!_logFile) {
    Serial.printf("ESPSL::begin(): Some error opening [%s] .. bailing out!\r\n", _sysLogFile);
    _logFile.close();
    return false;
  } // if (!_logFile)

  if (_logFile.available() > 0) {
    if (_Debug(3)) Serial.println("ESPSL::begin(): read record [0]");
    int l = _logFile.readBytesUntil('\n', lineBuf, sizeof(lineBuf));
        lineBuf[l]   = '\0';
        if (_Debug(4)) Serial.printf("ESPSL::begin(): rec[0] [%s]\r\n", lineBuf);
        sscanf(lineBuf,"%u;%d;%d;" 
                                , &_lastUsedLineID
                                , &_noLines
                                , &_lineWidth);
        if (_Debug(4)) Serial.printf("ESPSL::begin(): rec[0] -> [%8d][%d][%d]\r\n", _lastUsedLineID, _noLines, _lineWidth);
        Serial.flush();
        if (_noLines    < 10) _noLines = 10; 
        if (_lineWidth  < 50) _lineWidth  = 50; 
  } 

  checkSysLogFileSize("begin():", (_noLines + 1) * _lineWidth);
  
  if (_noLines != depth) {
    Serial.printf("ESPSL::begin(): lines in file (%d) <> %d !!\r\n", _noLines, depth);
  }
  if (_lineWidth != lineWidth) {
    Serial.printf("ESPSL::begin(): lineWidth in file (%d) <> %d !!\r\n", _lineWidth, lineWidth);
  }

  _logFile.close();
  init();

  return true; //We're all setup!
  
} // begin()

//-------------------------------------------------------------------------------------
// begin object
boolean ESPSL::begin(uint16_t depth, uint16_t lineWidth, boolean mode) 
{
  if (_Debug(1)) Serial.printf("ESPSL::begin(%d, %d, %d)..\n", depth, lineWidth, mode);
  
  if (lineWidth > _MAXLINEWIDTH) lineWidth = _MAXLINEWIDTH;

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
  if (_Debug(1)) Serial.printf("ESPSL::create(%d, %d)..\n", depth, lineWidth);

  int32_t bytesWritten;
  
  _noLines  = depth;
  if (lineWidth > _MAXLINEWIDTH) 
        _lineWidth  = _MAXLINEWIDTH;
  else  _lineWidth  = lineWidth;

  //_nextFree = 0;
  _cRec[0] = '\0';  
  // --- check if the file exists and can be opened ---
  File _logFile  = SPIFFS.open(_sysLogFile, "a");    // open for reading and writing
  if (!_logFile) {
    Serial.printf("ESPSL::create(): Some error opening [%s] .. bailing out!\r\n", _sysLogFile);
    _logFile.close();
    return false;
  } // if (!_logFile)


  sprintf(_cRec, "%08d;%d;%d; ", 0, _noLines, _lineWidth);
  fixLineWidth(_cRec, _lineWidth);
  bytesWritten = _logFile.print(_cRec);
  if (bytesWritten != _lineWidth) {
    Serial.printf("ESPSL::create(): ERROR!! written [%d] bytes but should have been [%d] for record [0]\r\n"
                                            , bytesWritten, _lineWidth);

    _logFile.close();
    return false;
  }
  
  int r;
  for (r=0; r < _noLines; r++) {
    yield();
    sprintf(_cRec, "%08d;%s=== empty log regel (%d) =================================="
                                                              , (r+_noLines)
                                                              , _emptyID
                                                              , r);
    fixLineWidth(_cRec, _lineWidth);
    bytesWritten = _logFile.print(_cRec);
    //Serial.printf("[%d] -> [%s]\r\n", 0, _cRec);
    if (bytesWritten != _lineWidth) {
      Serial.printf("ESPSL::create(): ERROR!! written [%d] bytes but should have been [%d] for record [%d]\r\n"
                                            , bytesWritten, _lineWidth, r);
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
  if (_Debug(1)) Serial.println("ESPSL::init()..");

  char logText[_MAXLINEWIDTH +20];
  char lineBuf[_MAXLINEWIDTH +10];
  int16_t recNr = 0;
  //_nextFree = 0;
      
  // --- check if the file exists and can be opened ---
  File _logFile  = SPIFFS.open(_sysLogFile, "r");    // open for reading and writing
  if (!_logFile) {
    Serial.printf("ESPSL::init(): Some error opening [%s] .. bailing out!\r\n", _sysLogFile);
    _logFile.close();
    return false;
  } // if (!_logFile)

  _oldestLineID   = 0;
  _lastUsedLineID = 0;
  recNr           = 0;
  while (_logFile.available() > 0) {
    recNr++;
    if (!_logFile.seek((recNr) * _lineWidth, SeekSet)) {
      Serial.printf("ESPSL::init(): seek to position [%d] failed\r\n", recNr);
    }
    if (_Debug(4)) Serial.printf("ESPSL::init(): -> read record (recNr) [%d]\r\n", recNr);
    int l = _logFile.readBytesUntil('\n', lineBuf, _lineWidth);
        lineBuf[l] = '\0';
        sscanf(lineBuf,"%u;%[^\0]", 
            &_oldestLineID,
            logText);
        if (_Debug(4)) Serial.printf("ESPSL::init(): testing recNr[%2d] -> [%08d][%s]\r\n", recNr
                                                                                  , _oldestLineID
                                                                                  , logText);
        if (_oldestLineID < _lastUsedLineID) {
          if (_Debug(4)) Serial.printf("ESPSL::init(): ----> recNr[%2d] _oldest[%8d] _lastUsed[%8d]\r\n"
                                                      , recNr
                                                      , _oldestLineID
                                                      , _lastUsedLineID);
          return true; 
        }
        else if (recNr >= _noLines) {
          _lastUsedLineID++;
          _oldestLineID = (_lastUsedLineID - _noLines) +1;
          if (_Debug(3)) Serial.printf("ESPSL::init(eof): -> recNr[%2d] _oldest[%8d] _lastUsed[%8d]\r\n"
                                                      , recNr
                                                      , _oldestLineID
                                                      , _lastUsedLineID);
          return true; 
        }
        _lastUsedLineID = _oldestLineID;
        yield();
  } // while ..

  return false;

} // init()


//-------------------------------------------------------------------------------------
boolean ESPSL::write(const char* logLine) 
{
  if (_Debug(3)) Serial.printf("ESPSL::write(%s)..\r\n", logLine);

  int32_t 	bytesWritten;
  uint16_t 	seekToLine;
  int nextFree;
  _cRec[0] = '\0';  
  
  // --- check if the file exists and can be opened ---
  File _logFile  = SPIFFS.open(_sysLogFile, "r+");    // open for reading and writing
  if (!_logFile) {
    Serial.printf("ESPSL::write(): Some error opening [%s] .. bailing out!\r\n", _sysLogFile);
    _logFile.close();
    return false;
  } // if (!_logFile)

  nextFree = (_lastUsedLineID % _noLines) + 1;
  if (_Debug(4)) Serial.printf("ESPSL::write(s): oldest[%8d], last[%8d], nextFree Slot[%3d]\r\n"
                                                      , _oldestLineID
                                                      , _lastUsedLineID
                                                      , nextFree);

  sprintf(_cRec, "%8d;%s ", (_lastUsedLineID +1), logLine);
  fixLineWidth(_cRec, _lineWidth);
  seekToLine = (_oldestLineID % _noLines) +1; // always skip rec. 0 (status rec)
  if (_Debug(4)) Serial.printf("ESPSL::write() -> seek[%d] [%s]\r\n", seekToLine, _cRec);
  if (!_logFile.seek((seekToLine * _lineWidth), SeekSet)) {
    Serial.printf("ESPSL::write(): seek to position [%d] failed\r\n", seekToLine);
  }
  bytesWritten = _logFile.print(_cRec);
  if (bytesWritten != _lineWidth) {
      Serial.printf("ESPSL::write(): ERROR!! written [%d] bytes but should have been [%d]\r\n"
                                       , bytesWritten, _lineWidth);
      _logFile.close();
      return false;
  }

  _lastUsedLineID++;
  _oldestLineID++;
  nextFree = (_lastUsedLineID % _noLines) + 1;  // always skip rec "0"
  if (_Debug(4)) Serial.printf("ESPSL::write(e): oldest[%8d], last[%8d], nextFree Slot[%3d]\r\n"
                                                      , _oldestLineID
                                                      , _lastUsedLineID
                                                      , nextFree);
  
  _logFile.close();

} // write()


//-------------------------------------------------------------------------------------
boolean ESPSL::writef(const char *fmt, ...) 
{
  if (_Debug(3)) Serial.printf("ESPSL::writef(%s)..\r\n", fmt);

  int32_t 	bytesWritten;
  uint16_t 	seekToLine;
  int nextFree;
  char lineBuf[_MAXLINEWIDTH + 10];
  _cRec[0] = '\0';  
  
  // --- check if the file exists and can be opened ---
  File _logFile  = SPIFFS.open(_sysLogFile, "r+");    // open for reading and writing
  if (!_logFile) {
    Serial.printf("ESPSL::writef(): Some error opening [%s] .. bailing out!\r\n", _sysLogFile);
    _logFile.close();
    return false;
  } // if (!_logFile)

  nextFree = (_lastUsedLineID % _noLines) + 1;
  if (_Debug(4)) Serial.printf("ESPSL::writef(s): oldest[%8d], last[%8d], nextFree Slot[%3d]\r\n"
                                                      , _oldestLineID
                                                      , _lastUsedLineID
                                                      , nextFree);

  va_list args;
  va_start (args, fmt);
  vsnprintf (lineBuf, (_MAXLINEWIDTH), fmt, args);
  va_end (args);

  sprintf(_cRec, "%8d;%s ", (_lastUsedLineID +1), lineBuf);
  fixLineWidth(_cRec, _lineWidth);
  seekToLine = (_oldestLineID % _noLines) +1; // always skip rec. 0 (status rec)
  if (_Debug(4)) Serial.printf("ESPSL::writef() -> seek[%d] [%s]\r\n", seekToLine, _cRec);
  if (!_logFile.seek((seekToLine * _lineWidth), SeekSet)) {
    Serial.printf("ESPSL::writef(): seek to position [%d] failed\r\n", seekToLine);
  }
  bytesWritten = _logFile.print(_cRec);
  if (bytesWritten != _lineWidth) {
      Serial.printf("ESPSL::writef(): ERROR!! written [%d] bytes but should have been [%d]\r\n"
                                       , bytesWritten, _lineWidth);
      _logFile.close();
      return false;
  }

  _lastUsedLineID++;
  _oldestLineID++;
  nextFree = (_lastUsedLineID % _noLines) + 1;  // always skip rec "0"
  if (_Debug(4)) Serial.printf("ESPSL::writef(e): oldest[%8d], last[%8d], nextFree Slot[%3d]\r\n"
                                                      , _oldestLineID
                                                      , _lastUsedLineID
                                                      , nextFree);
  
  _logFile.close();

} // writef()


//-------------------------------------------------------------------------------------
boolean ESPSL::writeD(const char *callFunc, int atLine, const char *fmt, ...)
{
  char tLine[_MAXLINEWIDTH + 10];
  char lineBuf[_MAXLINEWIDTH +10];
  
  tLine[0] 	  = '\0';
  lineBuf[0]  = '\0';

#if defined(ESP8266)
  sprintf(tLine, "[%7u|%6u] [%-12.12s(%4d)] " , ESP.getFreeHeap(), ESP.getMaxFreeBlockSize()
                                                , callFunc, atLine);
#else
  sprintf(tLine, "[%-12.12s(%4d)] ", callFunc, atLine);
#endif

  va_list args;
  va_start (args, fmt);
  vsnprintf (lineBuf, _lineWidth, fmt, args);
  va_end (args);
  while ((strlen(lineBuf) > 0) && (strlen(tLine) + strlen(lineBuf)) > (_MAXLINEWIDTH -1)) {
  	lineBuf[strlen(lineBuf)-1] = '\0';
  	yield();
  }
	if ((strlen(tLine) + strlen(lineBuf)) < _MAXLINEWIDTH) {
		strcat(tLine, lineBuf);
	}
  write(tLine);

}	// writeD()

//-------------------------------------------------------------------------------------
boolean ESPSL::writeD(int HH, int MM, int SS, const char *callFunc, int atLine, const char *fmt, ...)
{
  char tLine[_MAXLINEWIDTH +10];
  char lineBuf[_MAXLINEWIDTH +10];
  
  tLine[0]    = '\0';
  lineBuf[0]  = '\0';

#if defined(ESP8266)
    sprintf(tLine, "[%02d:%02d:%02d][%7u|%6u] [%-12.12s(%4d)] "
                                                , HH, MM, SS
                                                , ESP.getFreeHeap(), ESP.getMaxFreeBlockSize()
                                                , callFunc, atLine);
#else
    sprintf(tLine, "[%02d:%02d:%02d] [%-12.12s(%4d)] "
                                                , HH, MM, SS
                                                , callFunc, atLine);
#endif

  va_list args;
  va_start (args, fmt);
  vsnprintf (lineBuf, _lineWidth, fmt, args);
  va_end (args);
  while ((strlen(lineBuf) > 0) && (strlen(tLine) + strlen(lineBuf)) > (_MAXLINEWIDTH -1)) {
  	lineBuf[strlen(lineBuf)-1] = '\0';
  	yield();
  }
	if ((strlen(tLine) + strlen(lineBuf)) < _MAXLINEWIDTH) {
		strcat(tLine, lineBuf);
	}
  write(tLine);

}	// writeD()


//-------------------------------------------------------------------------------------
// set pointer to startLine
boolean ESPSL::startReading(int16_t startLine, uint8_t numLines) 
{
  if (_Debug(1)) Serial.printf("ESPSL::startReading([%d], [%d])..\r\n", startLine, numLines);
  
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
  if (_Debug(3)) Serial.printf("ESPSL::startReading(): _oldest[%d], _last[%d], Start[%d], End[%d]\r\n"
                                              , _oldestLineID
                                              , _lastUsedLineID
                                              , _readPointer, _readEnd);

} // startReading()


//-------------------------------------------------------------------------------------
// set pointer to startLine
boolean ESPSL::startReading(int16_t startLine) 
{
  if (_Debug(1)) Serial.printf("ESPSL::startReading([%d])..\r\n", startLine);
  startReading(startLine, 0);

} // startReading()

//-------------------------------------------------------------------------------------
// start reading from startLine
String ESPSL::readNextLine()
{
  if (_Debug(1)) Serial.printf("ESPSL::readNextLine(%d) ", _readPointer);
  
  int32_t recNr = _readPointer;
  uint8_t seekToLine;
  char    lineBuf[_lineWidth +10];
  char    logText[_lineWidth];

  if (_readPointer > _readEnd) return("EOF");
  // --- check if the file exists and can be opened ---
  File _logFile  = SPIFFS.open(_sysLogFile, "r");    // open for reading and writing
  if (!_logFile) {
    Serial.printf("ESPSL::readNextLine(): Some error opening [%s] .. bailing out!\r\n", _sysLogFile);
    _logFile.close();
    return "EOF";
  } // if (!_logFile)

  //checkSysLogFileSize("readNextLine():", (_noLines + 1) * _lineWidth);

  for (recNr = _readPointer; recNr <= _readEnd; recNr++) {
    seekToLine = (_readPointer % _noLines) +1;
    if (!_logFile.seek((seekToLine * _lineWidth), SeekSet)) {
      Serial.printf("ESPSL::readNextLine(): seek to position [%d] failed\r\n", seekToLine);
    }

    uint32_t  lineID;
    int l = _logFile.readBytesUntil('\n', lineBuf, sizeof(lineBuf));
    lineBuf[l] = '\0';
    //Serial.printf("ESPSL::init(): [%d] -> [%s]\r\n", l, lineBuf);
    sscanf(lineBuf,"%u;%[^\0]", 
            &lineID,
            logText);

    if (_Debug(4)) Serial.printf("ESPSL::readNextLine(): [%4d]->recNr[%4d][%8d]-> [%s]\r\n"
                                                            , seekToLine
                                                            , recNr
                                                            , lineID
                                                            , logText);
    _readPointer++;
    if (String(logText).indexOf(String(_emptyID)) == -1) {
      _logFile.close();
      if (_Debug(3)) return String(lineBuf);
      else           return String(logText);
    } else {
      if (_Debug(4)) Serial.printf("ESPSL::readNextLine(): SKIP[%s]\r\n", logText);
    }
    
  } // for ..

  _logFile.close();
  return String("EOF");

} //  readNextLine

//-------------------------------------------------------------------------------------
// start reading from startLine
String ESPSL::dumpLogFile() 
{
  if (_Debug(1)) Serial.println("ESPSL::dumpLogFile()..");

  char    lineBuf[_lineWidth +10];
  char    logText[_lineWidth];
  int16_t recNr;
      
  // --- check if the file exists and can be opened ---
  File _logFile  = SPIFFS.open(_sysLogFile, "r");    // open for reading and writing
  if (!_logFile) {
    Serial.printf("ESPSL::dumpLogFile(): Some error opening [%s] .. bailing out!\r\n", _sysLogFile);
    _logFile.close();
    return "Error!";
  } // if (!_logFile)

  checkSysLogFileSize("dumpLogFile():", (_noLines + 1) * _lineWidth);

  recNr = 0;
  while (_logFile.available() > 0) {
    uint32_t  lineID;
    int l = _logFile.readBytesUntil('\n', lineBuf, sizeof(lineBuf));
    lineBuf[l] = '\0';
    if (_Debug(5)) Serial.printf("ESPSL::init(): [%d] -> [%s]\r\n", l, lineBuf);
    sscanf(lineBuf,"%u;%[^\0]", 
            &lineID,
            logText);

    if (lineID == _lastUsedLineID)
            Serial.printf("ESPSL::dumpLogFile(L): recNr[%4d][%8d]-> [%s]\r\n"
                          "           >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\r\n"
                                                                      , recNr, lineID, logText);
    else if (lineID == _oldestLineID && recNr < _noLines)
            Serial.printf("ESPSL::dumpLogFile(O): recNr[%4d][%8d]-> [%s]\r\n", recNr, lineID, logText);
    else    Serial.printf("ESPSL::dumpLogFile( ): recNr[%4d][%8d]-> [%s]\r\n", recNr, lineID, logText);
    recNr++;
  } // while ..

  _logFile.close();
  return "EOF";

} // dumpLogFile

//-------------------------------------------------------------------------------------
// erase SysLog file from SPIFFS
boolean ESPSL::removeSysLog() 
{
  if (_Debug(1)) Serial.println("ESPSL::removeSysLog()..");
  SPIFFS.remove(_sysLogFile);
  return true;
  
} // removeSysLog()

//-------------------------------------------------------------------------------------
// returns ESPSL status info
boolean ESPSL::status() 
{
  Serial.printf("ESPSL::status():      _lineWidth[%8d]\r\n", _lineWidth);
  Serial.printf("ESPSL::status():        _noLines[%8d]\r\n", _noLines);
  if (_noLines > 0) {
    Serial.printf("ESPSL::status():   _oldestLineID[%8d] (%2d)\r\n", _oldestLineID
                                                          , (_oldestLineID % _noLines)+1);
    Serial.printf("ESPSL::status(): _lastUsedLineID[%8d] (%2d)\r\n", _lastUsedLineID
                                                          , (_lastUsedLineID % _noLines)+1);
  }
  Serial.printf("ESPSL::status():       _debugLvl[%8d]\r\n", _debugLvl);
  return true;
  
} // status()

  
//-------------------------------------------------------------------------------------
// check fileSize of _sysLogFile
boolean ESPSL::checkSysLogFileSize(const char* func, int32_t cSize)
{
  if (_Debug(4)) Serial.printf("ESPSL::checkSysLogFileSize(%d)..\r\n", cSize);
  int32_t fileSize = sysLogFileSize();
  if (fileSize != cSize) {
    Serial.printf("ESPSL::%s -> [%s] size is [%d] but should be [%d] .. error!\r\n"
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
void ESPSL::fixLineWidth(char *inLine, int len) 
{
  if (_Debug(4)) Serial.printf("ESPSL::fixLineWidth([%s], %d) ..\r\n", inLine, len);
  char tmpRec[_MAXLINEWIDTH + 10];
  tmpRec[0] = '\0';
  
  sprintf(tmpRec, "%s", inLine);
  inLine[0] = '\0';
  int16_t s = 0, l = 0;
  while (tmpRec[s] != '\0' && tmpRec[s] != '\n' && s < (_lineWidth -1)) {yield(); s++;}
  for (l = s; l < (len - 1); l++) {
  	yield();
    tmpRec[l] = ' ';
  }
  tmpRec[l]   = '\n';
  tmpRec[len] = '\0';

  while (tmpRec[l] != '\0') {yield(); l++;}
  strcat(inLine, tmpRec);
  if (_Debug(4)) Serial.printf("ESPSL::Length of inLine is now [%d] bytes\r\n", l);
  
} // fixLineWidth()

//===========================================================================================
int16_t  ESPSL::sysLogFileSize()
{
#if defined(ESP8266)
  {
    Dir dir = SPIFFS.openDir("/");         // List files on SPIFFS
    while (dir.next())  {
    			yield();
          String fileName = dir.fileName();
          size_t fileSize = dir.fileSize();
          if (_Debug(6)) Serial.printf("ESPSL::sysLogFileSize(): found: %s, size: %d\n"
                                                                             , fileName.c_str()
                                                                             , fileSize);
          if (fileName == _sysLogFile) {
            if (_Debug(6)) Serial.printf("ESPSL::sysLogFileSize(): fileSize[%d]\r\n", fileSize);
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
          if (_Debug(6)) Serial.printf("ESPSL::FS Found: %s, size: %d\n", fileName.c_str(), fileSize);
          if (fileName == _sysLogFile) {
            if (_Debug(6)) Serial.printf("ESPSL::sysLogFileSize(): fileSize[%d]\r\n", fileSize);
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
