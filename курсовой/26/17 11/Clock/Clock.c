#define F_CPU 8192000UL

#include <avr/interrupt.h>
#include <avr/io.h>
#include <util/delay.h>

const uint8_t digits[10]={0x3F,0x06,0x5B,0x4F,0x66,0x6D,0x7D,0x07,0x7F,0x6F};  // цифры для индикаторов
uint8_t time_difference[4]={0,0,0,0};  // разница зон
uint8_t temp_time[2];
volatile int ms=0;
volatile uint8_t hour=0,minute=0,second=0;
volatile uint8_t pos=0;  // позиция текущей зоны (1-4)
volatile uint8_t button=12;  // значение нажатой кнопки
volatile uint8_t flag=0;  // флаг нажатой кнопки
volatile uint8_t enable=0;  // включение времени часов
uint8_t counter=0,flag2=0;  // счётчик нажатых кнопок и флаг обработки нужной кнопки

// обработчик прерывания таймера Т0 по переполнению
ISR(TIMER0_OVF_vect) {
	// повторная установка регистра переполнения
	TCNT0=256-(0.001*F_CPU/1024);
	ms++;
	// секунда прошла
	if (ms==1000) {
		ms=0;
		// если часы запущены
		if (enable) second++;
		//PORTB=0b11111111;
	}
	// минута прошла
	if (enable && second==60) {
		second=0;
		minute++;
		// час прошёл
		if (minute==60) {
			minute=0;
			hour++;
			if (hour==24) hour=0;
		}
	}
	// отладка времени(часы=минуты, минуты=секунды)
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
	// проверка клавиатуры
	// счётчик нажатых клавиш
	counter=0;
	// проверка раз в 20мс
	if (ms%20==0) {
		// перебор столбцов
		for (uint8_t i=0;i<3;i++) {
			if (i==0) PORTC=0b00111111;
			else if (i==1) PORTC=0b01011111;
			else if (i==2) PORTC=0b01101111;
			// перебор строк
			for (uint8_t j=0;j<4;j++) {
				if (!(PINC&(1<<j))) {
					button=i*4+j;
					counter++;
				}			
			}
		}
		// нажата только 1 кнопка
		if (counter==1) {
			flag=1;
		} else {
			button=12;
			flag=0;
		}
	}
	// вывод на индикаторы
	// каждый раз из 5 выводится 1 разряд на 2мс из 10мс
	if (ms%10==0) {
		PORTB=~0b00010000;
		// вывод позиции зоны
		if (enable) PORTA=digits[pos+1];
		else PORTA=digits[pos];
	} else if (ms%10==2) {
		PORTB=~0b00001000;
		// вывод десятков часов
		PORTA=digits[((hour+time_difference[pos])%24)/10];
	} else if (ms%10==4) {
		PORTB=~0b00000100;
		// вывод единиц часов
		PORTA=digits[((hour+time_difference[pos])%24)%10]|0x80;
	} else if (ms%10==6) {
		PORTB=~0b00000010;
		// вывод десятков минут
		PORTA=digits[minute/10];
	} else if (ms%10==8) {
		PORTB=~0b00000001;
		// вывод единиц минут
		PORTA=digits[minute%10];
	}
}

// вывести один символ по UART
void show_char(char c) {
	while ((UCSRA&(1<<UDRE))==0);
	UDR=c;
}

// вывести строку по UART
void show_string(char *str) {
	unsigned char c;
	while((c=*str++)!=0) show_char(c);
}

