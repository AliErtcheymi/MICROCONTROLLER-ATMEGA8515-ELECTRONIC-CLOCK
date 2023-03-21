#define F_CPU 8000000UL  // make it 8M

#include <avr/interrupt.h>
#include <avr/io.h>
#include <util/delay.h>
#define RS 6
#define E  5

int pos;
int flag=0;
char uart_time[4];
int timezone[8]={'A','A','B','B','C','C','D','D'};
int time_difference[4];
int numbers='0';
int temp_time[4];
int hour=0,minute=0,second=0;
int poyas=0;
void send_a_command(unsigned char command);
void send_a_character(unsigned char character);

// function that describes timer interrupt, when occurs, seconds are being incremented and so the time
ISR(TIMER1_OVF_vect) {
	TCNT1=57724;  // 57724;  //65536 - 1 c * 8.000.000 / 1024
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

// T1 initialization commands to overflow every second
void timer_init() {
	TCCR1A=0;
	TCCR1B=5;
	TCNT1=57724;  // 57724;
	TIFR=0;
	TIMSK=0x80;  // allows T1 interrupt
	sei();
}
// uart initialization commands with the speed 9600 bod
void uart_init(void) {
	UBRR=51;
	UCR=(1<<TXEN)|(1<<RXEN);
}

// send a string via uart to IBM
void uart_puts(char *str) {
	unsigned char c;
	while((c=*str++)!=0) {
		while ((USR&(1<<UDRE))==0);
		UDR=c;
	}
}

// send a certain command to display to manage its options
void send_a_command (unsigned char command) {
    PORTB=command;
    PORTD&= ~(1<<RS);
    PORTD|= (1<<E);
    _delay_ms(50);
    PORTD&= ~(1<<E);
    PORTB =0;
}

// display one character on lcd
void send_a_character (unsigned char character) {
    PORTB=character;
    PORTD|= (1<<RS);
    PORTD|= (1<<E);
    _delay_ms(50);
    PORTD&= ~(1<<E);
    PORTB =0;
}

// a set of commands to clear the display
void clrscr(void) {
	send_a_command(0x01);  // komanda ochistit ekran
    send_a_command(0x38);  // rezhim 16x2
    send_a_command(0x0C);  // vklyuchit ekran 0C - no cursor 0D - black rectangle blink 0E - underline 0F - 0D + 0E
}

// procedure to display time as NN:HH:MM
void DRAW(char first, char second, int hour1) {
	send_a_command(0x01);
	send_a_command(0x0C);
	send_a_character(first);
	send_a_character(second);
	send_a_character(45);
	send_a_character(numbers+hour1/10);
	send_a_character(numbers+hour1%10);
	send_a_character(45);
	send_a_character(numbers+minute/10);
	send_a_character(numbers+minute%10);
}

// just a function to calculate cursor shift (when it has to skip ":" in HH:MM)
int calc(int pos) {
	if (pos<2) return 5-pos; 
	else return 4-pos;
}

// just a function to calculate cursor shift (when it has to skip ":" in NN:HH)
int calc2(int pos) {
	if (pos<2) return 8-pos; 
	else return 6-pos;
}

// procedure to enter time poyases edit mode
// inside you can change the name and current time in NN:HH to change previous poyas to a new one
void time_editor(void) {
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
			if (pos==2)	time_difference[poyas]=(time_difference[poyas]+1)%24;
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
}

int main(void) {
	DDRA=0x00;
	PORTA=0xFF;
	DDRB=0xFF;
    DDRD=0xFF;
    clrscr();
	while (PINA&0b00000010 && PINA&0b00000100);
	if (!(PINA&0b00000010)) {  // LEFT arrow was pressed and button mode was selected
		while (!(PINA&0b00000010));
		send_a_command(0x0D);
		send_a_character('X');
		send_a_character('X');
		send_a_character(45);
		send_a_character(numbers+temp_time[0]);
		send_a_character(numbers+temp_time[1]);
		send_a_character(45);
		send_a_character(numbers+temp_time[2]);
		send_a_character(numbers+temp_time[3]);
		pos=0;  // position of the cursor
		for (int i=0;i<calc(pos);i++)
			send_a_command(0x10);  // move cursor to 1st hour digit

		// cycle that waits until you leave the init menu by pressing RIGHT 4 times in a row
		// you have to input universal time (UTC), that is being used to calculate all other time zones across the globe
		while (pos!=4) {
			if (!(PINA&0b00000001)) {  // UP
				while (!(PINA&0b00000001));
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
					if (temp_time[pos-1]==2) {  // hours/10 == 2
						if (temp_time[pos]==4) temp_time[pos]=0;
					} else {  // hours/10 != 2
						if (temp_time[pos]==10) temp_time[pos]=0;
					}
				}
				if (pos==2 && temp_time[pos]==6) temp_time[pos]=0;
				if (pos==3 && temp_time[pos]==10) temp_time[pos]=0;
				hour=temp_time[0]*10+temp_time[1];
				minute=temp_time[2]*10+temp_time[3];
				DRAW('X','X',hour);
				send_a_command(0x0D);
				for (int i=0;i<calc(pos);i++)
					send_a_command(0x10);  // move cursor 1 digit left
			} else if (!(PINA&0b00000010)) {  // LEFT
				while (!(PINA&0b00000010));
				if (pos!=0) {
					pos--;
					if (pos==1) send_a_command(0x10);
					send_a_command(0x10);
				}
			} else if (!(PINA&0b00000100)) {  // RIGHT
				while (!(PINA&0b00000100));
				pos++;
				if (pos==2) send_a_command(0x14);
				send_a_command(0x14);
			} else if (!(PINA&0b00001000)) {  // DOWN
				while (!(PINA&0b00001000));
				temp_time[pos]--;
				if (pos==0 && temp_time[pos]==-1) {				
					temp_time[pos]=2;
					if (temp_time[pos+1]>3) temp_time[pos+1]=3;
				}
				if (pos==1 && temp_time[pos]==-1) {
					if (temp_time[pos-1]==2) temp_time[pos]=3;
					else temp_time[pos]=9;
				}
				if (pos==2 && temp_time[pos]==-1) temp_time[pos]=5;
				if (pos==3 && temp_time[pos]==-1) temp_time[pos]=9;
				hour=temp_time[0]*10+temp_time[1];
				minute=temp_time[2]*10+temp_time[3];
				DRAW('X','X',hour);
				send_a_command(0x0D);
				for (int i=0;i<calc(pos);i++)
					send_a_command(0x10);  // move cursor 1 digit left
			}
		}
		send_a_command(0x0C);  // disable the cursor
		hour=(temp_time[0]*10+temp_time[1])%24;
		minute=temp_time[2]*10+temp_time[3];

		for (int i=0;i<4;i++) {
			time_editor();
			poyas++;
		}
		timer_init();
	} else if (!(PINA&0b00000100)) {  // RIGHT arrow was pressed and uart mode was selected
		uart_init();
		uart_puts("Input setup time (HH:MM): ");
		int error=1;

		// cycle that repeats until correct input
		while (error) {
			char a;
			for (int i=0;i<4;i++) {
				if (i==2) {
					while (!(USR&(1<<RXC)));
					a=UDR;
					while ((USR&(1<<UDRE))==0);
					UDR=a;
				}
				while (!(USR&(1<<RXC)));
				uart_time[i]=UDR;
				while ((USR&(1<<UDRE))==0);
				UDR=uart_time[i];
			}
			if (uart_time[0]>='0' && uart_time[0]<='2' && uart_time[1]>='0' &&
			uart_time[1]<='9' && uart_time[2]>='0' && uart_time[2]<='9' &&
			uart_time[3]>='0' && uart_time[3]<='9' && ((uart_time[0]-'0')*10+uart_time[1]-'0')<=23 &&
			((uart_time[2]-'0')*10+uart_time[3]-'0')<=59 && a==':') error=0;
			if (error) uart_puts("\r\nTry again: ");
		}
		hour=((uart_time[0]-'0')*10+uart_time[1]-'0');
		minute=(uart_time[2]-'0')*10+uart_time[3]-'0';

		// cycle that gets 4 correct different time zones and their current time
		for (int i=0;i<4;i++) {
			uart_puts("\n\rInput ");
			while ((USR&(1<<UDRE))==0);
			UDR=i+'1';
			uart_puts(" name and time (NN:HH): ");
			error=1;
			int hour1, hour2;
			while (error) {
				while (!(USR&(1<<RXC)));
				timezone[i*2]=UDR;
				while ((USR&(1<<UDRE))==0);
				UDR=timezone[i*2];
				while (!(USR&(1<<RXC)));
				timezone[i*2+1]=UDR;
				while ((USR&(1<<UDRE))==0);
				UDR=timezone[i*2+1];
				while (!(USR&(1<<RXC)));
				char a=UDR;
				while ((USR&(1<<UDRE))==0);
				UDR=a;
				while (!(USR&(1<<RXC)));
				hour1=UDR;
				while ((USR&(1<<UDRE))==0);
				UDR=hour1;
				while (!(USR&(1<<RXC)));
				hour2=UDR;
				while ((USR&(1<<UDRE))==0);
				UDR=hour2;
				if (timezone[i*2]>='A' && timezone[i*2]<='Z' && timezone[i*2+1]>='A' && timezone[i*2+1]<='Z' && a==':' &&
				hour1>='0' && hour1<='9' && hour2>='0' && hour2<='9' && ((hour1-'0')*10+hour2-'0')<=23) error=0;
				if (error) uart_puts("\r\nTry again: ");
			}
			time_difference[i]=((hour1-'0')*10+hour2-'0'-hour+24)%24;
		}
		timer_init();
	}
	poyas=0;
	DRAW(timezone[2*poyas],timezone[2*poyas+1],(hour+time_difference[poyas])%24);

	// infinite cycle to be clock and wait for potential input:
	// UP or DOWN to change current displayed time zone
	// or RIGHT to enter edit mode of current selected time zone
	while (1) {
		if (!(PINA&0b00000001)) {  // UP
			while (!(PINA&0b00000001));
			poyas++;
			if (poyas==4) poyas=0;
			DRAW(timezone[2*poyas],timezone[2*poyas+1],(hour+time_difference[poyas])%24);
		}
		if (!(PINA&0b00000100)) {  // RIGHT
			while (!(PINA&0b00000100));
			time_editor();
			send_a_command(0x0C);
		}
		if (!(PINA&0b00001000)) {  // DOWN
			while (!(PINA&0b00001000));
			poyas--;
			if (poyas==-1) poyas=3;
			DRAW(timezone[2*poyas],timezone[2*poyas+1],(hour+time_difference[poyas])%24);
		}
	}
}
