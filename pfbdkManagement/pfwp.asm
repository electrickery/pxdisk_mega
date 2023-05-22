;       ********************************************************
;               PFBDK write protect
;       ********************************************************
;
;       NOTE :
;               This program controls the write
;               protect bits for the drives D:,
;               E:, F: and G:
;
;       USAGE :
;               PFWP d p 
;               where:
;                       d is D, E, F or G
;                       p is 0 for write protect, 1 for write enable

;       <> assemble condition <>
;
;        .Z80
;
;       <> loading address <>
;
                ORG     0100h   
;
;       <> constant values <>
;
;       BIOS entry
;
WBOOT           EQU     0EB03H          ; Warm Boot entry
CONST           EQU     WBOOT   +03H    ; Console status entry
CONIN           EQU     WBOOT   +06H    ; Console in entry
CONOUT          EQU     WBOOT   +09H    ; Console out entry
CALLX           EQU     WBOOT   +66H    ; Call extra entry
;
;       System area
;
DISBNK          EQU     0F52EH          ;
PKT_TOP         EQU     0F931H          ;
PKT_FMT         EQU     PKT_TOP
PKT_RDT         EQU     0F936H
SCRCH_BUF       EQU     0F93AH          ;
PKT_STS         EQU     0F9B6H
ARGS            EQU     00080h            ; 80h arg-size, 81h is space, 82h 1st arg char
;
;       Bank value
;
SYSBANK         EQU     0FFH            ; System bank
BANK0           EQU     000H            ; Bank 0 (RAM)
BANK1           EQU     001H            ; Bank 1 (ROM capsel 1)
BANK2           EQU     002H            ; Bank 2 (ROM capsel  2)
;
;       OS ROM jump table
;
EPSPSND         EQU     0030H
EPSPRCV         EQU     0033H
;
;
FMT             EQU     00h
DID             EQU     31H
SID             EQU     23H
FNC             EQU     0E0H
;SIZ             EQU     03H
;
;
BREAKKEY        EQU     03H     ;BREAK key code
LF              EQU     0AH     ;Line Feed code
CR              EQU     0DH     ;Carriage return code
SPCCD           EQU     20H     ;Space code
PERIOD          EQU     2EH     ;Period code
QMARK           EQU     3FH     ;Question mark code (3FH)
;
;
TERMINATOR      EQU     00H     ;Terminator code
;
;       BDOS function code table.
;
SPWK            EQU     01000H          ;SP bottom address
;
;       ********************************************************
;               MAIN PROGRAM
;       ********************************************************
;
;       NOTE :
;               Check args & send to PFBDK.
MAIN:
        LD      SP,SPWK         ; Set Stack pointer
;
READ:
        CALL    BANNER

        CALL    BREAKCHK        ; Check BREAK key (CTRL/C) press or not
        JP      Z,ABORT
        
        CALL    CHKARGS         ; Check arguments        
        JP      NZ, USAGE
;
        CALL    SENDCMD         ; Send command to PFBDK.
        JP      NZ,DISKERR      ; Disk access error.
        
        LD      A,(PKT_STS)     ; Return parameter.
        OR      A               ;
        JP      NZ,READERR      ; Read error.
;
        CALL    PRDATA          ; Display FDD data.
        
;
        JP      WBOOT
;
;       ********************************************************
;               SEND P-COMMAND with optional drive and wp-flag
;       ********************************************************
;
;       NOTE :
;
;       <> entry parameter <>
;               NON
;       <> return parameter <>
;               Same as EPSPSND
;       <> preserved registers <>
;               NON
;
;       CAUTION :
SENDCMD:
        LD      HL,PKT_FMT      ; EPSP packet top address.
        XOR     A               ;  Set FMT code.
        LD      (HL),A          ;
        INC     HL              ;
        LD      (HL),DID        ;  Set DID code.
        INC     HL              ;
        LD      (HL),SID        ;  Set SID code.
        INC     HL              ;
        LD      (HL),FNC        ;  Set FNC code.
        INC     HL              ;
        CALL    GETARGCNT
        LD      (HL), A
        INC     HL              ;
        LD      A,'P'           ;  Set P command.
        LD      (HL),A          ;
        INC     HL              ;
        LD      A,(DRIVE)       ;  Set drive.
        LD      (HL),A          ;
        INC     HL              ;
        LD      A,(WPFLAG)      ;  Set write protct flag.
        LD      (HL),A          ;
