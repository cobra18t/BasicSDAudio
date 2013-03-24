/*
 BasicSDAudio for ChipKit
 
 BasicSDAudio is derived from SimpleSDAudio by Lutz Lisseck and is released
 under the same license.
 
 Copyright (c) 2012, Thomas Fulenwider
 
 -----------------------------------------------------------------------
 More information in file BasicSDAudio.h or at the website:
 http://www.hackerspace-ffm.de/wiki/index.php?title=SimpleSDAudio
 
 Copyright (c) 2012, Lutz Lisseck 
 
 MIT-License: 
 Permission is hereby granted, free of charge, to any person obtaining a 
 copy of this software and associated documentation files (the 
 "Software"), to deal in the Software without restriction, including 
 without limitation the rights to use, copy, modify, merge, publish, 
 distribute, sublicense, and/or sell copies of the Software, and to 
 permit persons to whom the Software is furnished to do so, subject to 
 the following conditions: The above copyright notice and this permission 
 notice shall be included in all copies or substantial portions of the 
 Software. 
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY 
 KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF 
 MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. 
 IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY 
 CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, 
 TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE 
 SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. 
---------------------------------------------------------------------------
*/
#if (ARDUINO >= 100)
#include <Arduino.h>
#else
#include <WProgram.h>
#endif

/* For PIC32 */
#if defined(__PIC32MX__)
	#include <peripheral/timer.h>
	#include <peripheral/outcompare.h>
#else
	// This library should only used for PIC32 boards
	//
	// For Arduino (Atmel) based boards, use SimpleSDAudio by Lutz Lisseck
	//
#endif

#include <sd_l0.h>
#include <sd_l1.h>
#include <sd_l2.h>

#include <BasicSDAudio.h>

// ************* Instantiations ********************
SdPlayClass SdPlay;

/* This is ISR corresponding to the Timer2 interrupt */
extern "C"
{
void __ISR(_TIMER_2_VECTOR,ipl3) playSamp(void)
{
	SdPlay.interrupt();
	mT2ClearIntFlag();  // Clear interrupt flag
}
}


// ************* Class implementation **************
/**
 * Interrupt routine
 *
 * Hardly optimized, therefore badly readable
 */
void SdPlayClass::interrupt(void) {
  uint8_t  flags = _flags;  // local copy for faster access
  if(flags & BSDA_F_PLAYING) {
    if(!(flags & BSDA_F_HALFRATE) || ((flags ^= BSDA_F_HRFLAG) & BSDA_F_HRFLAG)) {
      if(_Buflen > 1) {
         #ifdef BSDA_OC1H
           //>>BSDA_OC1H = 0;
         #endif
         uint8_t temp;
         temp = *_pBufout++;
		 BSDA_OC1L(temp);		//set PWM duty
         if(flags & BSDA_F_STEREO) {
            _Buflen-=2; 
            temp = *_pBufout++;
			BSDA_OC2L(temp);
         } else {
           _Buflen--;
           if(flags & BSDA_F_BRIDGE) {
			BSDA_OC2L(temp);
           }
         }
         if(_pBufout >= _pBufoutend) _pBufout -= _Bufsize;
      } else {
        flags |= BSDA_F_UNDERRUN;
      }            
    } 
    _flags = flags;
  }
}

SdPlayClass::SdPlayClass(void) {
  _pBuf = NULL;
  _BufViaMalloc = false;
  SD_L0_CSPin = SD_L0_CHIP_SELECT_PIN_DEFAULT;
  _debug = 0;
}

SdPlayClass::~SdPlayClass(void) {
  deInit();
}

void SdPlayClass::setSDCSPin(uint8_t csPin) {
  SD_L0_CSPin = csPin;
}

void SdPlayClass::setWorkBuffer(uint8_t *pBuf, uint16_t bufSize) {
  _pBuf = pBuf;
  _Bufsize = bufSize;
}