// режим ввода начального времени и часов 4 зон через UART
void input_uart(void) {
	show_string("Input UTC time (HH:MM): ");
	uint8_t error=1;
	char a,b;
	// цикличный ввод часов UTC, если где-то была допущена ошибка
	while (error) {
		for (int i=0;i<2;i++) {
			// чтение ":" между НН и ММ
			if (i==1) {
				while (!(UCSRA&(1<<RXC)));
				a=UDR;
				show_char(a);
			}
			// ожидание приёма десятков часов/минут
			while (!(UCSRA&(1<<RXC)));
			temp_time[i]=UDR;
			if (i==0) {
				// проверка для часов
				if (temp_time[i]<'0' || temp_time[i]>'2') error=0;
			} else {
				// проверка для минут
				if (temp_time[i]<'0' || temp_time[i]>'5') error=0;
			}
			// отображение введённого символа
			show_char(temp_time[i]);
			temp_time[i]=(temp_time[i]-'0')*10;
			// ожидание приёма единиц часов/минут
			while (!(UCSRA&(1<<RXC)));
			b=UDR;
			if (b<'0' || b>'9') error=0;
			// отображение введённого символа
			show_char(b);
			temp_time[i]=temp_time[i]-'0'+b;
		}
		// проверка ввода
		if (temp_time[0]>23 || temp_time[1]>59 || a!=':' || error==0) error=1;
		else error=0;
		// вывод сообщения об ошибке
		if (error) show_string("\rTry again: ");
	}
	// сохранение часов и минут и запуск часов
	hour=temp_time[0];
	minute=temp_time[1];
	enable=1;
	ms=0;
	// ввод 4 зон
	for (int i=0;i<4;i++) {
		// вывод строки о вводе i+1 зоны
		show_string("\n\rInput ");
		show_char('1'+i);
		show_string(" zone (HH): ");
		error=1;
		// цикличный ввод зоны, если где-то была допущена ошибка
		while (error) {
			// ожидание приёма десятков часов
			while (!(UCSRA&(1<<RXC)));
			temp_time[0]=UDR;
			// отображение введённого символа
			show_char(temp_time[0]);
			// проверка ввода
			if (temp_time[0]<'0' || temp_time[0]>'2') error=0;
			temp_time[0]=(temp_time[0]-'0')*10;
			// ожидание приёма единиц часов
			while (!(UCSRA&(1<<RXC)));
			b=UDR;
			// отображение введённого символа
			show_char(b);
			// проверка ввода
			if (b<'0' || b>'9') error=0;
			temp_time[0]=temp_time[0]-'0'+b;
			// проверка ввода
			if (temp_time[0]<=23 && error==1) error=0;
			else error=1;
			// вывод сообщения об ошибке
			if (error) show_string("\r\nTry again: ");
		}
		// сохранение часов i+1 зоны
		time_difference[i]=(temp_time[0]-hour+24)%24;
		// переход на следующую зону
		pos=i;
	}
	// сообщение об окончании ввода
	show_string("\rDone!");
}

// режим ввода начального времени и часов 4 зон с матричной клавиатуры
void input_keyboard(void) {
	// ввод часов UTC
	flag=0;
	flag2=0;
	// ожидание нажатия "0", "1" или "2"
	while (flag2==0) {
		if (flag==1 && (button==0 || button==4 || button==7)) flag2=1;
	}
	// запись десятков часов
	if (button==0) hour=10;
	else if (button==4) hour=20;
	else if (button==7) hour=0;
	flag2=0;
	button=12;
	// ожидание отпускания кнопки
	while (flag);
	// ожидание нажатия "0", "1" ... "9", для изменения единиц часов
	while (flag2==0) {
		// если десятки часов=20, то ожидание только "0", "1", "2" или "3"
		if (flag==1 && hour==20 && (button==0 || button==4 || button==7 || button==8)) flag2=1;
		else if (flag==1 && hour!=20 && button!=3 && button!=11 && button!=12) flag2=1;
	}
	// изменение единиц часов
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
	// ввод минут UTC
	flag2=0;
	button=12;
	while (flag);
	// ожидание нажатия "0", "1" ... "5"
	while (flag2==0) {
		if (flag==1 && (button==0 || button==1 || button==4 || button==5 || button==7 || button==8)) flag2=1;
	}
	// запись десятков минут
	if (button==0) minute=10;
	else if (button==1) minute=40;
	else if (button==4) minute=20;
	else if (button==5) minute=50;
	else if (button==7) minute=0;
	else if (button==8) minute=30;
	flag2=0;
	button=12;
	// ожидание отпускания кнопки
	while (flag);
	// ожидание нажатия "0", "1" ... "9", для изменения единиц минут
	while (flag2==0) {
		if (flag==1 && button!=3 && button!=11 && button!=12) flag2=1;
	}
	// запись единиц минут
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
	// ожидание отпускания кнопки
	while (flag);
	// ввод четырёх зон
	for (uint8_t i=0;i<4;i++) {
		flag2=0;
		// ожидание нажатия "0", "1" или "2"
		while (flag2==0) {
			if (flag==1 && (button==0 || button==4 || button==7)) flag2=1;
		}
		// запись десятков часов
		if (button==0) temp_time[0]=10;
		else if (button==4) temp_time[0]=20;
		else if (button==7) temp_time[0]=0;
		flag2=0;
		button=12;
		// ожидание отпускания кнопки
		while (flag);
		// ожидание нажатия "0", "1" ... "9"
		while (flag2==0) {
			if (flag==1 && temp_time[0]==20 && (button==0 || button==4 || button==7 || button==8)) flag2=1;
			else if (flag==1 && temp_time[0]!=20 && button!=3 && button!=11 && button!=12) flag2=1;
		}
		// запись единиц часов
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
		// сохранение часов i+1 зоны 
		time_difference[i]=(temp_time[0]-hour+24)%24;
		flag2=0;
		button=12;
		// переход на следующую зону
		pos=i;
		// ожидание отпускания кнопки
		while (flag);
	}
}