;
        LD      A,SYSBANK       ; Select OS bank.
        LD      (DISBNK),A      ;
        LD      IX,EPSPSND      ; Call address (EPSP send)
        LD      A,01H           ; Receive after send.
        LD      HL,PKT_TOP      ; Packet top address.
        CALL    CALLX           ; Go !!
;
        RET
        
GETARGCNT:
        LD      A, (ARGS)
        CP      0
        RET     Z
        LD      A, 2
        RET

;80h arg-size, 81h is space, 82h 1st arg char   04 20 44 20 31
CHKARGS:
        LD      HL, ARGS
        
        LD      A, (HL)         ; args length
        LD      (SIZ), A
        CP      '0'
        JR      Z, CANOARG
        
        INC     HL              ; should be a space
        INC     HL              ; should point to drive letter
        LD      A, (HL)         ; should be drive (D-G)
        AND     0DFh            ; to upper case
        CP      'D'
        JP      Z, CADROK
        CP      'E'
        JP      Z, CADROK
        CP      'F'
        JP      Z, CADROK
        CP      'G'
        JP      Z, CADROK
        RET                     ; NZ, not D, E, F or G
        
CADROK:
        LD      (DRIVE), A
        INC     HL              ; should be a space

        INC     HL              ; should point to wp-flag
        LD      A, (HL)         ; expect '0' or '1'
        CP      '0'
        JP      Z, CAWPOK
        CP      '1'
        JP      Z, CAWPOK
        RET                     ; NZ, not 0 or 1
        
CAWPOK:
        LD      (WPFLAG), A
        LD      A, 03h          ; set size to 3 bytes; P, <drive>, <flag>
        LD      (SIZ), A
        CP      A               ; clear Z flag
        RET                     ; Z - args are OK
        
CANOARG:
        LD      A, 01h          ; set size to 1 byte; P
        LD      (SIZ), A
        CP      A               ; clear Z flag
        RET

;      
BANNER:
        LD      HL,BANNERMSG     ;FDD access error message.
        CALL    DSPMSG
        RET
      
DUMPCHR:
        PUSH    AF
        PUSH    HL        
        LD      C,A
        CALL    CONOUT
        POP     HL
        POP     AF
        RET
 
PRDATA:
        LD      A, (PKT_RDT + 2)
        CP      '0'
        JR      NZ, PRDRO
        LD      A, 'W'
        LD      (PMD), A
        JR      PRE
        
PRDRO:
        LD      A, 'O'
        LD      (PMD), A
PRE:        
        LD      A, (PKT_RDT + 6)
        CP      '0'
        JR      NZ, PRERO
        LD      A, 'W'
        LD      (PME), A
        JR      PRF    
            
PRERO:
        LD      A, 'O'
        LD      (PME), A
PRF:        
        LD      A, (PKT_RDT + 10)
        CP      '0'
        JR      NZ, PRFRO
        LD      A, 'W'
        LD      (PMF), A
        JR      PRG 
               
PRFRO:
        LD      A, 'O'
        LD      (PMF), A
PRG:     
        LD      A, (PKT_RDT + 14)
        CP      '0'
        JR      NZ, PRGRO
        LD      A, 'W'
        LD      (PMG), A
        JR      PREND  
              
PRGRO:
        LD      A, 'O'
        LD      (PMG), A
PREND:        
        LD      HL, PRTMSG
        CALL    DSPMSG
        RET






;
;********************************
;*                              *
;*      ERROR ROUTINES          *
;*                              *
;********************************
;
DISKERR:
        LD      HL,DKERRMSG     ;FDD access error message.
        CALL    DSPMSG
        JP      WBOOT
