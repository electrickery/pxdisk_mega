/////////////////////////////////////////////////
///  pxdisk
///  copyright(c) 2019 William R Cooke
///  
///  command line interpreter and blinking lights: 2023, Fred Jan Kraan
///  https://github.com/electrickery/pxdisk_mega
///
///  Use an SD card as multiple disk drives for 
///  Epson PX-8 and PX-4 CP/M laptop computers
/////////////////////////////////////////////////

#define MAJOR_VERSION 1
#define MINOR_VERSION 5
#define PATCH         2

#include <SPI.h>
#include <SD.h>

#include "pxdisk.h"

File root;

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

  if (console) DEBUGPORT.println();
  if (console) DEBUGPORT.println();
  if (console) DEBUGPORT.print(F("Beginning PFBDK on "));
  if (console) DEBUGPORT.print(F(BOARDTEXT));
  if (console) DEBUGPORT.print(F(" "));
  printVersion();
  if (console) DEBUGPORT.println("...");

  debugLedOn();
  pinMode(DEBUGLED, OUTPUT);
  
  pinMode(D_LED, OUTPUT);
  pinMode(E_LED, OUTPUT);
  pinMode(F_LED, OUTPUT);
  pinMode(G_LED, OUTPUT);

  PXPORT.begin(PXBAUDRATE);
  int sd = SD.begin(CS_PIN);

  // TODO:  Why in the haydes do we need to do this?
  // It appears to be a timing issue.  First use of a read 
  // seems to get everything initialized.  If we wait until needed
  // maybe it is taking too long and px8 times out?

  diskReadSector(0, 0, 0, 1, textBuffer);
  diskReadSector(0, 1, 0, 1, textBuffer);
  diskReadSector(1, 0, 0, 1, textBuffer);
  diskReadSector(1, 1, 0, 1, textBuffer);

  root = SD.open("/");
  if (console) DEBUGPORT.println();
  if (console) DEBUGPORT.println(F("Root directory:"));
  printDirectory(root, 1, false);
  root.close();

  // 
  debugLedOff();
  // 
  driveLedsOff();
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
  if (console) DEBUGPORT.write("R:");
  if (console) DEBUGPORT.println(diskNames[device]);
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

    if (console) DEBUGPORT.write("W:");
    if (console) DEBUGPORT.println(diskNames[device]);
//    DEBUGPORT.println(PXPORT.available());
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
  bool rtn = true;
  switch(f) {
    case FN_DISK_RESET:
    case FN_DISK_SELECT:
    case FN_DISK_READ_SECTOR:
    case FN_DISK_WRITE_SECTOR:
    case FN_DISK_WRITE_HST:
    case FN_DISK_COPY:
    case FN_DISK_FORMAT:
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
  if (console) DEBUGPORT.print(rtn, HEX);
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
 
  uint8_t thisSIZ = latestSIZ;
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
  if (console) DEBUGPORT.print("S_T: ");
  uint8_t device;
  uint8_t cks = 0;
  uint8_t returnCode = 0;
  sendByte(C_STX);
  cks -= C_STX;
  uint8_t i = 0;
  bool returnStatus = true;

  if (console) {
    DEBUGPORT.print(C_STX);
    DEBUGPORT.print(" ");
  }
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
    
  if (console) {
      DEBUGPORT.print("SEL");
      DEBUGPORT.print(0);
      DEBUGPORT.print(" ");
  }

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
    for(int i = 0; i < 128; i++)
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
        DEBUGPORT.print(F("Device: "));
        DEBUGPORT.print(device);
        DEBUGPORT.println(F(" is write protected"));  
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
    FN_DISK_COPY:             // Not currently supported
    break;
//////////////////////////////////////////////////////////////////////////////
    FN_DISK_FORMAT:           // Not currently supported
    break;

    default:
    break;
  }
  sendByte(C_ETX);

  if (console) {
    DEBUGPORT.print(C_ETX);
    DEBUGPORT.print(" ");
  }

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

    if (console) DEBUGPORT.println("IDLE got EOT");
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
      
      if (console) DEBUGPORT.println("E_ACK");
    }
    else
    {
      sendByte(C_NAK);
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
    if(isValidFNC(b))
    {
      latestFNC = b;
      latestCKS += b;
      state = ST_HD_FNC;
    }
    else
    {
      state = ST_ERR;
    }
    break;
//////////////////////////////////////////////////////////////////////////////
    case ST_HD_FNC:
    latestSIZ = b;
    latestCKS += b;
    state = ST_HD_SIZ;
    break;
//////////////////////////////////////////////////////////////////////////////
    case ST_HD_SIZ:
    latestCKS += b;
    if(latestCKS == 0)
    {
      sendByte(C_ACK);
      if (console) DEBUGPORT.println("HD ACK");
      state = ST_HD_CKS;
    }
    else
    {
      sendByte(C_NAK);
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
      state = ST_ERR;
    }
    break;
//////////////////////////////////////// Text ////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

    case ST_TX_STX:
    textBuffer[textCount] = b;
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
      if (console) DEBUGPORT.println("TX ETX");
    }
    else
    {
      state = ST_ERR;
      if (console) DEBUGPORT.print("E");
    }
    break;
//////////////////////////////////////////////////////////////////////////////
    case ST_TX_ETX:
    latestCKS += b;
    if(latestCKS == 0)
    {
       sendByte(C_ACK);
       state = ST_TX_CKS; 
      if (console) DEBUGPORT.println("Got TX CKS");
    }
    else
    {
      state = ST_ERR;
    }
    break;
