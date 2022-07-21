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
#include "usart.h"

//передающий буфер
unsigned char usartTxBuf[SIZE_BUF];
unsigned char txBufTail = 0;
unsigned char txBufHead = 0;
unsigned char txCount = 0;

//приемный буфер
unsigned char usartRxBuf[SIZE_BUF];
unsigned char rxBufTail = 0;
unsigned char rxBufHead = 0;
unsigned char rxCount = 0;

//инициализация usart`a
void USART_Init(void)
{
  UBRRH = 0;
  UBRRL = 51; //скорость обмена 9600 бод
  UCSRB = (1<<RXCIE)|(1<<TXCIE)|(1<<RXEN)|(1<<TXEN); //разр. прерыв при приеме и передачи, разр приема, разр передачи.
  UCSRC = (1<<URSEL)|(1<<UCSZ1)|(1<<UCSZ0); //размер слова 8 разрядов
}

//______________________________________________________________________________
//возвращает колличество символов передающего буфера
unsigned char USART_GetTxCount(void)
{
  return txCount;  
}

//"очищает" передающий буфер
void USART_FlushTxBuf(void)
{
  txBufTail = 0;
  txCount = 0;
  txBufHead = 0;
}

//помещает символ в буфер, инициирует начало передачи
void USART_PutChar(unsigned char sym)
{
  //если модуль usart свободен и это первый символ
  //пишем его прямо в регистр UDR
  if(((UCSRA & (1<<UDRE)) != 0) && (txCount == 0)) UDR = sym;
  else {
    if (txCount < SIZE_BUF){    //если в буфере еще есть место
      usartTxBuf[txBufTail] = sym; //помещаем в него символ
      txCount++;                   //инкрементируем счетчик символов
      txBufTail++;                 //и индекс хвоста буфера
      if (txBufTail == SIZE_BUF) txBufTail = 0;
    }
  }
}

//функция посылающая строку по usart`у
void USART_SendStr(unsigned char * data)
{
  unsigned char sym;
  while(*data){
    sym = *data++;
    USART_PutChar(sym);
  }
}

//обработчик прерывания по завершению передачи 
#pragma vector=USART_TXC_vect
__interrupt void usart_txc_my(void) 
{
  if (txCount > 0){              //если буфер не пустой
    UDR = usartTxBuf[txBufHead]; //записываем в UDR символ из буфера
    txCount--;                   //уменьшаем счетчик символов
    txBufHead++;                 //инкрементируем индекс головы буфера
    if (txBufHead == SIZE_BUF) txBufHead = 0;
  } 
} 

//______________________________________________________________________________
//возвращает колличество символов находящихся в приемном буфере
unsigned char USART_GetRxCount(void)
{
  return rxCount;  
}

//"очищает" приемный буфер
__monitor void USART_FlushRxBuf(void)
{
  rxBufTail = 0;
  rxBufHead = 0;
  rxCount = 0;
}

//чтение буфера
unsigned char USART_GetChar(void)
{
  unsigned char sym;
  if (rxCount > 0){                     //если приемный буфер не пустой  
    sym = usartRxBuf[rxBufHead];        //прочитать из него символ    
    rxCount--;                          //уменьшить счетчик символов
    rxBufHead++;                        //инкрементировать индекс головы буфера  
    if (rxBufHead == SIZE_BUF) rxBufHead = 0;
    return sym;                         //вернуть прочитанный символ
  }
  return 0;
}


//прерывание по завершению приема
#pragma vector=USART_RXC_vect
__interrupt void usart_rxc_my(void) 
{
  if (rxCount < SIZE_BUF){                //если в буфере еще есть место                     
      usartRxBuf[rxBufTail] = UDR;        //считать символ из UDR в буфер
      rxBufTail++;                             //увеличить индекс хвоста приемного буфера 
      if (rxBufTail == SIZE_BUF) rxBufTail = 0;  
      rxCount++;                                 //увеличить счетчик принятых символов
    }
} 