ABORT:
        LD      HL,ABORTMSG     ;Display ABORT message.
        CALL    DSPMSG
        JP      WBOOT
USAGE:
        LD      HL, USAGEMSG
        CALL    DSPMSG
        JP      WBOOT
READERR:
        LD      HL,RDERRMSG     ;FDD read error message.
        CALL    DSPMSG
        JP      WBOOT


;
;*************
;*   DSPMSG   *
;*************
;
;       Display string data to the console terminated by 00H.
;
;       on entry : HL = Top address of string data
;                       Data 00H is a terminator of string.
;
;       on exit  : none
;
;       Registers are not preserved.
;
DSPMSG:
        LD      A,(HL)          ;Display data
        OR      A               ;Check terminator
        RET     Z               ;If find terminator then Return
;
        PUSH    HL
        PUSH    BC
        LD      C,A
        CALL    CONOUT          ;Display data to the console
        POP     BC
        POP     HL
        INC     HL              ;Update pointer
        JR      DSPMSG          ;Repeat DSPMSG until find terminator
;
;
;
;***************
;*   DSPDATA   *
;***************
;
;       Convert 1 byte data, that addressed by IX, to HET format and
;       LIST out it to printer. And store character image of data to
;       CHRPKT.
;
;       on entry : B = Data quantity that to be LIST out
;                  (READPTR) = Indicate data address
;
;       on exit  : (READPTR) = Next data address
;                  Character image of datas are stored behind CHRPKT.
;
;       Registers are not preserved.
;
DSPDATA:
        LD      A,B
        OR      A
        RET     Z               ;If data quqntity = 0 then return
;
        LD      HL,CHRPKT       ;HL=Start address of character data
        LD      IX,(READPTR+0)    ;IX=MCT data top address
DSPDT00:
        PUSH    BC
        PUSH    HL
        PUSH    IX
;
        LD      A,(IX+0)          ;A=DATA
        LD      (HL),PERIOD     ;Store PERIOD mark (default data)
        BIT     7,A             ;If data is 80H through FFH or 00H through
        JR      NZ,DSPDT10      ;1FH then change to PERIOD mark and store
        CP      SPCCD           ;it in CHRPKT
        JR      C,DSPDT10       ;
        LD      (HL),A          ;Store read data to CHRPKT
DSPDT10:
        CALL    TOHEX           ;Convert to HEX
        PUSH    BC              ;Save lower 4 bit hex data
        LD      C,B
        CALL    CONOUT          ;LIST out upper 4bit hex data
        POP     BC
        CALL    CONOUT          ;LIST   out lower 4bit hex data
        LD      C,SPCCD
        CALL    CONOUT          ;List out space
;
        POP     IX
        POP     HL
        POP     BC
        INC     HL
        INC     IX
        DJNZ    DSPDT00         ;Repeat until B=0
;
        LD      (READPTR),IX    ;Store next data address
        LD      (HL),TERMINATOR ;Store terminator of character data
;
        RET
;
;*************
;*   TOHEX   *
;*************
;
;       Convert input data (A reg) to 2 byte HEX data (BC reg)
;
;       on entry :  A = input data
;
;       on exit  : BC = HEX data of input data
;                       (B = upper 4bit data)
;                       (C = lower 4bit data)
;
TOHEX:
        PUSH    AF
;
        RRA                     ;Shift upper4bit to lower 4 bit
        RRA
        RRA
        RRA
;
        CALL    TOHEX10         ;Convert upper 4bit
        LD      B,A
;
        POP     AF              ;Convert lower 4bit
        CALL    TOHEX10
        LD      C,A
        RET
;
;
;***************
;*   TOHEX10   *
;***************
;
;       Convert lower 4bit of input data to HED dat.
;
;          entry : A = input data
;       on exit  : A = HEX data of input data lower 4bit
;
TOHEX10:
        AND     0FH             ;Mask upper 4bit
        CP      0AH 
        JR      C,TOHEX20
