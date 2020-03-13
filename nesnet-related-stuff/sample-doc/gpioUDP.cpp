/*
  UDPSendReceive.pde:
  This sketch receives UDP message strings, prints them to the serial port
  and sends an "acknowledge" string back to the sender
  A Processing sketch is included at the end of file that can be used to send
  and received messages for testing with a computer.
  created 21 Aug 2010
  by Michael Margolis
  This code is in the public domain.
  adapted from Ethernet library examples
*/

#include <Arduino.h>

#include <ESP8266WiFi.h>
#include <WiFiUdp.h>

#ifndef STASSID
#define STASSID "NESnet_guest"
#define STAPSK  "infinite"
#endif

//original bad because MISO has slow rising edge, have to wait too long to sample SPI
//int spi_clk = 0;		//HIGH to boot from SPI
//int spi_mosi = 1;  //LED ESP-01   (slow rising edge input)
//int spi_miso = 2;  //LED ESP-01S  (slow rising edge input) HIGH boot from SPI
//int spi_int = 3;

int spi_miso = 0;		//HIGH to boot from SPI
int spi_int = 1;  //LED ESP-01   (slow rising edge input)
int spi_ctl = 1;  //LED ESP-01   (slow rising edge input)
#define CTL_PARALLEL HIGH
#define CTL_SERIAL LOW
int spi_clk = 2;  //LED ESP-01S  (slow rising edge input) HIGH boot from SPI
int spi_mosi = 3;
#define SPI_REG_LEN 10  //8bit data + 2bit Status

#define SERVER_IP "192.168.1.250" //desktop
//#define SERVER_IP "192.168.1.251" //Rpi
char conn0_ip[64] = SERVER_IP;

//unsigned int conn0_port = 1250;      // local port to listen on
unsigned int conn0_port = 1234;      // local port to listen on

//#define SET_UDP		0x00
//#define SET_TCP		0x01
uint8_t conn0_protocol = 0; //SET_UDP


//comment out define to disable serial
//#define SERIAL_DBG

// buffers for receiving and sending data
char packetBuffer[UDP_TX_PACKET_MAX_SIZE + 1]; //buffer to hold incoming packet,
char  ReplyBuffer[] = "acknowledged\r\n";       // a string to send back

//variable arrays
#define VAR_ARRAY_SIZE 64 //max of addressing mode b7 WR, b6=VARMODE
#define VAR_NUM_MASK 0x3F //64 addresses b5-0
int outgoing[VAR_ARRAY_SIZE]; //local 6502 writes to these, need to send to remote ESP
int incoming[VAR_ARRAY_SIZE]; //local 6502 reads from these, need to update when remote ESP sends

int update_flag = 0;
//int command_flag = 0;
int pending_out = 0;

int main_cmd = 0;
int main_cmd_flag = 0;

WiFiUDP Udp;

//pass a string to send UDP packet
void send_udp(char* message) {
    //Udp.beginPacket(SERVER_IP, conn0_port);
    Udp.beginPacket(conn0_ip, conn0_port);
    Udp.write(message);
    Udp.endPacket();
}


// Use WiFiClient class to create TCP connections
WiFiClient client;

void tcp_connect() {
  //client.connect(conn0_ip, conn0_port);
//    Serial.println("connection failed");
//    delay(5000);
//    return;
  //if (!client.connect(host, port)) {
  if (!client.connect(conn0_ip, conn0_port)) {
    //Serial.println("connection failed");
  	send_udp("TCP connection failed..");
    //delay(5000);
    //return;
  } else {
  	send_udp("TCP connection success!");

  	if (client.connected()) {
  	  client.println("TCP connected, test message");
  	} else {
  		send_udp("wasn't connected when testin before message");
  	  client.println("TCP not connected");
	}
  }

  return;
}

//sends C formatted string followed by a carriage return and newline
void send_tcp_string(char* message) {
  if (client.connected()) {
    //client.println("hello from ESP8266");
    client.println(message);
  }
}

