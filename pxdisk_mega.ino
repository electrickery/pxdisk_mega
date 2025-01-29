/////////////////////////////////////////////////
///  pxdisk
///  copyright(c) 2019 William R Cooke
///  
///  command line interpreter, blinking lights, 
///  SD-card management: 2023, Fred Jan Kraan
///  https://github.com/electrickery/pxdisk_mega
///
///  Use an SD card as multiple disk drives for 
///  Epson PX-8 and PX-4 CP/M laptop computers
/////////////////////////////////////////////////

#define MAJOR_VERSION 1
#define MINOR_VERSION 6
#define PATCH         4

#include <SPI.h>
#include <SD.h>

#include "pxdisk.h"

File root;

void(* resetFunc) (void) = 0; // create a standard reset function

//////////////////////////////////////////////////////////////////////////////
///  @fn Setup
///
///  @brief  Initializes all I/O and data structures
///
//////////////////////////////////////////////////////////////////////////////
void setup() 
{
  uint16_t timeOut = 100;

  DEBUGPORT.begin(DEBUGBAUDRATE);
  while (!DEBUGPORT && timeOut) {
    ; // wait for serial port to connect. Needed for native USB port only
    timeOut--;
    if (timeOut % 2) { // toggle debugLED every delay cycle
      debugLedOff();
    } else {
      debugLedOn();
    }
    delay(100L);
  }
  if (timeOut) { // if timeout hasn't happened, console is available
    console = true;
  }

  if (console) {
    DEBUGPORT.println();
    DEBUGPORT.println();
    DEBUGPORT.print(F("Beginning PFBDK on "));
    DEBUGPORT.print(F(BOARDTEXT));
    DEBUGPORT.print(F(" v"));
    printVersion();
    DEBUGPORT.println("...");
  }

  debugLedOn();
  pinMode(DEBUGLED, OUTPUT);
  
  pinMode(D_LED, OUTPUT);
  pinMode(E_LED, OUTPUT);
  pinMode(F_LED, OUTPUT);
  pinMode(G_LED, OUTPUT);

  PXPORT.begin(PXBAUDRATE);
  int sd = SD.begin(CS_PIN);

  if (!sd) {
    if (console) DEBUGPORT.println("No SD-card, halted.");
    while(1) {
      digitalWrite(DEBUGLED, !digitalRead(DEBUGLED));
      delay(250L);
    }
  }

  // TODO:  Why in the haydes do we need to do this?
  // It appears to be a timing issue.  First use of a read 
  // seems to get everything initialized.  If we wait until needed
  // maybe it is taking too long and px8 times out?

  diskReadSector(0, 0, 0, 1, textBuffer);
  diskReadSector(0, 1, 0, 1, textBuffer);
  diskReadSector(1, 0, 0, 1, textBuffer);
  diskReadSector(1, 1, 0, 1, textBuffer);

  root = SD.open("/");
  if (console) {

    printDirectory();
  }
  root.close();

  // 
  debugLedOff();
  // 
  driveLedsOff();

  for (int i = 0; i < MAX_TEXT; i++) {
    textBuffer[i] = 0;
  }
}

void printVersion() {
  if (console) {
    DEBUGPORT.print(MAJOR_VERSION);
    DEBUGPORT.print(".");
    DEBUGPORT.print(MINOR_VERSION);
    DEBUGPORT.print(".");
    DEBUGPORT.print(PATCH);
  }
}

//////////////////////////////////////////////////////////////////////////////
///  @fn diskReadSector
///  @param[in] uint8_t unit: Disk unit to read from, 0 or 1
///  @param[in] uint8_t disk: Disk in unit to read from, 0 or 1
///  @param[in] uint8_t track: Track to read from, 0 to 39
///  @param[in] uint8_t sector: Sector to read, 1 to 64
///  @param[in] uint8_t* buffer: Pointer to buffer to write into
///
///  @return bool: true if successful, false if not
///
///  @brief  Reads a 128 byte sector from given disk, track,sector 
///
//////////////////////////////////////////////////////////////////////////////
bool diskReadSector(uint8_t unit, uint8_t disk, uint8_t track, uint8_t sector, uint8_t *buffer)
{
  bool rtn = true;
  uint8_t device = unit * 2 + disk;
  File dsk = SD.open(diskNames[device], FILE_READ);
//  if (console) DEBUGPORT.write("R:");
//  if (console) DEBUGPORT.println(diskNames[device]);
  driveLedOn(device);
  if(dsk) {
    int result;
    rtn = dsk.seek(track * DISK_BYTES_PER_TRACK + (sector-1) * DISK_BYTES_PER_SECTOR);
    if(rtn)
    {
      result = dsk.read(buffer, DISK_BYTES_PER_SECTOR);
      if(result < 0)
      {
        rtn = false;
      }
    }
     dsk.close();
   } else {
    rtn = false;
    if (console) DEBUGPORT.println(F("OPEN & READ FAIL"));
  }
  return rtn;
}

