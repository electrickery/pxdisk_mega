/////////////////////////////////////////////////
///  pxdisk
///  copyright(c) 2019 William R Cooke
///
///  Use an SD card as multiple disk drives for 
///  Epson PX-8 and PX-4 CP/M laptop computers
/////////////////////////////////////////////////

#include <SD.h>

#if defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)
#define BOARD_MEGA
#define PXPORT          Serial2
#define DEBUG           true       // Change to true for debugging
#define DEBUGPORT       Serial
#define CS_PIN          53           // SD card CS pin
#endif
//
//#if defined(__AVR_ATmega328P__) || defined(__AVR_ATmega168__)
//#define BOARD_UNONANO
//#define PXPORT          Serial
//#define DEBUG           false        // !! Can't be used on these boards! !!
//#define CS_PIN          10           // SD card CS pin
//#endif

//#if defined(__AVR_ATMega4809__)
//#define BOARD_NANOEVERY
//#define PXPORT          Serial1
//#define DEBUG           true         // !! Change to true for debugging
//#define DEBUGPORT       Serial
//#define CS_PIN          10           // SD card CS pin
//#endif

#define MAX_TEXT   128
#define MAJOR_VERSION 2
#define MINOR_VERSION 0
#define BUILD         0

#define DEBUGBAUDRATE 115200
#define D_LED 9
#define E_LED 10
#define F_LED 11
#define G_LED 12

//////////////////////////////////////////////////////////////////////////////
///  Special characters used by the state machine.
//////////////////////////////////////////////////////////////////////////////
enum Characters
{
  C_SOH     =  0x01,         /// < Start of Header
  C_STX     =  0x02,         /// < Start of Text
  C_ETX     =  0x03,         /// < End of Text
  C_EOT     =  0x04,         /// < End of Transmission
  C_ENQ     =  0x05,         /// < Enquiry
  C_ACK     =  0x06,         /// < Acknowledge
  C_NAK     =  0x15,         /// < Negative Acknowledge
  C_FMT_MS  =  0x00,         /// < Format message from Master to Slave
  C_FMT_SM  =  0x01,         /// < Format message from Slave to Master
  C_SEL     =  0x31,         /// < Select command
  
};

const byte MY_ID_1 = 0x31;    /// < Id of first drive unit
const byte MY_ID_2 = 0x32;    /// < Id of second drive unit

//////////////////////////////////////////////////////////////////////////////
///  Device IDs that are relavant
//////////////////////////////////////////////////////////////////////////////
enum DeviceID
{
  ID_HX20   = 0x20,
  ID_PX8    = 0x22,
  ID_PX4    = 0x23,
  ID_FD1    = 0x31,
  ID_FD2    = 0x32
  
};

//////////////////////////////////////////////////////////////////////////////
///  States used by the state machine.
//////////////////////////////////////////////////////////////////////////////
enum States
{
  ST_BEGIN,       /// < Starting State                             :0     
  ST_IDLE,        /// < Returns to IDLE after all major blocks     :1
  ST_PS_POL,      /// < Received POLL from IDLE (not used)         :2
  ST_PS_SEL,      /// < Received SELECT from IDLE                  :3
  ST_PS_DID,      /// < Received Destination ID after SELECT       :4
  ST_PS_SID,      /// < Received Source ID after Destination ID     5
  ST_PS_ENQ,      /// < Received Enquiry after Source ID            6
   
  ST_HD_SOH,      /// < Received SOH from IDLE                      7
  ST_HD_FMT,      /// < Received Format character after SOH         8
  ST_HD_DID,      /// < Received Destination ID after Format        9
  ST_HD_SID,      /// < Received Source ID after Destination ID    10
  ST_HD_FNC,      /// < Received Function code after Source ID     11
  ST_HD_SIZ,      /// < Received Text Size after Function code     12
  ST_HD_CKS,      /// < Received header Checksum after Size        13

