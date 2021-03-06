/**
 * @file    uart.c
 * @brief   
 *
 * DAPLink Interface Firmware
 * Copyright (c) 2009-2016, ARM Limited, All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may
 * not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "string.h"

#include "sam3u.h"
#include "uart.h"

#define _CPU_CLK_HZ   SystemCoreClock
#define _CDC_BUFFER_SIZE (512)

#define MIN(a, b)     (((a) < (b)) ? (a) : (b))
#define MAX(a, b)     (((a) > (b)) ? (a) : (b))

#define I8   int8_t
#define I16  int16_t
#define I32  int32_t
#define U8   uint8_t
#define U16  uint16_t
#define U32  uint32_t

#define BIT_CDC_USB2UART_CTS  (9)
#define BIT_CDC_USB2UART_RTS  (10)

#define UART_PID              (8)
#define UART_RX_PIN           (11)
#define UART_TX_PIN           (12)
#define UART_RXRDY_FLAG       (1uL << 0)              // Rx status flag
#define UART_TXRDY_FLAG       (1uL << 1)              // Tx RDY Status flag
#define UART_TXEMPTY_FLAG     (1uL << 9)              // Tx EMPTY Status flag
#define UART_ENDTX_FLAG       (1uL << 4)              // Tx end flag
#define UART_RX_ERR_FLAGS     (0xE0)                  // Parity, framing, overrun error
#define UART_TX_INT_FLAG      UART_TXEMPTY_FLAG
#define PIO_UART_PIN_MASK     ((1uL << UART_RX_PIN) | (1uL << UART_TX_PIN))

#define PMC_BASE_ADDR         (0x400E0400)
#define PMC_PCER              *(volatile U32*)(PMC_BASE_ADDR + 0x10)    // Peripheral clock enable register

#define UART_BASE_ADDR        (0x400E0600)
#define OFF_UART_CR           (0x00)
#define OFF_UART_MR           (0x04)
#define OFF_UART_IER          (0x08)
#define OFF_UART_IDR          (0x0C)
#define OFF_UART_IMR          (0x10)
#define OFF_UART_SR           (0x14)
#define OFF_UART_RHR          (0x18)
#define OFF_UART_THR          (0x1C)
#define OFF_UART_BRGR         (0x20)
#define UART_CR               *(volatile U32*)(UART_BASE_ADDR + OFF_UART_CR)
#define UART_MR               *(volatile U32*)(UART_BASE_ADDR + OFF_UART_MR)
#define UART_IER              *(volatile U32*)(UART_BASE_ADDR + OFF_UART_IER)
#define UART_IDR              *(volatile U32*)(UART_BASE_ADDR + OFF_UART_IDR)
#define UART_IMR              *(volatile U32*)(UART_BASE_ADDR + OFF_UART_IMR)
#define UART_SR               *(volatile U32*)(UART_BASE_ADDR + OFF_UART_SR)
#define UART_RHR              *(volatile U32*)(UART_BASE_ADDR + OFF_UART_RHR)
#define UART_THR              *(volatile U32*)(UART_BASE_ADDR + OFF_UART_THR)
#define UART_BRGR             *(volatile U32*)(UART_BASE_ADDR + OFF_UART_BRGR)
#define OFF_PDC_RPR           (0x100)
#define OFF_PDC_RCR           (0x104)
#define OFF_PDC_TPR           (0x108)
#define OFF_PDC_TCR           (0x10C)
#define OFF_PDC_RNPR          (0x110)
#define OFF_PDC_RNCR          (0x114)
#define OFF_PDC_TNPR          (0x118)
#define OFF_PDC_TNCR          (0x11C)
#define OFF_PDC_PTCR          (0x120)
#define OFF_PDC_PTSR          (0x124)
#define UART_PDC_RPR          *(volatile U32*)(UART_BASE_ADDR + OFF_PDC_RPR)
#define UART_PDC_RCR          *(volatile U32*)(UART_BASE_ADDR + OFF_PDC_RCR)
#define UART_PDC_TPR          *(volatile U32*)(UART_BASE_ADDR + OFF_PDC_TPR)
#define UART_PDC_TCR          *(volatile U32*)(UART_BASE_ADDR + OFF_PDC_TCR)
#define UART_PDC_RNPR         *(volatile U32*)(UART_BASE_ADDR + OFF_PDC_RNPR)
#define UART_PDC_RNCR         *(volatile U32*)(UART_BASE_ADDR + OFF_PDC_RNCR)
#define UART_PDC_TNPR         *(volatile U32*)(UART_BASE_ADDR + OFF_PDC_TNPR)
#define UART_PDC_TNCR         *(volatile U32*)(UART_BASE_ADDR + OFF_PDC_TNCR)
#define UART_PDC_PTCR         *(volatile U32*)(UART_BASE_ADDR + OFF_PDC_PTCR)
#define UART_PDC_PTSR         *(volatile U32*)(UART_BASE_ADDR + OFF_PDC_PTSR)

#define PIOA_BASE_ADDR        (0x400E0C00)
#define PIOA_PDR             (*(volatile U32*) (PIOA_BASE_ADDR + 0x04)) // PIO Disable Register
#define PIOA_IFER            (*(volatile U32*) (PIOA_BASE_ADDR + 0x20)) // Input Filter Enable Register
#define PIOA_SODR            (*(volatile U32*) (PIOA_BASE_ADDR + 0x30)) // Set output data
#define PIOA_CODR            (*(volatile U32*) (PIOA_BASE_ADDR + 0x34)) // Clear output data register
#define PIOA_PDSR            (*(volatile U32*) (PIOA_BASE_ADDR + 0x3c)) // pin data status register
#define PIOA_IER             (*(volatile U32*) (PIOA_BASE_ADDR + 0x40)) // Interrupt Enable Register
#define PIOA_ISR             (*(volatile U32*) (PIOA_BASE_ADDR + 0x4c)) // Interrupt Status Register
#define PIOA_ABSR            (*(volatile U32*) (PIOA_BASE_ADDR + 0x70)) // Peripheral AB Select Register
#define PIOA_SCIFSR          (*(volatile U32*) (PIOA_BASE_ADDR + 0x80)) // System Clock Glitch Input Filtering Select Register
#define PIOA_AIMER           (*(volatile U32*) (PIOA_BASE_ADDR + 0xB0)) // Additional Interrupt Modes Enable Register
#define PIOA_ESR             (*(volatile U32*) (PIOA_BASE_ADDR + 0xC0)) // Edge Select Register
#define PIOA_FELLSR          (*(volatile U32*) (PIOA_BASE_ADDR + 0xD0)) // Falling Edge/Low Level Select Register
#define PIOA_REHLSR          (*(volatile U32*) (PIOA_BASE_ADDR + 0xD4)) // Rising Edge/High Level Select Register

typedef struct {
    U8 acBuffer[_CDC_BUFFER_SIZE + 1];   // Buffer is max. filled to BufferSize - 1
    U8 *pRead;
    U8 *pWrite;
} CIRCBUFFER;

static CIRCBUFFER _WriteBuffer;
static CIRCBUFFER _ReadBuffer;
static U32        _Baudrate;
static U8         _FlowControl;
static U8         _UARTChar0;   // Use static here since PDC starts transferring the byte when we already left this function
static U32        _TxInProgress;

static U32 _DetermineDivider(U32 Baudrate)
{
    U32 Div;
    //
    // Calculate divider for baudrate and round it correctly.
    // This is necessary to get a tolerance as small as possible.
    //
    Div = Baudrate << 4;
    Div = ((_CPU_CLK_HZ << 1) / Div) ;//+ 1;
    Div = Div >> 1;
    return Div;
}

static int _SetBaudrate(U32 Baudrate)
{
    U32 Div;
    Div = _DetermineDivider(Baudrate);

    if (Div >= 1) {
        UART_BRGR = Div;
        _Baudrate = _CPU_CLK_HZ / Div / 16;
        return _Baudrate;
    }

    return -1;
}

static void _Send1(void)
{
    U8 *pWrapAround;
    U8 *pRead;
    //
    // Use PDC for transferring the byte to the UART since direct write to UART_THR does not seem to work properly.
    //
    PIOA->PIO_MDDR = (1 << UART_TX_PIN); //Disable open-drain on TX pin
    pRead = _WriteBuffer.pRead;
    _UARTChar0 = *pRead++;
    pWrapAround = (U8 *)(_WriteBuffer.acBuffer + sizeof(_WriteBuffer.acBuffer));

    if (pRead == pWrapAround) {
        pRead = _WriteBuffer.acBuffer;
    }

    _WriteBuffer.pRead = pRead;
    _TxInProgress = 1;
    UART_PDC_TPR  = (U32)&_UARTChar0;
    UART_PDC_TCR  = 1;
    UART_PDC_PTCR = (1 << 8);               // Enable transmission
    UART_IER = UART_TX_INT_FLAG;            // enable Tx interrupt
}

static void _ResetBuffers(void)
{
    _WriteBuffer.pRead  = _WriteBuffer.acBuffer;
    _WriteBuffer.pWrite = _WriteBuffer.acBuffer;
    _ReadBuffer.pRead   = _ReadBuffer.acBuffer;
    _ReadBuffer.pWrite  = _ReadBuffer.acBuffer;
    _TxInProgress       = 0;
}


static int32_t _NumBytesWriteFree(CIRCBUFFER *pBuffer)
{
    //
    // Return free space in buffer
    //
    int32_t v;
    v = (int32_t)(pBuffer->pRead - pBuffer->pWrite - 1);

    if (v < 0) {
        v += sizeof(pBuffer->acBuffer);
    }

    return v;
}

void UART_IntrEna(void)
{
    NVIC_EnableIRQ(UART_IRQn);            // Enable USB interrupt
}

void UART_IntrDis(void)
{
    NVIC_DisableIRQ(UART_IRQn);           // Enable USB interrupt
}

void uart_set_control_line_state(uint16_t ctrl_bmp)
{
}

void uart_software_flow_control()
{
    int v;

    if (((PIOA->PIO_PDSR >> BIT_CDC_USB2UART_CTS) & 1) == 0) {
        _TxInProgress = 0;
        v = _CDC_BUFFER_SIZE - _NumBytesWriteFree(&_WriteBuffer); // NumBytes in write buffer

        if (v == 0) {  // No more characters to send ?: Disable further tx interrupts
            UART_IER = UART_TX_INT_FLAG;
        } else {
            _Send1();    //More bytes to send? Trigger sending of next byte
        }

    } else {
        UART_IDR = UART_TX_INT_FLAG;
    }
}
int32_t uart_initialize(void)
{
    //
    // Initially, disable UART interrupt
    //
    UART_IntrDis();
    PMC->PMC_WPMR  = 0x504D4300;                     // Disable write protect
    PMC->PMC_PCER0 = (1 << UART_PID) | (1 << 10);    // Enable peripheral clock for UART + PIOA
    PMC->PMC_WPMR  = 0x504D4301;                     // Enable write protect
    PIOA_PDR   = PIO_UART_PIN_MASK;         // Enable peripheral output signals (disable PIO Port A)
    PIOA_ABSR &= ~PIO_UART_PIN_MASK;        // Select "A" peripherals on PIO A (UART Rx, Tx)
    PIOA->PIO_MDER = PIO_UART_PIN_MASK;     //Enable Multi Drive Control (Open Drain) on the UART Lines so that they don't power nRF51
    UART_CR    = (0)
                 | (1 <<  2)                  // RSTRX: Reset Receiver: 1 = The receiver logic is reset.
                 | (1 <<  3)                  // RSTTX: Reset Transmitter: 1 = The transmitter logic is reset.
                 ;
    UART_CR    = (0)
                 | (0 <<  2)                  // RSTRX: Release Receiver reset
                 | (0 <<  3)                  // RSTTX: Release Transmitter reset
                 | (1 <<  4)                  // RXEN: Receiver Enable
                 | (0 <<  5)                  // RXDIS: Do not disable receiver
                 | (1 <<  6)                  // TXEN: Transmitter Enable
                 | (0 <<  7)                  // TXDIS: Do not disable transmitter
                 | (1 <<  8)                  // RSTSTA: Reset status/error bits
                 ;
    UART_MR    = (0)
                 | (4 <<  9)                  // PAR: Parity Type: 4     => No parity
                 | (0 << 14)                  // CHMODE: Channel Mode: 0 => Normal mode
                 ;
    _SetBaudrate(9600);
    _FlowControl = UART_FLOW_CONTROL_NONE;
    UART_IDR   = (0xFFFFFFFF);              // Disable all interrupts
    //
    // Reset all status variables
    //
    _ResetBuffers();
    //
    // Enable UART Tx/Rx interrupts
    //
    UART_IER   = (0)
                 | (1 <<  0)                  // Enable Rx Interrupt
                 | (0 <<  9)                  // Initially disable TxEmpty Interrupt
                 | (0 <<  4)                  // Initially disable ENDTx Interrupt
                 ;
    //
    //Set "RTS" to LOW to indicate that we are ready to receive
    //
    PIOA_CODR      = (1uL << BIT_CDC_USB2UART_RTS);  // RTS low: Ready to receive data
    PIOA->PIO_OER  = (1uL << BIT_CDC_USB2UART_RTS);  // Pins == output
    PIOA->PIO_PER  = (1uL << BIT_CDC_USB2UART_RTS);  // Pins == GPIO control
    //Set CTS as input
    PIOA->PIO_PER  = (1uL << BIT_CDC_USB2UART_CTS);  // Pins == GPIO control
    PIOA->PIO_ODR  = (1uL << BIT_CDC_USB2UART_CTS);  // Pins == Input
    PIOA->PIO_IER  = (1uL << BIT_CDC_USB2UART_CTS);
    //
    // Finally, re-enable UART interrupt
    //
    //NVIC_SetPriority(UART_IRQn, 1);
    UART_IntrEna();
    return 1;  // O.K. ???
}

int32_t uart_uninitialize(void)
{
    UART_IntrDis();
    UART_IDR   = (0xFFFFFFFF);              // Disable all interrupts
    _ResetBuffers();
    return 1;
}

int32_t uart_reset(void)
{
    uart_initialize();
    return 1;
}

int32_t uart_set_configuration(UART_Configuration *config)
{
    //
    // UART always works with no parity, 1-stop bit
    // Parity bit is configurable but not used in current implementation
    //
    UART_IntrDis();
    UART_IDR   = (0xFFFFFFFF);   // Disable all interrupts
    UART_CR    = (0)
                 | (1 <<  5)                  // RXDIS: Disable receiver
                 | (1 <<  7)                  // TXDIS: Disable transmitter
                 | (1 <<  8)                  // RSTSTA: Reset status/error bits
                 ;
    _FlowControl = config->FlowControl;
    _SetBaudrate(config->Baudrate);
    UART_CR    = (0)
                 | (0 <<  2)                  // RSTRX: Release Receiver reset
                 | (0 <<  3)                  // RSTTX: Release Transmitter reset
                 | (1 <<  4)                  // RXEN: Receiver Enable
                 | (0 <<  5)                  // RXDIS: Do not disable receiver
                 | (1 <<  6)                  // TXEN: Transmitter Enable
                 | (0 <<  7)                  // TXDIS: Do not disable transmitter
                 | (1 <<  8)                  // RSTSTA: Reset status/error bits
                 ;
    UART_IER   = (0)
                 | (1 <<  0)                  // Enable Rx Interrupt
                 | (0 <<  9)                  // Initially disable TxEmpty Interrupt
                 | (0 <<  4)                  // Initially disable ENDTx Interrupt
                 ;
    UART_IntrEna();
    return 1;
}


int32_t uart_get_configuration(UART_Configuration *config)
{
    config->Baudrate    = _Baudrate;
    config->DataBits    = UART_DATA_BITS_8;
    config->FlowControl = (UART_FlowControl) _FlowControl;//UART_FLOW_CONTROL_NONE;
    config->Parity      = UART_PARITY_NONE;
    config->StopBits    = UART_STOP_BITS_1;
    return 1;
}

int32_t uart_write_free(void)
{
    int32_t bytesFree = 0;
    bytesFree =  _NumBytesWriteFree(&_WriteBuffer);

    //are there still bytes to send?
    if (bytesFree < _CDC_BUFFER_SIZE) {
        bytesFree = 0;
        //pause to give NRF chip time to assert CTS line if needed
        //os_dly_wait(4);
    }

    return bytesFree;
}


int32_t uart_write_data(uint8_t *data, uint16_t size)
{
    uint16_t NumBytesWritten;
    uint16_t NumBytesAtOnce;
    U8 *pWrapAround;
    U8 *pWrite;
    //
    // Early out in case nothing needs to be written or buffer is full
    //
    size = MIN(_NumBytesWriteFree(&_WriteBuffer), size);

    if (size == 0) {  // Early out
        return 0;
    }

    //
    // Copy data into buffer. Also consider wrap-around
    //
    NumBytesWritten = size;
    pWrite = _WriteBuffer.pWrite;
    pWrapAround = (U8 *)(_WriteBuffer.acBuffer + sizeof(_WriteBuffer.acBuffer));

    if (size) {
        do {
            NumBytesAtOnce = MIN(size, (int)(pWrapAround - pWrite));   // Copy as much as possible and consider wrap-around
            memcpy(pWrite, data, NumBytesAtOnce);
            pWrite += NumBytesAtOnce;

            if (pWrite == pWrapAround) {
                pWrite = _WriteBuffer.acBuffer;
            }

            size -= NumBytesAtOnce;
            data += NumBytesAtOnce;
        } while (size);
    }

    _WriteBuffer.pWrite = pWrite;

    //
    // Trigger transfer if not already in progress
    //
    if (_TxInProgress == 0 && ((PIOA->PIO_PDSR >> BIT_CDC_USB2UART_CTS) & 1) == 0) {
        _Send1();
    }

    return NumBytesWritten;
}

int32_t uart_read_data(uint8_t *data, uint16_t size)
{
    uint16_t NumBytesRead;
    uint16_t NumBytesAtOnce;
    uint16_t v;
    uint16_t writeFreeBytes;
    U8 *pWrapAround;
    U8 *pRead;
    writeFreeBytes = _NumBytesWriteFree(&_ReadBuffer);

    //Check if RTS had been asserted, if there is space on the buffer then deassert RTS
    if (writeFreeBytes > 0 && ((PIOA->PIO_PDSR >> BIT_CDC_USB2UART_RTS) & 1)) {
        PIOA->PIO_CODR = 1 << BIT_CDC_USB2UART_RTS;
    }

    v = _CDC_BUFFER_SIZE - writeFreeBytes;
    size = MIN(v, size);

    if (size == 0) {  // Early out
        return 0;
    }

    NumBytesRead = size;
    pWrapAround = (U8 *)(_ReadBuffer.acBuffer + sizeof(_ReadBuffer.acBuffer));
    pRead = _ReadBuffer.pRead;

    if (size) {
        do {
            NumBytesAtOnce = MIN(size, (int)(pWrapAround - pRead));   // Copy as much as possible and consider wrap-around
            memcpy(data, pRead, NumBytesAtOnce);
            pRead += NumBytesAtOnce;

            if (pRead == pWrapAround) {
                pRead = _ReadBuffer.acBuffer;
            }

            size -= NumBytesAtOnce;
            data += NumBytesAtOnce;
        } while (size);
    }

    _ReadBuffer.pRead = pRead;
    return NumBytesRead;
}

void UART_IRQHandler(void)
{
    int Status;
    uint32_t v;
    U8 c;
    U8 *pWrapAround;
    Status = UART_SR;                                 // Examine status register

    if (Status & UART_RX_ERR_FLAGS) {                 // In case of error: Set RSTSTA to reset status bits PARE, FRAME, OVRE and RXBRK
        UART_CR = (1 << 8);
    }

    //
    // Handle Rx event
    //
    if (Status & UART_RXRDY_FLAG) {                   // Data received?
        c = UART_RHR;
        v = _NumBytesWriteFree(&_ReadBuffer);

        if (v) {
            *_ReadBuffer.pWrite++ = c;
            pWrapAround = (U8 *)(_ReadBuffer.acBuffer + sizeof(_ReadBuffer.acBuffer));

            if (_ReadBuffer.pWrite == pWrapAround) {
                _ReadBuffer.pWrite = _ReadBuffer.acBuffer;
            }

            //If this was the last available byte on the buffer then assert RTS
            if (v == 1) {
                PIOA->PIO_SODR = 1 << BIT_CDC_USB2UART_RTS;
            }
        }
    }

    //
    // Handle Tx event
    //
    if (Status & UART_IMR & UART_TX_INT_FLAG) {       // Byte has been send by UART
        v = _CDC_BUFFER_SIZE - _NumBytesWriteFree(&_WriteBuffer); // NumBytes in write buffer

        if (v == 0) {                               // No more characters to send ?: Disable further tx interrupts
            UART_IDR = UART_TX_INT_FLAG;
            PIOA->PIO_MDER = (1 << UART_TX_PIN);    //enable open-drain
            _TxInProgress = 0;
        } else if (((PIOA->PIO_PDSR >> BIT_CDC_USB2UART_CTS) & 1) == 0) {
            _Send1();                               //More bytes to send? Trigger sending of next byte
        } else {
            UART_IDR = UART_TX_INT_FLAG;            // disable Tx interrupt
            PIOA->PIO_MDER = (1 << UART_TX_PIN);    //enable open-drain
        }
    }
}
