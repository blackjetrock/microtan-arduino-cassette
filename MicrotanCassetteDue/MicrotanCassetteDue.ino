#include <SoftWire.h>

#include <SPI.h>
#include <SD.h>
#include "miniOled.h"

#define DEBUG 0

void cmd_help(String cmd);
  
// set up variables using the SD utility library functions:
Sd2Card card;
SdVolume volume;
SdFile root;

File myFile;

// Capture file
File dumpfile;


#define DEBUG_SERIAL 0
#define DIRECT_WRITE 0

#define BOARD_DUE 1
#define BOARD_MEGA 0

#if BOARD_DUE
const int micPin = 42;
const int statPin = 48;
const int dataPin = 49;
const int earPin = 44;
const int switchPin = 5;
const int MAX_BYTES = 40000;
const int chipSelect = 53;
const int button1Pin = 46;
const int button2Pin = 47;
const int button3Pin = 48;

#endif

#if BOARD_MEGA
const int micPin = 2;
const int statPin = 8;
const int dataPin = 9;
const int earPin = 3;
const int switchPin = 5;
const int MAX_BYTES = 5000;
const int chipSelect = 53;
const int button1Pin = 46;
const int button2Pin = 47;
const int button3Pin = 48;
#endif

typedef unsigned char BYTE;

typedef void (*FPTR)();
typedef void (*CMD_FPTR)(String cmd);

#define NUM_BUTTONS 3

// Debounce
#define MAX_BUT_COUNT 2

int but_pins[NUM_BUTTONS] = {button1Pin, button2Pin, button3Pin};

typedef struct _BUTTON
{
  int     count;
  boolean pressed;
  boolean last_pressed;   // For edge detection
  FPTR    event_fn;
} BUTTON;

BUTTON buttons[NUM_BUTTONS];

enum ELEMENT_TYPE
  {
    BUTTON_ELEMENT = 10,
    SUB_MENU,
    MENU_END,
  };

struct MENU_ELEMENT
{
  enum ELEMENT_TYPE type;
  char *text;
  void *submenu;
  void (*function)(struct MENU_ELEMENT *e);
};


struct MENU_ELEMENT *current_menu;
struct MENU_ELEMENT *last_menu;
struct MENU_ELEMENT *the_home_menu;
unsigned int menu_selection = 0;
unsigned int menu_size = 0;

#define MAX_LISTFILES 7
#define MAX_NAME 20

MENU_ELEMENT listfiles[MAX_LISTFILES];
int num_listfiles;
char names[MAX_LISTFILES][MAX_NAME];
char selected_file[MAX_NAME+1];
char current_file[MAX_NAME+1];

#define IF_2400 if(delta<500)
#define IF_1200 if(delta>=500)


void state_got2400()
{
}

// Start in this state
void state_start()
{
  int delta = 0;
  
  IF_2400
    {
      state_got2400();
    }
}

volatile int data_byte;
volatile int bit_count;
volatile int got_byte = 0;
volatile int state = 0;
volatile int data_bit = 2;
volatile int got_bit = 0;


enum
  {
    INIT = 0,
    RX_D0_1,
    RX_D0_2,
    RX_D0_3,
    RX_D1_1,
    RX_D1_2,
    RX_D1_3,
    RX_D1_4,
    RX_D1_5,
    RX_D1_6,
    RX_D1_7,
    RX_1,
    RX_0,
    BAD12,
    BAD24,
    BAD_START,
  };

#if 0

// Casio fx502p, original table
struct
{	
  FPTR entry_function;
  int goto_if_1200;
  int goto_if_2400;
  
} state_table[] =
  // decodes to a bit stream
  //                    1200          2400
  {
    {state_null,        RX_D0_1,       RX_D1_1},     //  INIT
    {state_null,        RX_D0_2,       RX_D1_1},       //  RX_D0_1
    {state_null,        RX_D0_3,       RX_D1_1},       //  RX_D0_2
    {state_null,        RX_0,          RX_D1_1},       //  RX_D0_3

    {state_null,        RX_D0_1,       RX_D1_2},     //  RX_D1_1
    {state_null,        RX_D0_1,       RX_D1_3},     //  RX_D1_2
    {state_null,        RX_D0_1,       RX_D1_4},     //  RX_D1_3
    {state_null,        RX_D0_1,       RX_D1_5},     //  RX_D1_4
    {state_null,        RX_D0_1,       RX_D1_6},     //  RX_D1_5
    {state_null,        RX_D0_1,       RX_D1_7},     //  RX_D1_6
    {state_null,        RX_D0_1,       RX_1},        //  RX_D1_7

    // We have a 1 or a 0 data bit
    {state_rx_1,        RX_D0_1,       RX_D1_1},         //  RX 1
    {state_rx_0,        RX_D0_1,       RX_D1_1},         //  RX 0
    
    {state_null,        BAD12,        BAD12},        //  7 BAD12
    {state_null,        BAD24,        BAD24},        //  7 BAD24
    {state_null,        BAD_START,    BAD_START},    //  7 BAD_START
  };

#else