  ST_TX_STX,      /// < Received STX from IDLE                     14
  ST_TX_TXT,      /// < Received SIZ bytes of Text after STX       15
  ST_TX_ETX,      /// < Received ETX after Text                    16
  ST_TX_CKS,      /// < Received Text Checksum after ETX           17
  ST_TX_ACK,      /// <  18

  ST_SENT_HDR,    /// < Header was sent to Master                  19
  ST_SENT_TXT,    /// < Text was sent to Master                   :20
  
  ST_ERR,         /// < An error occurred in the state machine    :21 
  ST_UNDEFINED    /// < An undefined state -- should never happen :22
};

//////////////////////////////////////////////////////////////////////////////
///  Functions codes used in CP/M disk systems
//////////////////////////////////////////////////////////////////////////////
enum Functions
{
  FN_DISK_RESET           = 0x0d,    /// < RESET?
  FN_DISK_SELECT          = 0x0e,    /// < SELECT?
  FN_DISK_READ_SECTOR     = 0x77,    /// < Read a single 128 byte sector
  FN_DISK_WRITE_SECTOR    = 0x78,    /// < Write a single 128 byte sector
  FN_DISK_WRITE_HST       = 0x79,    /// < CP/M WRITEHST flushes buffers
  FN_DISK_COPY            = 0x7a,    /// < Copy a complete disk -- not used
  FN_DISK_FORMAT          = 0x7c,    /// < Format a blank disk  -- not used
};


//////////////////////////////////////////////////////////////////////////////
///  BDOS return codes from BIOS disk functions 
//////////////////////////////////////////////////////////////////////////////
enum BDOS_ReturnCodes
{
  BDOS_RC_SUCCESS         = 0,       /// < Successful Completion
  BDOS_RC_READ_ERROR      = 0xfa,    /// < Disk Read Error
  BDOS_RC_WRITE_ERROR     = 0xfb,    /// < Disk Write Error
  BDOS_RC_SELECT_ERROR    = 0xfc,    /// < Disk Select Error
  BDOS_RC_DISK_WP_ERROR   = 0xfd,    /// < Disk Write Protect Error
  BDOS_RC_FILE_WP_ERROR   = 0xfe,    /// < Disk Write Protect Error
};

uint32_t count = 0;
int16_t selectedDevice = -1;         /// < Latest device selected ( 1 or 2 )
uint8_t latestDID = 0;               /// < Latest received Destination ID
uint8_t latestSID = 0;               /// < Latest received Source ID
uint8_t latestFNC = 0;               /// < Latest received Funcion code
uint8_t latestSIZ = 0;               /// < Latest received Text size
uint8_t latestCKS = 0;               /// < Latest received calculated checksum

uint8_t textBuffer[MAX_TEXT];        /// < Buffer to hold incoming/outgoing text


///////////////////////////////////////////////////////
////////////////   SD card / file /////////////////////
#define  DISK_BYTES_PER_SECTOR       128L
#define  DISK_SECTORS_PER_TRACK       64L
#define  DISK_BYTES_PER_TRACK        (DISK_BYTES_PER_SECTOR * DISK_SECTORS_PER_TRACK)
#define  DISK_TRACKS_PER_DISK         40L


//////////////////////////////////////////////////////////////////////////////
///  diskNames
///
///  @brief  Holds filenames of current image files
///
//////////////////////////////////////////////////////////////////////////////
char diskNames[4][12] = 
{
  "D.pfd", 
  "E.pfd", 
  "F.pfd", 
  "G.pfd"
}; 

// TODO:  Change to "d.img", "e.img", "f.img", "g.img"


