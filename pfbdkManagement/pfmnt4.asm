;       ********************************************************
;               PFBDK mount image
;       ********************************************************
;
;       NOTE :
;               This program sets the image mounted
;                for the drives D:, E:, F: and G:
;
;       USAGE :
;               PFMNT4 d name 
;               where:
;                       d is D, E, F or G
;                       name is the name if an existing image on the SD-card

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
LOADX           EQU     WBOOT   +5AH    ; Read one byte from specified bank
CALLX           EQU     WBOOT   +66H    ; 
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
ROMIDSTR        EQU     07FF8h
ROMIDLEN        EQU     03h
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
;
FMT             EQU     00h
DID             EQU     31H
SID             EQU     23H
FNC             EQU     0E0H    ; Epsp command selected for PFBDK management. 
                                ; first byte in the text block is the PFBDK command
;SIZ             EQU     03H    ; Variable value; 0 for one byte, 12 for 13 bytes
;
;
BREAKKEY        EQU     03H     ; BREAK key code
LF              EQU     0AH     ; Line Feed code
CR              EQU     0DH     ; Carriage return code
SPCCD           EQU     20H     ; Space code
PERIOD          EQU     2EH     ; Period code
QMARK           EQU     3FH     ; Question mark code (3FH)
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
        CALL    ARGDUMP
        CALL    CHKARGS         ; Check arguments        
;        JP      NZ, USAGE
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
        CALL    GETARGCNT       ; Expect 0 or 2
        LD      (HL), A
        INC     HL              ;
        LD      A,'M'           ;  Set M command.
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
        CP      0
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
        CALL    CLRMSG                  ; clear buffer, insert : and R
        LD      HL, PKT_RDT             ; point to start received TEXT D:
        LD      DE, MNTMSG              ; point to start print buffer
        LD      A, (HL)                 ; transfer char
        LD      (DE), A                 ;
        INC     HL                      ; next in TEXT
        LD      DE, MNTMSG + 03h        ; skip : and a space
        LD      BC, 12                  ; file name length; 8+3+dot
        CALL    PDLOOP
        LD      HL, PKT_RDT + 00Eh      ; W or O in TEXT
        LD      DE, MNTMSG + 011h       
        LD      A, (HL)
        LD      (DE), A
        LD      HL, MNTMSG
        CALL    DSPMSG
        
        CALL    CLRMSG                  ; clear buffer, insert : and R
        LD      HL, PKT_RDT + 010h      ; 2nd drive E:
        LD      DE, MNTMSG
        LD      A, (HL)
        LD      (DE), A
        INC     HL
        LD      DE, MNTMSG + 03h
        LD      BC, 12
        CALL    PDLOOP
        LD      HL, PKT_RDT + 01Eh      ;
        LD      DE, MNTMSG + 011h
        LD      A, (HL)
        LD      (DE), A
        LD      HL, MNTMSG
        CALL    DSPMSG

        CALL    CLRMSG
        LD      HL, PKT_RDT + 020h      ; 3rd drive F:
        LD      DE, MNTMSG
        LD      A, (HL)
        LD      (DE), A
        INC     HL
        LD      DE, MNTMSG + 03h
        LD      BC, 12
        CALL    PDLOOP
        LD      HL, PKT_RDT + 02Eh
        LD      DE, MNTMSG + 011h
        LD      A, (HL)
        LD      (DE), A
        LD      HL, MNTMSG
        CALL    DSPMSG
        
        CALL    CLRMSG
        LD      HL, PKT_RDT + 030h      ; 4th drive G:
        LD      DE, MNTMSG
        LD      A, (HL)
        LD      (DE), A
        INC     HL
        LD      DE, MNTMSG + 03h
        LD      BC, 12
        CALL    PDLOOP
        LD      HL, PKT_RDT + 03Eh
        LD      DE, MNTMSG + 011h
        LD      A, (HL)
        LD      (DE), A
        LD      HL, MNTMSG
        CALL    DSPMSG
        
        RET
        
PDLOOP:        
        LD      A, (HL)
        OR      A      
        CP      0        
        JR      Z, PDDONE       ; stop when 0 found
        LD      (DE), A
        INC     HL
        INC     DE
        DEC     BC
        LD      A, B
        OR      C
        RET     Z       ;, PDDONE       ; stop when 12 chars copied
        JR      PDLOOP
