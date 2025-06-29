/*
 * GccApplication1.c
 *
 * Created: 12/13/2018 5:37:51 AM
 *  Author: rhkgk
 */ 

#include "avr/io.h"
#include "util/delay.h"
#include <stdio.h>
#include <stdlib.h>
#include <avr/eeprom.h>
#include <avr/interrupt.h>
#include "12c-lcd.h"
#include <float.h>
#define S1 0x10&PINB
#define S2 0x20&PINB
#define S3 0x40&PINB
#define S4 0x80&PINB

#define en_flag_cw PINB&0x10
#define en_flag_ccw PINB&0x20
#define en_flag_sw PINB&0x40
#define  key_Data  (0xf0 & PINB)

int static_hz = 60;
int spotac_tacle=0;
int stan_flag=0;
int en_flag=1;
int en_flag_n = 0;
int en_sw_counter=1;
int en_sw_value;
int fir_flag=0;
int fi_save_value=0;
int autospot_sw=0,isr_cn=0,spot_onoff_counter=0;
int val,bal,nal,vbl,bbl,nbl;
int inte_2_value=0;
int break_time=0,flag_lock=0,lock_value=0;
float spot_time_hz=0, one_cycle=0 , halfcycle=0;
float sum,sensor_Value = 0,bun_value=0;
float sum, average =0, sum_rms=0;
float voltage_rms=0, voltagePP = 0.0;

char msg[20];
unsigned char key_value;

char firstmsg_1[100] = {"ac spot welder"}; 
	
void isr_init(void);
void spot_action(void);
void delay_ms(int ms);
void spottime_value(void);
void spottimeset(void);
void spotmult(void);
void spotrest (void);

ISR(INT0_vect)
{
	if(isr_cn == 1 & vbl>=8){
		_delay_ms(0.5);
		 PORTC = 0x20;
		 delay_ms(spot_time_hz * halfcycle);
		 PORTC = 0x00;
		 delay_ms(nbl);
		 isr_cn=0;
	} 
	if(isr_cn == 1 & vbl<8){
		_delay_ms(0.5);
         PORTC = 0x20;
	     delay_ms(vbl);
	     PORTC = 0x00;
	     delay_ms(nbl);
	     isr_cn=0;
	}
}
void spottimeset(void){
	_delay_ms(300);
	//LCD_initialize();
	val = eeprom_read_byte(0);
	if(val>=100)sprintf(msg,"TIME=%2d",val);
	else sprintf(msg,"TIME=%2d ",val);
	//lcd_on_off(1,0,0);
	LCD_string(0x80,msg);
	DDRB = 0x0f;
	en_flag=1;
	en_flag_n = 0;
	while(1){
		_delay_us(10);
		if(en_flag_cw && en_flag ==1 ){
			if(en_flag_ccw) en_flag_n =1;
			else en_flag_n =2;
			_delay_us(10);
			if(~en_flag_ccw && en_flag_n == 2){
				en_flag=0;
				DDRB = 0XFF;
				if(val<151){
					if(val>0){
						if(val<20)val++;
						else val+=5;
					}
					else val=1;
				}
				else val = 150;
				_delay_ms(10);
				if(val>=100)sprintf(msg,"TIME=%2d",val);  
				else sprintf(msg,"TIME=%2d ",val);  
				LCD_string(0x80,msg);
				_delay_ms(180);
				DDRB = 0xef;
			}
			if(en_flag_ccw && en_flag_n == 1){
			  en_flag=0;
			  DDRB = 0XFF;
			  if(val<151){
				  if(val>0){
					  if(val<=20)val--;
					  else val-=5;
				  }
				  else val =1;
			  }
			  else val =150;
			  _delay_ms(10);
			  if(val>=100)sprintf(msg,"TIME=%2d",val);
			  else sprintf(msg,"TIME=%2d ",val);
			  LCD_string(0x80,msg);
			  _delay_ms(180);
			  DDRB = 0xef;
		  }
	    }
		if(~en_flag_cw){
			en_flag=1;
			DDRB = 0x0f;
		}
		if(en_flag_sw){
			eeprom_write_byte(0,val);
			vbl = eeprom_read_byte(0);
			break;
		}
	  }
	}