//////////////////////////////////////////////////////////////////////////////
///  @fn Setup
///
///  @brief  Initializes all I/O and data structures
///
//////////////////////////////////////////////////////////////////////////////
void setup() 
{
#if DEBUG
  DEBUGPORT.begin(DEBUGBAUDRATE);
  DEBUGPORT.print("Beginning PXdisk_mega ");
  DEBUGPORT.print(MAJOR_VERSION);
  DEBUGPORT.print(".");
  DEBUGPORT.print(MINOR_VERSION);
  DEBUGPORT.print(".");
  DEBUGPORT.print(BUILD);
  DEBUGPORT.println("...");
#endif

  PXPORT.begin(38400);
  int sd = SD.begin(CS_PIN);

  // TODO:  Why in the haydes do we need to do this?
  // It appears to be a timing issue.  First use of a read 
  // seems to get everything initialized.  If we wait until needed
  // maybe it is taking too long and px8 times out?

  diskReadSector(0, 0, 0, 1, textBuffer);
  diskReadSector(0, 1, 0, 1, textBuffer);
  diskReadSector(1, 0, 0, 1, textBuffer);
  diskReadSector(1, 1, 0, 1, textBuffer);
  DEBUGPORT.println("Done init read");

  File root = SD.open("/");
  DEBUGPORT.println("Root directory:");
  printDirectory(root, 0, false);
  
  digitalWrite(D_LED, LOW);
  digitalWrite(E_LED, LOW);
  digitalWrite(F_LED, LOW);
  digitalWrite(G_LED, LOW);
  pinMode(D_LED, OUTPUT);
  pinMode(E_LED, OUTPUT);
  pinMode(F_LED, OUTPUT);
  pinMode(G_LED, OUTPUT);

  setLEDs(0xFF);
  delay(500);
  setLEDs(0);
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
//  File dsk = SD.open(diskNames[device], FILE_READ);
  File dsk = SD.open(diskNames[device], (O_READ | O_WRITE | O_CREAT));
  setLEDs(device);
#if DEBUG
  DEBUGPORT.println(diskNames[device]);
#endif
  if(dsk)
  {
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
   }
  else
  {
    rtn = false;
#if DEBUG
    DEBUGPORT.println("READ FAIL");
#endif
  }
  setLEDs(0);

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
  File dsk = SD.open(diskNames[device], FILE_WRITE);
  setLEDs(device);
  if(dsk)
  {
    dsk.seek(track * DISK_BYTES_PER_TRACK + (sector - 1) * DISK_BYTES_PER_SECTOR);
    dsk.write(buffer, DISK_BYTES_PER_SECTOR);
    dsk.close();
#if DEBUG
    DEBUGPORT.write("W:");
    DEBUGPORT.println(diskNames[device]);
    DEBUGPORT.println(PXPORT.available());
#endif 
  }
  else
  {
    rtn = false;
#if DEBUG
    DEBUGPORT.println("WRITE FAIL");
#endif
  }
  setLEDs(0);
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
#if DEBUG
  DEBUGPORT.write(hexNibble( b>>4));
  DEBUGPORT.write(hexNibble( b & 0xf));
  DEBUGPORT.write( 0x20);
  DEBUGPORT.write(13);
  DEBUGPORT.write(10);
#endif
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
  /// TODO: fix this
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
#if DEBUG
  DEBUGPORT.print("S_T: ");
#endif

  uint8_t cks = 0;
  uint8_t returnCode = 0;
  sendByte(C_STX);
  cks -= C_STX;
  uint8_t i = 0;
  bool returnStatus = true;
#if DEBUG
  {
    DEBUGPORT.print(C_STX);
    DEBUGPORT.print(" ");
  }
#endif
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
    
#if DEBUG
    DEBUGPORT.print("SEL");
    DEBUGPORT.print(0);
    DEBUGPORT.print(" ");
#endif

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

#if NOTDEBUG
      if(i % 64 == 0) DEBUGPORT.println();
      showHex(textBuffer[i]);
      DEBUGPORT.write((byt & 0x7f));
#endif

    }
    sendByte(returnCode);  // return code
    cks -= 0;
    break;
