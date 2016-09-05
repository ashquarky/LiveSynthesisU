#include "dynamic_libs/os_functions.h"
#include "dynamic_libs/ax_functions.h"
#include "dynamic_libs/vpad_functions.h"
#include "dynamic_libs/gx2_functions.h"
#include "system/memory.h"
#include "program.h"
#include "kbd.h"

#include <string.h>
#include <math.h>
#include <stdlib.h>

void axFrameCallback();
unsigned int generateSineWave(sample* samples, unsigned int maxLength, float freq);
unsigned int generateSquareWave(sample* samples, unsigned int maxLength, float freq);
unsigned int generateSawtoothWave(sample* samples, unsigned int maxLength, float freq);
unsigned int generateTriangleWave(sample* samples, unsigned int maxLength, float freq);

void doNothing() {} //stub for keyboard connection callbacks
void keyboardCallback(struct KBDKeyState* keyState);

voiceData voice1; //voiceData is in program.h

int octave = 0;
#define SINE_WAVE 0
#define SQUARE_WAVE 1
#define SAWTOOTH_WAVE 2
#define TRIANGLE_WAVE 3
int currentWaveType = 0;

void* screenBuffer;
unsigned int bufferSize;
/*!
	Main program, called by main.c on main thread.
*/
void runProgram(void* _screenBuffer, unsigned int _bufferSize) {
	/*!
		Init AX library and set up voice1.
		Most of this init code is refactored from HBL.
	*/
	screenBuffer = _screenBuffer;
	bufferSize = _bufferSize;
	
	//init 48KHz renderer
	unsigned int params[3] = {1, 0, 0};
	AXInitWithParams(params);
	AXRegisterFrameCallback((void*)axFrameCallback); //this callback doesn't really do much
	
	memset((void*)&voice1, 0, sizeof(voice1));
	voice1.samples = malloc(SAMPLE_BUFFER_MAX_SIZE); //allocate room for samples
	
	voice1.stopped = 1;
	
	voice1.voice = AXAcquireVoice(25, 0, 0); //get a voice, priority 25. Just a random number I picked
	if (!voice1.voice) {
		return;
	}
	AXVoiceBegin(voice1.voice);
	AXSetVoiceType(voice1.voice, 0);
	
	//Set volume?
	unsigned int vol = 0x80000000;
	AXSetVoiceVe(voice1.voice, &vol);

	//Device mix? Volume of DRC/TV?
	unsigned int mix[24];
	memset(mix, 0, sizeof(mix));
	mix[0] = vol;
	mix[4] = vol;
	
	AXSetVoiceDeviceMix(voice1.voice, 0, 0, mix);
	AXSetVoiceDeviceMix(voice1.voice, 1, 0, mix);
	
	AXVoiceEnd(voice1.voice);
	
	/*!
		Set up KBD library to run our USB keyboards.
		rw-r-r_0644 REd all this, you're awesome!
		See kbd.h for more details.
	*/
	InitKBDFunctionPointers();
	KBDSetup(doNothing, doNothing, keyboardCallback);
	
	/*!
		Enter main program loop
		axFrameCallback is running every audio frame on another thread (possibly on another core),
		and keyboardCallback whenever a keyboard event comes through.
	*/
	VPADData vpadData;
	int vpadError;
	while(1) {
		/*!
			Generate sound and prepare audio
		*/
		//If the keyboard callback has requested we change voice1...
		if (voice1.newSoundRequested) {
			//clear the flag
			voice1.newSoundRequested = 0;
			
			//wipe the existing samples
			memset(voice1.samples, 0, SAMPLE_BUFFER_MAX_SIZE);
			//generate new samples
			switch (currentWaveType) {
				case SINE_WAVE:
					voice1.numSamples = generateSineWave(voice1.samples, SAMPLE_BUFFER_MAX_SIZE, voice1.newFrequency);
					break;
				case SQUARE_WAVE:
					voice1.numSamples = generateSquareWave(voice1.samples, SAMPLE_BUFFER_MAX_SIZE, voice1.newFrequency);
					break;
				case SAWTOOTH_WAVE:
					voice1.numSamples = generateSawtoothWave(voice1.samples, SAMPLE_BUFFER_MAX_SIZE, voice1.newFrequency);
					break;
				case TRIANGLE_WAVE:
					voice1.numSamples = generateTriangleWave(voice1.samples, SAMPLE_BUFFER_MAX_SIZE, voice1.newFrequency);
					break;
				default:
					voice1.numSamples = generateSineWave(voice1.samples, SAMPLE_BUFFER_MAX_SIZE, voice1.newFrequency);
					break;
			}
			
			//flush the samples to memory
			DCFlushRange(voice1.samples, SAMPLE_BUFFER_MAX_SIZE);
			//mark the voice as modified so the audio callback will reload it
			voice1.modified = 1;
			//flush changes to memory
			DCFlushRange(&voice1, sizeof(voice1));
		}
		
		/*!
			Render to the screen
		*/
		
		OSScreenClearBufferEx(0, 0x000000FF);
		OSScreenClearBufferEx(1, 0x000000FF);
		
		OSScreenPutFontEx(0, 0, 0, "PoC - Live sound synthesis on the Wii U!");
		OSScreenPutFontEx(0, 0, 2, "Put together by QuarkTheAwesome");
		OSScreenPutFontEx(0, 0, 3, "USB Keyboard made possible by rw-r-r_0644, who is also awesome.");
	
		OSScreenPutFontEx(1, 0, 0, "PoC - Live sound synthesis on the Wii U!");
		OSScreenPutFontEx(1, 0, 2, "Put together by QuarkTheAwesome");
		OSScreenPutFontEx(1, 0, 3, "USB Keyboard made possible by rw-r-r_0644, who is also awesome.");
		
		char buf[256];
		__os_snprintf(buf, 255, "All notes offset by %d semitones.", octave);
		OSScreenPutFontEx(0, 0, 5, buf);
		OSScreenPutFontEx(1, 0, 5, buf);
		
		for (int x = 0; x < 96; x++) {
			OSScreenPutPixelEx(0, x, VISUALISER_SCALE_Y_OFFSET, 0xFFFFFFFF);
			OSScreenPutPixelEx(1, x, VISUALISER_SCALE_Y_OFFSET, 0xFFFFFFFF);
		}
		OSScreenPutPixelEx(0, 0, VISUALISER_SCALE_Y_OFFSET - 1, 0xFFFFFFFF);
		OSScreenPutPixelEx(1, 0, VISUALISER_SCALE_Y_OFFSET - 1, 0xFFFFFFFF);
		OSScreenPutPixelEx(0, 0, VISUALISER_SCALE_Y_OFFSET - 2, 0xFFFFFFFF);
		OSScreenPutPixelEx(1, 0, VISUALISER_SCALE_Y_OFFSET - 2, 0xFFFFFFFF);
		OSScreenPutPixelEx(0, 0, VISUALISER_SCALE_Y_OFFSET + 1, 0xFFFFFFFF);
		OSScreenPutPixelEx(1, 0, VISUALISER_SCALE_Y_OFFSET + 1, 0xFFFFFFFF);
		OSScreenPutPixelEx(0, 0, VISUALISER_SCALE_Y_OFFSET + 2, 0xFFFFFFFF);
		OSScreenPutPixelEx(1, 0, VISUALISER_SCALE_Y_OFFSET + 2, 0xFFFFFFFF);
		OSScreenPutPixelEx(0, 95, VISUALISER_SCALE_Y_OFFSET - 1, 0xFFFFFFFF);
		OSScreenPutPixelEx(1, 95, VISUALISER_SCALE_Y_OFFSET - 1, 0xFFFFFFFF);
		OSScreenPutPixelEx(0, 95, VISUALISER_SCALE_Y_OFFSET - 2, 0xFFFFFFFF);
		OSScreenPutPixelEx(1, 95, VISUALISER_SCALE_Y_OFFSET - 2, 0xFFFFFFFF);
		OSScreenPutPixelEx(0, 95, VISUALISER_SCALE_Y_OFFSET + 1, 0xFFFFFFFF);
		OSScreenPutPixelEx(1, 95, VISUALISER_SCALE_Y_OFFSET + 1, 0xFFFFFFFF);
		OSScreenPutPixelEx(0, 95, VISUALISER_SCALE_Y_OFFSET + 2, 0xFFFFFFFF);
		OSScreenPutPixelEx(1, 95, VISUALISER_SCALE_Y_OFFSET + 2, 0xFFFFFFFF);
		
		OSScreenPutFontEx(0, 0, 12, "96 samples @ 48KHz = 2ms (graph vertically inverted)");
		OSScreenPutFontEx(1, 0, 12, "96 samples @ 48KHz = 2ms (graph vertically inverted)");
		
		if (voice1.stopped) {
			__os_snprintf(buf, 255, "Stopped.");
		} else {
			switch (currentWaveType) {
				case SINE_WAVE:
					__os_snprintf(buf, 255, "Playing a %fHz sine wave...", voice1.newFrequency);
					break;
				case SQUARE_WAVE:
					__os_snprintf(buf, 255, "Playing a %fHz square wave...", voice1.newFrequency);
					break;
				case SAWTOOTH_WAVE:
					__os_snprintf(buf, 255, "Playing a %fHz sawtooth wave...", voice1.newFrequency);
					break;
				case TRIANGLE_WAVE:
					__os_snprintf(buf, 255, "Playing a %fHz triangle wave...", voice1.newFrequency);
					break;
				default:
					__os_snprintf(buf, 255, "Playing a %fHz sine wave...", voice1.newFrequency);
					break;
			}
			
			int sampleCounter = 0;
			for (int x = 0; x < 854; x++) {
				OSScreenPutPixelEx(0, x, (int)((signed char)voice1.samples[sampleCounter] / 4) + VISUALISER_Y_OFFSET, 0xFFFFFFFF);
				OSScreenPutPixelEx(1, x, (int)((signed char)voice1.samples[sampleCounter] / 4) + VISUALISER_Y_OFFSET, 0xFFFFFFFF);
				sampleCounter++;
				if (sampleCounter == voice1.numSamples) {
					sampleCounter = 0;
				}
			}
		}
		
		OSScreenPutFontEx(0, 0, 6, buf);
		OSScreenPutFontEx(1, 0, 6, buf);
		
		OSScreenPutFontEx(0, 0, 17, "Z/X = Increase/decrease octave | HOME = Exit | C = Change wave");
		OSScreenPutFontEx(1, 0, 17, "Z/X = Increase/decrease octave | HOME = Exit | C = Change wave");
		
		DCFlushRange(screenBuffer, bufferSize);
		
		OSScreenFlipBuffersEx(0);
		OSScreenFlipBuffersEx(1);
		
		/*!
			Check user inputs
		*/
		
		VPADRead(0, &vpadData, 1, &vpadError);
		if (vpadError) {
			continue;
		} else if (vpadData.btns_h & VPAD_BUTTON_HOME) {
			break; //break to cleanup and exit
		} else if (vpadData.btns_h & VPAD_BUTTON_A) {
			OSScreenFlipBuffersEx(1);
		}
	} // go round again
	
	/*!
		Free memory, clean up and return to main.c's cleanup routine
	*/
	free(voice1.samples);
	KBDTeardown();
	AXRegisterFrameCallback((void*)0); //kinda important to do
	AXQuit();
}

