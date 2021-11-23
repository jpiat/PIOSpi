/*
    SPI Master library for the Raspberry Pi Pico RP2040

    Copyright (c) 2021 Earle F. Philhower, III <earlephilhower@yahoo.com>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "PioSPI.h"
#include <hardware/gpio.h>
#include <hardware/pio.h>
#include "hardware/clocks.h"



PioSPI::PioSPI(pin_size_t tx, pin_size_t rx, pin_size_t sck, pin_size_t cs , bool cpha, bool cpol, uint32_t frequency) {
    _spi = {
            .pio = pio1,
            .sm = 0
    };
    _RX = rx ;
    _TX = tx ;
    _SCK = sck ;
    _CS = cs ;
    _CPHA = cpha ;
    _CPOL = cpol ;
    _BITORDER = MSBFIRST ;
    uint32_t system_clock_frequency = clock_get_hz(clk_sys);
    _clkdiv = ((float) system_clock_frequency)/(frequency * 4);  // 25MHz
    _initted = false ;
}

inline bool PioSPI::cpol() {
    return _CPOL;
}

inline bool PioSPI::cpha() {
    return _CPHA ;
}

inline uint8_t PioSPI::reverseByte(uint8_t b) {
    b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
    b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
    b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
    return b;
}

inline uint16_t PioSPI::reverse16Bit(uint16_t w) {
    return (reverseByte(w & 0xff) << 8) | (reverseByte(w >> 8));
}

// The HW can't do LSB first, only MSB first, so need to bitreverse
void PioSPI::adjustBuffer(const void *s, void *d, size_t cnt, bool by16) {
    if (_BITORDER == MSBFIRST) {
        memcpy(d, s, cnt * (by16 ? 2 : 1));
    } else if (!by16) {
        const uint8_t *src = (const uint8_t *)s;
        uint8_t *dst = (uint8_t *)d;
        for (size_t i = 0; i < cnt; i++) {
            *(dst++) = reverseByte(*(src++));
        }
    } else { /* by16 */
        const uint16_t *src = (const uint16_t *)s;
        uint16_t *dst = (uint16_t *)d;
        for (size_t i = 0; i < cnt; i++) {
            *(dst++) = reverse16Bit(*(src++));
        }
    }
}

byte PioSPI::transfer(uint8_t data) {
    uint8_t ret;
    if (!_initted) {
        return 0;
    }
    data = (_BITORDER == MSBFIRST) ? data : reverseByte(data);
    DEBUGSPI("SPI::transfer(%02x), cpol=%d, cpha=%d\n", data, cpol(), cpha());
    pio_spi_write8_read8_blocking(&_spi, (uint8_t *) &data,(uint8_t *) &ret, 1);
    ret = (_BITORDER == MSBFIRST) ? ret : reverseByte(ret);
    DEBUGSPI("SPI: read back %02x\n", ret);
    return ret;
}

uint16_t PioSPI::transfer16(uint16_t data) {
    uint16_t ret;
    if (!_initted) {
        return 0;
    }
    data = (_BITORDER == MSBFIRST) ? data : reverse16Bit(data);
    DEBUGSPI("SPI::transfer16(%04x), cpol=%d, cpha=%d\n", data, cpol(), cpha());
    pio_spi_write8_read8_blocking(&_spi, (uint8_t *) &data, (uint8_t *) &ret, 2);
    ret = (_BITORDER == MSBFIRST) ? ret : reverseByte(ret);
    DEBUGSPI("SPI: read back %02x\n", ret);
    return ret;
}

void PioSPI::transfer(void *buf, size_t count) {
    DEBUGSPI("SPI::transfer(%p, %d)\n", buf, count);
    uint8_t *buff = reinterpret_cast<uint8_t *>(buf);
    for (size_t i = 0; i < count; i++) {
        *buff = transfer(*buff);
        *buff = (_BITORDER == MSBFIRST) ? *buff : reverseByte(*buff);
        buff++;
    }
    DEBUGSPI("SPI::transfer completed\n");
}