//void send_tcp_binary(uint8_t* data, int len) {
void send_tcp_binary(char* data, int len) {
	int i;
	if (client.connected()) {
	//send 1 byte at a time because that's how the library works...
	
		for (i=0; i<len; i++) {
		       // client.write(data[i]);
			client.print( data[i], HEX);
		}
	}
}

void send_gpio_udp() {
    //Udp.beginPacket(SERVER_IP, conn0_port);
    Udp.beginPacket(conn0_ip, conn0_port);

    	Udp.write("GPIO READ 3210=0b");

	if(digitalRead(3) == HIGH) //spi_int
    		Udp.write("1");
	else
    		Udp.write("0");

	if(digitalRead(2) == HIGH) //spi_miso
    		Udp.write("1");
	else
    		Udp.write("0");

	if(digitalRead(1) == HIGH) //spi_mosi
    		Udp.write("1");
	else
    		Udp.write("0");

	if(digitalRead(0) == HIGH) //spi_clk
    		Udp.write("1");
	else
    		Udp.write("0");

	//bit bang out data via SPI

	//serial mode
	//Udp.write(" INT_LO");
	digitalWrite(spi_int, LOW);   // turn the LED on (HIGH is the voltage level)

	//Udp.write(" b0-");

	int i;
	int data=0;
  //TODO define SPI_REG_LEN
	for ( i=0; i<SPI_REG_LEN; i++) { //Will cause us to drop the first Status bits & set them to 1
		//data = data<<1;
		data = data>>1; //LSB first, shift to the right

		digitalWrite(spi_clk, HIGH);  
		//sample miso
		if(digitalRead(spi_miso) == HIGH) {
    		//	Udp.write("1");
			data +=0x80; //shifting bits in from the left
		}
	//	else {
    	//		Udp.write("0");
	//	}	 //45.7usec using UDP write for each bit first clk rise to last fall
		//28.4usec when just storing byte
    //24.6usec arduinoIDE CPU-80Mhz flash-40Mhz
    //12.8usec platformIO CPU-160Mhz flash-80Mhz = 23 cycles on 6502
    //7 cycles on 6502 per BIT-Branch ~= 3 BIT-BR loops + time for ESP to service ISR

		//shift out next bit
		digitalWrite(spi_clk, LOW);
	}	

	Udp.write("SPI READ=");
	char data_array[3] = "0x";
	Udp.write(data_array);

	String byte_string = String(data, HEX);
	byte_string.toCharArray(data_array, 3);
	Udp.write(data_array);

	//back to parallel mode
	//serial mode
	Udp.write(" INT_HI");
	digitalWrite(spi_int, HIGH);   // turn the LED on (HIGH is the voltage level)


    Udp.endPacket();
}

int fetch_spi_byte() {
	//bit bang out data via SPI

	digitalWrite(spi_ctl, CTL_SERIAL);   //Set the SPI register in serial mode

	//TODO can speed up this code by trashing status bits without polling and shifting data
	
	//set the MOSI pin to shift 1's into SPI register and set W/R bits to clear isr
	digitalWrite(spi_mosi, HIGH);

	int i;
	int data=0;
	for ( i=0; i<SPI_REG_LEN; i++) { //Will cause us to drop the first Status bits & set them to 1

		data = data>>1; //LSB first, shift to the right

		digitalWrite(spi_clk, HIGH);  //indicate to logic analyzer we're about to poll MISO

		//sample miso
		if(digitalRead(spi_miso) == HIGH) {
    		//	Udp.write("1");
			data +=0x80; //shifting bits in from the left
		}
	//	else {
    	//		Udp.write("0");
	//	}	 //45.7usec using UDP write for each bit first clk rise to last fall
		//28.4usec when just storing byte
    //24.6usec arduinoIDE CPU-80Mhz flash-40Mhz
    //12.8usec platformIO CPU-160Mhz flash-80Mhz = 23 cycles on 6502
    //7 cycles on 6502 per BIT-Branch ~= 3 BIT-BR loops + time for ESP to service ISR

		//shift out next bit
		digitalWrite(spi_clk, LOW);
	}	

	digitalWrite(spi_ctl, CTL_PARALLEL);  //Set SPI reg to parallel so 6502 can write again

  return data;
}

