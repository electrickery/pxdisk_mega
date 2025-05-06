// pxdisk.h
/// Based on pxDisk copyright(c) 2019 William R Cooke

bool console = true;

#if defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)
// On the Arduino Mega the SD-Card is on:
// 50 MISO
// 51 MOSI
// 52 SCK
// 53 CS
#define BOARD_MEGA
#define BOARDTEXT       "Mega2560"
#define PXPORT          Serial2    // pin 16 & 17
#define DEBUG           true       // Change to true for debugging
#define DEBUGPORT       Serial     // pin 0 & 1 and USB port
#define CS_PIN          53         // SD card CS pin
#define D_LED            9
#define E_LED           10
#define F_LED           11
#define G_LED           12
#define DEBUGLED        13
#endif

// The Pro Micro is equal to the Micro or Leonardo but smaller (and cheaper)
#if defined(__AVR_ATmega32U4__)
#define BOARD_MICRO
#define BOARDTEXT       "Pro_Micro"
#define PXPORT          Serial1    // pin 0 & 1
#define DEBUG           true       // Change to true for debugging
#define DEBUGPORT       Serial     // USB port
#define CS_PIN          10         // SD card CS pin
#define D_LED            2
#define E_LED            3
#define F_LED            4
#define G_LED            5
#define DEBUGLED         6
#endif

#define MAX_TEXT   128

#define DEBUGBAUDRATE 115200
#define PXBAUDRATE    38400

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
///  Device IDs that are relevant
//////////////////////////////////////////////////////////////////////////////
enum DeviceID
{
  ID_HX20   = 0x20,
  ID_PX8    = 0x22,
  ID_PX4    = 0x23,
  ID_FD1    = 0x31,
  ID_FD2    = 0x32,
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
  ST_TX_ACK,      /// <                                            18

  ST_SENT_HDR,    /// < Header was sent to Master                  19
  ST_SENT_TXT,    /// < Text was sent to Master                   :20
  
  ST_ERR,         /// < An error occurred in the state machine    :21 
  ST_UNDEFINED    /// < An undefined state -- should never happen :22
};

int oldState = ST_UNDEFINED; // Used to report state before ST_ERR

//////////////////////////////////////////////////////////////////////////////
///  Functions codes used in CP/M disk systems
//////////////////////////////////////////////////////////////////////////////
enum Functions
{
 FN_DISK_RESET          = 0x0d,    /// < RESET?
 FN_DISK_SELECT         = 0x0e,    /// < SELECT?
 FN_OPEN_FILE           = 0x0f,    /// < Opens a file to read
 FN_CLOSE_FILE          = 0x10,    /// < Closes file
 FN_FIND_FIRST          = 0x11,    /// < Find First
 FN_FIND_NEXT           = 0x12,    /// < Find First
 FN_CREATE_FILE         = 0x16,    /// < Creates file
 FN_RANDOM_READ         = 0x21,    /// < 
 FN_RANDOM_WRITE        = 0x22,    /// < 
 FN_COMPUTE_FS          = 0x23,    /// < Compute Filesize
 FN_DISK_READ_SECTOR    = 0x77,    /// < Read a single 128 byte sector
 FN_DISK_WRITE_SECTOR   = 0x78,    /// < Write a single 128 byte sector
 FN_DISK_WRITE_HST      = 0x79,    /// < CP/M WRITEHST flushes buffers
 FN_DISK_COPY           = 0x7a,    /// < Copy a complete disk -- not used
 FN_DISK_FORMAT         = 0x7c,    /// < Format a blank disk  -- not used
 FN_DISK_BOOT           = 0x80,    /// < Send BOOT.SYS
 FN_LOAD_OPEN           = 0x81,    /// < Relocate DBASIC.SYS
 FN_READ_BLOCK          = 0x83,    /// < Send DBASIC.SYS
 FN_IMAGE_CMDS          = 0xE0,    /// < Commands managing disk images
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
uint8_t filecnt[4];                  /// < Holds the filecount for each directory (A/, B/, C/, D/)
uint8_t textBuffer[MAX_TEXT];        /// < Buffer to hold incoming/outgoing text

///////////////////////////////////////////////////////
////////////////   SD card / file /////////////////////
#define  DISK_BYTES_PER_SECTOR       128L
#define  DISK_SECTORS_PER_TRACK       64L
#define  DISK_BYTES_PER_TRACK        (DISK_BYTES_PER_SECTOR * DISK_SECTORS_PER_TRACK)
#define  DISK_TRACKS_PER_DISK         40L
#define  DISK_SECTORS                (DISK_SECTORS_PER_TRACK * DISK_TRACKS_PER_DISK)

//////////////////////////////////////////////////////////////////////////////
///  diskNames
///
///  @brief  Holds filenames of current image files
///
//////////////////////////////////////////////////////////////////////////////
#define DRIVECOUNT 4
#define DRIVENAMESIZE 13
char diskNames[DRIVECOUNT][DRIVENAMESIZE] = 
{
  "d.img", 
  "e.img", 
  "f.img", 
  "g.img"
}; 

bool writeProtect[DRIVECOUNT] = {0, 0, 0, 0};

bool ledOn;

uint32_t ledTime;
#define LEDTIMEOUT 250

enum PFBDKFuncs
{
  PFC_DIR    = 'D',
  PFC_MOUNT  = 'M',
  PFC_NEWIMG = 'N',
  PFC_PROT   = 'P',
  PFC_RST    = 'R',
};

// Size of the returned block -1
#define DTEXTSIZE 128
#define MTEXTSIZE  64
#define NTEXTSIZE   0
#define PTEXTSIZE  16
#define RTEXTSIZE   0

//////////////////////////////////////////////////////////////////////////////
/// for console/debug command interpreter

#define SERIALBUFSIZE         30

char serialBuffer[SERIALBUFSIZE];
byte setBufPointer = 0;
bool consoleCommand = false;
char latestE0Command;
uint8_t receivedSize;
#define LF 0x0A

// directory text block
#define ENTRIESPERMESSAGE 8
#define ENTRYSIZE         16
#define ENTRYNAMESIZE     12
