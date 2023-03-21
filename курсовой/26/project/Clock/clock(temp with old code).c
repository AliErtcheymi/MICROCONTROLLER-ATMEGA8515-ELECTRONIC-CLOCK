#define F_CPU 800000UL  // make it 8M

#include <avr/interrupt.h>
#include <avr/io.h>
#include <util/delay.h>
#define RS 6
#define E  5

int pos;
int flag=0;
char uart_time[4];
int timezone[8]={'M','A','M','O','K','A','C','E'};
int time_difference[4]={11,3,2,1};//11,3,2,1};
int numbers='0';
int temp_time[4]={0,0,0,0};
int hour=0,minute=0,second=0;
int poyas=0;  // position of the poyas, Magadan-Moscow-Kaliningrad-Europe
void send_a_command(unsigned char command);
void send_a_character(unsigned char character);

ISR(TIMER1_OVF_vect) {
	TCNT1=65450;  // 57724;  //65536 - 1 c * 1024 / 8.000.000 
	second++;
	if (second==60) {
		second=0;
		minute++;
		if (minute==60) {
			minute=0;
			hour++;
			if (hour==24) hour=0;
		}
		if (!flag) DRAW(timezone[2*poyas],timezone[2*poyas+1],(hour+time_difference[poyas])%24);
	}
}

void timer_init() {
	TCCR1A=0;
	TCCR1B=5;
	TCNT1=65450;  // 57724;
	TIFR=0;  // has interrupt flag
	TIMSK=0x80;  // allows T1 interrupt
}

void uart_init(void) {
	UBRR=51;
	UCR=(1<<TXEN)|(1<<RXEN);
}

void uart_puts(char *str) {
	unsigned char c;
	while((c=*str++)!=0) {
		while ((USR&(1<<UDRE))==0);
		UDR=c;
	}
}

void send_a_command (unsigned char command) {
    PORTB=command;
    PORTD&= ~(1<<RS);
    PORTD|= (1<<E);
    _delay_ms(50);
    PORTD&= ~(1<<E);
    PORTB =0;
}

void send_a_character (unsigned char character) {
    PORTB=character;
    PORTD|= (1<<RS);
    PORTD|= (1<<E);
    _delay_ms(50);
    PORTD&= ~(1<<E);
    PORTB =0;
}

void clrscr(void) {
	send_a_command(0x01);// komanda ochistit ekran
    send_a_command(0x38);// rezhim 16x2
    send_a_command(0x0C);// vklyuchit ekran 0C - no cursor 0D - black rectangle blink 0E - underline 0F - 0D + 0E
}

void DRAW(char first, char second, int hour1) {
	send_a_command(0x01);
	send_a_command(0x0C);
	send_a_character(first);//////// first initial output
	send_a_character(second);
	send_a_character(45);
	send_a_character(numbers+hour1/10);
	send_a_character(numbers+hour1%10);
	send_a_character(45);
	send_a_character(numbers+minute/10);
	send_a_character(numbers+minute%10);///////
}

int calc(int pos) {
	if (pos<2) return 5-pos; 
	else return 4-pos;
}

int calc2(int pos) {
	if (pos<2) return 8-pos; 
	else return 6-pos;
}