boolean SdPlayClass::init(uint8_t soundMode) {
  // make backup of control registers
  //>>_oc_cr1_bup = BSDA_OC_CR1_REG;
  //>>_oc_cr2_bup = BSDA_OC_CR2_REG; 
  
  // if buffer not set, create one dynamically
  if(_pBuf == NULL) {
    _Bufsize = 1024;
    _pBuf = (uint8_t*)malloc(_Bufsize);
    if(_pBuf) _BufViaMalloc = true;
  }
  
  if(_pBuf == NULL) {
    _lastError = BSDA_ERROR_NULL;
    return(false);
  }
  if(_Bufsize < 1024) {
    _lastError = BSDA_ERROR_BUFTOSMALL;
    return(false);
  }
  
  // hardcore SPI pin Init
  //SPI.begin();
  
  _Bufsize = _Bufsize & 0xfe00;  // clamp to 512 byte units
  
  // Init SD card, many errors can occur here...
  uint8_t ret;
  ret = SD_L2_Init(_pBuf);
  if(ret) {
    _pBuf = NULL; // used as indicator that class has been initialized
    _lastError = ret;
    return(false);
  }

  stop();   // also used to reset output buffer
  
  _flags &= ~(BSDA_F_UNDERRUN | BSDA_F_HALFRATE | BSDA_F_HRFLAG | BSDA_F_STEREO | BSDA_F_BRIDGE);
  
  if(soundMode & BSDA_MODE_HALFRATE)     _flags |= BSDA_F_HALFRATE;
  if(soundMode & BSDA_MODE_STEREO)       _flags |= BSDA_F_STEREO;
  if(soundMode & BSDA_MODE_MONO_BRIDGE)  _flags |= BSDA_F_BRIDGE;

  BSDA_CFG_TMRINTOFF;	//config int off macro
  
  BSDA_OPEN_TMR;		//Open timer macro
  
  //digitalWrite(BSDA_OC1L_PIN, LOW);
  pinMode(BSDA_OC1L_PIN, OUTPUT);
  BSDA_OPEN_OC1L;		//Open OC macro

	if((soundMode & BSDA_MODE_STEREO) || (soundMode & BSDA_MODE_MONO_BRIDGE) 
	  || (soundMode & BSDA_MODE_QUADRO)) {
	// configure 2 channels
	if(soundMode & BSDA_MODE_MONO_BRIDGE) {
		//>>STILL NEED TO INVERT Add bridging (invert 2nd PWM)
		pinMode(BSDA_OC1H_PIN, OUTPUT);	
		BSDA_OPEN_OC1H;		//Open OC macro
	}
	if(soundMode & BSDA_MODE_STEREO) {
		// Add stereo OC
		pinMode(BSDA_OC2L_PIN, OUTPUT);	
		BSDA_OPEN_OC2L;		//Open OC macro
	}
	if(soundMode & BSDA_MODE_QUADRO) {
		// configure 4 channels
		#ifdef BSDA_QUAD_OC_ENABLE
		pinMode(BSDA_OC1H_PIN, OUTPUT); 
		pinMode(BSDA_OC2L_PIN, OUTPUT);		
		pinMode(BSDA_OC2H_PIN, OUTPUT);
		BSDA_OPEN_OC1H;		//Open OC macro
		BSDA_OPEN_OC2L;		//Open OC macro
		BSDA_OPEN_OC2H;		//Open OC macro
		#endif
	}	
	} else {
	// configure 1 channel
	
	}
  BSDA_OC2L(127);
  // Set PWM to mid-level
  #ifdef BSDA_OC1H
	//>>BSDA_OC1H = 0;
  #endif
  //>>BSDA_OC1L = 80;
  //>>BSDA_OC2L = 80;
  
  //BSDA_CFG_TMRINTON;	//config int on macro
  _fileinfo.Size = 0;

  return(true);
}

/**
 * Disables PWM interrupts, disables sd-card (can be ejected then)
 */
void SdPlayClass::deInit(void) {
  stop();
  BSDA_CFG_TMRINTOFF;	//config int off macro

  // restore control registers
  //>>BSDA_OC_CR1_REG = _oc_cr1_bup;
  //>>BSDA_OC_CR2_REG = _oc_cr2_bup;

  if(_BufViaMalloc) {
    _BufViaMalloc = false;
    free(_pBuf);
  }  

  _fileinfo.Size = 0;   // used as indicator that file has been selected
  _pBuf = NULL;         // used as indicator that class has been initialized
}


void SdPlayClass::dir(void (*callback)(char *))
{
  if(!_pBuf) {
    _lastError = BSDA_ERROR_NOT_INIT;
  } else {
      stop();
      SD_L2_Dir(0, 0x00, 0x18, callback);
  }
}