int playingScancode = 0;
/*!
	Keyboard callback, interprets keypresses and requests new tones.
*/
void keyboardCallback(struct KBDKeyState* keyState) {
	//If key was pressed...
	if (keyState->state == KBD_KEY_PRESSED) {
		switch (keyState->scancode) {
			case 0x04: //A
				voice1.newFrequency = (float)(440 * exp((-9 + octave) * log(2)/12));
				break;
			case 0x1A: //W
				voice1.newFrequency = (float)(440 * exp((-8 + octave) * log(2)/12));
				break;
			case 0x16: //S
				voice1.newFrequency = (float)(440 * exp((-7 + octave) * log(2)/12));
				break;
			case 0x08: //E
				voice1.newFrequency = (float)(440 * exp((-6 + octave) * log(2)/12));
				break;
			case 0x07: //D
				voice1.newFrequency = (float)(440 * exp((-5 + octave) * log(2)/12));
				break;
			case 0x09: //F
				voice1.newFrequency = (float)(440 * exp((-4 + octave) * log(2)/12));
				break;
			case 0x17: //T
				voice1.newFrequency = (float)(440 * exp((-3 + octave) * log(2)/12));
				break;
			case 0x0A: //G
				voice1.newFrequency = (float)(440 * exp((-2 + octave) * log(2)/12));
				break;
			case 0x1C: //Y
				voice1.newFrequency = (float)(440 * exp((-1 + octave) * log(2)/12));
				break;
			case 0x0B: //H
				voice1.newFrequency = (float)(440 * exp((0 + octave) * log(2)/12));
				break;
			case 0x18: //U
				voice1.newFrequency = (float)(440 * exp((1 + octave) * log(2)/12));
				break;
			case 0x0D: //J
				voice1.newFrequency = (float)(440 * exp((2 + octave) * log(2)/12));
				break;
			case 0x0E: //K
				voice1.newFrequency = (float)(440 * exp((3 + octave) * log(2)/12));
				break;
			case 0x12: //O
				voice1.newFrequency = (float)(440 * exp((4 + octave) * log(2)/12));
				break;
			case 0x0F: //L
				voice1.newFrequency = (float)(440 * exp((5 + octave) * log(2)/12));
				break;
			case 0x13: //P
				voice1.newFrequency = (float)(440 * exp((6 + octave) * log(2)/12));
				break;
			case 0x33: //;
				voice1.newFrequency = (float)(440 * exp((7 + octave) * log(2)/12));
				break;
			case 0x34: //'
				voice1.newFrequency = (float)(440 * exp((8 + octave) * log(2)/12));
				break;
			case 0x28: //Enter, this is the wrong note (A above the octave)
				voice1.newFrequency = (float)(440 * exp((12 + octave) * log(2)/12));
				break;
			case 0x1D: //Z
				octave -= 12;
				return;
			case 0x1B: //X
				octave += 12;
				return;
			case 0x06: //C
				if (currentWaveType == 3) {
					currentWaveType = 0;
				} else {
					currentWaveType++;
				}
				return;
			default:
				return;
		}
		//update currently playing scancode
		playingScancode = keyState->scancode;
		
		//request new sound
		voice1.newSoundRequested = 1;
		//clear the modified flag
		voice1.modified = 0;
		
		//flush everything to memory
		DCFlushRange(&voice1.newFrequency, sizeof(voice1.newFrequency));
		DCFlushRange(&voice1.newSoundRequested, sizeof(voice1.newSoundRequested));
		DCFlushRange(&voice1, sizeof(voice1));
	//Otherwise, if the key was released...
	} else if (keyState->state == KBD_KEY_RELEASED) {
		//If the key released corresponds to the note we're playing now...
		if (keyState->scancode == playingScancode) {
			//Stop the voice
			voice1.stopRequested = 1;
			DCFlushRange(&voice1, sizeof(voice1));
			//Nothing is playing
			playingScancode = 0;
		}
	}
}

