// 

#if defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)
#define BOARD_MEGA
#define PXPORT          Serial2
#define DEBUG           true       // Change to true for debugging
#define DEBUGPORT       Serial
#define CS_PIN          53           // SD card CS pin
#endif

#define MAX_TEXT   128

#define DEBUGBAUDRATE 115200

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

//////////////////////////////////////////////////////////////////////////////
/// for console/debug command interpreter

#define SERIALBUFSIZE         30
char serialBuffer[SERIALBUFSIZE];
byte setBufPointer = 0;