;
        ADD     A,07H           ;If 0AH through 0FH then "A" to "F"
;
TOHEX20:
        ADD     A,30H
        RET
;
;****************
;*   BREAKCHK   *
;****************
;
;       Check BREAK key (CTRL/C) press or not
;
;       on entry : none
;
;       on exit  : Z flag = 1 --- Break key is pressed
;                         = 0 --- Break key is not pressed
;
BREAKCHK:
        CALL    CONST
        INC     A
        RET     NZ              ;If key buffer is empty the return
;
        CALL    CONIN           
        CP      BREAKKEY        ;Check bREAK key or not
        RET     Z               ;If BREAK key then return
        JR      BREAKCHK        ;Repeat BREAKCHK until buffer is empty
;
CHKWAIT:
        CALL    CONST
        INC     A
        RET     NZ
;
        CALL    CONIN
        CP      BREAKKEY
        JP      Z,WBOOT
        CP      SPCCD
        JR      NZ,CHKWAIT
;
        CALL    CONIN
        CP      BREAKKEY
        JP      Z,WBOOT
        RET

;
;
;************************
;*                      *
;*      WORK AREA       *
;*                      *
;************************
;

DRIVE:
        DEFB      'D'
WPFLAG:
        DEFB      '1'
READPTR:
        DEFS      2               ;Pointer of READPKT
CHRPKT:
        DEFS      20              ;Character data packet of MCT read data
SIZ:    
        DEFB      01h
;
;************************
;*                      *
;*      MESSAGE AREA    *
;*                      *
;************************
;
CRLFMSG:  
        DEFB      CR, LF
        DEFB      TERMINATOR
;
BANNERMSG:
        DEFB      CR, LF
        DEFB      'PFBDK 1.0'
        DEFB      CR, LF
        DEFB      TERMINATOR
        
DKERRMSG:
        DEFB      CR, LF
        DEFB      'PFBDK access error !!'
        DEFB      CR, LF
        DEFB      TERMINATOR
;
ABORTMSG:
        DEFB      CR, LF
        DEFB      'Aborted'
        DEFB      CR, LF
        DEFB      TERMINATOR
        
USAGEMSG:
        DEFB      CR, LF
        DEFB      'Usage: PFWP <drive> <wp-state>'
        DEFB      CR, LF
        DEFB      ' drive = D, E, F, G.' 
        DEFB      CR, LF
        DEFB      ' wp-state: 0 = read-only, 1 = read/write'
        DEFB      CR, LF
        DEFB      TERMINATOR

RDERRMSG:
        DEFB      CR,LF
        DEFB      'PFBDK read error !!'
        DEFB      CR,LF
        DEFB      TERMINATOR
        
ROMSG:
        DEFB     'RO'
        DEFB      TERMINATOR
        
RWMSG:
        DEFB     'RW'
        DEFB      TERMINATOR

PRTMSG:
        DEFB    'D: R'
PMD:
        DEFB    '-'
        DEFB    CR, LF
        DEFB    'E: R'
PME:
        DEFB    '-'
        DEFB    CR, LF
        DEFB    'F: R'
PMF:
        DEFB    '-'
        DEFB    CR, LF
        DEFB    'G: R'
PMG:
        DEFB    '-'
        DEFB    CR, LF
        DEFB      TERMINATOR

;
;
        END


;PKT_TOP         EQU     0F931H          ;
;PKT_FMT         EQU     PKT_TOP

;PKT_TOP + 0  FMT code (0).
;PKT_TOP + 1  DID code
;PKT_TOP + 2  SID code
;PKT_TOP + 3  FNC code
;PKT_TOP + 4  SIZ data (n)
;PKT_TOP + 5  data 0 'P'
;PKT_TOP + 6  data 1 <drive>; DEFG
;PKT_TOP + 7  data 2 <wpflag<; 01
;...
;PKT_TOP + (5+n)  data n
