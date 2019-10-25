/*
 * uart2.cpp
 *
 *  Created on: 5 Feb, 2017
 *      Author: sylam
 */

/*
 * Plug wifi module "ESP-01s" onto the main board.
 * MAKE SURE BOTH POSITION AND ORIENTATION ARE CORRECT before power up.
 * Open MINICOM and type "AT+RST" followed by [Enter].
 * Some AT commands:
 * AT
 * AT+CWMODE=1
 * AT+CWLAP
 * AT+CWJAP="SSID","PASSWORD"
 * AT+CIFSR
 * AT+CIPSTART="UDP","0",0,3105,2
 * AT+CIPSTART="UDP","0",0,3106,2
 */


#include <p32mx250f128d.h>
#include <math.h>
/////////////////////////////////////PID
namespace
{
int target = 0;			// preset speed

class PID {
public:
	PID(int p, int in, int id, int d) {
			P = p;
			In = in; Id = id;	// I break "I" into two parts to avoid decimal
			D = d;
			acc = last = 0;
	}
	void reset(void) {acc = last = 0;}
	int control(int x, int target) {
			int error = x - target;
			int diff = x - last;
			last = x;
			acc += error;
			if (acc > 2000000000) acc = 2000000000;
			if (acc < -2000000000) acc = -2000000000;
			return - P * error - In * (acc >> Id) - D * diff;
	}
private:
	int P, In, Id, D, acc, last;
} pid_speed(2800,0,0,0), pid_track(800,0,0,0);


int floor1, flag1;
class Counter {
public:
	Counter(void) { counts = last = 0; }
	void sample(short i) {
		short d = i - last;
		counts <<= 4;			// shift out old data
		counts |= d;			// record new data
		last = i;
	}
	int getCount(void) {		// sum up data in "counts"
		int sum = 0, temp = counts;
		for (int i = 0; i < 8; i++) {
			sum += temp & 15;
			temp >>= 4;
		}
		return sum;
	}
private:
	short last;
	int counts;		// holds 8 pcs of 4-bit samples
} left, right;

unsigned short bound(int control) {
	if (control < 0) return 0;
	return control > 35000 ? 35000 : control;
	//if control> period, then follow period, otherwise control.
}

} // anonymous namespace for privacy

int getFloorReading(void) { return floor1; }
unsigned getLeft(void) { return left.getCount(); }
unsigned getRight(void) { return right.getCount(); }
bool checkFlag(void) { int f = flag1; flag1 = 0; return f; }


///////////////////////////////////////

//////////////////////////////// USB STUFF

extern "C" { // function provided by cdc stack

void USBDeviceInit(void);
void USBDeviceTasks(void);
void CDCDeviceTasks(void);
bool USBCDCTxREADY(void);
void putUSBUSART(char *data, unsigned char length);

void USBTasks(void) {
	USBDeviceTasks();
	U1OTGIR = 0xFF;
	IFS1bits.USBIF = 0;
	CDCDeviceTasks();
}

} //extern "C"

/////////////////////////////////Moter

void Moter_Init (void){ ////////right wheel
	OC1RS = 0x0; //3d04:25% 61A8:50% 927C:75% afc8:90% C350:100%duty cycle = percentage* period(T3:49999 +1)
	OC1R = 0x0;
	OC1CON = 0x800E; //ref Timer 3, PWM fault disaled
	OC1CONbits.ON = 1;

	OC2RS = 0x0; //3d04:25% 61A8:50% 927C:75% afc8:90% C350:100%duty cycle = percentage* period(T3:49999 +1)
	OC2R = 0x0;
	OC2CON = 0x800E; //ref Timer 3, PWM fault disaled
	OC2CONbits.ON = 1;
}



/////////////////////////////////

static class RingBuffer {
public:
	RingBuffer(void) { read = write = 0; }
	char get(void) {
		char c = buffer[read];
		if (read == SIZE-1) read = 0;
		else read++;
		return c;
	}
	void save(char c) {
		buffer[write] = c;
		if (write == SIZE-1) write = 0;
		else write++;
	}
	int empty(void) { return read == write; }
private:
	enum {SIZE=128};		// an alternative to #define
	char buffer[SIZE];
	int read, write;
} tx;

