/* Name: main.c
 * Projekt: rgb_led_strip_steuerung
 * Author: Thorsten Kattanek
 * Erstellt am: 25.03.2016
 * Copyright: Thorsten Kattanek
 * License: GNU GPL v2 (see License.txt), GNU GPL v3 or proprietary (CommercialLicense.txt)
 *
 */

#define F_CPU   12000000UL

#include <avr/io.h>
#include <avr/wdt.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <avr/pgmspace.h>   /* benötigt von usbdrv.h */

#include <string.h>

#define BAUD 115200
#include <util/setbaud.h>

volatile unsigned char red_pw,green_pw,blue_pw;
volatile unsigned char red_pwm_counter,green_pwm_counter,blue_pwm_counter;

void InitPWMChannels();
void SetRGB(unsigned char r, unsigned char g, unsigned char b);
void InitUART(void);
void SendStringUart(char* string);
unsigned char CheckNewString(char* IncomingString);
void WaitESP_OK(char* IncomingString);
void ExecuteCommand(unsigned char* cmd_string);

volatile unsigned char red = 0;
volatile unsigned char status = 0;

#define MAX_UART_STRING 128

volatile char UARTString[MAX_UART_STRING];
volatile unsigned char UARTStringLen = 0;
volatile char NewUARTString = 0;

volatile unsigned char r,g,b;

#define SINGLE_COLOR_MODE 0
#define GARDIENT_MODE 1    

volatile unsigned char color_mode = SINGLE_COLOR_MODE;

const uint16_t pwmtable_8[32] PROGMEM =
{
    0, 1, 2, 2, 2, 3, 3, 4, 5, 6, 7, 8, 10, 11, 13, 16, 19, 23,
    27, 32, 38, 45, 54, 64, 76, 91, 108, 128, 152, 181, 215, 255
};

int main(void)
{ 
    char IncomingString[MAX_UART_STRING];


    unsigned char step = 0;
    unsigned char wait_count0 = 0;

    DDRC = 0x0f;
    PORTC = 0x00;

    InitPWMChannels();
    InitUART();
    sei();

    //SendStringUart("Auchtung\r\n");
    
    
    _delay_ms(3000);
    
    // ESP Modul auf RS232 Verbindung testen
    SendStringUart("AT\r\n");
    WaitESP_OK(IncomingString);
    
    PORTC |= 0x01;
       
    /////////////////////////////////////////////
    
    SendStringUart("AT+CWMODE=1\r\n");
    WaitESP_OK(IncomingString);
    
    SendStringUart("AT+CWJAP=\"LINUX-ANDONE\",\"andone1974\"\r\n");
    WaitESP_OK(IncomingString);
    
    SendStringUart("AT+CIPMUX=1\r\n");
    WaitESP_OK(IncomingString);
    
    SendStringUart("AT+CIPSERVER=1,4040\r\n");
    WaitESP_OK(IncomingString);
    
    PORTC |= 0x02;
    
    // Startfarbe für Farbverlauf
    r = g = b = 0;
    //r = 255;

    while( 1 )
    {   
	switch(color_mode)
	{
	    case SINGLE_COLOR_MODE:
		break;
		
	    case GARDIENT_MODE:
		if(wait_count0 == 0)
		{
		    switch(step)
		    {
		    case 0:
			g++;
			if(g==255) step = 1;
			break;

		    case 1:
			r--;
			if(r==1) step = 2;
			break;

		    case 2:
			g--;
			b++;
			if(b==255) step = 3;
			break;

		    case 3:
			b--;
			r++;
			if(r==255) step = 0;
			break;
		    }
		    SetRGB(r, g, b);
		}
		wait_count0++;
		break;
	}
	
	if(CheckNewString(IncomingString))
	{
	    char buffer[5];
		    
	    for(char i=0; i<4; i++)
		buffer[i] = IncomingString[i];
	    buffer[4] = 0;
	    
	    if(strcmp(buffer,"+IPD") == 0)
	    {

		unsigned char i=4;
		while((IncomingString[i++] != ':') && i < MAX_UART_STRING-1){};
		
		unsigned char* cmd_string = (unsigned char*)IncomingString + i;

		ExecuteCommand(cmd_string);
	    }
	}
	
    }  
}

/* ------------------------------------------------------------------------- */
void InitPWMChannels()
{
    DDRB = (1 << PB1 )|(1 << PB2)|(1 << PB3);
    TCCR1A = (1<<COM1A1) | (1 << COM1B1)| (1<<WGM10);
    TCCR1B = (1 << CS10);
    TCCR2 |=  (1 << WGM20) | (1 << CS20) | (1 << COM21);
    TCCR2 |= (1 << CS20);

    OCR1A = 0;
    OCR1B = 0;
    OCR2 = 0;
}