int main(void) {
	// настройка портов
	DDRC=0b01110000;
	PORTC=0b00001111;
	DDRB=0xFF;
	DDRA=0xFF;
	// настройка таймера Т0 так, чтобы он переполнялся раз в 1мс
	TCCR0=0;
	TCCR0=(0<<WGM01)|(0<<WGM00);
	TCNT0=256-(0.001*F_CPU/1024);
	TIFR|=(1<<TOV0);
	TIMSK|=(1<<TOIE0);
	TCCR0|=(1<<CS02)|(0<<CS01)|(1<<CS00);
	// настройка UART 
	UBRRL=F_CPU/9600/16-1;
	UCSRC=(1<<URSEL)|(1<<UCSZ1)|(1<<UCSZ0);
	UCSRB=(1<<TXEN)|(1<<RXEN);
	// разрешение прерываний
	sei();
	flag2=0;
	// ожидание нажатия "<-" или "->"
	while (flag2==0) {
		if (flag==1 && (button==3 || button==11)) flag2=1;
	}
	// если "<-" ввод через UART
	if (button==3) input_uart();
	// если "->" ввод с матричной клавиатуры
	else if (button==11) input_keyboard();
	// бесконечный цикл опроса кнопок клавиатуры
	while (1) {
		flag2=0;
		// ожидание нажатия "0", "1", "2", "<-" или "->"
		while (flag2==0) {
			if (flag==1 && (button==3 || button==11 || button==0 || button==4 || button==7)) flag2=1;
		}
		// если "<-", выбрать предыдущую позицию зон
		if (button==3) {
			if (pos==0) pos=3;
			else pos--;
		// если "->", выбрать следующую позицию зон
		} else if (button==11) {
			if (pos==3) pos=0;
			else pos++;
		// если "0", "1" или "2", изменить десятки часов выбранной зоны
		} else if (button==0 || button==4 || button==7) {
			if (button==0) temp_time[0]=10;
			else if (button==4) temp_time[0]=20;
			else if (button==7) temp_time[0]=0;
			flag2=0;
			button=12;
			while (flag);
			// ожидание нажатия "0", "1" ... "9", для изменения единиц часов
			while (flag2==0) {
				// если десятки часов=20, то ожидание только "0", "1", "2" или "3"
				if (flag==1 && temp_time[0]==20 && (button==0 || button==4 || button==7 || button==8)) flag2=1;
				else if (flag==1 && temp_time[0]!=20 && button!=3 && button!=11 && button!=12) flag2=1;
			}
			// изменение единиц часов выбранной зоны
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
			// запись полученного нового значения зоны в массив
			time_difference[pos]=(temp_time[0]-hour+24)%24;
			flag2=0;
			button=12;
		}
		// ожидание отпускания кнопки
		while (flag);
	}
}
