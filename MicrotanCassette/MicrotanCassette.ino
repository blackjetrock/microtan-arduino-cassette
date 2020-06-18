#include <SPI.h>
#include <SD.h>

void cmd_help(String cmd);
  
// set up variables using the SD utility library functions:
Sd2Card card;
SdVolume volume;
SdFile root;

File myFile;

#define DEBUG_SERIAL 0

const int micPin = 2;
const int statPin = 8;
const int dataPin = 9;
const int earPin = 3;
const int switchPin = 5;
const int MAX_BYTES = 5000;

const int chipSelect = 53;

void setup()
{
  Serial.begin(115200);
  Serial.println("\nMicrotan 65 Cassette Interface");

  pinMode(micPin,     INPUT);
  pinMode(switchPin,  INPUT); 
  pinMode(statPin,    OUTPUT); 
  pinMode(dataPin,    OUTPUT);
  pinMode(earPin,     OUTPUT);
  
  attachInterrupt(digitalPinToInterrupt(micPin), lowISR, FALLING);
      
  pinMode(LED_BUILTIN, OUTPUT);
  
  if (!SD.begin(53)) {
    Serial.println("SD Card initialisation failed!");
  }
  else
    {
      Serial.println("SD card initialised.");
    }
}

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

typedef unsigned char BYTE;

typedef void (*FPTR)();
typedef void (*CMD_FPTR)(String cmd);

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
int bytecount = 15;

void state_init()
{
#if DEBUG_SERIAL  
  Serial.println("I");
#endif  
  
}

// Where the received data goes

unsigned char stored_bytes[MAX_BYTES] =
  {
    'T', 'E', 'S', 'T', ' ', ' ', '.', 'A',
    0x0a, 0x10, 0x11, 0x12, 0x13, 0x14
  };

void state_bytedone()
{
  // There's a spurious character at the start, we don't want o store it at all
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
}


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

  // Turn interrupts of as we want good timing for the data
  noInterrupts();
  
  // Send stored data back out
  // Header
  for(i=0; i<2400*30; i++)
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
      if( b == 12 )
	{
	  // 60ms of zeros
	  for(int z=0;z<300;z++)
	    {
	      send_bit(0);
	    }
	}
    }
  interrupts();
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


void cmd_display(String cmd)
{
  int i;
  char ascii[17];
  char c[2];
  
  int ascii_i = 0;
  
  Serial.print("Byte Count:");
  Serial.print(bytecount);
  Serial.print("  Index:");
  Serial.println(indx);

  for(i=0; i<bytecount; i++)
    {
      if( (i%16)==0 )
	{
	  Serial.print(" ");
	  Serial.print(ascii);
	  ascii_i = 0;
	  
	  Serial.println("");
	  if( i < 0x10)
	    {
	      Serial.print("0");
	    }
	  Serial.print(i, HEX);
	  Serial.print(":");
	}

      if( stored_bytes[i] < 0x10 )
	{
	  Serial.print("0");
	}
      
      Serial.print(stored_bytes[i], HEX);
      Serial.print(" ");

      if( isprint(stored_bytes[i]) )
	{
	  sprintf(c, "%c", stored_bytes[i]);
	}
	else
	  {
	    c[0] ='.';
	  }
      ascii[ascii_i++] = c[0];
    }

  Serial.print(ascii);
  Serial.print(" ");
  Serial.println("");
}

// Clear the buffer
void cmd_clear(String cmd)
{
  bytecount = -1;    // We reset to -1 so we drop the leading spurious character
}

void cmd_close(String cmd)
{
  myFile.close();
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

// read the file with th egiven name into the buffer
void cmd_readfile(String cmd)
{
  String arg;
  
  arg = cmd.substring(strlen("read "));

  Serial.print("Reading file '");
  Serial.print(arg);
  Serial.println("'");
  
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

      Serial.print(bytecount);
      Serial.println(" bytes read.");
    }
  else
    {
      // if the file didn't open, print an error:
      Serial.print("Error opening ");
      Serial.println(arg);
    }
}

void cmd_listfiles(String cmd)
{
  File dir;
  int numTabs = 0;
  
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

void cmd_writefile(String cmd)
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
  
  Serial.println("");
  Serial.print("Writing ");
  Serial.print(bytecount);
  Serial.print(" bytes to '");
  Serial.print(filename);
  Serial.print("'");
  Serial.println("");

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
      
      Serial.print(bytecount);
      Serial.println(" bytes written");
    }
  else
    {
      Serial.println("Could not open file");
    }


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

void loop()
{


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