void send_spi_byte(int data) {
	//bit bang data into SPI reg

	digitalWrite(spi_ctl, CTL_SERIAL);   //Set the SPI register in serial mode

	//need to set W & clear R bits
	//W=1 tells 6502 it can write to $5000 if desired (for next command)
	//R=0 tells 6502 our reply is in $5000 register
	data = data << 2; //lower 2 bits clear
	data += 1; //W set

	int i;
	for ( i=0; i<SPI_REG_LEN; i++) { //Will cause us to drop the first Status bits & set them to 1

		digitalWrite(spi_clk, HIGH);  //prepare to latch next bit & indicate bit about to change

		if(data & 0x01) {
			//bit set
			digitalWrite(spi_mosi, HIGH);
		} else {
			//bit clear
			digitalWrite(spi_mosi, LOW);
		}

		data = data>>1; //LSB first, shift to the right

		//latch the bit
		digitalWrite(spi_clk, LOW);
	}	

	digitalWrite(spi_ctl, CTL_PARALLEL);  //Set SPI reg to parallel so 6502 can write again

}

void print_udp_byte(int data) {
	Udp.beginPacket(conn0_ip, conn0_port);
 
	Udp.write("DATA BYTE= 0x");
	char data_array[3];// = "0x";
	//Udp.write(data_array);

	String byte_string = String(data, HEX);
	byte_string.toCharArray(data_array, 3);
	Udp.write(data_array);

	Udp.endPacket();
}

void print_udp_array(int *data, int len) {

	char data_str[3];// = "0x";

	Udp.beginPacket(conn0_ip, conn0_port);

	Udp.write("DATA ARRAY hi->lo:");

	len--; //offset to index of array

	while (len >= 0) {
		Udp.write(" ");

		String byte_string = String(data[len], HEX);
		byte_string.toCharArray(data_str, 3);
		Udp.write(data_str);

		len--;
	}

	Udp.endPacket();
}


//bit7: W/R 1-write 0-read
#define WR_CMD	0x80
//bit6: quick/immediate==1 access 2x64Bytes of vars/regs
#define VARIABLE_CMD	0x40
//bit5: long/rainbow==1 next byte contains length
#define MED_LEN_MASK  0x0F   //0 is currently invalid length
//	med==0 length in lower nibble
#define RAINBOW_CMD 0x20
#define RAINBOW_CONN_MASK  0x0F   //lower nibble stores connection number to send message
//bit4: special==1 command in lower nibble
//	the lengths are pre-defined
#define SPECIAL_CMD 0x10
//#define SPECIAL_OPERANDS 0x20 //operands associated with special opcode
#define FAST_CMD   0x08 //special command that gets reply in ISR instead of main thread
//#define SLOW_CMD   0x00 //special command that gets reply in ISR instead of main thread
#define SPECIAL_CMD_MASK  0x0F

//special commands (0-15)
//could expand to have 15 indicate that next byte is the command, IDK how many we'll want..
//SLOW COMMANDS
#define SCMD_RESET 0	//NO OPERANDS, SLOW_CMD
	#define RESET_VAL 0xA5 //expected reply that the ESP was reset
#define SCMD_MARK_READ 1//NO OPERANDS, SLOW
//#define SCMD_MOD_WIFI  2+SPECIAL_OPERANDS//SLOW
//#define SCMD_MOD_CONN  3+SPECIAL_OPERANDS//SLOW
//FAST COMMANDS
#define SCMD_MSG_POLL 1+FAST_CMD
#define SCMD_MSG_SENT 2+FAST_CMD
	//reply is number of messages in the incoming buffer

#define UNDEF_CMD 0x100 //can't fit in byte
int command = UNDEF_CMD;
char data_main[256];	//messages from the NES
int data_idx = 0;

uint8_t msg_buff[1024]; //messages from the internet 1KB buffer 0x0000 - 0x03FF (4pages)
uint8_t *msg_ptr = msg_buff;
uint8_t pending_msgs = 0;
//uint8_t pending_out = 0;

