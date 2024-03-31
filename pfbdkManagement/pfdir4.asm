;       ********************************************************
;               PFBDK SD card directory
;       ********************************************************
;
;       NOTE :
;               This program requests and displays the contents of the 
;               root directory of the SD card. The directory listing is 
;               retrieved in parts, where each part is 128 bytes large 
;               and contains 8 filenames / file sizes.
;
;       USAGE :
;               PFDIR4 

;       <> assemble condition <>
;
;        .Z80
;
;       <> loading address <>
;
                ORG     0100h   
;
;       <> constant values <>

MAJOR_V         EQU     '1'
MINOR_V         EQU     '1'

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
PKT_TOP         EQU     0F931H          ; Epsp header (0-4) and text (5- ) send buffer
PKT_FMT         EQU     PKT_TOP
PKT_RDT         EQU     0F936H          ; Epsp text receive buffer
SCRCH_BUF       EQU     0F93AH          ;
PKT_STS         EQU     0F9B6H
ARGS            EQU     00080h            ; 80h arg-size, 81h is space, 82h 1st arg char
;
;       Bank value
;
SYSBANK         EQU     0FFH    ; System bank
BANK0           EQU     000H    ; Bank 0 (RAM)
BANK1           EQU     001H    ; Bank 1 (ROM capsel 1)
BANK2           EQU     002H    ; Bank 2 (ROM capsel  2)
;
;       OS ROM jump table
;
EPSPSND         EQU     0030H
EPSPRCV         EQU     0033H
;
;       EPSP
FMT             EQU     00h
DID             EQU     31H
SID             EQU     23H
FNC             EQU     0E0H    ; Epsp command selected for PFBDK management. 
                                ; first byte in the text block is the PFBDK command
SENDSIZ         EQU     01H     ; Variable value; 0 for one byte, 1 for two bytes
ENTPPART        EQU     08H     ; Number of directory entries per part
;
;
BREAKKEY        EQU     03H     ; BREAK key code
LF              EQU     0AH     ; Line Feed code
CR              EQU     0DH     ; Carriage return code
SPCCD           EQU     20H     ; Space code
PERIOD          EQU     2EH     ; Period code
QMARK           EQU     3FH     ; Question mark code (3FH)
FILNAMSZ        EQU     12      ; 8.3 
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

        CALL    GETARGSZ
        JP      NZ, USAGE       ; 
PARTLOOP:

        CALL    DSPNO
;
        CALL    SENDCMD         ; Send command to PFBDK.
        JP      NZ,DISKERR      ; Disk access error.

        LD      A,(PKT_STS)     ; Return parameter.
        OR      A               ;
        JP      NZ,READERR      ; Read error.
;
        CALL    PRDATA          ; Display FDD data.
        
        CALL    CONIN
        CP      ' '
        JR      Z,PLNEXT
        CP      CR
        JR      Z,PLNEXT
        CP      LF
        JR      Z,PLNEXT
        
PLDONE:
        JP      WBOOT
        
PLNEXT:        
        LD      A, (DIRPART)
        INC     A
        LD      (DIRPART), A
        JR      PARTLOOP
;
;
;       ********************************************************
;               SEND D-COMMAND with drive number
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
        LD      HL, PKT_TOP     ; EPSP packet top address.
        XOR     A               ;  Set FMT code.
        LD      (HL), A         ;
        INC     HL              ;
        LD      (HL), DID       ;  Set DID code.
        INC     HL              ;
        LD      (HL), SID       ;  Set SID code.
        INC     HL              ;
        LD      (HL), FNC       ;  Set FNC code.
        INC     HL              ;
        LD      A, SENDSIZ      ; 
        LD      (HL), A
        INC     HL              ;
        LD      A, 'D'          ;  Set DIR command.
        LD      (HL),A          ;
        INC     HL              ;
        LD      A,(DIRPART)     ;  Set dir part (eight entries per part).
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

