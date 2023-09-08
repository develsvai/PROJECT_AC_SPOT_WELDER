#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#define lcd_address 0x27
volatile unsigned char  lcd_con=0;


void lcd_cont(unsigned char reg);
void lcd_Data(unsigned char reh);
void dis_cmd(char cmd_value);
void dis_data(char data_value);
void twi_master_transmit(unsigned char addr, unsigned char data);
unsigned char twi_master_Recive(unsigned char addr);


void twi_master_transmit(unsigned char addr, unsigned char data){
	TWCR = 0xa4;
	while(!(TWCR & 0x80) || ((TWSR &0xf8)!=0x08));
	TWDR = addr <<1;
	TWCR = 0x84;
	while(!(TWCR & 0x80) ||((TWSR & 0xf8) != 0x18));
	TWDR = data;
	TWCR = 0x84;
	while(!(TWCR & 0x80) || ((TWSR & 0xf8)!=0x28));
	TWCR = 0x94;
}

unsigned char twi_master_Recive(unsigned char addr){
	unsigned char data;
	TWCR = 0xa4;
	while(!(TWCR & 0x80) || ((TWSR &0xf8)!=0x08));
	TWDR = (addr <<1) | 1;
	TWCR = 0x84;
	while(!(TWCR & 0x80) ||((TWSR & 0xf8) != 0x40));
	TWCR = 0x84;
	while(!(TWCR & 0x80) || ((TWSR & 0xf8)!=0x58));
	data = TWDR;
	TWCR = 0x94;
	
	return data;
}

void dis_cmd(char cmd_value)
{
	char cmd_value1;
	cmd_value1 = cmd_value & 0xF0;        //mask lower nibble
	//because PA4-PA7 pins are used.
	lcd_cont(cmd_value1);               // send to LCD
	cmd_value1 =((cmd_value<<4) & 0xF0);   //shift 4-bit and
	//mask
	lcd_cont(cmd_value1);               // send to LCD
}
void dis_data(char data_value)
{
	char data_value1;
	data_value1=data_value & 0xF0;
	lcd_Data(data_value1);
	data_value1=((data_value<<4) & 0xF0);
	lcd_Data(data_value1);
}

void lcd_cont(unsigned char reg)
{
	_delay_us(100);
	twi_master_transmit(lcd_address, reg);
	twi_master_transmit(lcd_address, reg | 0x08);
	twi_master_transmit(lcd_address, reg | 0x0c);
	_delay_us(1);
	twi_master_transmit(lcd_address, reg | 0x08);
}

void lcd_Data(unsigned char reh)
{
	_delay_us(100);
	twi_master_transmit(lcd_address , reh);
	twi_master_transmit(lcd_address, reh | 0x09);
	twi_master_transmit(lcd_address, reh | 0x0d);
	_delay_us(1);
	twi_master_transmit(lcd_address,  reh | 0x09);
}

void LCD_initialize(void)
{

	TWBR = 10;
	TWSR = 0x00;
	_delay_ms(40);
	dis_cmd(0x28);		// DL=0(4bit) N=1(2Line) F=0(5x7)
	_delay_us(50);
	dis_cmd(0x0c);		// LCD ON, Cursor X, Blink X
	_delay_us(50);
	dis_cmd(0x01);
	_delay_ms(2);
	dis_cmd(0x06);		// Entry Mode
	_delay_us(50);

}


void LCD_string(unsigned char address, unsigned char *Str)
{
	int i=0;
	dis_cmd(address);
	while(*Str != '\0')
	{
		if(address+i == 0x90)dis_cmd(0xc0);
		dis_data(*Str++);
		i++;
	}
}
void LCD_strings(unsigned char address, char *S){

	dis_cmd(address); // LCD display start position
	dis_data(*S);
	
}


void lcd_on_off(int D, int C, int B)
{
	unsigned loo;
	loo = 0b00001000;
	if(D==1)  loo |= 0b00000100;
	if(C==1)  loo |= 0b00000010;
	if(B==1)  loo |= 0b00000001;
	dis_cmd(loo);
}

void lcd_position(unsigned char row, unsigned char col){
	unsigned char cmd;
	
	cmd = 0x08 | ((row-1)*0x40+(col-1));
	dis_cmd(cmd);
}