#define OPCODE 0
#define DATA 1
int command_mode = OPCODE;

uint8_t special_data[16];
uint8_t special_idx = 0;

int special_cmd(uint8_t cur_command) {
	
	int next_mode = OPCODE;

	//Fast commands have bit 3 set
	if (cur_command & FAST_CMD) {

		switch(cur_command&SPECIAL_CMD_MASK) {
			case SCMD_MSG_POLL:
				send_spi_byte(pending_msgs);
				break;
			case SCMD_MSG_SENT:
				send_spi_byte(pending_out);
				break;
		}
		
	} else { //slow command
	//	if (cur_command & SPECIAL_OPERANDS) {
	//		//need more data to process command
	//		next_mode = DATA;
	//		special_idx = 0;
	//	} else {
			//no operands, ready for main to process command
			main_cmd = cur_command & SPECIAL_CMD_MASK;
			main_cmd_flag = 1;
	//	}
	}

	return next_mode;
}

/*
int special_data(uint8_t cur_data) {

	int next_mode = DATA;

	//next byte of data
	special_data[special_idx] = cur_data;
	special_idx++;

	switch (command) {
	       
		case SCMD_MOD_CONN:
			next_mode = DATA;
			break;		
		case SCMD_MOD_WIFI:
			next_mode = DATA;
			break;		
	
	}

	return next_mode;
}
*/

ICACHE_RAM_ATTR void miso_isr() {
	//TODO disable interrupts
	//apparently this is already happening..?
	//when MISO falls while shifting data, this ISR doesn't get re-triggered..

	//measured 2.2-2.4usec from mosi falling edge to next instruction (spi_clk going high)
	//digitalWrite(spi_clk, HIGH);  //logic analyzer sees when ISR starts

	//fetch command byte & clear interrupt
	int data = fetch_spi_byte();

	//process command byte
	int reply = data^0xFF;

	int temp;

	//what mode are we in?
	if (command_mode == OPCODE) {
		command = data;

		if (command & VARIABLE_CMD) {
			if (command & WR_CMD) {
				//WRITE set mode and wait for data bytes
				command_mode = DATA;
			} else {
				//READ don't need data, just fetch reply
				temp = command & VAR_NUM_MASK;
				//reply = incoming[temp];
				send_spi_byte(incoming[temp]);
			}
		}
		else if (command & SPECIAL_CMD) {
			//command in nibble, or defined length
			//command_mode = special_cmd(command);
			//STAY IN COMMAND MODE
			special_cmd(command);

		} else { //message mode
			if (command & WR_CMD) {
				//not variable index addressing
				//assume single byte command for now
				command = data;
				command_mode = DATA;
				if (command & RAINBOW_CMD) {
					data_main[0] = (command & RAINBOW_CONN_MASK);
					data_idx = 1;
				} else { //medium mode length in lower nibble of command
					data_main[0] = 0; //always connection #0
					data_main[1] = (command & MED_LEN_MASK);
					data_idx = 2;
				}
			} else {
				//READ mode, fetch next byte
				send_spi_byte(*msg_ptr);
				msg_ptr++; //inc ptr for next read
				//send_spi_byte(msg_buff[idx]);
			}
		}
	} else { //DATA mode
		if ((command & (VARIABLE_CMD+WR_CMD))==(VARIABLE_CMD+WR_CMD)) {
			//WRITE variable
			temp = command & VAR_NUM_MASK;
			outgoing[temp] = data;
			//incoming[temp] = data; //TODO remove for testing only
			//got all the data
			command_mode = OPCODE;
			//inform main a variable was updated so it can be sent
			update_flag = 1;
		//{ else if (command & SPECIAL_CMD) {
		//	//more data for the command
		//	command_mode = special_data(data);
		} else {
			//WRITE opcodes bytes that follow coming from SPI reg-6502
			if (command & WR_CMD) {
				//not variable mode, have main process command
				data_main[data_idx] = data;
				//long mode, data_main[0] == 
				//if (command & RAINBOW_CMD) {
					if (data_idx == (data_main[1]+1)) { //+1 for connection number
						//received all incoming data
						data_main[data_idx+1] = 0; //null terminate TODO remove
						command_mode = OPCODE;
						//command_flag = 1;
						//TODO just increment this
						pending_out = 1;
					}
				//} else {
				//	//medium mode, len in opcode lower nibble (1-15 is valid)
				//	//TODO use zero length for special case?  Currently invalid
				//	if (data_idx == (command&MED_LEN_MASK)) {
				//	//if (data_idx == command) {
				//		//received all incoming data
				//		data_main[data_idx] = 0; //null terminate TODO remove
				//		command_mode = OPCODE;
				//		//command_flag = 1;
				//		pending_out = 1;
				//	}
				//}
				data_idx++;
			} else {
			//READ opcodes, put data in SPI reg for the 6502
			//Done up above...
			}
		}
	}
	
	
	//if 6502 expecting reply, send it
	//send_spi_byte(reply);
 
	//debug UDP message
	//print_udp_byte(data);
 
}