//////////////////////////////////////////////////////////////////////////////
///  @fn diskWriteSector
///  @param[in] uint8_t unit: Disk unit to write to, 0 or 1
///  @param[in] uint8_t disk: Disk in unit to write to, 0 or 1
///  @param[in] uint8_t track: Track to write to, 0 to 39
///  @param[in] uint8_t sector: Sector to write, 1 to 64
///  @param[in] uint8_t* buffer: Pointer to buffer to read from
///
///  @return bool: true if successful, false if not
///
///  @brief  Writes a 128 byte sector to given disk, track,sector 
///
//////////////////////////////////////////////////////////////////////////////
bool diskWriteSector(uint8_t unit, uint8_t disk, uint8_t track, uint8_t sector, uint8_t *buffer)
{
  bool rtn = true;
  uint8_t device = unit * 2 + disk;
//  File dsk = SD.open(diskNames[device], FILE_WRITE);
  File dsk = SD.open(diskNames[device], O_READ | O_WRITE | O_CREAT);
  if(dsk)
  {
    dsk.seek(track * DISK_BYTES_PER_TRACK + (sector - 1) * DISK_BYTES_PER_SECTOR);
    dsk.write(buffer, DISK_BYTES_PER_SECTOR);
    dsk.close();

    driveLedOn(device);
  }
  else
  {
    rtn = false;
    if (console) DEBUGPORT.println("OPEN & WRITE FAIL");
  }
  return rtn;
}

//////////////////////////////////////////////////////////////////////////////
///  @fn hexNibble
///  @param[in] uint8_t n: The nibble to convert (0 to 15)
///
///  @return uint8_t Character HEX(0-9, A-F) representation of n
///
///  @brief  Converts nibble to hex character
///
//////////////////////////////////////////////////////////////////////////////
uint8_t hexNibble(uint8_t n)
{
  uint8_t rtn = 0;
  if(n < 16)
  {
    rtn = n + 0x30;
    if(n > 9)
    {
      rtn = n + 0x37;
    }
  }
  return rtn;
}

//////////////////////////////////////////////////////////////////////////////
///  @fn showHex
///  @param[in] uint8_t b: Byte to print
///
///  @brief  Sends HEX representation of byte to Arduino terminal
///
//////////////////////////////////////////////////////////////////////////////
void showHex(uint8_t b)
{
  if (console) {
    DEBUGPORT.write(hexNibble( b >> 4));
    DEBUGPORT.write(hexNibble( b & 0xf));
    DEBUGPORT.write( 0x20);
    DEBUGPORT.println();
  }
}

//////////////////////////////////////////////////////////////////////////////
///  @fn isValidSID
///  @param[in] uint8_t id:  The ID number to check
///
///  @return bool: true if id is valid as a source ID
///
///  @brief  Source ID can be from HX20, PX8, or PX4
///
//////////////////////////////////////////////////////////////////////////////
bool isValidSID(uint8_t id)
{
  bool rtn = false;
  if(id == 0x20 || id == 0x22 || id == 0x23)  // 20=HX20, 22=PX8, 23=PX4
  {
    rtn = true;
  }
  return rtn;
}

//////////////////////////////////////////////////////////////////////////////
///  @fn isValidDID
///  @param[in] uint8_t id:  The ID number to check
///
///  @return bool: true if id is valid as a destination ID
///
///  @brief  Destination ID can be to disk unit 1 or 2
///
//////////////////////////////////////////////////////////////////////////////
bool isValidDID(uint8_t id)
{
  bool rtn = false;
  if(id == 0x31 || id == 0x32)    // 31=First Disk (D,E) 32=Second Disk (F,G)
  {
    rtn = true;
  }
  return rtn;
}

//////////////////////////////////////////////////////////////////////////////
///  @fn isValidFNC
///  @param[in] uint8_t f:  The function code to check
///
///  @return bool: true if this is a valid disk function code
///
///  @brief  Checks that function code is valid for disk device
///
//////////////////////////////////////////////////////////////////////////////
bool isValidFNC(uint8_t f)
{
  bool rtn = false;
  switch(f) {
    case FN_DISK_RESET:
    case FN_DISK_SELECT:
    case FN_DISK_READ_SECTOR:
    case FN_DISK_WRITE_SECTOR:
    case FN_DISK_WRITE_HST:
    case FN_DISK_COPY:
    case FN_DISK_FORMAT:
    case FN_IMAGE_CMDS:
      rtn = true;
      break;
    default:
      break;
  }
  return rtn;
}

//////////////////////////////////////////////////////////////////////////////
///  @fn receiveByte
///
///  @return uint8_t: Next byte received from Master
///
///  @brief  Waits until there is an incoming byte and returns it.
///
//////////////////////////////////////////////////////////////////////////////
uint8_t receiveByte()
{
  /// TODO:  fix rtn: uint8_t won't work right!
  uint8_t rtn;
  while( (rtn = PXPORT.read()) < 0);
  return rtn;
}

//////////////////////////////////////////////////////////////////////////////
///  @fn sendByte
///  @param[in] uint8_t b: Byte to send to Master
///
///  @brief  Sends a raw byte to the Master device
///
//////////////////////////////////////////////////////////////////////////////
void sendByte(uint8_t b)
{
  PXPORT.write(b);
}