// Microtan 65 table for FAST mode
struct
{	
  FPTR entry_function;
  int goto_if_1200;
  int goto_if_2400;
  
} state_table[] =
  // decodes to a bit stream
  //                    1200          2400
  {
    {state_null,        RX_0,          RX_1},           //  INIT
    {state_null,        RX_D0_2,       RX_D1_1},       //  RX_D0_1
    {state_null,        RX_D0_3,       RX_D1_1},       //  RX_D0_2
    {state_null,        RX_0,          RX_D1_1},       //  RX_D0_3

    {state_null,        RX_D0_1,       RX_D1_2},     //  RX_D1_1
    {state_null,        RX_D0_1,       RX_D1_3},     //  RX_D1_2
    {state_null,        RX_D0_1,       RX_D1_4},     //  RX_D1_3
    {state_null,        RX_D0_1,       RX_D1_5},     //  RX_D1_4
    {state_null,        RX_D0_1,       RX_D1_6},     //  RX_D1_5
    {state_null,        RX_D0_1,       RX_D1_7},     //  RX_D1_6
    {state_null,        RX_D0_1,       RX_1},        //  RX_D1_7

    // We have a 1 or a 0 data bit
    {state_rx_1,        INIT,         INIT},            //  RX 1
    {state_rx_0,        INIT,         INIT},           //  RX 0
    
    {state_null,        BAD12,        BAD12},        //  7 BAD12
    {state_null,        BAD24,        BAD24},        //  7 BAD24
    {state_null,        BAD_START,    BAD_START},    //  7 BAD_START
  };

#endif

void state_null()
{
}

// Start bit just received
void state_start_byte()
{
  data_byte = 0;
  bit_count = 0;
}

void state_rx_0()
{
  data_bit = 0;
  got_bit = 1;
}

void state_rx_1()
{
  data_bit = 1;
  got_bit = 1;
}

void xstate_rx_0()
{
  data_byte <<=1;
  bit_count++;
  if( bit_count == 8)
    {
      // Got a data byte
      got_byte =1;
      //      state = WAIT_CHECK;
    }

}

void xstate_rx_1()
{
  data_byte <<=1;
  data_byte++;
  bit_count++;
  if( bit_count == 8)
    {
      // Got a data byte
      got_byte =1;
      //      state = WAIT_CHECK;
    }
  
}

void state_check_ok()
{
}

void state_check_bad()
{
}


int last_micstate = 0, micstate = 0;
volatile long t, t2 = 0;
long delta = 0;
int ix = 0;

long mint = 100000;
long maxt = 0;
long quiet = 0;
long trans = 0;
volatile long hz2400=0, hz1200=0;

long count = 0;
long last = 0;

enum
  {
    BINIT = 0,
    BRX_H_1,
    BRX_START,
    BRX_D0_0,
    BRX_D1_0,
    BRX_D2_0,
    BRX_D3_0,
    BRX_D4_0,
    BRX_D5_0,
    BRX_D6_0,
    BRX_D7_0,
    BRX_D0_1,
    BRX_D1_1,
    BRX_D2_1,
    BRX_D3_1,
    BRX_D4_1,
    BRX_D5_1,
    BRX_D6_1,
    BRX_D7_1,
    BRX_PARITY_0,
    BRX_PARITY_1,
    BRX_DUMMY1,
    BRX_BYTEDONE,
  };

int byte_state = 0;
int databyte = 0;

void state_clrbyte()
{
  databyte=0;
}

// Microtan has inverted data so the data stored here is inverted
void state_databit1()
{
  databyte >>= 1;
}

void state_databit0()
{
  databyte >>= 1;
  databyte+=0x80;
}

int calcpar(int b)
{
  int i;
  int one_count = 0;
  for(i=0; i<8; i++)
    {
      one_count += ((b >> i) & 1);
    }

  return((one_count & 1));
}

void state_chkpar0()
{
  int par = calcpar(databyte);
  if( par == 0 )
    {
      // All ok
#if DEBUG_SERIAL      
      Serial.println("D");
      Serial.println(databyte,HEX);
#endif
    }
  else
    {
#if DEBUG_SERIAL
      Serial.println("E");
      Serial.println(databyte,HEX);
#endif
    }
}

void state_chkpar1()
{
  int par = calcpar(databyte);
  if( par == 1 )
    {
      // All ok
#if DEBUG_SERIAL
      Serial.println("D");
      Serial.println(databyte,HEX);
#endif
    }
  else
    {
#if DEBUG_SERIAL
      Serial.println("E");
      Serial.println(databyte,HEX);
#endif
    }
}

// How many bytes in the buffer. We have a default for testing without a Microtan
int bytecount = 24;

void state_init()
{
#if DEBUG_SERIAL  
  Serial.println("I");
#endif  
  
}

// Where the received data goes

char stored_bytes[MAX_BYTES] = "Test data example. 01234567890";


// Two versions of this, one that writes data to a buffer that you have to then write to
// a file, and a second that writes directly to the SD card

#if !DIRECT_WRITE

void state_bytedone()
{
  // There's a spurious character at the start, we don't want to store it at all
  if( bytecount >= 0 )
    {
      stored_bytes[bytecount] = databyte;
      
      Serial.print(databyte, HEX);
      Serial.print(" ");
      
      if( bytecount == 0 )
	{
	  if( databyte & 1 )
	    {
	      digitalWrite(LED_BUILTIN, HIGH);
	    }
	  else
	    {
	      digitalWrite(LED_BUILTIN, LOW);
	    }
	}
    }
  
  //  myFile.println(databyte, HEX);
  bytecount++;
  if( bytecount >= MAX_BYTES )
    {
      bytecount = MAX_BYTES;
    }
  
  if( (bytecount % 16) == 0 )
    {
      Serial.println("");
    }
}

#else

