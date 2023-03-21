#define F_CPU 8192000UL

#include <avr/interrupt.h>
#include <avr/io.h>
#include <util/delay.h>

const uint8_t digits[10]={0x3F,0x06,0x5B,0x4F,0x66,0x6D,0x7D,0x07,0x7F,0x6F};  // ����� ��� �����������
uint8_t time_difference[4]={0,0,0,0};  // ������� ���
uint8_t temp_time[2];
volatile int ms=0;
volatile uint8_t hour=0,minute=0,second=0;
volatile uint8_t pos=0;  // ������� ������� ���� (1-4)
volatile uint8_t button=12;  // �������� ������� ������
volatile uint8_t flag=0;  // ���� ������� ������
volatile uint8_t enable=0;  // ��������� ������� �����
uint8_t counter=0,flag2=0;  // ������� ������� ������ � ���� ��������� ������ ������

// ���������� ���������� ������� �0 �� ������������
ISR(TIMER0_OVF_vect) {
	// ��������� ��������� �������� ������������
	TCNT0=256-(0.001*F_CPU/1024);
	ms++;
	// ������� ������
	if (ms==1000) {
		ms=0;
		// ���� ���� ��������
		if (enable) second++;
		//PORTB=0b11111111;
	}
	// ������ ������
	if (enable && second==60) {
		second=0;
		minute++;
		// ��� ������
		if (minute==60) {
			minute=0;
			hour++;
			if (hour==24) hour=0;
		}
	}
	// ������� �������(����=������, ������=�������)
	/*if (ms==1000) {
		ms=0;
		if (enable) minute++;
		//PORTB=0b11111111;
	}
	if (minute==60) {
		minute=0;
		hour++;
		if (hour==24) hour=0;
	}*/
	// �������� ����������
	// ������� ������� ������
	counter=0;
	// �������� ��� � 20��
	if (ms%20==0) {
		// ������� ��������
		for (uint8_t i=0;i<3;i++) {
			if (i==0) PORTC=0b00111111;
			else if (i==1) PORTC=0b01011111;
			else if (i==2) PORTC=0b01101111;
			// ������� �����
			for (uint8_t j=0;j<4;j++) {
				if (!(PINC&(1<<j))) {
					button=i*4+j;
					counter++;
				}			
			}
		}
		// ������ ������ 1 ������
		if (counter==1) {
			flag=1;
		} else {
			button=12;
			flag=0;
		}
	}
	// ����� �� ����������
	// ������ ��� �� 5 ��������� 1 ������ �� 2�� �� 10��
	if (ms%10==0) {
		PORTB=~0b00010000;
		// ����� ������� ����
		if (enable) PORTA=digits[pos+1];
		else PORTA=digits[pos];
	} else if (ms%10==2) {
		PORTB=~0b00001000;
		// ����� �������� �����
		PORTA=digits[((hour+time_difference[pos])%24)/10];
	} else if (ms%10==4) {
		PORTB=~0b00000100;
		// ����� ������ �����
		PORTA=digits[((hour+time_difference[pos])%24)%10]|0x80;
	} else if (ms%10==6) {
		PORTB=~0b00000010;
		// ����� �������� �����
		PORTA=digits[minute/10];
	} else if (ms%10==8) {
		PORTB=~0b00000001;
		// ����� ������ �����
		PORTA=digits[minute%10];
	}
}

// ������� ���� ������ �� UART
void show_char(char c) {
	while ((UCSRA&(1<<UDRE))==0);
	UDR=c;
}

// ������� ������ �� UART
void show_string(char *str) {
	unsigned char c;
	while((c=*str++)!=0) show_char(c);
}