//////////////////////////////////////////////////////////////////////////////
///  @fn sendHeader
///
///  @brief  Sends header to Master based on most recently received data.
///
//////////////////////////////////////////////////////////////////////////////
void sendHeader()
{
  uint8_t cks = 0;
  sendByte(C_SOH);
  cks -= C_SOH;
  sendByte(C_FMT_SM);
  cks -= C_FMT_SM;
  sendByte(latestSID); // Source becomes destination
  cks -= latestSID;
  sendByte(latestDID); // and vice versa
  cks -= latestDID;
  sendByte(latestFNC);
  cks -= latestFNC;
 
  uint8_t thisSIZ;
  switch(latestFNC)
  {
    case FN_DISK_READ_SECTOR:
    thisSIZ = 0x80;
    break;
    
    case FN_DISK_WRITE_SECTOR:
    thisSIZ = 0;
    break;

    case FN_DISK_COPY:
    thisSIZ = 0x02;
    break;
    
    case FN_DISK_FORMAT:
    thisSIZ = 0x02;
    break;
    
    case FN_IMAGE_CMDS:
    thisSIZ = getTxtSize(textBuffer[0]);
    break;
    
    default:
    thisSIZ = 0x00;
    break;
    
  }
  sendByte(thisSIZ);
  latestSIZ = thisSIZ;
  cks -= thisSIZ;
  sendByte(cks);
}

//////////////////////////////////////////////////////////////////////////////
///  @fn sendText
///
///  @brief  Sends text to Master based on most recently received data.
///
//////////////////////////////////////////////////////////////////////////////
void sendText()
{
  uint8_t device;
  uint8_t cks = 0;
  uint8_t returnCode = 0;
  sendByte(C_STX);
  cks -= C_STX;
  uint8_t i = 0;
  bool returnStatus = true;
  char managementCommand;

  switch(latestFNC)
  {
//////////////////////////////////////////////////////////////////////////////
    case FN_DISK_RESET:
      sendByte(returnCode);       // return success
      cks -= 0;
      break;
//////////////////////////////////////////////////////////////////////////////
    case FN_DISK_SELECT:   // ??? Where is this?  Not in PX8 OSRM ch 15 epsp.html
      sendByte(0);
      cks -= 0;
      break;
//////////////////////////////////////////////////////////////////////////////
    case FN_DISK_READ_SECTOR:
      returnStatus = diskReadSector(selectedDevice, textBuffer[0] - 1, 
                        textBuffer[1], textBuffer[2], textBuffer);
      if(!returnStatus) // error!
      {
        returnCode = BDOS_RC_READ_ERROR;
      }
      for(int i = 0; i < MAX_TEXT; i++)
      {
        uint8_t byt = textBuffer[i];
        sendByte(byt);
        cks -= byt;
      }
      sendByte(returnCode);  // return code
      cks -= 0;
      break;
//////////////////////////////////////////////////////////////////////////////
    case FN_DISK_WRITE_SECTOR:
    device = selectedDevice * 2 + textBuffer[0] - 1;
    if (writeProtect[device] == true) {
        returnCode = BDOS_RC_WRITE_ERROR; // not the correct code, but the PX is happy.
    } else {
        returnStatus = diskWriteSector(selectedDevice, textBuffer[0] - 1, 
                       textBuffer[1], textBuffer[2], &textBuffer[4]);
        if(!returnStatus) // error!
        {
            returnCode = BDOS_RC_WRITE_ERROR;
        }
    }
    sendByte(returnCode);  // return code
    cks -= 0;
    break;
//////////////////////////////////////////////////////////////////////////////
    case FN_DISK_WRITE_HST:   // We always write, so nothing needs to flush
      sendByte(0);
      break;
//////////////////////////////////////////////////////////////////////////////
    case FN_DISK_COPY:             // Not currently supported
      if (console) DEBUGPORT.println(F("FN_DISK_COPY not supported"));
      sendByte(C_NAK);
      break;
//////////////////////////////////////////////////////////////////////////////
    case FN_DISK_FORMAT:           // Not currently supported
      if (console) DEBUGPORT.println(F("FN_DISK_FORMAT not supported"));
      sendByte(C_NAK);
      break;
//////////////////////////////////////////////////////////////////////////////
    case FN_IMAGE_CMDS:
      managementCommand = latestE0Command;
      setBufPointer = receivedSize + 1;
      if (console) {
        DEBUGPORT.println(F("FN_IMAGE_CMDS not very supported"));
        // debug echo 
        DEBUGPORT.print("textB: ");
        for (uint8_t i = 0; i < setBufPointer; i++) {
          DEBUGPORT.print(textBuffer[i], HEX); DEBUGPORT.print(" "); 
        }
        DEBUGPORT.println();
        DEBUGPORT.print(F("serB: "));
      }
      for (uint8_t i = 0; i < setBufPointer; i++) {
        serialBuffer[i] = textBuffer[i]; // prep the commandInterpreter
        if (console) {
          if (textBuffer[i] > 0x01F && textBuffer[i] < 0x7F) {
            DEBUGPORT.write(textBuffer[i]); DEBUGPORT.print(" ");
          } else {
            DEBUGPORT.print(textBuffer[i], HEX);
            DEBUGPORT.print("h ");
          }
        }
      }
      if (console) DEBUGPORT.println();
      commandInterpreter(); // execute command
      for (uint8_t i = 0; i < getTxtSize(managementCommand); i++) {
        sendByte(textBuffer[i]);
        cks -= textBuffer[i];
      }

      sendByte(returnCode);
      cks -= 0;
      break;
//////////////////////////////////////////////////////////////////////////////
    default:
      break;
  }
  sendByte(C_ETX);

  cks -= C_ETX;
  sendByte(cks);
}