void PioSPI::transfer(void *txbuf, void *rxbuf, size_t count) {
    if (!_initted) {
        return;
    }

    DEBUGSPI("SPI::transfer(%p, %p, %d)\n", txbuf, rxbuf, count);
    uint8_t *txbuff = reinterpret_cast<uint8_t *>(txbuf);
    uint8_t *rxbuff = reinterpret_cast<uint8_t *>(rxbuf);

    // MSB version is easy!
    if (_BITORDER == MSBFIRST) {
        if (rxbuf == NULL) { // transmit only!
            pio_spi_write8_blocking(&_spi,  (uint8_t *) txbuff, count);
            return;
        }
        if (txbuf == NULL) { // receive only!
            pio_spi_read8_blocking(&_spi,  (uint8_t *) rxbuff, count);
            return;
        }
        // transmit and receive!
        pio_spi_write8_read8_blocking(&_spi,  (uint8_t *) txbuff,  (uint8_t *) rxbuff, count);
        return;
    }

    // If its LSB this isn't nearly as fun, we'll just let transfer(x) do it :(
    for (size_t i = 0; i < count; i++) {
        *rxbuff = transfer(*txbuff);
        *rxbuff = (_BITORDER == MSBFIRST) ? *rxbuff : reverseByte(*rxbuff);
        txbuff++;
        rxbuff++;
    }
    DEBUGSPI("SPI::transfer completed\n");
}

void PioSPI::beginTransaction(SPISettings settings) {
    if(!_initted){
        uint32_t system_clock_frequency = clock_get_hz(clk_sys);
        _clkdiv = ((float) system_clock_frequency)/((float) settings.getClockFreq() * 4);  // 25MHz
        _BITORDER = settings.getBitOrder() ;
        _CPHA = (settings.getDataMode() == SPI_MODE1) || (settings.getDataMode() == SPI_MODE3);
        _CPOL = (settings.getDataMode() == SPI_MODE2) || (settings.getDataMode() == SPI_MODE3);
        uint cpha = _CPHA ? pio_add_program(_spi.pio, &spi_cpha1_program) : pio_add_program(_spi.pio, &spi_cpha0_program) ;
        gpio_init(_CS);
        gpio_put(_CS, 1);
        gpio_set_dir(_CS, GPIO_OUT);
        pio_spi_init(_spi.pio, 
                    _spi.sm,
                    cpha ,
                    8,       // 8 bits per SPI frame
                    _clkdiv,
                    cpha,
                    _CPOL,
                    _SCK,
                    _TX,
                    _RX);
        _running = true ;
        _initted = true ;
    }
    //TODO: take settings into account
    
    //TODO: take mode into account
    //TODO: take clk into account
    gpio_put(_CS, 0);
}

void PioSPI::endTransaction(void) {
    gpio_put(_CS, 1);
}

bool PioSPI::setRX(pin_size_t pin) {
    if (!_running) {
        _RX = pin;
        return true;
    }

    return false;
}

bool PioSPI::setCS(pin_size_t pin) {
    if (!_running) {
        _CS = pin;
        return true;
    }

    return false;
}

bool PioSPI::setSCK(pin_size_t pin) {
    if (!_running) {
        _SCK = pin;
        return true;
    }

    return false;
}

bool PioSPI::setTX(pin_size_t pin) {
   if (!_running) {
        _TX= pin;
        return true;
    }

    return false;
}

void PioSPI::begin() {
    gpio_init(_CS);
    gpio_put(_CS, 1);
    gpio_set_dir(_CS, GPIO_OUT);
}

void PioSPI::end() {
    pio_sm_set_enabled(_spi.pio, _spi.sm, false);
    pio_sm_unclaim(_spi.pio, _spi.sm);
}


