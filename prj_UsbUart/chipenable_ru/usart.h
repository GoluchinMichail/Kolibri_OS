//***************************************************************************
//
//  Author(s)...: Pashgan    http://ChipEnable.Ru   
//
//  Target(s)...: ATMega8535
//
//  Compiler....: IAR EWA 5.11A
//
//  Description.: USART/UART. Используем кольцевой буфер
//
//  Data........: 3.01.10 
//
//***************************************************************************
#ifndef USART_H
#define USART_H

#include <ioavr.h>

//размер буфера
#define SIZE_BUF 16

void USART_Init(void); //инициализация usart`a
unsigned char USART_GetTxCount(void); //взять число символов передающего буфера
void USART_FlushTxBuf(void); //очистить передающий буфер
void USART_PutChar(unsigned char sym); //положить символ в буфер
void USART_SendStr(unsigned char * data); //послать строку по usart`у
unsigned char USART_GetRxCount(void); //взять число символов в приемном буфере
void USART_FlushRxBuf(void); //очистить приемный буфер
unsigned char USART_GetChar(void); //прочитать приемный буфер usart`a 

#endif //USART_H