//////////////////////////////////////////////////////////////////////////////
/////////////////////////////// State Machine ////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
///  @fn stateMachine
///  @param[in] uint8_t b:  Incoming byte from Master
///
///  @brief  Performs action based on current state and incoming byte
///
//////////////////////////////////////////////////////////////////////////////
void stateMachine(uint8_t b)
{
  static States state = ST_BEGIN;
  static uint8_t textCount = 0;
  
  switch(state)
  {
    case ST_UNDEFINED:              // Should never happen, but you never know
      break;
///////////////////////////////////////////////////////////////////////////////
    case ST_BEGIN:                  // Starting state
      if(b == C_SEL)
      {
        state = ST_PS_SEL;
      }
      else
      {
        // do nothing. Wait for C_SEL
      }
      break;
//////////////////////////////////////////////////////////////////////////////
   case ST_IDLE:
     if(b == C_SEL)
     {
       state = ST_PS_SEL;
       debugLedOn();
     }
     else if(b == C_SOH)
     {
       latestCKS = C_SOH;
       state = ST_HD_SOH;
       debugLedOn();
     }
     else if(b == C_STX)
     {
       latestCKS = b;
       textCount = 0;
       state = ST_TX_STX;
       debugLedOn();
     }
     else if(b == C_EOT)
     {
//     if (console) DEBUGPORT.println("IDLE got EOT");
     }
     else
     {
       // state = ST_ERR;  just stay where we are on unrecognized char
       //  get 00 when px8 turned off
     }
     break;

//////////////////////////////////////// Select / Poll ///////////////////////
    case ST_PS_SEL:
      if(isValidDID(b))
      {
        latestDID = b;
        state = ST_PS_DID;
      }
      else
      {
        oldState = state;
        state = ST_ERR;
      }
      break;
//////////////////////////////////////////////////////////////////////////////
    case ST_PS_DID:
      if(isValidSID(b))
      {
        latestSID = b;
        state = ST_PS_SID;
      }
      else
      {
        oldState = state;
        state = ST_ERR;
      }
      break;
//////////////////////////////////////////////////////////////////////////////
    case ST_PS_SID:
      if(b == C_ENQ)
      {
          sendByte(C_ACK);
          selectedDevice = latestDID - MY_ID_1;
          state = ST_PS_ENQ;
      }
      else
      {
        sendByte(C_NAK);
        oldState = state;
        state = ST_ERR;
      }
      break;
//////////////////////////////////////////////////////////////////////////////
   case ST_PS_ENQ:
     if(b == C_SOH)
     {
       latestCKS = b;
       state = ST_HD_SOH;
     }
     else
     {
       oldState = state;
       state = ST_ERR;
     }
     break;
//////////////////////////////////   Header  /////////////////////////////////
    case ST_HD_SOH:
      if(b == C_FMT_MS)
      {
        latestCKS += b;
        state = ST_HD_FMT;
      }
      else
      {
        oldState = state;
        state = ST_ERR;
      }
      break;
//////////////////////////////////////////////////////////////////////////////
    case ST_HD_FMT:
      if(isValidDID(b))
      {
        latestCKS += b;
        latestDID = b;
        state = ST_HD_DID;
      }
      break;
//////////////////////////////////////////////////////////////////////////////
    case ST_HD_DID:
      if(isValidSID(b))
      {
        latestSID = b;
        latestCKS += b;
        state = ST_HD_SID;
      }
      break;
//////////////////////////////////////////////////////////////////////////////
    case ST_HD_SID:
      if (isValidFNC(b))
      {
        latestFNC = b;
        latestCKS += b;
        state = ST_HD_FNC;
      }
      else
      {
        oldState = state;
        state = ST_ERR;
      }
      break;
//////////////////////////////////////////////////////////////////////////////
    case ST_HD_FNC:
      latestSIZ = b;
      receivedSize = b;
      latestCKS += b;
      state = ST_HD_SIZ;
      break;
//////////////////////////////////////////////////////////////////////////////
    case ST_HD_SIZ:
      latestCKS += b;
      if(latestCKS == 0)
      {
        sendByte(C_ACK);
        state = ST_HD_CKS;
      }
      else
      {
        sendByte(C_NAK);
        oldState = state;
        state = ST_ERR;
      }
      break;
//////////////////////////////////////////////////////////////////////////////
    case ST_HD_CKS:
      if(b == C_STX)
      {
        latestCKS = b;
        textCount = 0;
        state = ST_TX_STX;
      }
      else
      {
        oldState = state;
        state = ST_ERR;
      }
      break;
//////////////////////////////////////// Text ////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

    case ST_TX_STX:
      textBuffer[textCount] = b;
      if (textCount ==0) latestE0Command = b;

      latestCKS += b;
      if(textCount == latestSIZ)
      {
        state = ST_TX_TXT;
      }
      else
      {
        textCount++;
      }
      break;
//////////////////////////////////////////////////////////////////////////////
    case ST_TX_TXT:
      latestCKS += b;
      if(b == C_ETX)
      {
        state = ST_TX_ETX;
      }
      else
      {
        oldState = state;
        state = ST_ERR;
      }
      break;
//////////////////////////////////////////////////////////////////////////////
    case ST_TX_ETX:
      latestCKS += b;
      if(latestCKS == 0)
      {
         sendByte(C_ACK);
         state = ST_TX_CKS; 
      }
      else
      {
        oldState = state;
        state = ST_ERR;
      }
      break;
//////////////////////////////////////////////////////////////////////////////
    case ST_TX_CKS:
      if(b == C_EOT)
      {
        sendHeader();
        state = ST_SENT_HDR;
      }
      else
      {
        
      }
      break;
//////////////////////////////////////////////////////////////////////////////
   case ST_SENT_HDR:
     if(b == C_ACK)
     {
       sendText();
       state = ST_SENT_TXT;
     }
     else
     {
       oldState = state;
       state = ST_ERR;
     }
     break;

//////////////////////////////////////////////////////////////////////////////
    case ST_SENT_TXT:
      if(b == C_ACK)
      {
        sendByte(C_EOT);
        state = ST_IDLE;
      }
      else if(b == C_NAK)
      {
        sendText();
      }
      debugLedOff();
    break;
//////////////////////////////////////////////////////////////////////////////
    case ST_ERR:
    if (console) {
      DEBUGPORT.print("Err; old state was: ");
      DEBUGPORT.print(oldState); 
    }
    state = ST_IDLE;
    break;

    default:
    break;
  }
}