int main(void) {
	DDRA=0x00;
	PORTA=0xFF;  // PORTB = input
	DDRB=0xFF;
    DDRD=0xFF;
    clrscr();
	while (PINA&0b00000010 && PINA&0b00000100);
	if (!(PINA&0b00000010)) {  // left arrow
		while (!(PINA&0b00000010));//_delay_ms(100);
		send_a_command(0x0D);
		send_a_character('U');//////// first initial output
		send_a_character('T');
		send_a_character(45);
		send_a_character(numbers+temp_time[0]);
		send_a_character(numbers+temp_time[1]);
		send_a_character(45);
		send_a_character(numbers+temp_time[2]);
		send_a_character(numbers+temp_time[3]);///////
		pos=0;  // position of the cursor(kinda)
		for (int i=0;i<calc(pos);i++)
			send_a_command(0x10);  // move cursor to 1st hour digit
		while (pos!=4) {
			if (!(PINA&0b00000001)) {  // UP
				while (!(PINA&0b00000001));//_delay_ms(100);
				temp_time[pos]++;
				if (pos==0) {
					if (temp_time[pos]==2 && temp_time[pos+1]>3) temp_time[pos+1]=0;
					if (temp_time[pos]==3 && temp_time[pos+1]>3) temp_time[pos+1]=0;
				}
				if (pos==0 && temp_time[pos]==3) {
					temp_time[pos]=0;
					if (temp_time[pos+1]>3) temp_time[pos+1]=0;
				}
				if (pos==1) {
					if (temp_time[pos-1]==2) {  // в часах слева стоит 2
						if (temp_time[pos]==4) temp_time[pos]=0;
					} else {  // в часах слева не стоит 2
						if (temp_time[pos]==10) temp_time[pos]=0;
					}
				}
				if (pos==2 && temp_time[pos]==6) temp_time[pos]=0;
				if (pos==3 && temp_time[pos]==10) temp_time[pos]=0;
				hour=temp_time[0]*10+temp_time[1];
				minute=temp_time[2]*10+temp_time[3];
				DRAW('U','T',hour);
				send_a_command(0x0D);
				for (int i=0;i<calc(pos);i++)
					send_a_command(0x10);  // move cursor to 1st hour digit~~~~~~~~~~~~~~~~~~~~~~~~~~~~
			} else if (!(PINA&0b00000010)) {  // LEFT
				while (!(PINA&0b00000010));//_delay_ms(100);
				if (pos!=0) {
					pos--;
					if (pos==1) send_a_command(0x10);
					send_a_command(0x10);
				}
			} else if (!(PINA&0b00000100)) {  // RIGHT
				while (!(PINA&0b00000100));//_delay_ms(100);
				pos++;
				if (pos==2) send_a_command(0x14);
				send_a_command(0x14);
			} else if (!(PINA&0b00001000)) {  // DOWN
				while (!(PINA&0b00001000));//_delay_ms(100);
				temp_time[pos]--;
				if (pos==0 && temp_time[pos]==-1) {				
					temp_time[pos]=2;
					if (temp_time[pos+1]>3) temp_time[pos+1]=3;  // maybe make 20 instead of 23 after going from 0X to 2X
				}
				if (pos==1 && temp_time[pos]==-1) {
					if (temp_time[pos-1]==2) temp_time[pos]=3;
					else temp_time[pos]=9;
				}
				if (pos==2 && temp_time[pos]==-1) temp_time[pos]=5;
				if (pos==3 && temp_time[pos]==-1) temp_time[pos]=9;
				hour=temp_time[0]*10+temp_time[1];
				minute=temp_time[2]*10+temp_time[3];
				DRAW('U','T',hour);
				send_a_command(0x0D);
				for (int i=0;i<calc(pos);i++)
					send_a_command(0x10);  // move cursor to 1st hour digit~~~~~~~~~~~~~~~~~~~~~~~~~
			}
		}
		send_a_command(0x0C);  // disable the cursor
		hour=(/*time_difference[poyas]+*/temp_time[0]*10+temp_time[1])%24;
		minute=temp_time[2]*10+temp_time[3];
	} else if (!(PINA&0b00000100)) {  // right arrow
		uart_init();
		uart_puts("Input UTC time (HHMM): ");
		int error=1;  // flag until correct input
		while (error) {
			for (int i=0;i<4;i++) {
				while(!(USR&(1<<RXC))) {};
					uart_time[i]=UDR;
				while ((USR&(1<<UDRE))==0);
					UDR=uart_time[i];
			}
			if (uart_time[0]>='0' && uart_time[0]<='2' && uart_time[1]>='0' && 
			uart_time[1]<='9' && uart_time[2]>='0' && uart_time[2]<='9' && 
			uart_time[3]>='0' && uart_time[3]<='9' && ((uart_time[0]-'0')*10+uart_time[1]-'0')<=23) error=0;
			if (error) uart_puts("\r\nTry again: ");
		}
		hour=(/*time_difference[poyas]+*/(uart_time[0]-'0')*10+uart_time[1]-'0');
		minute=(uart_time[2]-'0')*10+uart_time[3]-'0';
	}
	timer_init();
	sei();
	DRAW(timezone[2*poyas],timezone[2*poyas+1],(hour+time_difference[poyas])%24);
	while (1) {
		if (!(PINA&0b00000001)) {  // UP
			while (!(PINA&0b00000001));
			int prev=time_difference[poyas];
			poyas++;
			if (poyas==4) poyas=0;
			//if (time_difference[poyas]-prev+hour<'0') hour=(time_difference[poyas]-prev+hour+24)%24;
			//else hour=(time_difference[poyas]-prev+hour)%24;
			DRAW(timezone[2*poyas],timezone[2*poyas+1],(hour+time_difference[poyas])%24);
		}
		if (!(PINA&0b00000100)) {  // RIGHT
			while (!(PINA&0b00000100));
			flag=1;
			pos=0;
			send_a_command(0x0D);  // enable the cursor
			DRAW(timezone[2*poyas],timezone[2*poyas+1],(hour+time_difference[poyas])%24);  // draw the time
			send_a_command(0x0D);
			for (int i=0;i<8;i++)
				send_a_command(0x10);  //cursor is on first letter
			while (pos!=3) {
				if (!(PINA&0b00000001)) {  // UP
					while (!(PINA&0b00000001));
					if (pos<2) {
						timezone[2*poyas+pos]++;
						if (timezone[2*poyas+pos]=='Z'+1) timezone[2*poyas+pos]='A';
					}
					if (pos==2) {/////////////////////////////////////////////////
						//int temp=time_difference[poyas];
						//time_difference[poyas]=(time_difference[poyas]+1)%24;
						//hour=(time_difference[poyas]-temp+hour)%24;
						time_difference[poyas]=(time_difference[poyas]+1)%24;
						//if (hour<24) hour++;
						//else
						//if (time_difference[poyas]-prev+hour<'0') hour=(time_difference[poyas]-prev+hour+24)%24;
						//else hour=(time_difference[poyas]-prev+hour)%24;
					}
					DRAW(timezone[2*poyas],timezone[2*poyas+1],(hour+time_difference[poyas])%24);
					send_a_command(0x0D);
					for (int i=0;i<calc2(pos);i++)
						send_a_command(0x10);  // move cursor to 1st hour digit
				} else if (!(PINA&0b00000010)) {  // LEFT
					while (!(PINA&0b00000010));
					if (pos!=0) {
						pos--;
						if (pos==1) {
							send_a_command(0x10);
							send_a_command(0x10);
						}
						send_a_command(0x10);
					}
				} else if (!(PINA&0b00000100)) {  // RIGHT
					while (!(PINA&0b00000100));
					pos++;
					if (pos==2) {
						send_a_command(0x14);
						send_a_command(0x14);
					}
					send_a_command(0x14);
				} else if (!(PINA&0b00001000)) {  // DOWN
					while (!(PINA&0b00001000));
					if (pos<2) {
						timezone[2*poyas+pos]--;
						if (timezone[2*poyas+pos]=='A'-1) timezone[2*poyas+pos]='Z';
					}
					if (pos==2) {
						//int temp=time_difference[poyas];
						//if (time_difference[poyas]!=0) time_difference[poyas]=(time_difference[poyas]-1)%24;
						//else time_difference[poyas]=23;
						//hour=(time_difference[poyas]-temp+hour)%24;////////////////////////////////
						/*time_difference[poyas]--;
						if (time_difference[poyas]-prev+hour<'0') hour=(time_difference[poyas]-prev+hour+24)%24;
							else hour=(time_difference[poyas]-prev+hour)%24;*/
						if (time_difference[poyas]!=0) time_difference[poyas]=(time_difference[poyas]-1)%24;
						else time_difference[poyas]=23;
					}
					DRAW(timezone[2*poyas],timezone[2*poyas+1],(hour+time_difference[poyas])%24);
					send_a_command(0x0D);
					for (int i=0;i<calc2(pos);i++)
						send_a_command(0x10);  // move cursor to 1st hour digit
				}
			}
			flag=0;
			send_a_command(0x0C);
		}
		if (!(PINA&0b00001000)) {  // DOWN
			while (!(PINA&0b00001000));
			int next=time_difference[poyas];
			poyas--;
			if (poyas==-1) poyas=3;
			//if (time_difference[poyas]-next+hour<'0') hour=(time_difference[poyas]-next+hour+24)%24;
			//else hour=(time_difference[poyas]-next+hour)%24;
			DRAW(timezone[2*poyas],timezone[2*poyas+1],(hour+time_difference[poyas])%24);
		}
	}
}

