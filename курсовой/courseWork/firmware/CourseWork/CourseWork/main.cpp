/*
 * CourseWork.cpp
 *
 * Created: 17/10/2022 12:27:23
 * Author : Ali Moussa
 */ 

#include <avr/io.h>
#include <avr/interrupt.h>


int main(void)
{
	
	DDRB = 0xFF;
    /* Replace with your application code */
    while (1) 
    {
		PORTB = 0x00;
		for (int i = 0; i < 400000; ++i){
			asm("nop");
		}
		PORTB = 0xFF;
		for (int i = 0; i < 400000; ++i){
			asm("nop");
		}
    }
}