// This one writes dirctly to SD card
// A clear is needed to reset counters etc

int filename_i = 0;
char filename[9] = "";

void state_bytedone()
{
  // There's a spurious character at the start, we don't want to store it at all
  if( bytecount >= 0 )
    {
      if( bytecount == 0 )
	{
	  // Reset filename capture
	  filename_i = 0;
	}

      // We need to capture the filename
      if( (bytecount >=0) && (bytecount <=7) )
	{
	  filename[filename_i++] = databyte;
	}

      if( bytecount == 8 )
	{
	  Serial.println(filename);
	}
      
      // Write to file
      dumpfile.write(databyte);
      
      Serial.print(databyte, HEX);
      Serial.print(" ");
      
      if( bytecount == 0 )
	{
	  if( databyte & 1 )
	    {
	      digitalWrite(LED_BUILTIN, HIGH);
	    }
	  else
	    {
	      digitalWrite(LED_BUILTIN, LOW);
	    }
	}
    }
  
  //  myFile.println(databyte, HEX);
  bytecount++;
  if( bytecount >= MAX_BYTES )
    {
      bytecount = MAX_BYTES;
    }
  
  if( (bytecount % 16) == 0 )
    {
      Serial.println("");
    }
}

#endif


struct
{	
  FPTR entry_function;
  int goto_if_0;
  int goto_if_1;
  
} byte_state_table[] =
  // decodes to a bit stream
  //                    0                1
  {
    {state_init,        BINIT,          BRX_START},    //  BINIT       0
    {state_clrbyte,     BRX_START,      BRX_H_1},      //  BRX_H_1     1
    {state_clrbyte,     BRX_D0_0,       BRX_D0_1},     //  BRX_START   2
    {state_databit0,    BRX_D1_0,       BRX_D1_1},     //  BRX_D0_0    3
    {state_databit0,    BRX_D2_0,       BRX_D2_1},     //  BRX_D1_0    4
    {state_databit0,    BRX_D3_0,       BRX_D3_1},     //  BRX_D2_0    5
    {state_databit0,    BRX_D4_0,       BRX_D4_1},     //  BRX_D3_0    6
    {state_databit0,    BRX_D5_0,       BRX_D5_1},     //  BRX_D4_0    7
    {state_databit0,    BRX_D6_0,       BRX_D6_1},     //  BRX_D5_0    8
    {state_databit0,    BRX_D7_0,       BRX_D7_1},     //  BRX_D6_0    9
    {state_databit0,    BRX_PARITY_0,   BRX_PARITY_1}, //  BRX_D7_0   10
    {state_databit1,    BRX_D1_0,       BRX_D1_1},     //  BRX_D0_1   11
    {state_databit1,    BRX_D2_0,       BRX_D2_1},     //  BRX_D1_1   12
    {state_databit1,    BRX_D3_0,       BRX_D3_1},     //  BRX_D2_1   13
    {state_databit1,    BRX_D4_0,       BRX_D4_1},     //  BRX_D3_1   14
    {state_databit1,    BRX_D5_0,       BRX_D5_1},     //  BRX_D4_1   15
    {state_databit1,    BRX_D6_0,       BRX_D6_1},     //  BRX_D5_1   16
    {state_databit1,    BRX_D7_0,       BRX_D7_1},     //  BRX_D6_1   17
    {state_databit1,    BRX_PARITY_0,   BRX_PARITY_1}, //  BRX_D7_1   18
    {state_chkpar0,     BRX_DUMMY1,     BRX_DUMMY1},   //  BRX_PARITY_0  19
    {state_chkpar1,     BRX_DUMMY1,     BRX_DUMMY1},   //  BRX_PARITY_1  20
    {state_null,        BRX_BYTEDONE,   BRX_BYTEDONE}, //  BRX_DUMMY1    21
    {state_bytedone,    BINIT,          BINIT},        //  BRX_BYTEDONE  22

  };

// Sends a bit out of the ear port

// for fx502p
const int base_period=200;
//const int base_period=104;

void send_bit(int b)
{
  int i;

#if 0
  Serial.print(b);
#endif
  
  if( b )
    {
      // Send 1
      for(i=0;i<1;i++)
	{
	  digitalWrite(earPin, LOW);
	  delayMicroseconds(base_period*2);
	  digitalWrite(earPin, HIGH);
	  delayMicroseconds(base_period);
	}
    }
  else
    {
      // Send 0
      for(i=0;i<1;i++)
	{
	  digitalWrite(earPin, LOW);
	  delayMicroseconds(base_period);
	  digitalWrite(earPin, HIGH);
	  delayMicroseconds(base_period);
	}
    }
}

//
// Sends the data buffer back to the Microtan
//