; Check the size of the argument buffer. The argument is the part of the directory.
;                                       There are 3 options:
;                                          0: no arguments - set part to 0
;                                          2: 2 chars, a space and a number. The
;                                             number is interpreted as part number.
;                                      other: error situation. NZ triggers usage msg.
GETARGSZ:
        LD      A, (ARGS)
        CP      0
        JR      Z, GA0ARG
        CP      2
        JR      Z, GA1ARG
        RET                     ; NZ : inproper args
        
GA0ARG:                         ; No arguments, assuming part 0
        LD      HL, GA0ARGMSG
        CALL    DSPMSG
        LD      A, '0'          ; emulate ASCII input
        LD      (DIRPART), A
        RET
        
GA1ARG:                         ; One argument (space and number)
        LD      HL, ARGS
        INC     HL              ; space
        INC     HL              ; arg char
        LD      A, (HL)
        LD      (DIRPART), A
        
        RET

; send the characher in A to the console. Send CR & LF to console.
DSPNO:
        LD      C, A
        CALL    CONOUT
        LD      HL, CRLF
        CALL    DSPMSG
        
        RET
        
; shortcut to send banner to console.
BANNER:
        LD      HL,BANNERMSG     ;FDD access error message.
        CALL    DSPMSG
        RET
      
; Send char to console. Saves AF & HL
DUMPCHR:
        PUSH    AF
        PUSH    HL        
        LD      C,A
        CALL    CONOUT
        POP     HL
        POP     AF
        RET

; Print directory data. 8 entries, each <filename> <filesize>
PRDATA:
        LD      A, ENTPPART
        LD      (DIRECNT), A     ; set number of
        LD      A, (PKT_RDT)
        LD      HL, PKT_RDT
        LD      (DIREPTR), HL

PDLOOP:        
        CP      020h            ; is it a space?
        RET     Z
        
        LD      HL, (DIREPTR)
        LD      DE, DIRLINE
        LD      BC, FILNAMSZ
        LDIR
;        LD      DE, FILESIZ
;        LD      BC, 3
;        LDIR
        
        LD      B, H
        LD      C, L
        LD      A, (BC)
        LD      E, A
        INC     BC
        LD      A, (BC)
        LD      L, A
        INC     BC
        LD      A, (BC)
        LD      H, A
        CALL    BIN2BCD
        CALL    BCD2BUF
        
        LD      HL, DIRLINE
        CALL    DSPMSG
        LD      HL, (DIREPTR)
        LD      BC, 010h
        ADD     HL, BC        ; next dir entry in PKT_RDT
        LD      (DIREPTR), HL
        LD      A, (DIRECNT)
        DEC     A
        OR      A
        LD      (DIRECNT), A
        CP      0
        RET     Z
        JR      PDLOOP
        RET

; Convert packed BCD to ASCII. The data is in DE and HL. 
BCD2BUF:
        LD      A, D
        CALL    RRC4
        ADD     A, '0'
        LD      BC, DECSIZE     ; message buffer pointer
        LD      (BC), A
        LD      A, D
        AND     00Fh
        ADD     A, '0'
        INC     BC
        LD      (BC), A
        
        LD      A, E
        CALL    RRC4
        ADD     A, '0'
        INC     BC
        LD      (BC), A
        LD      A, E
        AND     00Fh
        ADD     A, '0'
        INC     BC
        LD      (BC), A
        
        LD      A, H
        CALL    RRC4
        ADD     A, '0'
        INC     BC
        LD      (BC), A
        LD      A, H
        AND     00Fh
        ADD     A, '0'
        INC     BC
        LD      (BC), A
        
        LD      A, L
        CALL    RRC4
        ADD     A, '0'
        INC     BC
        LD      (BC), A
        LD      A, L
        AND     00Fh
        ADD     A, '0'
        INC     BC
        LD      (BC), A
        
        RET
        
BIN2BCD:
; From:   https://www.msx.org/forum/development/msx-development/32-bit-long-ascii?page=0          
; Routine for converting a 24-bit binary number to decimal
; In: E:HL = 24-bit binary number (0-16777215)
; Out: DE:HL = 8 digit decimal form (packed BCD)
; Changes: AF, BC, DE, HL & IX

