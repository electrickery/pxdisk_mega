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
DID             EQU     31H
SID             EQU     23H
FNC             EQU     0E0H
SIZ             EQU     03H
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
;
        JP      WBOOT
;
;       ********************************************************
;               SEND P-COMMAND with drine and wp-flag
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
        LD      (HL),SIZ        ;  Set SIZ data.
        INC     HL              ;
        LD      A,001h          ;  Set drive code. Not checked by PFBDK
        LD      (HL),A          ;
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

;80h arg-size, 81h is space, 82h 1st arg char   04 20 44 20 31
CHKARGS:
        LD      HL, ARGS
        
        LD      A, (HL)         ; args length, not used
        INC     HL              ; should be a space
        INC     HL              ; should be dDeEfFgG
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
        INC     HL              ; wp-flag
        LD      A, (HL)
        CP      '0'
        JP      Z, CAWPOK
        CP      '1'
        JP      Z, CAWPOK
        RET                     ; NZ, not 0 or 1
        
CAWPOK:
        LD      (WPFLAG), A
        RET                     ; Z - args are OK
        
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
        LD      C,A
        CALL    CONOUT          ;Display data to the console
        POP     HL
        INC     HL              ;Update pointer
        JR      DSPMSG          ;Repeat DSPMSG until find terminator
;
;

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
;
;************************
;*                      *
;*      MESSAGE AREA    *
;*                      *
;************************
;
CRLF:  
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
;
;
        END
