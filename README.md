# PXBDK - Arduino Mega simulator of the Epson's Epsp based disk drives

This is my fork of William R. Cooke's PXBDK sketch found at http://wrcooke.net/projects/pfbdk/pfbdk.html

The intend is to add some features making it somewhat more convenient to use. The route will be very incremental to keep 
the basic functionality working.

The first feature is a command line on the debug/console port of the Arduino Mega. Its usage is now:

	Usage:
 	C                - temp debug for driveNames array
 	D                - root directory
 	H                - this help
 	M[dnnnnnnnn.eee] - mount file nnnnnnnn.eee on drive d

The goal is to change the images assigned to the simulated disk drives D: to G:. This could be made persistent via the 
Arduino EEPROM or a special file on the disk. 

There is a 3D-printed case and pictures on my own page at: https://electrickery.nl/comp/tf20/pxdisk/

fjkraan@electrickery.nl, 2023-04-06