#define AMPLITUDE 100

//Returns number of actually made samples
unsigned int generateSquareWave(sample* samples, unsigned int maxLength, float freq) {
	int sinX = 0;
	float t = (M_PI * 2 * freq) / (48000); //freqHz sine @ 48KHz sample rate
	for (unsigned int i = 0; i < maxLength; i++) {
		if ((t * sinX) >= (2 * M_PI)) { //do the minimum number of samples
			return i - 1;
		}
		float sinResult = (float)(sin(t * sinX));
		if (sinResult >= 0) {
			samples[i] = (sample)AMPLITUDE;
		} else {
			samples[i] = (sample)-AMPLITUDE;
		}
		//samples[i] = (sample)(amplitude * sinResult);
		sinX++;
	}
	return maxLength;
}

unsigned int generateSineWave(sample* samples, unsigned int maxLength, float freq) {
	int sinX = 0;
	float t = (M_PI * 2 * freq) / (48000); //freqHz sine @ 48KHz sample rate
	for (unsigned int i = 0; i < maxLength; i++) {
		if ((t * sinX) >= (2 * M_PI)) { //do the minimum number of samples
			return i - 1;
		}
		float sinResult = (float)(sin(t * sinX));
		samples[i] = (sample)(AMPLITUDE * sinResult);
		sinX++;
	}
	return maxLength;
}