void send_databytes()
{
  int i, b;
  int j;
  int databyte;
  int parity;
 
  Serial.println("Sending...");
  Oled.clearDisplay();
  Oled.printString("Sending...", 0,1);
  

  // Make ear pin an output while sending then input at all other times so
  // cassette inputs will still work

  pinMode(earPin,     OUTPUT);

  // Turn interrupts of as we want good timing for the data
  noInterrupts();
  
  // Send stored data back out
  // Header
  for(i=0; i<2400*5; i++)
    {
#if 0      
      digitalWrite(earPin, LOW);
      delayMicroseconds(base_period);
      digitalWrite(earPin, HIGH);
      delayMicroseconds(base_period);
#else
      send_bit(0);
#endif
      
    }
  
  // Now send data back
  for(b=0; b<bytecount; b++)
    {
      // Start bit
      send_bit(1);
      
      parity = 0;
      databyte = stored_bytes[b];
      
      // Send bits
      for(j=0; j<8; j++)
	{
	  if( !(databyte & 1) )
	    {
	      parity++;
	      send_bit(1);
	    }
	  else
	    {
	      send_bit(0);
	    }
	  databyte >>=1;
	}
      
      // Send parity
      if( (parity & 1) )
	{
	  send_bit(1);
	}
      else
	{
	  send_bit(0);
	}
      
      // Send two 0's as stop
      send_bit(0);
      send_bit(0);

      // Send extra delay after header as load seems to fail without this
      if( b == 11 )
	{
	  // 60ms of zeros
	  for(int z=0;z<300;z++)
	    {
	      send_bit(0);
	    }
	}
    }
  interrupts();

  pinMode(earPin,     INPUT);
    
  Serial.println("Sent.");
  Oled.printString("Sending...", 0,1);
  delay(2000);
  
  draw_menu(current_menu, true);

}

//
// Allow interaction with serial monitor
//

// Commands
int indx = 0;

void cmd_next(String cmd)
{
  indx++;
}

void cmd_prev(String cmd)
{
  indx--;
}

void cmd_index(String cmd)
{
  String arg;
  
  Serial.println("INDEX");
  arg = cmd.substring(1);
  Serial.println(arg);
  
  indx = arg.toInt();
}

// Modify the buffer
void cmd_modify(String cmd)
{
  String arg;
  
  Serial.println("MOD");
  arg = cmd.substring(1);

  if( indx <= MAX_BYTES )
    {
      stored_bytes[indx] = arg.toInt();
    }
}

#define DISPLAY_COLS 16

// Displays the file data, with offset and address (from start address stored in file data)

void cmd_display(String cmd)
{
  int i;
  int address = stored_bytes[10]*256+stored_bytes[11]-12;
  
  char ascii[DISPLAY_COLS+2];
  char c[2];
  char line[50];
  
  int ascii_i = 0;
  ascii[0] ='\0';
  
  Serial.print("Byte Count:");
  Serial.print(bytecount);
  Serial.print("  Index:");
  Serial.println(indx);

  for(i=0; i<bytecount; i++)
    {
      if( (i%DISPLAY_COLS)==0 )
	{
	  sprintf(line, "%s\n%04X %04X:", ascii, i, address); 	  
	  Serial.print(line);
	  ascii_i = 0;
	}

      sprintf(line, "%02X ", stored_bytes[i]);
      Serial.print(line);
      
      if( isprint(stored_bytes[i]) )
	{
	  sprintf(c, "%c", stored_bytes[i]);
	}
	else
	  {
	    c[0] ='.';
	  }
      ascii[ascii_i++] = c[0];
      ascii[ascii_i] = '\0';
      
      address++;
    }

  ascii[ascii_i++] = '\0';

  // Pad to the ascii position
  for(i=ascii_i-1; i<DISPLAY_COLS; i++)
    {
      Serial.print("   ");
    }
  
  Serial.print(ascii);
  Serial.print(" ");
  Serial.println("");
}

// Clear the buffer


void cmd_clear(String cmd)
{
  bytecount = -1;    // We reset to -1 so we drop the leading spurious character

#if DIRECT_WRITE
    SD.remove(filename);
    dumpfile = SD.open("dumpfile.bin", FILE_WRITE);
#endif
  
}


// Close the capture file
void cmd_close(String cmd)
{
  dumpfile.close();
}

void cmd_send(String cmd)
{
  send_databytes();
}

// Deletes a file
void cmd_deletefile(String cmd)
{
  String arg;
  
  arg = cmd.substring(strlen("delete "));

  Serial.print("Deleting file '");
  Serial.print(arg);
  Serial.println("'");
  
  SD.remove(arg);
}

// read the file with the given name into the buffer

void core_read(String arg, boolean oled_nserial)
{
  if( oled_nserial )
    {
      Oled.clearDisplay();
      Oled.printString("Reading file ", 0, 0);
      Oled.printString(arg.c_str(), 0, 1);
    }
  else
    {
      Serial.print("Reading file '");
      Serial.print(arg);
      Serial.println("'");
    }
  
  myFile = SD.open(arg);

  if (myFile)
    {
      // Read from the file and store it in the buffer
      bytecount = 0;
      
      while (myFile.available())
	{
	  stored_bytes[bytecount++] = myFile.read();
	  if( bytecount >= MAX_BYTES )
	    {
	      bytecount = MAX_BYTES;
	    }
	  
	}
      
      // close the file:
      myFile.close();

      if ( oled_nserial )
	{
	  Oled.printInt(bytecount, 0,4);
	  Oled.printString(" bytes read.");
	  delay(3000);
	}
      else
	{
	  Serial.print(bytecount);
	  Serial.println(" bytes read.");
	}
    }
  else
    {
      // if the file didn't open, print an error:
      if( oled_nserial )
	{
	  Oled.printString("Error opening", 0, 4);
	  Oled.printString(arg.c_str(), 0, 5);
	  
	}
      else
	{
	  Serial.print("Error opening ");
	  Serial.println(arg);
	}
    }
}

void cmd_readfile(String cmd)
{
  String arg;
  
  arg = cmd.substring(strlen("read "));

  core_read(arg, false);
}

