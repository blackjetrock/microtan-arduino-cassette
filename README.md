# microtan-arduino-cassette
Arduino based cassette system for Microtan computer

Uses Arduino Mega or Uno, depending on how much memory you need. Uno has 2K RAM mega has 8K. You can't use all of it for a buffer to store cassette information.

The connections to the cassette interface are not the external ones that attach to a tape recorder. They are instead pins 11 and 12 of connector B1 on the TANEX. This is logic level and removes the need for amplifiers and so on. Ground is also needed on B1.

The code runs only in fast mode. The data read from the Microtan is dumped to a buffer that can then go to an SD card.

SD card read isn't working yet.

When using the SAVE command the code displays data as it is received. The serial port on the Arduino runs at 115200 and there's enough time to send the bytes out of the port as the data is received.

Commands

clear:    Clears the memory buffer ready for a new SAVE.

display:  Displays the current contents of the buffer

savefile: Saves the buffer to a file on the attached SD card. The filename used is the embedded name in the data. 
          Data is curently saved in ASCII hex but binary is probably better as it can be directly sent back to the Microtan.

send:     Sends the data buffer back to the Microtan. There's a spurious byte at the start of the buffer that isn't sent back.



There are other commands used in development, I need to remove those or at least tidy them up.
