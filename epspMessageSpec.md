## E0h message specification

This is a specification drafted before the implementation, so it could be inaccurate. It is now 
slowly transformed into actual documentation.

These are the sub-commands implemented for the new E0h message:

	D                - SD-card root directory
	M[dnnnnnnnn.eee] - mount file nnnnnnnn.eee on drive d
	Nnnnnnnnn.eee    - create an image file nnnnnnnn.eee
	P[dw]            - write protect drive d; w=0 RW, w=1 RO

The D and N command should be send to the unit, not the drive. But this would mean a destination
should be defined for this.

### D
 
D[0] returns first four root directory entries
D[n] returns four root directory entries starting with the 4*nth 
an entry name starting with 0E5h indicates there are no more entries.

The D command returns a text block of up to 128 bytes containing up to four directory entries (name and size)

	nnnnnnnn.eee              ssssss
	nnnnnn.eee                ssssss
	nnnnnnnn.eee                ssss
	nnnnnnnn.eee              ssssss

All image file names are in the 8.3 format, as the SD library presents them.

Possible alternative:

	nnnnnnnn.eeesss<LF>
	nnnnnnnn.eeesss<LF>
	nnnnnnnn.eeesss<LF>
	nnnnnnnn.eeesss<LF>
	nnnnnnnn.eeesss<LF>
	nnnnnnnn.eeesss<LF>
	nnnnnnnn.eeesss<LF>
	nnnnnnnn.eeesss<LF>
  
nnnnnnnn.eee is the filename in 8.3 format, sss are the three lowest bytes of an int (as returned by the SD-library.)
This allows eight files in a 128 byte text block.
  

### M

M returns the currently mounted images per drive
Mdnnnnnnnn.eee mounts the image nnnnnnnn.eee on drive d and returns the currently mounted images per drive

The M command always returns a text block of 64 bytes containing drive letters and image names

	D nnnnnnnn.eee  
	E nnnnnnnn.eee  
	F nnnnnnnn.eee  
	G nnnnnnnn.eee  
   
Actually implemented:
	Dnnnnnnnn.eeepRp<LF>
	Ennnnnnnn.eeepRp<LF>
	Fnnnnnnnn.eeepRp<LF>
	Gnnnnnnnn.eeepRp<LF>

D, E, F, G are drives, nnnnnnnn.eee is the filename in 8.3 format, p is the Write-protect flag; 0 = RW, 1 = R0
R is a constant


### N
 
Nnnnnnnnn.eee creates an image named nnnnnnnn.eee in the SD card root directory. The size is 327680 bytes
and its contents is all 0E5h.

The N command always returns an one byte text block and a single byte return code.


### P

P returns the write protect flag state for all drives
Pdw sets or clears the write protect flag w for drive d and returns the write protect flag state for all drives

The P command always returns a text block of 16 bytes containing drive letters and write protect-status.
 
	D p 
	E p 
	F p 
	G p 