void cmd_listfiles(String cmd)
{
  File dir;
  
  dir = SD.open("/");

  // return to the first file in the directory
  dir.rewindDirectory();
  
  while (true) {
    
    File entry =  dir.openNextFile();
    if (! entry) {
      // no more files
      break;
    }

    Serial.print(entry.name());

    if (entry.isDirectory())
      {
	Serial.println("/");
      }
    else
      {
	// files have sizes, directories do not
	Serial.print("\t\t");
	Serial.println(entry.size(), DEC);
      }
    entry.close();
  }

  dir.close();
}

void cmd_initsd(String cmd)
{
  if (!SD.begin(53)) {
    Serial.println("SD Card initialisation failed!");
  }
  else
    {
      Serial.println("SD card initialised.");
    }

}


// Writes the buffer to a file.
// Deletes any file that exists with the same name so that the resulting
// file is the same size as the buffer


void core_writefile(boolean oled_nserial)
{
  char filename[20] = "U___.txt";
  int i;
  
  // Get the filename from the data, we can't allow spaces in filenames
  sprintf(filename, "%c%c%c%c%c%c%c%c",
	  (stored_bytes[0]==' ')?'_':stored_bytes[0],
	  (stored_bytes[1]==' ')?'_':stored_bytes[1],
	  (stored_bytes[2]==' ')?'_':stored_bytes[2],
	  (stored_bytes[3]==' ')?'_':stored_bytes[3],
	  (stored_bytes[4]==' ')?'_':stored_bytes[4],
	  (stored_bytes[5]==' ')?'_':stored_bytes[5],
	  (stored_bytes[6]==' ')?'_':stored_bytes[6],
	  (stored_bytes[7]==' ')?'_':stored_bytes[7]
	  );

  if( oled_nserial )
    {
      Oled.clearDisplay();
      Oled.printString("", 0,0);
      Oled.printString("Writing ");
      Oled.printInt(bytecount);
      Oled.printString(" bytes to");
      Oled.printString("'", 0, 1);
      Oled.printString(filename);
      Oled.printString("'");
    }
  else
    {
      Serial.println("");
      Serial.print("Writing ");
      Serial.print(bytecount);
      Serial.print(" bytes to '");
      Serial.print(filename);
      Serial.print("'");
      Serial.println("");
    }
  
  // Delete so we have no extra data if the file is currently larger than the buffer
  SD.remove(filename);
  
  // Open file for writing
  myFile = SD.open(filename, FILE_WRITE);

  if( myFile )
    {
      // Write data
      for(i=0; i<bytecount; i++)
	{
	  myFile.write(stored_bytes[i]);
	}
      
      myFile.close();

      if( oled_nserial )
	{
	  Oled.setCursorXY(0,3);
	  Oled.printInt(bytecount);
	  Oled.printString(" bytes written");
	}
      else
	{
	  Serial.print(bytecount);
	  Serial.println(" bytes written");
	}
    }
  else
    {
      if(oled_nserial)
	{
	  Oled.printString("Could not open file", 0, 5);

	}
      else
	{
	  Serial.println("Could not open file");
	}
    }

  if( oled_nserial )
    {
      delay(2000);
    }
}

void cmd_writefile(String cmd)
{
  core_writefile(false);
}

const int NUM_CMDS = 14;

String cmd;
struct
{
  String cmdname;
  CMD_FPTR   handler;
} cmdlist [NUM_CMDS] =
  {
    {"m",           cmd_modify},
    {"clear",       cmd_clear},
    {"display",     cmd_display},
    {"next",        cmd_next},
    {"prev",        cmd_prev},
    {"i",           cmd_index},
    {"close",       cmd_close},
    {"write",       cmd_writefile},
    {"send",        cmd_send},
    {"list",        cmd_listfiles},
    {"initsd",      cmd_initsd},
    {"help",        cmd_help},
    {"read",        cmd_readfile},
    {"delete",      cmd_deletefile},
  };


void cmd_help(String cmd)
{
  int i;
  
  for(i=0; i<NUM_CMDS; i++)
    {
      Serial.println(cmdlist[i].cmdname);
    }
}

void run_monitor()
{
  char c;
  int i;
  String test;
  
  if( Serial.available() )
    {
      c = Serial.read();

      switch(c)
	{
	case '\r':
	case '\n':
	  // We have a command, process it
	  for(i=0; i<NUM_CMDS; i++)
	    {
	      test = cmd.substring(0, (cmdlist[i].cmdname).length());
	      if( test == cmdlist[i].cmdname )
		{
		  (*(cmdlist[i].handler))(cmd);
		}
	    }

	  cmd = "";
	  break;

	default:
	  cmd += c;
	  break;
	}
    }
}

// The switch menu/OLED display system
void to_back_menu(struct MENU_ELEMENT *e)
{
  menu_selection = 0;
  current_menu = last_menu;
  draw_menu(current_menu, true);
}

void to_home_menu(struct MENU_ELEMENT *e)
{
  menu_selection = 0;
  current_menu = the_home_menu;
  draw_menu(current_menu, true);
}

void button_clear(MENU_ELEMENT *e)
{
  bytecount = -1;

  Oled.clearDisplay();
  Oled.printString("Buffer Cleared", 0,1);
  delay(3000);
  draw_menu(current_menu, true);
}

void button_write(MENU_ELEMENT *e)
{
  core_writefile(true);

  delay(3000);
  draw_menu(current_menu, true);
}


// The button function puts up to the first 7 files on screen then set sup a button handler
// which will display subsequent pages.
// We use the menu structures to display the names and allow selection