static class ReadBuffer : public RingBuffer {
public:
	ReadBuffer(void) { free();}
	void save(char c) {
		RingBuffer::save(c);		// send to cdc (buffered) as usual
		if (!hold)					// strip UDP message into buffer
			switch (ipd) {
			case IPD:				// phase: read length
				if (c == ':') ipd = len ? -1 : 0;
				else {
					int i = c - '0';
					if (i > 9) ipd = 0;
					else len = len * 10 + i;
				}
				break;
			case -1:				// phase: save message
				buffer[index++] = c;
				if (!--len) {
					buffer[index] = 0;
					hold = true;	// message is ready for processing
				}
				break;
			default:				// phase: look for header "IPD,"
				ipd = ipd << 8 | c;
			}
	}
	char *getString(void) { return hold ? buffer : 0; }
	void free(void) { ipd = len = index = 0; hold = false; }
private:
	enum {SIZE=100, IPD='I'<<24|'P'<<16|'D'<<8|','};
	int index, ipd, len;
	char buffer[SIZE];
	bool hold;
} rx;





extern "C"	// function required by cdc stack
{

void CDCRxChars(char *c, int length) {		// you got a message from cdc



	for (int i = 0; i < length; i++) {
		if (c[i] != '\n') {					// skip linefeed
			tx.save(c[i]);
			if (c[i] == '\r') tx.save('\n');// add linefeed after carriage return
		}
	}
	IEC1bits.U2TXIE = 1;
}

void CDCTxReadyNotify(void) {				// you may send message to cdc
	static char buffer[64];
	unsigned char len = 0;
	while (!rx.empty()) buffer[len++] = rx.get();
	if (len) putUSBUSART(buffer, len);
}

} //extern "C"



/////////////////////////////////
char Packet[]="1234",
	 CarX[]="123",
	 CarY[]="123",
	 BallX[]="123",
	 BallY[]="123",
	 CheckSum[]="12";

char Output[]="\n\r Car: 123 123 Ball: 123 123";


int Ball_X,Ball_Y,Car_X,Car_Y;

int t3FLAG;

int Temp_int;
void  Change_to_int(char temp)
{
	if (temp=='1')
		Temp_int=1;
	if (temp=='2')
		Temp_int=2;
	if (temp=='3')
	    Temp_int=3;
	if (temp=='4')
		Temp_int=4;
	if (temp=='5')
		Temp_int=5;
	if (temp=='6')
	    Temp_int=6;
	if (temp=='7')
		Temp_int=7;
	if (temp=='8')
		Temp_int=8;
	if (temp=='9')
		Temp_int=9;
	if (temp=='a')
		Temp_int=10;
	if (temp=='b')
		Temp_int= 11;
	if (temp=='c')
		Temp_int=12;
	if (temp=='d')
		Temp_int=13;
	if (temp=='e')
	    Temp_int=14;
	if (temp=='f')
		Temp_int=15;
	if (temp=='0')
		Temp_int=0;
}

int Hex_to_Dex(char temp2,char temp1,char temp0)
{
	int H2,H1,H0;

	Change_to_int(temp2);
	H2=Temp_int;
	Change_to_int(temp1);
	H1=Temp_int;
	Change_to_int(temp0);
	H0=Temp_int;

	return H2*16*16 + H1*16 + H0;
}

void Dex_to_Char(void)
{
	int x,y,z;

	x=Car_X/100;
	y=Car_X/10-10*x;
	z=Car_X-x*100-y*10;

	Output[8]=x+'0';
	Output[9]=y+'0';
	Output[10]=z+'0';

	x=Car_Y/100;
	y=Car_Y/10-10*x;
	z=Car_Y-x*100-y*10;

	Output[12]=x+'0';
	Output[13]=y+'0';
	Output[14]=z+'0';

	x=Ball_X/100;
	y=Ball_X/10-10*x;
	z=Ball_X-x*100-y*10;

	Output[22]=x+'0';
	Output[23]=y+'0';
	Output[24]=z+'0';

	x=Ball_Y/100;
	y=Ball_Y/10-10*x;
	z=Ball_Y-x*100-y*10;

	Output[26]=x+'0';
	Output[27]=y+'0';
	Output[28]=z+'0';



}
/////////////////////////////////
int abs (int value)
{
	if (value<0)
		value = value * -1;
	return value;
}




unsigned lcnt, rcnt;
int direction=0;

int init;

int PCar_X,PCar_Y, Sum_X=0, Sum_Y=0, n=0, temp_target=0, temp_direction=0;


int state1 = 0, state2 = 0;
int count1 = 0;