//////////////////////////////////////////////////////////////////////////////
///  @fn loop
///
///  @brief  Gets next incoming byte and passes it to state machine
///
//////////////////////////////////////////////////////////////////////////////
void loop() 
{
  int av;
  while(true)  // TODO  PXPORT.available() > 0)
  {
    if((av = PXPORT.available()) > 32)
    {
      if (console) {
        DEBUGPORT.print('!'); 
        DEBUGPORT.print(av);
      }
    }
    
    if (PXPORT.available() > 0) {   // Wait for something to come in from PX
        uint8_t b = receiveByte();
        stateMachine(b);
    }
    if (DEBUGPORT.available() > 0) { // Wait for something to come in from console
        consoleCommand = true;
        commandCollector();
        consoleCommand = false;
    }
    if (ledTime < millis()) {
      driveLedsOff();
    }
  }
}

void commandCollector() { // only used when console is active
  if (console && DEBUGPORT.available() > 0) {
    int inByte = DEBUGPORT.read();
    switch(inByte) {
    case '\n':
      commandInterpreter();
      clearSerialBuffer();
      setBufPointer = 0;
      break;
    case '\r':
      break;  // ignore carriage return
    default:
      serialBuffer[setBufPointer] = inByte;
      setBufPointer++;
      if (setBufPointer >= SERIALBUFSIZE) {
        DEBUGPORT.println(F("Serial buffer overflow. Cleanup."));
        clearSerialBuffer();
        setBufPointer = 0;
      }
    }
  }
}


void commandInterpreter() {
  byte bufByte = serialBuffer[0];
  File root; // declaration inside the case doesn't work, why?
  
  switch(bufByte) {
    case 'C':
    case 'c':
      checkName(0);
      checkName(1);
      checkName(2);
      checkName(3);
      break;
    case 'D':
    case 'd':
      loadDirectory();
      break;
    case 'H':
    case 'h':
    case '?':
      DEBUGPORT.println();
      DEBUGPORT.print(F("Usage ("));
      printVersion();
      DEBUGPORT.println(F("):"));

      DEBUGPORT.println(F(" C                - temp debug for driveNames[][]"));
      DEBUGPORT.println(F(" D[p]             - SD-card root directory"));
      DEBUGPORT.println(F(" H                - this help"));
      DEBUGPORT.println(F(" M[dnnnnnnnn.eee] - mount file nnnnnnnn.eee on drive d"));
      DEBUGPORT.println(F(" Nnnnnnnnn.eee    - create an image file nnnnnnnn.eee"));
      DEBUGPORT.println(F(" P[dw]            - write protect drive d; w=0 RW, w=1 RO"));
      DEBUGPORT.println(F(" R                - temp reset Arduino"));
      break;
    case 'M':
    case 'm':
      mountImage();
      break;
    case 'N':
    case 'n':
      newFile();
      break;
    case 'P':
    case 'p':
      protect();
      break;
    case 'R':
    case 'r':
      resetFunc();
      break;
    default:
      DEBUGPORT.print(bufByte);
      DEBUGPORT.println(F(" unsupported"));
      return;
  }
}