// File selected
void button_select_file(MENU_ELEMENT *e)
{
  strcpy(selected_file, e->text);

  // Back a menu
  to_back_menu(e);
  
}


// Allow a file to be selected. The file name will be stored for a later 'read' command.

int file_offset = 0;

#define FILE_PAGE 7

void but_ev_file_up()
{
#if DEBUG
  Serial.print("Before-menu_selection: ");
  Serial.print(menu_selection);
  Serial.print("file_offset: ");
  Serial.println(file_offset);
#endif
  
  if( menu_selection == 0 )
    {
      if( file_offset == 0 )
	{
	  // Don't move
	}
      else
	{
	  // Move files back one
	  file_offset--;
	}
    }
  else
    {
      // Move cursor up
      menu_selection--;
    }

#if DEBUG
  Serial.print("Before-menu_selection: ");
  Serial.print(menu_selection);
  Serial.print("file_offset: ");
  Serial.println(file_offset);
#endif
  
  button_list(NULL);

  if( menu_selection >= menu_size )
    {
      menu_selection = menu_size - 1;
    }
}

void but_ev_file_down()
{
#if DEBUG  
  Serial.print("Before-menu_selection: ");
  Serial.print(menu_selection);
  Serial.print("file_offset: ");
  Serial.println(file_offset);
#endif
  
  // Move cursor down one entry
  menu_selection++;
  
  // Are we off the end of the menu?
  if( menu_selection == menu_size )
    {
      // 
      if( menu_selection >= MAX_LISTFILES,1 )
	{
	  menu_selection--;

	  // If the screen is full then we haven't reached the end of the file list
	  // so move the list up one
	  if( menu_size == MAX_LISTFILES )
	    {
	      file_offset++;
	    }
	}
    }

  // We need to make sure cursor is on menu
  if( menu_selection >= menu_size )
    {
      menu_selection = menu_size - 1;
    }

#if DEBUG  
  Serial.print("Before-menu_selection: ");
  Serial.print(menu_selection);
  Serial.print("file_offset: ");
  Serial.println(file_offset);
  Serial.print("menu_size: ");
  Serial.println(menu_size);
#endif
  
  button_list(NULL);
}

// Store file name and exit menu
// File can be read later

void but_ev_file_select()
{
  strcpy(current_file, listfiles[menu_selection].text);
  file_offset = 0;

  Oled.clearDisplay();
  Oled.printString("Selected file", 0, 0);
  Oled.printString(current_file, 0, 2);
  delay(3000);

  menu_selection = 0;
  to_home_menu(NULL);
  
  buttons[0].event_fn = but_ev_up;
  buttons[1].event_fn = but_ev_down;
  buttons[2].event_fn = but_ev_select;
}

void button_list(MENU_ELEMENT *e)
{
  File dir;
  int file_n = 0;
  num_listfiles = 0;
  int i;

  dir = SD.open("/");

  // return to the first file in the directory
  dir.rewindDirectory();
  
  while (num_listfiles < MAX_LISTFILES) {

    File entry =  dir.openNextFile();

    if (! entry) {
      // no more files
      // terminate menu
      listfiles[num_listfiles].text = "";
      listfiles[num_listfiles].type = MENU_END;
      listfiles[num_listfiles].submenu = NULL;
      listfiles[num_listfiles].function = button_select_file;
      entry.close();
      break;
    }

    
    // We don't allow directories and don't ount them
    if (entry.isDirectory())
      {
      }
    else
      {
#if DEBUG	
	Serial.print("BList-file_n:");
	Serial.print(file_n);
	Serial.print(entry.name());
	Serial.print("  num_listfiles:");
	Serial.println(num_listfiles);
#endif
	// Create a new menu element
	// we also don't want to display anything before the offset
	if( file_n >= file_offset )
	  {
	    strncpy(&(names[num_listfiles][0]), entry.name(), MAX_NAME);
	    //	Oled.printString(&(names[num_listfiles][0]));
	    listfiles[num_listfiles].text = &(names[num_listfiles][0]);
	    listfiles[num_listfiles].type = BUTTON_ELEMENT;
	    listfiles[num_listfiles].submenu = NULL;
	    listfiles[num_listfiles].function = button_select_file;
	    
	    num_listfiles++;
	  }
	// Next file
	file_n++;

      }
    entry.close();
    
  }

  dir.close();

  // terminate menu
  listfiles[num_listfiles].text = "";
  listfiles[num_listfiles].type = MENU_END;
  listfiles[num_listfiles].submenu = NULL;
  listfiles[num_listfiles].function = button_select_file;

  // We know how big the menu is now
  if( num_listfiles != 0 )
    {
      menu_size = num_listfiles;
    }
  
  // Set up menu of file names
  current_menu = &(listfiles[0]);
  draw_menu(current_menu, false);

  // Button actions modified
  buttons[0].event_fn = but_ev_file_up;
  buttons[1].event_fn = but_ev_file_down;
  buttons[2].event_fn = but_ev_file_select;

}


#define COLUMNS 4
#define PAGE_LENGTH 24

// Display the buffer

int display_offset = 0;

void but_page_up()
{
  if( display_offset > PAGE_LENGTH )
    {
      display_offset -= PAGE_LENGTH;
    }
  else
    {
      display_offset = 0;
    }
  button_display(NULL);
}