void Trace_Ball(void)
{

	// target=10;
	PORTC=0b110;

	//direction =0;
	//550 800 150
	if (Car_X < 430) //stop
	{
		temp_target = 1;
		temp_direction = 0;
	}
	if (Car_X > 580) //push ball
	{
		temp_target = 7																										;
		/*if (Car_Y < 200  && state1 == 0) //turn left if the car goes too right
		{
			count ++;
			direction = -15;

			if(count>400)
				{
				state1 = 1;
				direction = 1;
				count = 0;
				}


		}*/


	} // end 580
	if (Car_X > 430 && Car_X < 580)
	{

		if ( state2 == 0) //short pause before shooting
			{
				count1++;

				temp_target = 0;
				if (count1 >1000)
					{
					state2 = 1;
					temp_target = 0;
					temp_direction = 0;

					count1 = 0;
					}

			}
		if ( state2 == 1)
		{

		temp_target = 50; //shooting speed

		if (Car_X <460 && Car_X >430) //drifting its tail to shoot the ball
			temp_direction = 5000;
		else
			temp_direction = 0;
		}
	} //end shoot



	target = temp_target;
	direction = temp_direction;

}





int main(void) {
	char *s, buffer[128];
	static int count;

	USBDeviceInit();
	Moter_Init();

	while (1) {
		USBTasks();
		//Wifi
		if (USBCDCTxREADY()){
			s = rx.getString();
			if (s) {
				unsigned char len = 0;
				while ((buffer[len] = s[len])) len++;


				Packet[0]=buffer[0];
				Packet[1]=buffer[1];
				Packet[2]=buffer[2];
				Packet[3]=buffer[3];

				BallX[0]=buffer[4];
				BallX[1]=buffer[5];
				BallX[2]=buffer[6];

				BallY[0]=buffer[7];
				BallY[1]=buffer[8];
				BallY[2]=buffer[9];

				CarX[0]=buffer[10];
				CarX[1]=buffer[11];
				CarX[2]=buffer[12];


				CarY[0]=buffer[13];
				CarY[1]=buffer[14];
				CarY[2]=buffer[15];

				CheckSum[0]=buffer[16];
				CheckSum[1]=buffer[17];





				Car_X=Hex_to_Dex(CarX[0],CarX[1],CarX[2]);
				Car_Y=Hex_to_Dex(CarY[0],CarY[1],CarY[2]);

				Ball_X=Hex_to_Dex(BallX[0],BallX[1],BallX[2]);
				Ball_Y=Hex_to_Dex(BallY[0],BallY[1],BallY[2]);

				Dex_to_Char();


				//putUSBUSART(Output, 31);

				rx.free();

				if(t3FLAG )
				{
					t3FLAG =0;


				}
			}
		}
		//Counter
		if (checkFlag()) {		// flag set every 20 ms

					T2CON = 0;
					T4CON = 0;
					int lcnt = 1;
					int rcnt = 2;
					lcnt = TMR2;
					rcnt = TMR4;
					TMR2 = 0;
					TMR4 = 0;
					T4CONSET = 0x8002;
					T2CONSET = 0x8002;
					IFS0bits.T2IF = 0;
					IFS0bits.T4IF = 0;
					if (++count == 50) { count = 0; LATAINV = 1; }
				}
	}
}

extern "C"
{
void __attribute__ ((interrupt)) u2ISR(void) {



	while (U2STAbits.URXDA) rx.save(U2RXREG);
	IFS1bits.U2RXIF = 0;
	if (IEC1bits.U2TXIE) {
		while (!U2STAbits.UTXBF) {
			if (tx.empty()) { IEC1bits.U2TXIE = 0; break; }
			else U2TXREG = tx.get();
		}
		IFS1bits.U2TXIF = 0;
	}
}
void __attribute__ ((interrupt)) t3ISR(void) {

	static int count;
		if (++count == 400) {	// 2 second (800)
			count = 0;
			t3FLAG = 1;			// cannot print string at interrupt leve
								// so set a flag and let main() print the message
			LATAINV=1;			// toggle LED

		}

		Trace_Ball();
		right.sample(TMR4);
		left.sample(TMR2);
		int rt = right.getCount(), lt = left.getCount();
		int control_speed = pid_speed.control(rt + lt, target);
		int control_track = pid_track.control(rt - lt, direction);
		//OC1RS = 0x10000;
		//OC2RS = 0x10000;

		OC1RS = bound(control_speed + control_track); //control speed means on the right track
		OC2RS = bound(control_speed - control_track); //control track means adjustment


	IFS0bits.T3IF = 0;
}

}