; by Alwin Henseler

                 LD C,E
                 PUSH HL
                 POP IX          ; input value in C:IX
                 LD HL,1
                 LD D,H
                 LD E,H          ; start value corresponding with 1st 1-bit
                 LD B,24         ; bitnr. being processed + 1

FIND1:           ADD IX,IX
                 RL C            ; shift bit 23-0 from C:IX into carry
                 JR C,NEXTBIT
                 DJNZ FIND1      ; find highest 1-bit

; All bits 0:
                 RES 0,L         ; least significant bit not 1 after all ..
                 RET

DBLLOOP:         LD A,L
                 ADD A,A
                 DAA
                 LD L,A
                 LD A,H
                 ADC A,A
                 DAA
                 LD H,A
                 LD A,E
                 ADC A,A
                 DAA
                 LD E,A
                 LD A,D
                 ADC A,A
                 DAA
                 LD D,A          ; double the value found so far
                 ADD IX,IX
                 RL C            ; shift next bit from C:IX into carry
                 JR NC,NEXTBIT   ; bit = 0 -> don't add 1 to the number
                 SET 0,L         ; bit = 1 -> add 1 to the number
NEXTBIT:         DJNZ DBLLOOP
                 RET        
         
RRC4:
        AND     0F0h
        RRC     A
        RRC     A
        RRC     A
        RRC     A
        RET
; end BIN2BCD
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
        CALL    ARGDUMP         ; Print argument string with quotes
        LD      HL, USAGEMSG
        CALL    DSPMSG          ; Print usage
        JP      WBOOT
READERR:
        LD      HL,RDERRMSG     ;FDD read error message.
        CALL    DSPMSG
        JP      WBOOT

ARGDUMP:
        LD      C, '"'
        CALL    CONOUT
        LD      HL, ARGS + 2    ; skip the arg size and space, assume
        CALL    DSPMSG          ;  the 0 at the end is structural
        LD      C, '"'
        CALL    CONOUT
        RET
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
;************************
;*                      *
;*      WORK AREA       *
;*                      *
;************************
;

DIRPART:
        DEFB      '0'
DIREPTR:
        DEFW      0, 0
DIRECNT:
        DEFW      ENTPPART
SIZ:    
        DEFB      00h
        
DIRLINE:
        DEFB      '             '
DECSIZE:
        DEFB      '        '
        DEFB      CR, LF
        DEFB      TERMINATOR
;
;************************
;*                      *
;*      MESSAGE AREA    *
;*                      *
;************************
;
;
BANNERMSG:
        DEFB      'PFDIR4 v'
        DEFB      MAJOR_V
        DEFB      '.'
        DEFB      MINOR_V
CRLF:
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
        
RDERRMSG:
        DEFB      CR,LF
        DEFB      'PFBDK read error !!'
        DEFB      CR,LF
        DEFB      TERMINATOR
        
USAGEMSG:
        DEFB      CR,LF
        DEFB      'PFDIR4 [n]'
        DEFB      CR,LF
        DEFB      TERMINATOR

GA0ARGMSG:
        DEFB      CR,LF
        DEFB      'No part no. Assume 0.'
        DEFB      CR,LF
        DEFB      TERMINATOR

PARTNOMSG:
        DEFB      CR,LF
        DEFB      'Part no '
        DEFB      TERMINATOR

;
;
        END

; Transmit buffer
;PKT_TOP         EQU     0F931H          ;
;PKT_FMT         EQU     PKT_TOP

;PKT_TOP + 0  FMT code (0).
;PKT_TOP + 1  DID code
;PKT_TOP + 2  SID code
;PKT_TOP + 3  FNC code
;PKT_TOP + 4  SIZ data (n)
;PKT_TOP + 5  data 0 'P'
;PKT_TOP + 6  data 1 <drive>;  values: DEFG
;PKT_TOP + 7  data 2 <wpflag>; values: 01
;...
;PKT_TOP + (5+n)  data n