void spotmult(void){
	_delay_ms(300);
	//LCD_initialize();
	bal = eeprom_read_byte(1);
	sprintf(msg,"MULT=%2d ",bal);
	//lcd_on_off(1,0,0);
	LCD_string(0x88,msg);
    DDRB = 0x0f;
	int en_flag=1;
	int en_flag_n = 0;
	while(1){ 
		_delay_us(10);
		  if(en_flag_cw && en_flag ==1 ){
			  if(en_flag_ccw) en_flag_n =1;
			  else en_flag_n =2;
			  _delay_us(10);
			if(~en_flag_ccw && en_flag_n == 2){
				en_flag=0;
				DDRB = 0XFF;
				if(bal<21){
					if(bal>0){
						bal++;
					}
					else bal=1;
				}
				else bal = 20;
				_delay_ms(10);
				//dis_cmd(0x01);
				sprintf(msg,"MULT=%2d ",bal);
				//lcd_on_off(1,0,0);
				LCD_string(0x88,msg);
				_delay_ms(180);
				DDRB = 0xef;
			 }
			if(en_flag_ccw && en_flag_n == 1){
			 en_flag=0;
			 DDRB = 0XFF;
			 if(bal<21){
				 if(bal>0){
					 bal--;
				 }
				 else bal =1;
			 }
			 else bal = 20;
			 _delay_ms(10);
			// dis_cmd(0x01);
			 sprintf(msg,"MULT=%2d ",bal);
			// lcd_on_off(1,0,0);
			 LCD_string(0x88,msg);
			 _delay_ms(180);
			 DDRB = 0xef;
			}
		}
		if(~en_flag_cw){
			en_flag=1;
			DDRB = 0x0f;
		}
		if(en_flag_sw){
			eeprom_write_byte(1,bal);
			bbl = eeprom_read_byte(1);
			break;
		}
	  }
	}
void spotrest (void){
	_delay_ms(300);
	//LCD_initialize();
	nal = eeprom_read_byte(2);
	sprintf(msg,"REST=%2d ",nal);
	//lcd_on_off(1,0,0);
	LCD_string(0xc0,msg);
    DDRB = 0x0f;
	int en_flag=1;
	int en_flag_n = 0;
	while(1){
		_delay_us(10);
		if(en_flag_cw && en_flag ==1 ){
			if(en_flag_ccw) en_flag_n =1;
			else en_flag_n =2;
			_delay_us(10);
			if(~en_flag_ccw && en_flag_n == 2){
				en_flag=0;
				DDRB = 0XFF;
				if(nal<201){
					if(nal>4){
				        nal+=5;
					}
					else nal=5;
				}
				else nal = 200;
				_delay_ms(10);
				//dis_cmd(0x01);
				sprintf(msg,"REST=%2d ",nal);
				lcd_on_off(1,0,0);
				LCD_string(0xc0,msg);
				_delay_ms(180);
				DDRB = 0xef;
			}
			if(en_flag_ccw && en_flag_n == 1){
				en_flag=0;
				DDRB = 0XFF;
			 if(nal<201){
				 if(nal>4){
					  nal-=5;
				 }
				 else nal =5;
			 }
			 else nal =200;
			 _delay_ms(10);
			 //dis_cmd(0x01);
			 sprintf(msg,"REST=%2d ",nal);
			 lcd_on_off(1,0,0);
			 LCD_string(0xc0,msg);
			 _delay_ms(180);
			 DDRB = 0xef;
			}
		}
		if(~en_flag_cw){
			en_flag=1;
			DDRB = 0x0f;
		}
	    if(en_flag_sw){
		   eeprom_write_byte(2,nal);
		   nbl = eeprom_read_byte(2);
		   break;
	   }
	}
}
float adc(void){
	int ADval;
	float Vin;
	char val[20];
	DDRA = 0x00;
	DDRC = 0xff;
	PORTC = 0x40;
	ADMUX = 0x40;
	ADCSRA = 0x87;
	while(1){
		ADCSRA |= 0x40;
		while((ADCSRA & 0x10)==0);
		ADval = (int)ADCL + ((int)ADCH<<8);
		Vin = (float)ADval;
		return Vin;
	}
} 
void auto_spot(void){
if(autospot_sw==1){
LCD_string(0xc9, "ON  ");
spot_onoff_counter =2;
}
else{ 
LCD_string(0xc9,"OFF  ");
spot_onoff_counter =1;
}
_delay_ms(300); 
break_time=0;
PORTC = 0x40;
DDRB = 0x0f;
  while(1){
	  key_value = spot_onoff_counter | key_Data;
	  switch(key_value){
		 case 17|33:spot_onoff_counter =2;
					LCD_string(0xc9, "ON  ");
					_delay_ms(150);
		            break;
		  
		 case 18|34:spot_onoff_counter =1;
		            LCD_string(0xc9, "OFF ");
					PORTC = 0x00;
					autospot_sw =0;
					_delay_ms(150);
					break;
					
					
		 case 66 : autospot_sw=1;
				   break_time=1;
				   PORTC = 0x40;
				   eeprom_write_byte(3,autospot_sw);
				   break;
				   
		 case 65 : autospot_sw =0;
		           eeprom_write_byte(3,autospot_sw);
				   break_time=1;
				   PORTC = 0x00;
		           break;		
				   
	  }
	  if(break_time ==1){
		  en_sw_counter=1;
		  break_time=0;
		  _delay_ms(150);
		  break;
	  }
  }
}
void first_page(void){
 for(int i=0; i<5; i++){
	 _delay_ms(10);
	LCD_initialize();
	lcd_on_off(1,0,0);
	LCD_string(0x81,firstmsg_1);
	LCD_string(0xc0,"");
  }
    _delay_ms(3000);
}
void spottime_value(void){
_delay_ms(300);
LCD_initialize();
lcd_on_off(1,0,0);
LCD_string(0xce,"LK");
isr_init();
en_sw_counter=1;
en_sw_value;
en_flag=1;
en_flag_n = 0;
while(1){
	vbl = eeprom_read_byte(0);
	sprintf(msg,"time=%2d",vbl);
	LCD_string(0x80,msg);
	bbl = eeprom_read_byte(1);
	sprintf(msg,"mult=%2d",bbl);
	LCD_string(0x88,msg);
	nbl = eeprom_read_byte(2);
	sprintf(msg,"rest=%2d",nbl);
	LCD_string(0xc0,msg);
	autospot_sw = eeprom_read_byte(3);
	if(autospot_sw == 1){
		PORTC = 0x40;
		LCD_string(0xc9,"auto");
	}
	if(autospot_sw == 0){
		PORTC =0x00;
		LCD_string(0xc9,"sw  ");
	}

en_sw_value = en_sw_counter | en_flag_sw;			   
	switch(en_sw_value){
		case 65 : _delay_us(80);
		          LCD_string(0x80,"TIME");
		          en_sw_counter =2;
		          spottimeset();
				  break;
				  
		case 66 : _delay_us(80);
		          LCD_string(0x88,"MULT");
		          en_sw_counter =3;
		          spotmult();
				  break;
				  
		case 67 : _delay_us(80);
		          LCD_string(0xc0,"REST");
		          en_sw_counter = 4;
		          spotrest();
				  break;
				  
		case 68 :en_sw_counter =5;
		         auto_spot();
		         break;
				 
		case 69 :en_sw_counter =1;
		         _delay_ms(400);
		         break;
				 
		  }
		  
		  	if(en_flag_cw && en_flag ==1 ){
			  	if(en_flag_ccw) en_flag_n =1;
			  	else en_flag_n =2;
			  	_delay_us(10);
			  	if(~en_flag_ccw && en_flag_n == 2){
				   en_flag=0;
			  	}
			  	if(en_flag_ccw && en_flag_n == 1){
					if(autospot_sw == 1) lock_value = 250;
					else lock_value = 32000;
				  	en_flag=0;
				  	DDRB = 0XFF;
					spotac_tacle=1;
					flag_lock=0; 
					break;
			  	}
		  	}
		  	if(~en_flag_cw){
			  	en_flag=1;
			  	DDRB = 0x0f;
		  	}
			  
			if(S4){
				if(autospot_sw == 1) lock_value = 250;
				else lock_value = 32000;
				DDRB = 0XFF;
				spotac_tacle=1;
				flag_lock=0;
				_delay_ms(100);
				break;
			}
       }
 }