//////////////////////////////////////////////////////////////////////////////
    case FN_DISK_WRITE_SECTOR:
    returnStatus = diskWriteSector(selectedDevice, textBuffer[0] - 1, 
                   textBuffer[1], textBuffer[2], &textBuffer[4]);
    if(!returnStatus) // error!
    {
      returnCode = BDOS_RC_WRITE_ERROR;
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

#if DEBUG
  DEBUGPORT.print(C_ETX);
  DEBUGPORT.print(" ");
#endif

  cks -= C_ETX;
  sendByte(cks);
  
#if DEBUG
  DEBUGPORT.println(cks);
#endif
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
    }
    else if(b == C_SOH)
    {
      latestCKS = C_SOH;
      state = ST_HD_SOH;
    }
    else if(b == C_STX)
    {
      latestCKS = b;
      textCount = 0;
      state = ST_TX_STX;
    }
    else if(b == C_EOT)
    {
#if DEBUG
       DEBUGPORT.println("IDLE got EOT");
#endif
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
      
#if DEBUG
      DEBUGPORT.println("E_ACK");
#endif

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
#if DEBUG
      DEBUGPORT.println("HD ACK");
#endif
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
#if DEBUG
      DEBUGPORT.println("TX ETX");
#endif
    }
    else
    {
      state = ST_ERR;
#if DEBUG
      DEBUGPORT.print("E");
#endif
    }
    break;
//////////////////////////////////////////////////////////////////////////////
    case ST_TX_ETX:
    latestCKS += b;
    if(latestCKS == 0)
    {
       sendByte(C_ACK);
       state = ST_TX_CKS; 
#if DEBUG
      DEBUGPORT.println("Got TX CKS");
#endif
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
#if DEBUG
      DEBUGPORT.println("SENT HDR");
#endif
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
#if DEBUG
      DEBUGPORT.println("SENT_TXT");
#endif
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
#if DEBUG
      DEBUGPORT.println("Sent EOT");
#endif
    }
    else if(b == C_NAK)
    {
      sendText();
    }
    break;
//////////////////////////////////////////////////////////////////////////////
    case ST_ERR:
#if DEBUG
    DEBUGPORT.println("Err");
#endif
    state = ST_IDLE;
    break;

    default:
    break;
    
  }
#if DEBUG
  DEBUGPORT.println(state);
#endif
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
#if DEBUG
      DEBUGPORT.print('!'); 
      DEBUGPORT.print(av);
#endif
    }
    
    while(PXPORT.available() == 0);   // Wait for something to come in
    
    uint8_t b = receiveByte();
#if NOTDEBUG   
    DEBUGPORT.print(millis());
    DEBUGPORT.print(" : ");
    showHex(b);
#endif
    stateMachine(b);
  }
}

void setLEDs(byte ledPatt)
{
    digitalWrite(D_LED, HIGH);
    digitalWrite(E_LED, HIGH);
    digitalWrite(F_LED, HIGH);
    digitalWrite(G_LED, HIGH);
    if (ledPatt && 0b00000001) digitalWrite(D_LED, LOW);
    if (ledPatt && 0b00000010) digitalWrite(E_LED, LOW);
    if (ledPatt && 0b00000100) digitalWrite(F_LED, LOW);
    if (ledPatt && 0b00001000) digitalWrite(G_LED, LOW);
}

void printDirectory(File dir, int numTabs, bool recursive) {
  while (true) {

    File entry =  dir.openNextFile();
    if (! entry) {
      // no more files
      break;
    }
    for (uint8_t i = 0; i < numTabs; i++) {
      DEBUGPORT.print('\t');
    }
    DEBUGPORT.print(entry.name());
    if (entry.isDirectory()) {
      DEBUGPORT.println("/");
      if (recursive) printDirectory(entry, numTabs + 1, true);
    } else {
      // files have sizes, directories do not
      DEBUGPORT.print("\t\t");
      DEBUGPORT.println(entry.size(), DEC);
    }
    entry.close();
  }
}