// ����� ����� ���������� ������� � ����� 4 ��� ����� UART
void input_uart(void) {
	show_string("Input UTC time (HH:MM): ");
	uint8_t error=1;
	char a,b;
	// ��������� ���� ����� UTC, ���� ���-�� ���� �������� ������
	while (error) {
		for (int i=0;i<2;i++) {
			// ������ ":" ����� �� � ��
			if (i==1) {
				while (!(UCSRA&(1<<RXC)));
				a=UDR;
				show_char(a);
			}
			// �������� ����� �������� �����/�����
			while (!(UCSRA&(1<<RXC)));
			temp_time[i]=UDR;
			if (i==0) {
				// �������� ��� �����
				if (temp_time[i]<'0' || temp_time[i]>'2') error=0;
			} else {
				// �������� ��� �����
				if (temp_time[i]<'0' || temp_time[i]>'5') error=0;
			}
			// ����������� ��������� �������
			show_char(temp_time[i]);
			temp_time[i]=(temp_time[i]-'0')*10;
			// �������� ����� ������ �����/�����
			while (!(UCSRA&(1<<RXC)));
			b=UDR;
			if (b<'0' || b>'9') error=0;
			// ����������� ��������� �������
			show_char(b);
			temp_time[i]=temp_time[i]-'0'+b;
		}
		// �������� �����
		if (temp_time[0]>23 || temp_time[1]>59 || a!=':' || error==0) error=1;
		else error=0;
		// ����� ��������� �� ������
		if (error) show_string("\rTry again: ");
	}
	// ���������� ����� � ����� � ������ �����
	hour=temp_time[0];
	minute=temp_time[1];
	enable=1;
	ms=0;
	// ���� 4 ���
	for (int i=0;i<4;i++) {
		// ����� ������ � ����� i+1 ����
		show_string("\n\rInput ");
		show_char('1'+i);
		show_string(" zone (HH): ");
		error=1;
		// ��������� ���� ����, ���� ���-�� ���� �������� ������
		while (error) {
			// �������� ����� �������� �����
			while (!(UCSRA&(1<<RXC)));
			temp_time[0]=UDR;
			// ����������� ��������� �������
			show_char(temp_time[0]);
			// �������� �����
			if (temp_time[0]<'0' || temp_time[0]>'2') error=0;
			temp_time[0]=(temp_time[0]-'0')*10;
			// �������� ����� ������ �����
			while (!(UCSRA&(1<<RXC)));
			b=UDR;
			// ����������� ��������� �������
			show_char(b);
			// �������� �����
			if (b<'0' || b>'9') error=0;
			temp_time[0]=temp_time[0]-'0'+b;
			// �������� �����
			if (temp_time[0]<=23 && error==1) error=0;
			else error=1;
			// ����� ��������� �� ������
			if (error) show_string("\r\nTry again: ");
		}
		// ���������� ����� i+1 ����
		time_difference[i]=(temp_time[0]-hour+24)%24;
		// ������� �� ��������� ����
		pos=i;
	}
	// ��������� �� ��������� �����
	show_string("\rDone!");
}

// ����� ����� ���������� ������� � ����� 4 ��� � ��������� ����������
void input_keyboard(void) {
	// ���� ����� UTC
	flag=0;
	flag2=0;
	// �������� ������� "0", "1" ��� "2"
	while (flag2==0) {
		if (flag==1 && (button==0 || button==4 || button==7)) flag2=1;
	}
	// ������ �������� �����
	if (button==0) hour=10;
	else if (button==4) hour=20;
	else if (button==7) hour=0;
	flag2=0;
	button=12;
	// �������� ���������� ������
	while (flag);
	// �������� ������� "0", "1" ... "9", ��� ��������� ������ �����
	while (flag2==0) {
		// ���� ������� �����=20, �� �������� ������ "0", "1", "2" ��� "3"
		if (flag==1 && hour==20 && (button==0 || button==4 || button==7 || button==8)) flag2=1;
		else if (flag==1 && hour!=20 && button!=3 && button!=11 && button!=12) flag2=1;
	}
	// ��������� ������ �����
	if (button==0) hour+=1;
	else if (button==1) hour+=4;
	else if (button==2) hour+=7;
	else if (button==4) hour+=2;
	else if (button==5) hour+=5;
	else if (button==6) hour+=8;
	else if (button==7) hour+=0;
	else if (button==8) hour+=3;
	else if (button==9) hour+=6;
	else if (button==10) hour+=9;
	// ���� ����� UTC
	flag2=0;
	button=12;
	while (flag);
	// �������� ������� "0", "1" ... "5"
	while (flag2==0) {
		if (flag==1 && (button==0 || button==1 || button==4 || button==5 || button==7 || button==8)) flag2=1;
	}
	// ������ �������� �����
	if (button==0) minute=10;
	else if (button==1) minute=40;
	else if (button==4) minute=20;
	else if (button==5) minute=50;
	else if (button==7) minute=0;
	else if (button==8) minute=30;
	flag2=0;
	button=12;
	// �������� ���������� ������
	while (flag);
	// �������� ������� "0", "1" ... "9", ��� ��������� ������ �����
	while (flag2==0) {
		if (flag==1 && button!=3 && button!=11 && button!=12) flag2=1;
	}
	// ������ ������ �����
	if (button==0) minute+=1;
	else if (button==1) minute+=4;
	else if (button==2) minute+=7;
	else if (button==4) minute+=2;
	else if (button==5) minute+=5;
	else if (button==6) minute+=8;
	else if (button==7) minute+=0;
	else if (button==8) minute+=3;
	else if (button==9) minute+=6;
	else if (button==10) minute+=9;
	enable=1;
	ms=0;
	flag2=0;
	button=12;
	// �������� ���������� ������
	while (flag);
	// ���� ������ ���
	for (uint8_t i=0;i<4;i++) {
		flag2=0;
		// �������� ������� "0", "1" ��� "2"
		while (flag2==0) {
			if (flag==1 && (button==0 || button==4 || button==7)) flag2=1;
		}
		// ������ �������� �����
		if (button==0) temp_time[0]=10;
		else if (button==4) temp_time[0]=20;
		else if (button==7) temp_time[0]=0;
		flag2=0;
		button=12;
		// �������� ���������� ������
		while (flag);
		// �������� ������� "0", "1" ... "9"
		while (flag2==0) {
			if (flag==1 && temp_time[0]==20 && (button==0 || button==4 || button==7 || button==8)) flag2=1;
			else if (flag==1 && temp_time[0]!=20 && button!=3 && button!=11 && button!=12) flag2=1;
		}
		// ������ ������ �����
		if (button==0) temp_time[0]+=1;
		else if (button==1) temp_time[0]+=4;
		else if (button==2) temp_time[0]+=7;
		else if (button==4) temp_time[0]+=2;
		else if (button==5) temp_time[0]+=5;
		else if (button==6) temp_time[0]+=8;
		else if (button==7) temp_time[0]+=0;
		else if (button==8) temp_time[0]+=3;
		else if (button==9) temp_time[0]+=6;
		else if (button==10) temp_time[0]+=9;
		// ���������� ����� i+1 ���� 
		time_difference[i]=(temp_time[0]-hour+24)%24;
		flag2=0;
		button=12;
		// ������� �� ��������� ����
		pos=i;
		// �������� ���������� ������
		while (flag);
	}
}