void but_page_down()
{
  display_offset += PAGE_LENGTH;
  
  if( display_offset >= bytecount )
    {
      display_offset = bytecount-PAGE_LENGTH;
    }

  if( display_offset < 0 )
    {
      display_offset = 0;
    }
  
  button_display(NULL);
}

void but_page_exit()
{
  display_offset = 0;
  draw_menu(current_menu, true);

  buttons[0].event_fn = but_ev_up;
  buttons[1].event_fn = but_ev_down;
  buttons[2].event_fn = but_ev_select;

}

void button_display(MENU_ELEMENT *e)
{
  int i;
  char ascii[17];
  char c[5];
  
  int ascii_i = 0;

  Oled.clearDisplay();
  
  //  Oled.printString("Byte Count:", 0,1);
  //Oled.printInt(bytecount, 12, 1);

  for(i=0; (i<bytecount) && (i<PAGE_LENGTH); i++)
    {
      if( isprint(stored_bytes[i+display_offset]) )
	{
	  sprintf(ascii, "%c", stored_bytes[i+display_offset]);
	}
      else
	{
	  sprintf(ascii, ".");
	}
      
      sprintf(c,     "%02X",  stored_bytes[i+display_offset]);
      
      Oled.printString(ascii, 9+(i%COLUMNS)*1, i/COLUMNS+1);
      Oled.printString(c    , 0+(i%COLUMNS)*2, i/COLUMNS+1);

    }

  // Drop into page up and down and exit buttoin handlers
  buttons[0].event_fn = but_page_up;
  buttons[1].event_fn = but_page_down;
  buttons[2].event_fn = but_page_exit;
  
}


void button_send(MENU_ELEMENT *e)
{
  send_databytes();
  draw_menu(current_menu, true);
}

/* read the current file from SD card */

void button_read(MENU_ELEMENT *e)
{
  core_read(current_file, true);

  draw_menu(current_menu, true);
}

struct MENU_ELEMENT home_menu[] =
   {
     {BUTTON_ELEMENT, "List",                       NULL, button_list},
     {BUTTON_ELEMENT, "Clear",                      NULL, button_clear},
     {BUTTON_ELEMENT, "Send",                       NULL, button_send},
     {BUTTON_ELEMENT, "Write",                      NULL, button_write},
     {BUTTON_ELEMENT, "Display",                    NULL, button_display},
     {BUTTON_ELEMENT, "Read",                       NULL, button_read},
     {MENU_END,       "",       NULL,                    NULL},
  };

// Clear flag indicates whether we redraw the menu text and clear the screen. Or not.

void draw_menu(struct MENU_ELEMENT *e, boolean clear)
{
  int i = 0;
  char curs = ' ';
  char etext[20];
  
  // Clear screen
  if(clear)
    {
      Oled.clearDisplay();
    }
  
  while( e->type != MENU_END )
    {
      sprintf(etext, "%13s", e->text);
      
      switch(e->type)
	{
	case BUTTON_ELEMENT:
	  Oled.setCursorXY(0, i);
	  //Oled.printChar(curs);
	  if( clear,1 )
	    {
	      Oled.printString(etext, 1, i);
	    }
	  break;

	case SUB_MENU:
	  Oled.setCursorXY(0, i);
	  //Oled.printChar(curs);
	  if ( clear,1 )
	    {
	      Oled.printString(etext, 1, i);
	    }
	  break;
	}
      e++;
      i++;
    }
  
  menu_size = i;

#if DEBUG
  Serial.print("menu_size:");
  Serial.println(menu_size);
#endif
  
  // Blank the other entries
  //make sure menu_selection isn't outside the menu
  if( menu_selection >= menu_size )
    {
      menu_selection = menu_size-1;
    }

  for(; i<MAX_LISTFILES; i++)
    {
      Oled.printString("               ", 0, i);
    }

  for(i=0;i<menu_size;i++)
    {
      if( i == menu_selection )
	{
	  curs = '>';	  
	}
      else
	{
	  curs = ' ';
	}

      Oled.setCursorXY(0, i);
      Oled.printChar(curs);
    }
}

// Null button event function
void but_ev_null()
{ 
}

void but_ev_up()
{
  if( menu_selection == 0 )
    {
      menu_selection = menu_size - 1;
    }
  else
    {
      menu_selection = (menu_selection - 1) % menu_size;
    }
  
  draw_menu(current_menu, false);
}

void but_ev_down()
{

  menu_selection = (menu_selection + 1) % menu_size;

  draw_menu(current_menu, false);
}

void but_ev_select()
{
  struct MENU_ELEMENT *e;
  int i = 0;
  
  // Do what the current selection says to do
  for(e=current_menu, i=0; (e->type != MENU_END); i++, e++)
    {
      if( i == menu_selection )
	{
	  switch(e->type)
	    {
	    case SUB_MENU:
	      current_menu = (MENU_ELEMENT *)e->submenu;
	      draw_menu(current_menu, true);
	      break;
	      
	    default:
	      // Do action
	      (e->function)(e);
	      break;
	    }
	}
    }
}


void init_buttons()
{
  for(int i=0; i<NUM_BUTTONS; i++)
    {
      buttons[i].count = 0;
      buttons[i].pressed = false;
      buttons[i].last_pressed = false;
      buttons[i].event_fn = but_ev_null;
    }

  buttons[0].event_fn = but_ev_up;
  buttons[1].event_fn = but_ev_down;
  buttons[2].event_fn = but_ev_select;
}

