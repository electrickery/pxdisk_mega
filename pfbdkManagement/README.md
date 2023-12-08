## Workflow description

The programs are created with code editors on a Linux PC. Currently I use (https://www.geany.org/)[geany] and Featherpad.

The cross-assembler is the (https://github.com/udo-munk/z80pack)[Z80 assmbler from z80pack]. 
Assembly to a CP/M-80 program is executed with a small shell script:


	#!/bin/sh
	#
	FILE=$1
	NOEXT=${FILE%%.*}; echo "$NOEXT"
	./z80pack/z80asm/z80asm -fb -o$NOEXT -l $FILE && mv ${NOEXT}.bin ${NOEXT}.com



Finally the result is transferred to the PX-4 with the (https://electrickery.nl/comp/px8/filink.html)[filink protocol] 
using a cable and a USB-RS232c adapter. A script to send one file to the PX-4:

	#!/bin/sh
	#
	
	# Usage: filinkSend.sh <port> <file1>
	
	PROG=./filink
	PORT=$1
	FILE1=$2
	
	stty -F $PORT cs8 -cstopb -parenb speed 4800 > /dev/null
	
	$PROG > $PORT < $PORT $FILE1 