PDDONE:
        RET

CLRMSG:
        LD      HL, MNTMSG
        LD      A, 020h
        LD      (HL), A
        LD      DE, MNTMSG + 1
        LD      BC, 010h
        LDIR
        LD      HL, MNTMSG + 1 
        LD      A, ':'
        LD      (HL), A
        LD      HL, MNTMSG + 10h
        LD      A, 'R'
        LD      (HL), A
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
        CALL    ARGDUMP
        LD      HL, USAGEMSG
        CALL    DSPMSG
        JP      WBOOT
READERR:
        LD      HL,RDERRMSG     ;FDD read error message.
        CALL    DSPMSG
        JP      WBOOT

ARGDUMP:
        LD      A, (ARGS)
        ADD     A, '0'
        LD      C, A
        CALL    CONOUT
        LD      A, (ARGS)
        OR      A
        JR      Z, ADNOARG
        LD      C, ' '
        CALL    CONOUT
        LD      C, '"'
        CALL    CONOUT
        LD      HL, ARGS + 2    ; skip the arg size and space, assume
        CALL    DSPMSG          ;  the 0 at the end is structural
        LD      C, '"'
        CALL    CONOUT
ADNOARG:
        LD      HL, CRLFMSG
        CALL    DSPMSG
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
        
CHKPX:
        LD      C, SYSBANK
        LD      HL, ROMIDSTR
        LD      DE, ROMIDCPY
        
        CALL    LOADX
        LD      (DE), A
        INC     HL
        INC     DE
        CALL    LOADX
        LD      (DE), A
        INC     HL
        INC     DE
        CALL    LOADX
        LD      (DE), A
        INC     HL
        INC     DE
        CALL    LOADX
        LD      (DE), A
        
        LD      DE, ROMIDCPY
        LD      HL, PINID
        LD      A, (DE)
CPPIN: ;        Pine check (PX-4, HX-40, HX-40)
        CP      (HL)
        JR      NZ, CPMPL
        INC     DE
        LD      A, (DE)
        CP      'I'
        JR      NZ, CPUNK
        INC     DE
        LD      A, (DE)
        CP      'N'
        JR      NZ, CPUNK

        LD      A, 'P'
        LD      (BOARDID), A
        RET
        
CPMPL: ;        Maple check (PX-8, HC-80, HX-80)
        LD      HL, MPLID
        CP      (HL)
        JR      NZ, CPUNK
        INC     DE
        LD      A, (DE)
        CP      'P'
        JR      NZ, CPUNK
        INC     DE
        LD      A, (DE)
        CP      'L'
        JR      NZ, CPUNK
        
        
        
CPUNK:  ; Unknown board
        CALL   DSPBOARD
        RET 
        
        
DSPBOARD:
        LD      HL, BOARDID
        CALL    DSPMSG
        LD      HL, ROMIDCPY
        CALL    DSPMSG
        LD      C, CR
        CALL    CONOUT
        LD      C, LF
        CALL    CONOUT
        RET

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
SIZ:    
        DEFB      00h
ROMIDCPY:
        DEFB      '---'
        DEFB       TERMINATOR
BOARDID:
        DEFB      '?'
        
PINID:
        DEFB       'PIN'
MPLID:
        DEFB       'MPL'
TXTBUFP:
        DEFW
;
;************************
;*                      *
;*      MESSAGE AREA    *
;*                      *
;************************
;
;
BANNERMSG:
        DEFB      'PFMNT4 v1.0'
CRLFMSG:
        DEFB      CR, LF
        DEFB      TERMINATOR
        
DKERRMSG:
        DEFB      CR, LF
        DEFB      'PFBBDK access error !!'
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
        DEFB      'Usage: PFMNT4 <drive> <name>'
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

MNTMSG:        ; 0123456789ABCDEF01
        DEFB    '-: ____________ R_' 
        DEFB    CR, LF
        DEFB    TERMINATOR

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
        DEFB    TERMINATOR

BRDID:
        DEFB    'Board is a '
        DEFB    TERMINATOR

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
;PKT_TOP + 7 to 7+12  data 2-14 <imagename>; 