void init_spi_reg() {
  //initialize mapper spi register to 0xFF with W set
	digitalWrite(spi_int, LOW);  //serial 
	int i;

	digitalWrite(spi_mosi, HIGH); //SPI reg input 
  //TODO define SR length
	for ( i=0; i<10; i++) {
		//data = data<<1;

		digitalWrite(spi_clk, HIGH);  
		digitalWrite(spi_clk, LOW);  
		//sample miso
  }

	digitalWrite(spi_int, HIGH);   //parallel

}
/*
  test (shell/netcat):
  --------------------
	  nc -u 192.168.esp.address 8888
then type the message to send...

The first time, netcat didn't get an acknowledge reply, but it did after second message
*/

void setup() {
#ifdef SERIAL_DBG
  Serial.begin(115200);
#endif
  WiFi.mode(WIFI_STA);
  WiFi.begin(STASSID, STAPSK);
  while (WiFi.status() != WL_CONNECTED) {
#ifdef SERIAL_DBG
    Serial.print('.');
#endif
    delay(500);
  }
#ifdef SERIAL_DBG
  Serial.print("Connected! IP address: ");
  Serial.println(WiFi.localIP());
  Serial.printf("UDP server on port %d\n", conn0_port);
#endif
  Udp.begin(conn0_port);

//let's send a UDP packet to initialize connection with socketsv.py
    //Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());
    //Udp.beginPacket("192.168.0.100", 1234);
 //   Udp.beginPacket(SERVER_IP, conn0_port);
	Udp.beginPacket(conn0_ip, conn0_port);
    Udp.write("first UDP message opening 'connection' with socketsv.py");
    Udp.endPacket();

#ifdef SERIAL_DBG
  send_udp("SERIAL ENABLED");
#else
  send_udp("SERIAL DISABLED");
#endif

	//GPIO init

  //pinMode(0, INPUT); //clk
  //pinMode(1, INPUT); //MOSI

  pinMode(spi_clk, OUTPUT); //clk
  pinMode(spi_mosi, OUTPUT); //MOSI
  pinMode(spi_int, OUTPUT); //INT

  //pinMode(0, OUTPUT); //clk
  //pinMode(1, OUTPUT); //MOSI

  //pinMode(2, INPUT); //MISO
  pinMode(spi_miso, INPUT); //MISO
  //pinMode(3, INPUT); //INT

  //digitalWrite(0, LOW);   // turn the LED on (HIGH is the voltage level)
  //digitalWrite(1, LOW);   // turn the LED on (HIGH is the voltage level)
//  digitalWrite(spi_clk, LOW);   // turn the LED on (HIGH is the voltage level)
//  digitalWrite(spi_mosi, LOW);   // turn the LED on (HIGH is the voltage level)
//  digitalWrite(spi_int, HIGH);   // turn the LED on (HIGH is the voltage level)

  //flush SPI reg with 0xFF and set W bit so 6502 can perform first write
  init_spi_reg();

  send_udp("CLK MOSI output low, others input mode");

  //attachInterrupt(digitalPinToInterrupt(spi_miso), miso_isr, CHANGE);
  attachInterrupt(digitalPinToInterrupt(spi_miso), miso_isr, FALLING);
  send_udp("MISO pin interrupts enabled");

	//initialize data array & test debug
//	int i;	
//	for ( i=0; i<64; i++) {
//		incoming[i] = (i<<2);	
//		outgoing[i] = i+1;
//		update_flag = 1;
//	}

	//test TCP connection
	//figure out why this stopped working...
	//tcp_connect();
	//send_tcp("test tcp message");

}