unsigned int generateSawtoothWave(sample* samples, unsigned int maxLength, float freq) {
	int wavelengthSamples = (int)(48000 / (freq)); //number of samples in one wavelength
	char step = (char)((AMPLITUDE * 2) / wavelengthSamples); //"step" in amplitude per sample
	char tempSample = (char)-AMPLITUDE;
	int i = 0;
	for (; i < wavelengthSamples; i++) {
		tempSample += step;
		samples[i] = (sample)tempSample;
	}
	return i;
}

//this generator needs some serious love
unsigned int generateTriangleWave(sample* samples, unsigned int maxLength, float freq) {
	int wavelengthSamples = (int)(48000 / (freq * 2)); //number of samples in one wavelength
	char step = (char)((AMPLITUDE * 2) / wavelengthSamples); //"step" in amplitude per sample
	char tempSample = (char)-AMPLITUDE;
	int i = 0;
	for (; i < wavelengthSamples; i++) {
		tempSample += step;
		samples[i] = (sample)tempSample;
	}
	i--;
	int ii = 0;
	for (; ii < wavelengthSamples; ii++) {
		samples[ii + i] = samples[i - ii];
	}
	return i + ii;
}

ax_buffer_t voiceBuffer;

void axFrameCallback() {
	if (voice1.stopRequested) {
		//Stop the voice
		AXSetVoiceState(voice1.voice, 0);
		//Clear the flag
		voice1.stopRequested = 0;
		voice1.stopped = 1;
		DCFlushRange(&voice1, sizeof(voice1));
	} else if (voice1.modified) {
		//does this really need to happen in the callback?
		DCInvalidateRange(&voice1, sizeof(voice1));
		memset(&voiceBuffer, 0, sizeof(voiceBuffer));
		voiceBuffer.samples = voice1.samples;
		voiceBuffer.format = 25; //almost definitely wrong
		voiceBuffer.loop = 1;
		voiceBuffer.cur_pos = 0;
		voiceBuffer.end_pos = voice1.numSamples - 1;
		voiceBuffer.loop_offset = 0;
	
		unsigned int ratioBits[4];
		ratioBits[0] = (unsigned int)(0x00010000 * ((float)48000 / (float)AXGetInputSamplesPerSec()));
		ratioBits[1] = 0;
		ratioBits[2] = 0;
		ratioBits[3] = 0;
	
		AXSetVoiceOffsets(voice1.voice, &voiceBuffer);
		AXSetVoiceSrc(voice1.voice, ratioBits);
		AXSetVoiceSrcType(voice1.voice, 1);
		AXSetVoiceState(voice1.voice, 1);
		voice1.modified = 0;
		voice1.stopped = 0;
		DCFlushRange(&voice1, sizeof(voice1));
	} 
}