/*
 BasicSDAudio for ChipKit
 
 BasicSDAudio is derived from SimpleSDAudio by Lutz Lisseck and is released
 under the same license.
 
 Copyright (c) 2012, Thomas Fulenwider
 
 -----------------------------------------------------------------------
 
 Easily play audio files with Arduino from SD card in decent quality but 
 with very low external hardware.
 
 Visit our hackerspace website for more information: 
 http://www.hackerspace-ffm.de/wiki/index.php?title=SimpleSDAudio
 
 Pin-Mappings:
 =================================================================================
 PLATFORM DEFAULTS                     SD_CS MOSI MISO SCK SS  PWM1 PWM2 Ocx1 Ocx2
 Uno32, Max32 (standard ChipKit)  	   4     11   12   13  10  5    6    2    3
 Fubarino SD					  	   4     11   12   13  10  7    8 	 2    3

 **may collide with eth-shield cs-pin. Move eth-cs-pin or use only mono mode
 
 SD MMC Card Pinout
 =================================================================================
  /-----------------+  Pin   Arduino       Level shift network req.? 
 /  1 2 3 4 5 6 7 8 |   1    SD_CS         yes
 |9                 |   2    MOSI          yes       Arduino-Pin            GND
 |   Contact side   I   3    GND           -             |                   |
 |                  |   4    3.3V          -             +-[1.8k]--+--[3.3k]-+
 |                  |   5    SCK           yes                     |
 |                  |   6    GND           -                     SD-Pin
 |                  |   7    MISO          no                                 
 |                  |   8    not connect   -          Level shifting network 
 +------------------+   9    not connect   -             for 5V Arduinos
 
 Audio connection options
 =================================================================================
 For mode BSDA_MODE_MONO:
   - Very very simple (but dangerous due DC-offset voltage and too loud)
     - Connect GND to GND of active speakers
     - Connect PWM1 to L or R input of active speakers

   - Very very simple for loudspeaker (also not good due DC-offset voltage)
     - GND --[100R to 500R]--- Speaker --- PWM1

   - Better (for active speakers)
     - PWM1 ---||----[10k]---+----[1k]--- GND
              100nF          |
                             Line in of active speaker
                             
 For mode BSDA_MODE_STEREO:  
   - Use same circuits like for BSDA_MODE_MONO, but build it two times, 
     PWM1 for left, PWM2 for right channel
 
 For mode BSDA_MODE_MONO_BRIDGE: (only usefull for direct speaker drive, louder)
   - Very very simple for loudspeaker (also not good due DC-offset voltage)
     - PWM1 --[100R to 500R]--- Speaker --- PWM2

   - Better for loudspeaker 
     - PWM1 --[100R]--||----- Speaker --- PWM2
                     10uF   

  Version history:
  1.02 		Added 4 channel or 16-Bit Stereo support, 
            rewritten interrupt in assembler
  1.01      Added compatibility for Arduino 1.0 IDE
  1.00      Initial release  
 
 Copyright (c) 2012, Lutz Lisseck (lutz. lisseck AT gmx. de)
 
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

 -------------------------------------------------------------------------------
*/
#ifndef __BASICSDAUDIO_H__
#define __BASICSDAUDIO_H__

#if (ARDUINO >= 100)
#include <Arduino.h>
#else
#include <WProgram.h>
#endif

#include <sd_l2.h>

#define BSDA_VERSIONSTRING      "1.02"

// Sound Mode Flags
#define BSDA_MODE_FULLRATE      0x00    // 62.500 kHz @ 16 MHz, 31.250 kHz @ 8 MHz
#define BSDA_MODE_HALFRATE      0x10    // 31.250 kHz @ 16 MHz, 15.625 kHz @ 8 MHz

#define BSDA_MODE_MONO          0x00    // Use only 1st PWM pin
#define BSDA_MODE_STEREO        0x01    // Use both PWM pins for stereo output
#define BSDA_MODE_QUADRO		0x04	// Uses four PWM pins for either 4 speakers or Stereo 16 Bit
#define BSDA_MODE_MONO_BRIDGE   0x02    // Use both PWM pins for more power