void reset() {

	int i;
	for ( i=0; i<VAR_ARRAY_SIZE; i++) {
		incoming[i] = 0;
		outgoing[i] = 0;
	}

	//TODO what other things should be cleared/reset..?
	
	//indicate to 6502 that we accepted reset command
	send_spi_byte(RESET_VAL);
}

#define MODIFY_CONN 0x10
	#define MODIFY_CONN_IP		0x00
	#define MODIFY_CONN_PORT	0x01
	#define MODIFY_CONN_PROTOCOL	0x02
		#define SET_UDP		0x00
		#define SET_TCP		0x01
void update_metadata( uint8_t meta_cmd, char *data ){

	switch (meta_cmd) {

		case MODIFY_CONN:
			switch ( data[0] ) {
				//value starting in data[1]
				case MODIFY_CONN_IP:
					strcpy(conn0_ip, &data[1]);
					break;
				case MODIFY_CONN_PORT:
					conn0_port = data[1] | (data[2]<<8);
					break;
				case MODIFY_CONN_PROTOCOL:
					if ( data[1] == SET_TCP ) {
						//establish connection if not already made
  						//if (!client.connected()) {
  						//	tcp_connect();
  						//} 
						//assume IP address may have changed and always make new connection
  						tcp_connect();
					}
					conn0_protocol = data[1];
					break;
			}
			break;

	}

	return;
}

#define WR_VAR_ALL "WVA " //next 64Bytes are all variables
#define WR_VAR_NUM "WVN " //next byte is var number, then value
#define HEADERLEN 4 
#define NEWLINE 1