void printDirectory() {
  root = SD.open("/");
  DEBUGPORT.println(F("Root directory:"));

  while (console) {

    File entry =  root.openNextFile();
    if (! entry) {
      // no more files
      break;
    }
    DEBUGPORT.print(" ");
    String name = entry.name();
    DEBUGPORT.print(name);
    if (entry.isDirectory()) {
      DEBUGPORT.println("/");
//      printDirectory();
    } else {
      // files have sizes, directories do not
      DEBUGPORT.print("\t");
      if (name.length() < 7) DEBUGPORT.print("\t");
      DEBUGPORT.println(entry.size(), DEC);
    }
    entry.close();
  } 
  root.close();
}

void loadDirectory() {  // D[d]
  if (consoleCommand) {
    printDirectory();
  } else {
    uint8_t dirOffset = 0;
    if (setBufPointer == 2) dirOffset = serialBuffer[1] - '0';
    if (dirOffset > 9) dirOffset = 0;
    
    root = SD.open("/");
    if (console) {
      DEBUGPORT.print(F("Root directory ("));
      DEBUGPORT.print(dirOffset);
      DEBUGPORT.println("):");
    }
    uint8_t entryCount = 0;
    int entryNameOffset;
    int entrySizeOffset;
  
      // skip previously send entries
    for (uint8_t dirIndex = 0; dirIndex < (dirOffset * ENTRIESPERMESSAGE); dirIndex++) {
      File tmpEntry = root.openNextFile();
      if (console) DEBUGPORT.print(".");
      tmpEntry.close();
    }
    if (console) DEBUGPORT.println();
    fillTextBuffer(' ');

    while (1) {
      File entry =  root.openNextFile();
      if (!entry) {
        // no more files
        if (console) DEBUGPORT.print(F(" -- no more entries-- "));
        break;
      }
      if (entryCount >= ENTRIESPERMESSAGE) {
        break;
      }
  
      entryNameOffset = ENTRYSIZE * entryCount;
      entrySizeOffset = ENTRYSIZE * entryCount + ENTRYNAMESIZE;
      String name = entry.name();
      if (entry.isDirectory()) name += '/';  // don't use full 8.3 for directories :-)
      uint32_t size = entry.size();
      if (console) {
        DEBUGPORT.print(" ");
        DEBUGPORT.print(name);
      }
      for (int c = 0; c < ENTRYNAMESIZE; c++) {
        if (name.charAt(c) == 0) break;
        textBuffer[entryNameOffset + c] = name.charAt(c);
      }
      
      if (console) DEBUGPORT.print(" ");
  
      uint8_t sizeByte = long2byte(size, 2);
      textBuffer[entrySizeOffset++] = sizeByte;
      if (console) {
        if (sizeByte < 0x10) DEBUGPORT.print("0");
        DEBUGPORT.print(sizeByte, HEX);
      }

      sizeByte = long2byte(size, 1);
      textBuffer[entrySizeOffset++] = sizeByte;
      if (console) {
        if (sizeByte < 0x10) DEBUGPORT.print("0");
        DEBUGPORT.print(sizeByte, HEX);
      }

      sizeByte = long2byte(size, 0);
      textBuffer[entrySizeOffset++] = sizeByte;
      if (console) {
        if (sizeByte < 0x10) DEBUGPORT.print("0");
        DEBUGPORT.print(sizeByte, HEX);
      }

      textBuffer[entrySizeOffset] = LF;
      if (console) DEBUGPORT.println();
      entry.close();
      entryCount++;
    }
    root.close();
//    if (console) dumpTextBuffer();
  }
}

void clearSerialBuffer() {
  byte i;
  for (i = 0; i < SERIALBUFSIZE; i++) {
    serialBuffer[i] = 0;
  }
}

// M command - M[dnnnnnnnn.eee]
void mountImage() {
  bool mountResult = true;
  char drive;
  if (setBufPointer == 1) { // list current mounted images
    mountReport();
  } else if (setBufPointer == 2) {
      if (console) {
        DEBUGPORT.println(F("No filename specified"));
        mountResult = false;
      }
  } else if (setBufPointer >= 7) { // mount a new image to a drive (Mdn.eee)
    uint8_t bufSize = setBufPointer;
    // Mdnnnnnnnn.eee > maximal command length = 14
    if (bufSize > DRIVENAMESIZE+1) bufSize = DRIVENAMESIZE+1; 
    drive = toupper(serialBuffer[1]);
    if (drive >= 'D' or drive <= 'G') {
      mountResult = remount(bufSize, drive);
      mountReport();
    } else {
      if (console) {
        DEBUGPORT.print(F("Illegal drive: "));
        DEBUGPORT.println(drive);
        mountResult = false;
      }
    }
  } else {
    if (console) DEBUGPORT.println(F("Image name too short"));
    mountResult = false;
  }
  if (!mountResult) {
    if (console) {
      DEBUGPORT.print(F("Mounting failed for '"));
      DEBUGPORT.println(drive); 
      DEBUGPORT.println(F("'")); 
    }
  }
}

bool mountCheck(String filename) {
  File dsk = SD.open(filename, FILE_READ);
  if(dsk) {
    if (console) {
      DEBUGPORT.print(F("Opening '"));
      DEBUGPORT.print(filename);
      DEBUGPORT.println(F("' successful"));
    }
    dsk.close();
    return true;
  }
  if (console) {
    DEBUGPORT.print(F("Opening '"));
    DEBUGPORT.print(filename);
    DEBUGPORT.println(F("' failed"));
  }
  return false;
}