void delay_ms(int ms)
{
	while (ms-- != 0)
	_delay_ms(1);
}
int auto_read(void){
float maxData = adc();
float minData = maxData;
int current_data=0;
while(1){
	DDRB = 0xBF;
	for(int n=0; n<1000; n++){
		sensor_Value =adc();
		sum +=sensor_Value;
	}
	average = (float)sum/1000;
	sum =0;
	for(int j=0; j<1000; j++){
		current_data =adc();
		if (current_data > maxData) maxData = current_data;
		if (current_data < minData) minData = current_data;
		sum_rms +=(current_data - average);
	}
	voltage_rms = (sum_rms/1000)/150;
	sum_rms=0;
	voltagePP = ((maxData - minData)/60);
	maxData =0;
	minData=0;
	bun_value = voltagePP - voltage_rms;
	inte_2_value = ((int)bun_value);
	if(fir_flag==0 && inte_2_value>0){
		fi_save_value = inte_2_value+30;
		fir_flag=1;
	}
	break;
  }
}
void spot_action(void) {
LCD_initialize();
lcd_on_off(1,0,0);
isr_init();
	vbl = eeprom_read_byte(0);
	sprintf(msg,"time=%2d",vbl);
	LCD_string(0x80,msg);
	bbl = eeprom_read_byte(1);
	sprintf(msg,"mult=%2d",bbl);
	LCD_string(0x88,msg);
	nbl = eeprom_read_byte(2);
	sprintf(msg,"rest=%2d",nbl);
	LCD_string(0xc0,msg);
	autospot_sw = eeprom_read_byte(3);
		if(autospot_sw == 1){
			PORTC = 0x40;
			LCD_string(0xc9,"auto");
		}
		if(autospot_sw == 0){
			PORTC =0x00;
			LCD_string(0xc9,"sw  ");
		}
spotac_tacle =1;
flag_lock =0;
if(autospot_sw == 1) lock_value = 250;
else lock_value = 32000;
while(1){
  DDRB = 0xBF;
  while(autospot_sw==1){
	flag_lock++;
	LCD_string(0xce," U");
	auto_read();
	if(vbl<8){
	  if(autospot_sw ==1 && spotac_tacle ==1 && inte_2_value>2 && inte_2_value < fi_save_value){
		  flag_lock=0;
		  auto_read();
		  if(autospot_sw ==1 && spotac_tacle ==1 &&  inte_2_value>2 && inte_2_value < fi_save_value) {
			  LCD_string(0xce," L");
			  for(int i=0; i<2; i++){
				  PORTC = 0x04;
				  _delay_ms(5);
				  PORTC = 0x00;
				  _delay_ms(5);
			  }
			  for(int j=0; j<bbl; j++){
				  sei();
				  isr_cn =1;
				  _delay_ms(100);
			  }
			  cli();
			  _delay_ms(220);
		  }
	  }  
	}
	else{ 
		one_cycle = 1000/static_hz;
		halfcycle = one_cycle/2;
		spot_time_hz = (eeprom_read_byte(0) / halfcycle);
	  	 if(autospot_sw ==1 && spotac_tacle ==1 &&  inte_2_value>2 && inte_2_value < fi_save_value ) {
		  	 flag_lock=0;
		  	 auto_read();
		  	 if(autospot_sw ==1 && spotac_tacle ==1 && inte_2_value>2 && inte_2_value < fi_save_value) {
			  	 LCD_string(0xce," L");
			  	 for(int i=0; i<2; i++){
				  	 PORTC = 0x04;
				  	 _delay_ms(5);
				  	 PORTC = 0x00;
				  	 _delay_ms(5);
			  	 }
			  	 for(int j=0; j<bbl; j++){
				  	 sei();
				  	 isr_cn =1;
				  	 _delay_ms(100);
			  	 }
			  	 cli();
			  	 _delay_ms(220);
		  	  }
	  	   }
	    }
		if(en_flag_sw){
		 flag_lock = lock_value +100;
		}
		if(flag_lock>lock_value){
			 spotac_tacle =0;
			 DDRB = 0X0f;
			 PORTC = 0x00;
			 spottime_value();
			 break;
		}
  }
  while(autospot_sw ==0){
	DDRB = 0x3f;
	flag_lock++;
	LCD_string(0xce," U");
	if(vbl<8){
	 if(spotac_tacle == 1 && autospot_sw == 0 && S4){
		 _delay_ms(50);
		 flag_lock=0;
		 DDRB = 0XFF;
		 LCD_string(0xce," L");
		 for(int i=0; i<2; i++){
			 PORTC = 0x04;
			 _delay_ms(10);
			 PORTC = 0x00;
			 _delay_ms(10);
		 }
		 for(int j=0; j<bbl; j++){
			 sei();
			 isr_cn =1;
			 _delay_ms(100);
		 }
		 cli();
		 _delay_ms(50);
	 }
	}
	else{
	  one_cycle = 1000/static_hz;
	  halfcycle = one_cycle/2;
	  spot_time_hz = (eeprom_read_byte(0) / halfcycle);
	  if(spotac_tacle == 1 && autospot_sw == 0 && S4){
		  _delay_ms(50);
		  flag_lock=0;
		  DDRB = 0XFF;
		  LCD_string(0xce," L");
		  for(int i=0; i<2; i++){
			  PORTC = 0x04 ; 
			  _delay_ms(10);
			  PORTC = 0x00;
			  _delay_ms(10);
		  }
		  for(int j=0; j<bbl; j++){
			  sei();
			  isr_cn =1;
			  _delay_ms(100);
		  }
		  cli();
		  _delay_ms(50);
	    }
	  }
	  if(en_flag_sw){
		  flag_lock = lock_value +100;
	  }
	  if(flag_lock>lock_value){
		  spotac_tacle =0;
		  DDRB = 0X0f;
		  PORTC = 0x00;
		  spottime_value();
		  break;
	  }
    }
	if(en_flag_sw){
		spottime_value();
		break;
	}
  }
 }
void main_Set(void){
	DDRC = 0xff;
	PORTC = 0x00;
	cli();
	DDRD = 0xf0;
	DDRA = 0x00;
	fir_flag =0;
	fi_save_value=0;
	first_page();
	spot_action();
}
void isr_init(void){
	cli();
	GICR  = 0x40;
	MCUCR |= 1<<ISC01 | 1<<ISC00;
	sei();
} 
int main(void){
	main_Set();
	while(1);
}
    