void loop() {


	// if there's data available, read a packet
	int i;
	int packetSize = Udp.parsePacket();
	if (packetSize) {
#ifdef SERIAL_DBG
	Serial.printf("Received packet of size %d from %s:%d\n    (to %s:%d, free heap = %d B)\n",
		packetSize,
		Udp.remoteIP().toString().c_str(), Udp.remotePort(),
		Udp.destinationIP().toString().c_str(), Udp.conn0_port(),
		ESP.getFreeHeap());
#endif
	
	// read the packet into packetBufffer
	int n = Udp.read(packetBuffer, UDP_TX_PACKET_MAX_SIZE);
	packetBuffer[n] = 0;

#ifdef SERIAL_DBG
	Serial.println("Contents:");
	Serial.println(packetBuffer);
#endif

	//ESP received a packet from remote console/server
	//interpret the header included in the UDP/TCP packet payload
	if (packetBuffer[0]=='W' & packetBuffer[1]=='V' & packetBuffer[2]=='A') {
		//update all variables
  		//send_udp("WVA command received");
		for( i=0; i<(packetSize-HEADERLEN-NEWLINE); i++) {
			incoming[i] = packetBuffer[i+HEADERLEN];
		}


	} else if (packetBuffer[0]=='W' & packetBuffer[1]=='V' & packetBuffer[2]=='N') {
		//next byte gives variable number, then value
  		//send_udp("WVN command received");
		//send udp 0 WVN 0A   <-  sets variable zero to ascii A
		incoming[packetBuffer[HEADERLEN]-0x30] = packetBuffer[HEADERLEN+1]; //convert from ascii to hex

	} else if (packetBuffer[0]=='M'){

		//copy data into buffer
		for( i=0; i<(packetSize-HEADERLEN-NEWLINE); i++) {
			msg_buff[i] = (uint8_t)packetBuffer[i+HEADERLEN];
		}
		//reset pointer to begining
		msg_ptr = msg_buff;
		//increment message count
		pending_msgs++;

	} else {
		//doesn't match
  		send_udp("unknown command received:");
  		send_udp(packetBuffer);
	}

//	incoming[0] = packetBuffer[0];
//	incoming[1] = packetBuffer[1];
//	incoming[2] = packetBuffer[2];
//	incoming[3] = packetBuffer[3];
//	
//	incoming[4] = packetBuffer[4];
//	incoming[5] = packetBuffer[5];
//	incoming[6] = packetBuffer[4]-0x30;

	//debug print statement of current variables
//	print_udp_array(incoming, VAR_ARRAY_SIZE);
//
//	// send a reply, to the IP address and port that sent us the packet we received
//	Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());
//	Udp.write(ReplyBuffer);
//	Udp.endPacket();
  } //end of incoming packet

	//check if the 6502 updated any of it's variables and transmit
	if (update_flag) {
		//for now, let's just transmit all variables
		print_udp_array(incoming, VAR_ARRAY_SIZE);
		print_udp_array(outgoing, VAR_ARRAY_SIZE);
		update_flag = 0;
	}

	//if (command_flag) {
	if (pending_out) {

	  	//send_udp(&data_main[2]);

		//process message 
		//Byte0: command/conn#
		//Byte1: length
		//Bytes2+ data/message
		//command_flag = 0;

		switch (data_main[0]) {

			case 0: //send to conn# 0
	  		//	send_udp(data_main);
				if (conn0_protocol == SET_UDP) {
	  				send_udp(&data_main[2]);
				} else {
	  				//send_tcp_string(&data_main[2]);
					//send_tcp_binary((uint8_t*)&data_main[2],  data_main[1]);
	  				//send_tcp_string(&data_main[2]);
	  				//send_udp(&data_main[2]);

					/*
	  				send_tcp_string(&data_main[2]);
					//<RP>

					send_tcp_binary(&data_main[2],  data_main[1]);
	  				send_tcp_string(&data_main[2]);
					//<525000RP>
					
					client.write(   (uint8_t*)&data_main[2],  data_main[1]);
	  				//send_tcp_string(&data_main[2]);
					//<RP
					//	1:      0x52
					//	2:      0x50
					//	3:      0x0
					//	4:      0x0
					//	5:      0x52
					//	6:      0x50
					*/

					client.write(   (uint8_t*)&data_main[2],  data_main[1]);
					//seems that write pushes data to outgoing buffer
					//but packet doesn't get sent till print/println is called
    					client.print("\n");
					//produces desired result:
					//len:    4       <RP  >
					//1:      0x52
					//2:      0x50
					//3:      0x0
					//4:      0x0
					//TODO IDK how sending binary version of newline works though...
					//reqpage:
					//  .byte 4
					//  .byte "RP" ;request page
					//  ;.word $0000 ;page num
					//  ;.word $1A25 ;page num (assembler sets little endian as needed)
					//  .byte $0D ;CR
					//  .byte $0A ;LF
					//seems it doesn't work...
					//len:    2       <RP>
					//1:      0x52
					//2:      0x50
					//3:
					//4:
					//seems arduino client.write is pretty worthless for binary data..
					//must be just trying convert to ascii 
					//which means binary CR/LF get translated to send  *facepalm*
					//https://github.com/esp8266/Arduino/blob/master/doc/esp8266wifi/client-class.rst
				}
				break;

			case 0xF: //meta data
				update_metadata( data_main[2], &data_main[3] );
				break;

		}
		//TODO just decrement this
		pending_out = 0;
	}

	if (main_cmd_flag) {
		switch(main_cmd) {
			case SCMD_RESET: 
				reset(); 
				break;
			case SCMD_MARK_READ: 
				if (pending_msgs > 0) {
					pending_msgs--;
				}
				break;
			default:
				; //compiler requires a statement..?
		}
		//command processed, can accept new command
		main_cmd_flag = 0;
	}

//send every second
//  delay(3000);               // wait for a second
//  send_gpio_udp();

}