int main(void) {
	// ��������� ������
	DDRC=0b01110000;
	PORTC=0b00001111;
	DDRB=0xFF;
	DDRA=0xFF;
	// ��������� ������� �0 ���, ����� �� ������������ ��� � 1��
	TCCR0=0;
	TCCR0=(0<<WGM01)|(0<<WGM00);
	TCNT0=256-(0.001*F_CPU/1024);
	TIFR|=(1<<TOV0);
	TIMSK|=(1<<TOIE0);
	TCCR0|=(1<<CS02)|(0<<CS01)|(1<<CS00);
	// ��������� UART 
	UBRRL=F_CPU/9600/16-1;
	UCSRC=(1<<URSEL)|(1<<UCSZ1)|(1<<UCSZ0);
	UCSRB=(1<<TXEN)|(1<<RXEN);
	// ���������� ����������
	sei();
	flag2=0;
	// �������� ������� "<-" ��� "->"
	while (flag2==0) {
		if (flag==1 && (button==3 || button==11)) flag2=1;
	}
	// ���� "<-" ���� ����� UART
	if (button==3) input_uart();
	// ���� "->" ���� � ��������� ����������
	else if (button==11) input_keyboard();
	// ����������� ���� ������ ������ ����������
	while (1) {
		flag2=0;
		// �������� ������� "0", "1", "2", "<-" ��� "->"
		while (flag2==0) {
			if (flag==1 && (button==3 || button==11 || button==0 || button==4 || button==7)) flag2=1;
		}
		// ���� "<-", ������� ���������� ������� ���
		if (button==3) {
			if (pos==0) pos=3;
			else pos--;
		// ���� "->", ������� ��������� ������� ���
		} else if (button==11) {
			if (pos==3) pos=0;
			else pos++;
		// ���� "0", "1" ��� "2", �������� ������� ����� ��������� ����
		} else if (button==0 || button==4 || button==7) {
			if (button==0) temp_time[0]=10;
			else if (button==4) temp_time[0]=20;
			else if (button==7) temp_time[0]=0;
			flag2=0;
			button=12;
			while (flag);
			// �������� ������� "0", "1" ... "9", ��� ��������� ������ �����
			while (flag2==0) {
				// ���� ������� �����=20, �� �������� ������ "0", "1", "2" ��� "3"
				if (flag==1 && temp_time[0]==20 && (button==0 || button==4 || button==7 || button==8)) flag2=1;
				else if (flag==1 && temp_time[0]!=20 && button!=3 && button!=11 && button!=12) flag2=1;
			}
			// ��������� ������ ����� ��������� ����
			if (button==0) temp_time[0]+=1;
			else if (button==1) temp_time[0]+=4;
			else if (button==2) temp_time[0]+=7;
			else if (button==4) temp_time[0]+=2;
			else if (button==5) temp_time[0]+=5;
			else if (button==6) temp_time[0]+=8;
			else if (button==7) temp_time[0]+=0;
			else if (button==8) temp_time[0]+=3;
			else if (button==9) temp_time[0]+=6;
			else if (button==10) temp_time[0]+=9;
			// ������ ����������� ������ �������� ���� � ������
			time_difference[pos]=(temp_time[0]-hour+24)%24;
			flag2=0;
			button=12;
		}
		// �������� ���������� ������
		while (flag);
	}
}