void update_buttons()
{
  for(int i=0; i<NUM_BUTTONS; i++)
    {
      if( digitalRead(but_pins[i]) == LOW )
	{
	  if( buttons[i].count < MAX_BUT_COUNT )
	    {
	      buttons[i].count++;
	      if( buttons[i].count == MAX_BUT_COUNT )
		{
		  // Just got to MAX_COUNT
		  buttons[i].pressed = true;
		}
	    }
	}
      else
	{
	  if( buttons[i].count > 0 )
	    {
	      buttons[i].count--;
	      
	      if( buttons[i].count == 0 )
		{
		  // Just got to zero
		  buttons[i].pressed = false;
		}
	    }
	}
      
      // If buton has gone from pressed to not pressed then we treat that as a key event
      if( (buttons[i].last_pressed == true) && (buttons[i].pressed == false) )
	{
	  (buttons[i].event_fn)();
	}

      buttons[i].last_pressed = buttons[i].pressed;
    }
}

void setup()
{
  bytecount = strlen(&(stored_bytes[0]));
  
  Serial.begin(115200);
  Serial.println("\nMicrotan 65 Cassette Interface");

  pinMode(micPin,     INPUT);
  pinMode(switchPin,  INPUT); 
  pinMode(statPin,    OUTPUT); 
  pinMode(dataPin,    OUTPUT);

  for(int i=0; i<NUM_BUTTONS;i++)
    {
      pinMode(but_pins[i], INPUT);
    }
  
  delay(700);

  Oled.init();  //initialze OLED display
  //Oled.wideFont = true;
  //Oled.chrSpace=3;
  //Oled.drawLine(3, 0xe0);

  //Oled.printString("Hello",0,0); 
  //Oled.printInt(F_CPU/1000000,0,5); 
  //Oled.printString("MHz",0,6);

  Oled.wideFont = false;
  Oled.chrSpace=1;
  
  Oled.printString("Microtan 65", 0, 0);
  Oled.printString("Cassette Interface",0,1); 
  delay(1000);
  
  // Oled.clearDisplay();
  //Oled.printBigNumber("-31",6,4);
  //Oled.printBigNumber("9",12,0);
  
//  delay(1000);
//  Oled.printBigNumber("8",9,4); 
//  Oled.printBigNumber("5",12,4);
  //delay(1000);
  //Oled.printBigNumber("   ", 6,4);  // clear those positions
  //Oled.printBigNumber(62, 6,4); 
  //Oled.setPowerOff();


  
  attachInterrupt(digitalPinToInterrupt(micPin), lowISR, FALLING);
      
  pinMode(LED_BUILTIN, OUTPUT);
  
  if (!SD.begin(53)) {
    Serial.println("SD Card initialisation failed!");
    Oled.printString("SD Fail", 0, 3);
  }
  else
    {
      Serial.println("SD card initialised.");
      Oled.printString("SD OK", 0, 3);
    }

  delay(2000);
  
  current_menu = &(home_menu[0]);
  last_menu = &(home_menu[0]);
  the_home_menu = last_menu;

  to_home_menu(NULL);

  init_buttons();
}

void loop()
{

  update_buttons();
  run_monitor();
  
  if( digitalRead(switchPin),0 )
    {
      // Send data in buffer back
      send_databytes();      
    }
  
  if( got_bit )
    {
      got_bit = 0;
      
#if 0
      Serial.print(data_bit);

      count++;
      if ((count % 32)==0 )
	{
	  Serial.println("");
	}
#endif
      
      // Use a state machine to decode frames into byte codes
      if ( data_bit == 0 )
	{
	  // Move to next state
	  byte_state = byte_state_table[byte_state].goto_if_0;
	  
	  // Execute entry function
	  (*byte_state_table[byte_state].entry_function)();
	}
      else
	{
	  // Move to next state
	  byte_state = byte_state_table[byte_state].goto_if_1;
	  
	  // Execute entry function
	  (*byte_state_table[byte_state].entry_function)();
	}

      #if 0
      if( byte_state != BINIT )
	{
	  Oled.printString("Rx", 12,7);
	}
      else
	{
	  Oled.printString("  ", 12,7);
	}
      #endif
      
#if 0
      Serial.print(":");
      Serial.print(byte_state);
      Serial.println(":");
#endif 
      
      //      Serial.print(" ");
      //Serial.print(state);
      //Serial.print(":");
      
      if( millis() - last > 2000,0 )
	{
	  Serial.println("");
	  Serial.print(byte_state);
	  Serial.println("");
	  
	  last = millis();
	}
    }
}

// 
// This is called when a falling edge is detected on the input pin
void lowISR()
{
  digitalWrite(statPin, HIGH);
  t = micros();
  delta = t - t2;
  t2 = t;
    if ( delta > 500 )
      //    if ( delta > (3*base_period) )
    {
      hz1200++;
      digitalWrite(dataPin, HIGH);

      // Got a 1 bit
      data_bit = 1;
      got_bit = 1;

#if 0      
      // Move to next state
      state = state_table[state].goto_if_1200;

      // Execute entry function
      (*state_table[state].entry_function)();
#endif      
    }
  else 
    {
      hz2400++;
      digitalWrite(dataPin, LOW);
      data_bit = 0;
      got_bit = 1;

#if 0      
      // Move to next state
      state = state_table[state].goto_if_2400;

      // Execute entry function
      (*state_table[state].entry_function)();
#endif      
    }  
  digitalWrite(statPin, LOW);

}