//////////////////////////////////////////////////////////////////////////////
    case ST_TX_CKS:
    if(b == C_EOT)
    {
      sendHeader();
      if (console) DEBUGPORT.println("SENT HDR");
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
      if (console) DEBUGPORT.println("SENT_TXT");
    }
    else
    {
      state = ST_ERR;
    }
    break;

//////////////////////////////////////////////////////////////////////////////
    case ST_SENT_TXT:
    if(b == C_ACK)
    {
      sendByte(C_EOT);
      state = ST_IDLE;
      if (console) DEBUGPORT.println("Sent EOT");
    }
    else if(b == C_NAK)
    {
      sendText();
    }
    debugLedOff();
    break;
//////////////////////////////////////////////////////////////////////////////
    case ST_ERR:
    if (console) DEBUGPORT.println("Err");
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
        commandCollector();
    }
    if (ledTime < millis()) {
      driveLedsOff();
    }
  }
}

void printDirectory(File dir, int numTabs, bool recursive) {
  while (console) {

    File entry =  dir.openNextFile();
    if (! entry) {
      // no more files
      break;
    }
    for (uint8_t i = 0; i < numTabs; i++) {
      DEBUGPORT.print(" ");
    }
    String name = entry.name();
    DEBUGPORT.print(name);
    if (entry.isDirectory()) {
      DEBUGPORT.println("/");
      if (recursive) printDirectory(entry, numTabs + 1, true);
    } else {
      // files have sizes, directories do not
      DEBUGPORT.print("\t");
      if (name.length() < 7) DEBUGPORT.print("\t");
      DEBUGPORT.println(entry.size(), DEC);
    }
    entry.close();
  } 
}


void commandCollector() {
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
        DEBUGPORT.println("Serial buffer overflow. Cleanup.");
        clearSerialBuffer();
        setBufPointer = 0;
      }
    }
  }
}

void(* resetFunc) (void) = 0; // create a standard reset function

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
      root = SD.open("/");
      DEBUGPORT.println(F("Root directory:"));
      printDirectory(root, 1, false);
      root.close();
      break;
    case 'H':
    case 'h':
    case '?':
      DEBUGPORT.println();
      DEBUGPORT.print(F("Usage ("));
      printVersion();
      DEBUGPORT.println(F("):"));

      DEBUGPORT.println(F(" C                - temp debug for driveNames[][]"));
      DEBUGPORT.println(F(" D                - SD-card root directory"));
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

void clearSerialBuffer() {
  byte i;
  for (i = 0; i < SERIALBUFSIZE; i++) {
    serialBuffer[i] = 0;
  }
}

// M command
void mountImage() {
  bool mountResult;
  char drive;
  if (setBufPointer == 1) { // list current mounted images
    if (console) {
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
    }
  } else { // mount a new image to a drive
    uint8_t bufSize = setBufPointer;
    // Mdnnnnnnnn.eee > maximal command length = 14
    if (bufSize > DRIVENAMESIZE+1) bufSize = DRIVENAMESIZE+1; 
    drive = toupper(serialBuffer[1]);
    if (drive >= 'D' or drive <= 'G') {
      mountResult =  remount(bufSize, drive);
    } else {
      if (console) {
        DEBUGPORT.print(F("Illegal drive: "));
        DEBUGPORT.println(drive);
      }
    }
  }
  if (!mountResult) {
    if (console) {
      DEBUGPORT.print(F("Mounting failed for "));
      DEBUGPORT.println(drive); 
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
    DEBUGPORT.println("' failed");
  }
  return false;
}

bool remount(uint8_t bufSize, char drive) {
    uint8_t driveIndex = drive - 'D';
    String command(serialBuffer);
    String filename = command.substring(2, bufSize);
    filename.toUpperCase();
    filename.trim(); // remove extra spaces
    if (console) {
      if (mountCheck(filename)) {
        DEBUGPORT.print("New file name for ");
        DEBUGPORT.print(drive);
    //    DEBUGPORT.print(" [");
    //    DEBUGPORT.print(driveIndex);
        DEBUGPORT.print(": ");
    //    DEBUGPORT.print(bufSize, DEC);
    //    DEBUGPORT.print("] ");
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
  if (console) {
    if (checkFilePresence(filename)) {
      DEBUGPORT.print(F("File: "));
      DEBUGPORT.print(filename);
      DEBUGPORT.println(F(" exists. Aborting."));
    } else {
      createFile(filename);
      DEBUGPORT.print("File created: ");
      DEBUGPORT.println(filename);
    } 
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
       DEBUGPORT.print("D: ");
       DEBUGPORT.println((writeProtect[0]) ? "RO" : "RW");
       DEBUGPORT.print("E: ");
       DEBUGPORT.println((writeProtect[1]) ? "RO" : "RW");
       DEBUGPORT.print("F: ");
       DEBUGPORT.println((writeProtect[2]) ? "RO" : "RW");
       DEBUGPORT.print("G: ");
       DEBUGPORT.println((writeProtect[3]) ? "RO" : "RW");
     }
   } else if (setBufPointer == 3) {
     char drive = toupper(serialBuffer[1]);
     uint8_t driveIndex = drive - 'D';
     bool wpStatus = (serialBuffer[2] == '1') ? true : false;
     if (drive >= 'D' or drive <= 'G') {
       writeProtect[driveIndex] = wpStatus;
       if (console) {
         DEBUGPORT.print(drive);
         DEBUGPORT.print(": ");
         DEBUGPORT.println((writeProtect[driveIndex]) ? "RO" : "RW");
       }
     } else {
      if (console) {
         DEBUGPORT.print("Illegal drive: ");
         DEBUGPORT.println(drive);
      }
     }
   } else {
     if (console) DEBUGPORT.println("unsupported");
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
      if (console) DEBUGPORT.println("Unknown drive");
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