/* ------------------------------------------------------------------------- */
void SetRGB(unsigned char r, unsigned char g, unsigned char b)
{
    OCR1A = r;
    OCR1B = g;
    OCR2 = b;
}

/* ------------------------------------------------------------------------- */
void InitUART(void)
{
    UBRRH = UBRRH_VALUE;
    UBRRL = UBRRL_VALUE;
    /* evtl. verkuerzt falls Register aufeinanderfolgen (vgl. Datenblatt)
    UBRR = UBRR_VALUE;
    */
    #if USE_2X
    /* U2X-Modus erforderlich */
    UCSRA |= (1 << U2X);
    #else
    /* U2X-Modus nicht erforderlich */
    UCSRA &= ~(1 << U2X);
    #endif

    UCSRB |= (1<<TXEN | 1<<RXEN | 1<<RXCIE);	// Transmitter Enable Bit setzen
    UCSRC = (1<<URSEL)|(1 << UCSZ1)|(1 << UCSZ0); // Asynchron 8N1
}

/* ------------------------------------------------------------------------- */
void SendStringUart(char* string)
{
    int i=0;
    while(string[i] != 0)
    {
	while (!(UCSRA & (1<<UDRE)))  /* warten bis Senden moeglich                   */
	{}

	UDR = string[i];
	i++;
    }      
}

/* ------------------------------------------------------------------------- */
unsigned char CheckNewString(char* IncomingString)
{
    if(NewUARTString)
    {
	for(int i=0; i<UARTStringLen; i++)
	IncomingString[i] = UARTString[i];

	NewUARTString = 0;
	UARTStringLen = 0;
	
	return 1;
    }
    else return 0;
}

/* ------------------------------------------------------------------------- */
ISR(USART_RXC_vect)
{
    unsigned char byte = UDR;

    if(byte != '\r' && 
    byte != '\n' &&
    UARTStringLen < MAX_UART_STRING-1 &&
    NewUARTString != 1)
    {
	UARTString[UARTStringLen++] = byte;
    }
    else
    {
	if(UARTStringLen > 0)
	{
	    UARTString[UARTStringLen++] = '\0';
	    NewUARTString = 1;
	}
    }
}

/* ------------------------------------------------------------------------- */
void WaitESP_OK(char* IncomingString)
{   
    do
    {
	while(!CheckNewString(IncomingString)){}
    }
    while(strcmp(IncomingString,"OK"));
}

/* ------------------------------------------------------------------------- */
void ExecuteCommand(unsigned char* cmd_string)
{
    // Format: command=value    : value_types string; int
    // Command Maximal 16 Zeichen
    // Value Maximal 16 Zeichen

#define CMD_MAX_LEN 16

    unsigned char* value_string;
    unsigned char i = 0;

    while(i<CMD_MAX_LEN && cmd_string[i] != '=')
    {
        i++;
    }

    if(i == CMD_MAX_LEN)
    {
        // Command zu Groß
        return;
    }

    cmd_string[i] = 0;
    value_string = &(cmd_string[i]) + 1;

    if(strcmp(cmd_string,"red") == 0)
    {
        int v;
        if((value_string[0] >= '0' && value_string[0] <= '9') || value_string[0] == '-')
        {
            // Als Zahl identifiziert
            v = atoi(value_string);
            r = v;
            SetRGB(r,g,b);
        }
        if(strcmp(value_string,"on") == 0)
            r=255;
        if(strcmp(value_string,"off") == 0)
            r=0;
        SetRGB(r,g,b);
    }

    if(strcmp(cmd_string,"green") == 0)
    {
        int v;
        if((value_string[0] >= '0' && value_string[0] <= '9') || value_string[0] == '-')
        {
            // Als Zahl identifiziert
            v = atoi(value_string);
            g = v;
            SetRGB(r,g,b);
        }
        if(strcmp(value_string,"on") == 0)
            g=255;
        if(strcmp(value_string,"off") == 0)
            g=0;
        SetRGB(r,g,b);
    }

    if(strcmp(cmd_string,"blue") == 0)
    {
        int v;
        if((value_string[0] >= '0' && value_string[0] <= '9') || value_string[0] == '-')
        {
            // Als Zahl identifiziert
            v = atoi(value_string);
            b = v;
            SetRGB(r,g,b);
        }
        if(strcmp(value_string,"on") == 0)
            b=255;
        if(strcmp(value_string,"off") == 0)
            b=0;
        SetRGB(r,g,b);
    }
    
    if(strcmp(cmd_string,"colormode") == 0)
    {
        if(strcmp(value_string,"singlecolor") == 0)
            color_mode = SINGLE_COLOR_MODE;
        if(strcmp(value_string,"gardient") == 0)
            color_mode = GARDIENT_MODE;
    }
}