void mountReport() {
  if (consoleCommand) {
      DEBUGPORT.println(F("Mounted files:"));
      DEBUGPORT.print(" D - ");
      DEBUGPORT.print(diskNames[0]);
      DEBUGPORT.print("  ");
      DEBUGPORT.println((writeProtect[0]) ? "RO" : "RW");
      DEBUGPORT.print(" E - ");
      DEBUGPORT.print(diskNames[1]);
      DEBUGPORT.print("  ");
      DEBUGPORT.println((writeProtect[1]) ? "RO" : "RW");
      DEBUGPORT.print(" F - ");
      DEBUGPORT.print(diskNames[2]);
      DEBUGPORT.print("  ");
      DEBUGPORT.println((writeProtect[2]) ? "RO" : "RW");
      DEBUGPORT.print(" G - ");
      DEBUGPORT.print(diskNames[3]);
      DEBUGPORT.print("  ");
      DEBUGPORT.println((writeProtect[3]) ? "RO" : "RW");
  } else {
    textBuffer[0x00] = 'D'; textBuffer[0x0D] = 'R'; textBuffer[0x0E] = (writeProtect[0] ? 'O' : 'W'); textBuffer[0x0F] = LF;
    textBuffer[0x10] = 'E'; textBuffer[0x1D] = 'R'; textBuffer[0x1E] = (writeProtect[1] ? 'O' : 'W'); textBuffer[0x1F] = LF;
    textBuffer[0x20] = 'F'; textBuffer[0x2D] = 'R'; textBuffer[0x2E] = (writeProtect[2] ? 'O' : 'W'); textBuffer[0x2F] = LF;
    textBuffer[0x30] = 'G'; textBuffer[0x3D] = 'R'; textBuffer[0x3E] = (writeProtect[3] ? 'O' : 'W'); textBuffer[0x3F] = LF;
    for (uint8_t i = 0; i < 0x0C; i++) {
      textBuffer[0x01+i] = diskNames[0][0+i];
      textBuffer[0x11+i] = diskNames[1][0+i];
      textBuffer[0x21+i] = diskNames[2][0+i];
      textBuffer[0x31+i] = diskNames[3][0+i];
    }
  }
}

void charCpy(char* dest, char *source, uint8_t len) {
  for (uint8_t i = 0; i < len; i++) {
    dest[i] = source[i];
  }
}

bool remount(uint8_t bufSize, char drive) {
    uint8_t driveIndex = drive - 'D';
    String command(serialBuffer);
    String filename = command.substring(2, bufSize);
    filename.toUpperCase();
    filename.trim(); // remove extra spaces
    if (console) {
      if (mountCheck(filename)) {
        DEBUGPORT.print(F("New file name for "));
        DEBUGPORT.print(drive);
        DEBUGPORT.print(": ");
        DEBUGPORT.print(filename);
        DEBUGPORT.println();
      }
      filename.toCharArray(diskNames[driveIndex], filename.length()+1);
      diskNames[driveIndex][DRIVENAMESIZE-1] = 0x0; // probably just paranoia
      return true;
    } else {
      return false;
    }
}

// temp C command
void checkName(uint8_t drive) {
  if (console) {
    DEBUGPORT.print(F("Drive: "));
    DEBUGPORT.write(drive + 'D');
    DEBUGPORT.print(": ");
    for (uint8_t i = 0; i < DRIVENAMESIZE; i++) {
      uint8_t nameByte = diskNames[drive][i];
      if (nameByte < 0x10) DEBUGPORT.print("0");
      DEBUGPORT.print(nameByte, HEX);
      DEBUGPORT.print(" ");
    }
    DEBUGPORT.println();
  }
}

void fillTextBuffer(char filler) {
  for (uint8_t i = 0; i < DISK_BYTES_PER_SECTOR; i++) {
    textBuffer[i] = filler;
  }
}

// N command
void newFile() {
  uint8_t bufSize = setBufPointer;
  String command(serialBuffer);
  if (bufSize > DRIVENAMESIZE) bufSize = DRIVENAMESIZE;
  String filename = command.substring(1, bufSize);
  filename.toLowerCase();
  filename.trim(); // remove extra spaces
  if (checkFilePresence(filename)) {
    if (console) {
      DEBUGPORT.print(F("File: "));
      DEBUGPORT.print(filename);
      DEBUGPORT.println(F(" exists. Aborting."));
    }
    textBuffer[0x00] = 1;
  } else {
    if (console) { 
      DEBUGPORT.print(F("File created: "));
      DEBUGPORT.println(filename);
    }
    createFile(filename);
    textBuffer[0x00] = 0;
  }
}

