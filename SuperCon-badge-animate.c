/* 
 * File:   SuperCon-badge-animate.c
 * Author: szczys@hotmail.com
 *
 * User code should be placed in this file. Follow the examples below:
 *   Write to display through AuxBuffer[] and call displayLatch() to show changes on badge
 *   use getTime() for upcounting (approximate) milliseconds for non-blocking delays
 *   call getControl() to return a key mask of debounced button presses
 *   call pollAccel() to populate accelerometer data and read AccX(high/low), AccY(high/low), AccZ(high/low)
 *
 * MIT License (see license.txt)
 * Copyright (c) 2016 Hackaday.com
 */

#include "HaD_Badge.h"
#include <stdio.h>


// UART Code from https://electrosome.com/uart-pic-microcontroller-mplab-xc8/
char UART_Init(const long int baudrate)
{
  unsigned int x;
  x = (_XTAL_FREQ - baudrate*64)/(baudrate*64);   //SPBRG for Low Baud Rate
  if(x>255)                                       //If High Baud Rage Required
  {
    x = (_XTAL_FREQ - baudrate*16)/(baudrate*16); //SPBRG for High Baud Rate
    BRGH = 1;                                     //Setting High Baud Rate
  }
  if(x<256)
  {
    SPBRG = x;                                    //Writing SPBRG Register
    SYNC = 0;                                     //Setting Asynchronous Mode, ie UART
    SPEN = 1;                                     //Enables Serial Port
    TRISC7 = 1;                                   //As Prescribed in Datasheet
    TRISC6 = 1;                                   //As Prescribed in Datasheet
    CREN = 1;                                     //Enables Continuous Reception
    TXEN = 1;                                     //Enables Transmission
    return 1;                                     //Returns 1 to indicate Successful Completion
  }
  return 0;                                       //Returns 0 to indicate UART initialization failed
}
void UART_Write(char data)
{
  while(!TRMT);
  TXREG = data;
}
char UART_TX_Empty()
{
  return TRMT;
}

char UART_Data_Ready()
{
  return RCIF;
}

char UART_Read()
{
  while(!RCIF);
  return RCREG;
}


void drawRightBar(uint8_t val){
    if(val >16){val = 16;}
    uint8_t i = 0;
    for (;i<16-val;i++){
        Buffer[i] &= ~0x01;
    }
    for (;i<16;i++){
        Buffer[i] |= 0x01;
    }
}

void drawMedic(){
    uint8_t i = 4;
    Buffer[i++] |= 0b00110000;
    Buffer[i++] |= 0b00110000;
    Buffer[i++] |= 0b11111100;
    Buffer[i++] |= 0b11111100;
    Buffer[i++] |= 0b00110000;
    Buffer[i++] |= 0b00110000;
}

void drawClear(){
    uint8_t i = 4;
    for(;i<10;i++){
       Buffer[i] &= ~0b11111100;
    }
}

void drawInfected(){
    uint8_t i = 4;
    Buffer[i++] |= 0b10000100;
    Buffer[i++] |= 0b01001000;
    Buffer[i++] |= 0b00110000;
    Buffer[i++] |= 0b00110000;
    Buffer[i++] |= 0b01001000;
    Buffer[i++] |= 0b10000100;
}

void drawScreen(uint8_t medic, uint8_t rightBar, uint8_t infected){
    drawClear();
    if(medic){ drawMedic();}
    if(infected){drawInfected();}
    drawRightBar(rightBar);
    displayLatch();
}

#define MAX_HEALTH (16)
#define MIN_HEALTH (0)
#define MEDIC_SEND_CNT (40)

uint8_t health = MAX_HEALTH;
uint8_t medic = 0;
uint8_t infected = 0;

void increment(uint8_t* val){
    if(*val<MAX_HEALTH){(*val)++;}
    else *val = MAX_HEALTH;
    
}
void decrement(uint8_t* val){
    if(*val>MIN_HEALTH){(*val)--;}
    else *val = MIN_HEALTH;
}
#define DECAY_SEC (15) // seconds to lose 1 health

void HandleInfection(uint8_t remote_infected, uint32_t rnd){
    if(remote_infected){//remote infected
        // Infection probability is 50% unless I'm a medic, it's 25%
        if ((rnd > (255-12) && medic == 0) ||
            (rnd > (255-6) && medic == 1)){
            infected = 1;
        }
    }
}

uint8_t GetRand(){
    asm("call 0x2B0C");
    return WREG;
}

// Main loop for the program
void animateBadge(void) {
    uint32_t nextTime = getTime();
    uint32_t lastHealthTx = getTime();
    int8_t decaySec = DECAY_SEC;
    Brightness = 8;
    uint8_t heartbeat = 0;
    RXFlag = 0; // disable kernel message reception
    pollAccel();
    uint8_t rnd = GetRand();
    uint8_t response = 0;
    uint32_t healthTxd = 0;
    uint32_t healthRxd = 0;
    
    if(0 == UART_Init(4800)){
        displayPixel(0,0,ON);
    }
    
    while(1) {
        
        //This shows how to use non-blocking getTime() function
        if (getTime() > nextTime) {
            nextTime = getTime()+1000;  //prepare next event for about 1000ms (1 second) from now
            displayPixel(0,15,heartbeat);
            displayPixel(1,15,heartbeat);
            displayPixel(0,14,heartbeat);
            displayPixel(1,14,heartbeat);
            if(health>MIN_HEALTH){
                heartbeat = (heartbeat + 1) & 0x01;
            }else{
                heartbeat = 1;
            }
           
            if(--decaySec < 0){
                decrement(&health);
                if(infected){
                    decaySec = DECAY_SEC/2;
                }else{
                    decaySec = DECAY_SEC;
                }
            }
            HandleInfection(0, rnd);// random (lower) chance of infection
            medic = (healthTxd > MEDIC_SEND_CNT) && (healthRxd > MEDIC_SEND_CNT);
            displayPixel(0,0,OFF); // clear the TX indicator
            displayPixel(1,0,OFF); // clear the RX indicator
        }

        rnd = GetRand();

        // Handle packet receive
        if(UART_Data_Ready()){
            uint8_t ch = UART_Read();
            if(ch == 'i' || ch == 'h'){
                displayPixel(1,0,ON);                
                if(getTime() > lastHealthTx+200){ // don't count our own tx's
                    increment(&health);
                    healthRxd++;
                    HandleInfection(ch == 'i', rnd);
                }
            }
        }

        //This shows how to get user input
        switch (getControl()) {
            case (ESCAPE):
                displayClose();
                return;
            case (LEFT):
                increment((uint8_t*)&Brightness);
                break;
            case (RIGHT):
                decrement((uint8_t*)&Brightness);
                break;
            case (UP):
                break;
            case (DOWN):
                if(health > 0){
                    if(infected){
                        UART_Write('i');
                    }else{
                        UART_Write('h');
                    }
                    lastHealthTx = getTime();
                    healthTxd++;
                    displayPixel(0,0,ON); // Set the TX indicator
                }
                break;
        }
       drawScreen(medic,health,infected);

    }
}