// Error codes from BasicSDAudio, see other sd_l*.h for more error codes
#define BSDA_ERROR_NULL         0x80    // Null pointer
#define BSDA_ERROR_BUFTOSMALL   0x81    // Buffer to small
#define BSDA_ERROR_NOT_INIT     0x82    // System not initialized properly

// Flags
uint8_t const BSDA_F_PLAYING  = 0x01;   // 1 if playing active
uint8_t const BSDA_F_STOPPED  = 0x02;   // 1 if stopped or file reached end
uint8_t const BSDA_F_UNDERRUN = 0x04;   // 1 if buffer underrun occured
uint8_t const BSDA_F_HALFRATE = 0x08;   // If 1, only every 2nd interrupt is used for sample refresh
uint8_t const BSDA_F_HRFLAG   = 0x10;   // Flag to find every 2nd interrupt
uint8_t const BSDA_F_STEREO   = 0x20;   // If 1, OCxB outputs the second channel
uint8_t const BSDA_F_BRIDGE   = 0x40;   // If 1, OCxB outputs the same signal but inverted (for more output power)


//------------------------------------------------------------------------------
#if defined(_BOARD_UNO_) || defined(_BOARD_MEGA_)
// Uno32

	#define BSDA_PIN_TO_OC(p) (p == 3 ? 1 : (p == 5 ? 2 : (p == 6 ? 3 : (p == 9 ? 4 : 5)))) //Uno32

	// pin settings
    #define BSDA_OC1L_PIN 5	
    #define BSDA_OC2L_PIN 6
	#define BSDA_OC1H_PIN 9
    #define BSDA_OC2H_PIN 10
	
	// Output-compare settings
    #define BSDA_OC1L(d)    SetDCOC2PWM(d)	//PWM duty macro for CH1 low word: pin 5
    #define BSDA_OC2L(d)    SetDCOC3PWM(d)	//PWM duty macro for CH2 low word: pin 6
    #define BSDA_OC1H(d)    SetDCOC4PWM(d)	//PWM duty macro for CH1 high word:pin 9
    #define BSDA_OC2H(d)    SetDCOC5PWM(d)	//PWM duty macro for CH2 high word:pin10
	
	#define BSDA_OPEN_OC1L 	OpenOC2(OC_ON | OC_TIMER2_SRC | OC_PWM_FAULT_PIN_DISABLE,0,0)
	#define BSDA_OPEN_OC2L 	OpenOC3(OC_ON | OC_TIMER2_SRC | OC_PWM_FAULT_PIN_DISABLE,0,0)
	#define BSDA_OPEN_OC1H 	OpenOC4(OC_ON | OC_TIMER2_SRC | OC_PWM_FAULT_PIN_DISABLE,0,0)
	#define BSDA_OPEN_OC2H 	OpenOC5(OC_ON | OC_TIMER2_SRC | OC_PWM_FAULT_PIN_DISABLE,0,0)
	
//------------------------------------------------------------------------------
#else defined(_BOARD_FUBARINO_SD_)
// Max32

	// pin settings
    #define BSDA_OC1L_PIN 7	
    #define BSDA_OC2L_PIN 8
	#define BSDA_OC1H_PIN 9
    #define BSDA_OC2H_PIN 10
	
	// Output-compare settings
    #define BSDA_OC1L(d)    SetDCOC2PWM(d)	//PWM duty macro for CH1 low word: pin 7
    #define BSDA_OC2L(d)    SetDCOC3PWM(d)	//PWM duty macro for CH2 low word: pin 8
    #define BSDA_OC1H(d)    SetDCOC4PWM(d)	//PWM duty macro for CH1 high word:pin 9
    #define BSDA_OC2H(d)    SetDCOC5PWM(d)	//PWM duty macro for CH2 high word:pin10
	
	#define BSDA_OPEN_OC1L 	OpenOC2(OC_ON | OC_TIMER2_SRC | OC_PWM_FAULT_PIN_DISABLE,0,0)
	#define BSDA_OPEN_OC2L 	OpenOC3(OC_ON | OC_TIMER2_SRC | OC_PWM_FAULT_PIN_DISABLE,0,0)
	#define BSDA_OPEN_OC1H 	OpenOC4(OC_ON | OC_TIMER2_SRC | OC_PWM_FAULT_PIN_DISABLE,0,0)
	#define BSDA_OPEN_OC2H 	OpenOC5(OC_ON | OC_TIMER2_SRC | OC_PWM_FAULT_PIN_DISABLE,0,0)
	