/**
 * Sets file to play.
 *
 * \return true if successfull, false if not (fetch error-code using getLastError)
 */
boolean SdPlayClass::setFile(char *fileName) {
  if(!_pBuf) {
    _lastError = BSDA_ERROR_NOT_INIT;
    return(false);
  }
  uint8_t retval;
  stop();
  _fileinfo.Size = 0;
  retval = SD_L2_SearchFile((uint8_t *)fileName, 0UL, 0x00, 0x18, &_fileinfo);
  
  if(retval) {
     _lastError = retval;
     return(false);
  } else {
     return(true);
  }
}

void SdPlayClass::worker(void) {
  if(_pBuf && _fileinfo.Size) {
    uint16_t buflencpy;
	buflencpy = _Buflen; 

    if(_fileinfo.ActBytePos < _fileinfo.Size) {
        // At least space for 1 sector?
        if(buflencpy < (_Bufsize - 512)) {
            int16_t ret;
            ret = SD_L1_ReadBlock(_fileinfo.ActSector++, _pBuf + _Bufin);
            if(!ret) {
               uint32_t BytesLeft = _fileinfo.Size - _fileinfo.ActBytePos;
               _Bufin += 512;
               _fileinfo.ActBytePos += 512;
               if(_Bufin >= _Bufsize) _Bufin -= _Bufsize; 
               if(BytesLeft >= 512UL) {	 
					_Buflen += 512; 
                } else {
					_Buflen += BytesLeft; 
                }
            } else {
              stop();
              _lastError = ret;
            }
        }
    } else {
      // Playback done
      if(buflencpy <= 1) {
        stop();
      }
    }
  }
}

/**
 * Stops playback and set playposition to zero.
 */
void SdPlayClass::stop(void) {
	//BSDA_CFG_TMRINTOFF;	//config int on macro
	pinMode(BSDA_OC1L_PIN, OUTPUT);
	
	_flags &= ~BSDA_F_PLAYING;
	_flags |= BSDA_F_STOPPED;

    _Buflen = 0;
    _Bufin = 0;
    _pBufout = _pBuf;
    _pBufoutend = _pBuf + _Bufsize;
    
    if(_fileinfo.Size) {
        _fileinfo.ActSector = SD_L2_Cluster2Sector(_fileinfo.FirstCluster);
        _fileinfo.ActBytePos = 0;
    }
	
}

/**
 * if not playing, start playing. if playing start from zero again
 */
void SdPlayClass::play(void) {
	
    if(_fileinfo.Size) {
        if(_flags & BSDA_F_PLAYING) {
            stop();
        }
		_flags |= BSDA_F_PLAYING;
		_flags &= ~BSDA_F_STOPPED;
    }
		BSDA_CFG_TMRINTON;	//config int on macro
}

/**
 * pauses playing if not playing, resumes playing if was paused
 */
void SdPlayClass::pause(void) {
  if(!(_flags & BSDA_F_STOPPED)) {
	_flags ^= BSDA_F_PLAYING;
  }
}

/** 
 * Return if stopped (also true after playback reached end)
 */
boolean SdPlayClass::isStopped(void) {
    boolean ret = false;
    if(_flags & BSDA_F_STOPPED) {
        ret = true;       
    }
    return(ret);
}

/** 
 * Return if playing 
 */
boolean SdPlayClass::isPlaying(void) {
    boolean ret = false;
    if(_flags & BSDA_F_PLAYING) {
        ret = true;       
    }
    return(ret);
}

/** 
 * Return if paused 
 */
boolean SdPlayClass::isPaused(void) {
    boolean ret = false;
    if(!(_flags & BSDA_F_PLAYING) && !(_flags & BSDA_F_STOPPED)) {
        ret = true;       
    }
    return(ret);
}

/** 
 * Returns and clears underrun flag state
 */
boolean SdPlayClass::isUnderrunOccured(void) {
    boolean ret = false;
    if(_flags & BSDA_F_UNDERRUN) {
        ret = true;
		_flags &= ~BSDA_F_UNDERRUN;       
    }
    return(ret);
}

/** 
 * Returns and clears last error code
 */
uint8_t SdPlayClass::getLastError(void) {
    uint8_t temp = _lastError;
    _lastError = 0;
    return(temp);
}