bool createFile(String filename) {
  bool rtn = true;
  fillTextBuffer(0xE5);
  File dsk = SD.open(filename, O_READ | O_WRITE | O_CREAT);
  if(dsk) {
    dsk.seek(0);
    for (uint16_t sc = 0; sc < DISK_SECTORS; sc++) {
        dsk.write(textBuffer, DISK_BYTES_PER_SECTOR);
        if (console && sc % DISK_SECTORS_PER_TRACK == 0) DEBUGPORT.print(".");
    }
    if (console) DEBUGPORT.println();
    dsk.close();
  }
  else
  {
    rtn = false;
#if DEBUG
    DEBUGPORT.println(F("OPEN & WRITE FAIL"));
#endif
  }
  return rtn;
}

// P command
void protect() {
   if (setBufPointer == 1) { // list current protect status
     if (console) {
       reportP();
     }
   } else if (setBufPointer == 3) {
     char drive = toupper(serialBuffer[1]);
     uint8_t driveIndex = drive - 'D';
     bool wpStatus = (serialBuffer[2] == '1') ? true : false;
     if (drive >= 'D' or drive <= 'G') {
       writeProtect[driveIndex] = wpStatus;
       if (console) {
         reportP();
       }
     } else {
      if (console) {
         DEBUGPORT.print(F("Illegal drive: "));
         DEBUGPORT.println(drive);
      }
     }
   } else {
     if (console) {
      DEBUGPORT.print(setBufPointer, DEC);
      DEBUGPORT.println(F(" unsupported"));
     }
   }
}

void reportP() {
  if (consoleCommand) {
    DEBUGPORT.print("D: ");
    DEBUGPORT.println((writeProtect[0]) ? "RO" : "RW");
    DEBUGPORT.print("E: ");
    DEBUGPORT.println((writeProtect[1]) ? "RO" : "RW");
    DEBUGPORT.print("F: ");
    DEBUGPORT.println((writeProtect[2]) ? "RO" : "RW");
    DEBUGPORT.print("G: ");
    DEBUGPORT.println((writeProtect[3]) ? "RO" : "RW"); 
  } else {
    textBuffer[0]  = 'D'; textBuffer[1]  = ' '; textBuffer[2]  = (writeProtect[0] + '0'); textBuffer[3]  = LF;
    textBuffer[4]  = 'E'; textBuffer[5]  = ' '; textBuffer[6]  = (writeProtect[1] + '0'); textBuffer[7]  = LF;
    textBuffer[8]  = 'F'; textBuffer[9]  = ' '; textBuffer[10] = (writeProtect[2] + '0'); textBuffer[11] = LF;
    textBuffer[12] = 'G'; textBuffer[13] = ' '; textBuffer[14] = (writeProtect[3] + '0'); textBuffer[15] = LF;
  }
}

uint8_t getTxtSize(char command) {
  switch(command) {
    case 'D':
      return DTEXTSIZE;
      break;
    case 'M':
      return MTEXTSIZE;
      break;
    case 'N':
      return NTEXTSIZE;
      break;
    case 'P':
      return PTEXTSIZE;
      break;
    default:
      return 0;
  }
}

bool checkFilePresence(String filename) {
  File dsk = SD.open(filename, FILE_READ);
  if(dsk) {
    dsk.close();
    return true;
  } else {
    return false;
  }
}

void driveLedOn(uint8_t drive) {
  ledOn = true;
  ledTime = millis() + LEDTIMEOUT;
  switch(drive) {
    case 0:
      digitalWrite(D_LED, LOW);
      break;
    case 1:
      digitalWrite(E_LED, LOW);
      break;
    case 2:
      digitalWrite(F_LED, LOW);
      break;
    case 3:
      digitalWrite(G_LED, LOW);
      break;
    default:
      if (console) DEBUGPORT.println(F("Unknown drive"));
  }
}

void driveLedsOff() {
  digitalWrite(D_LED, HIGH);
  digitalWrite(E_LED, HIGH);
  digitalWrite(F_LED, HIGH);
  digitalWrite(G_LED, HIGH);
  ledOn = false;
}

void debugLedOn() {
  digitalWrite(DEBUGLED, HIGH);
}

void debugLedOff() {
  digitalWrite(DEBUGLED, LOW);
}

uint8_t long2byte(uint32_t value, uint8_t byte) {
  uint8_t mmsb = value / 0x1000000;
  uint8_t mlsb = value / 0x10000 - mmsb * 0x100;
  uint8_t lmsb = value / 0x100 - mlsb * 0x100 - mmsb * 0x10000;
  uint8_t llsb = value - lmsb * 0x100 - mlsb * 0x10000 - mmsb * 0x1000000;
  if (byte == 0) return llsb;
  if (byte == 1) return lmsb;
  if (byte == 2) return mlsb;
  if (byte == 3) return mmsb;
  return 0;
}

void dumpTextBuffer() { // should be called only when console is true
  if (console) return;
  uint8_t d;
  for (int i = 0; i < MAX_TEXT; i++) {
    if ((i % 16) == 0) {
      DEBUGPORT.println();
      if (i < 0x10) DEBUGPORT.print("0");
      DEBUGPORT.print(i, HEX);
      DEBUGPORT.print(": ");
    }
    d = textBuffer[i];
    if (d < 0x10) DEBUGPORT.print("0");
    DEBUGPORT.print(d, HEX);
    DEBUGPORT.print(" ");
  }
  DEBUGPORT.println();
}