//------------------------------------------------------------------------------	
	
	
#endif


// timer settings
#define BSDA_USE_TIMER 2


#if BSDA_USE_TIMER == 3
	#define BSDA_OPEN_TMR 	OpenTimer3(T3_ON | T3_PS_1_4, 255)
	#define BSDA_CFG_TMRINTON	ConfigIntTimer3(T3_INT_ON | T2_INT_PRIOR_3)
	#define BSDA_CFG_TMRINTOFF	ConfigIntTimer3(T3_INT_OFF | T2_INT_PRIOR_3)
#else
	#define BSDA_OPEN_TMR 	OpenTimer2(T2_ON | T2_PS_1_4, 255)
	#define BSDA_CFG_TMRINTON	ConfigIntTimer2(T2_INT_ON | T2_INT_PRIOR_3)
	#define BSDA_CFG_TMRINTOFF	ConfigIntTimer2(T2_INT_OFF | T2_INT_PRIOR_3)
#endif
  
    
class SdPlayClass {
  private:
    uint8_t _oc_cr1_bup;        // Backup of 1st control register 
    uint8_t _oc_cr2_bup;        // Backup of 2nd control register 
    uint8_t *_pBuf;             // pointer to working buffer, used for audio and all kind of file access
    uint16_t _Bufsize;          // size of working buffer, must be a multiple of 512, at least 1024
    uint8_t *_pBufoutend;       // points to _pBuf + _Bufsize
    volatile uint16_t _Buflen;  // how much bytes are availible in buffer
    uint16_t _Bufin;            // index where next byte can put into the buffer
    volatile uint8_t *_pBufout; // pointer where next byte can read from the buffer
    boolean  _BufViaMalloc;     // Set to true if Buf created dynamically
    
    volatile uint8_t  _flags;
    
    SD_L2_File_t _fileinfo;
    uint8_t _lastError;
  
  public:
    SdPlayClass(void);  // constructor
    ~SdPlayClass(void); // destructor
    
    void    interrupt(void); // Only for internal use!
    
    // Optional: call this before init to set SD-Cards CS-Pin to other than default    
    void    setSDCSPin(uint8_t csPin); 
    
    // Optional: call this if you want to use your own buffer (at least 1024 bytes, must be multiple of 512)
    void    setWorkBuffer(uint8_t *pBuf, uint16_t bufSize); 
    
    // Call this to set sound mode, see BSDA_MODE_* flags above for modes
    boolean init(uint8_t soundMode);
    
    // Optional: call this to free resources 
    void    deInit(void);
    
    // Optional: output directory list
    void    dir(void (*callback)(char *));  

    // After  init, call this to select audio file
    boolean setFile(char *fileName);
    
    // Call this continually in main loop 
    void    worker(void);    
    
    void    stop(void);  // stops playback if playing, sets playposition to zero
    void    play(void);  // if not playing, start playing. if playing start from zero again
    void    pause(void); // pauses playing if not playing, resumes playing if was paused
    
    boolean isStopped(void);
    boolean isPlaying(void);
    boolean isPaused(void);
    
    boolean isUnderrunOccured(void); 
    uint8_t getLastError(void);
    
    uint8_t _debug;
};

extern SdPlayClass SdPlay;

#endif
