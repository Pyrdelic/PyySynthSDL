#define PROGRAM_COUNT 2
#define TRACKER_FRAMES_TO_STEP 60
#define TRACKER_EFFECTS_COUNT 2

#define FPS 120
#define FRAME_TARGET_TIME 1000 / FPS

#define TRACK_COUNT 16

//#define M_PI 3.141592653589793

#define SAMPLE_RATE 44100
#define AMPLITUDE 28000
#define FREQUENCY 440
#define BUFFER_SIZE 256
#define FREQ_A4 440.0
#define DISPLAY_BUFFER_SIZE 320

// where to clip before hitting sample upper and lower limits.
#define CLIP_EDGE 100

#define MAX_VOICES 16
#define MAX_INSTRUMENTS 16
#define N_INSTRUMENTS 16
#define N_VOICES 16

#define RB_SIZE 512

#define NOTE_ON 0x90
#define NOTE_OFF 0x80

#include <stdio.h>
#include <math.h>
#include <limits.h>
#include <stdint.h>

// for rand()
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <stdint.h>

#include <SDL.h>
#include <glad/glad.h>
//#include "constants.h";
#include "PortMIDI/portmidi.h";

//#include "pv_graphics.h";
//#include "pv_tracker.h";
#include "pv_synth.h";
#include "visbuf.h";

int gRunning = 0;
SDL_Window* gWindow = NULL;
SDL_GLContext gOpenGLContext = NULL;

SDL_Renderer* gSDLRenderer = NULL;
//SDL_Texture* gTexture = NULL;
//GLuint gOpenGLTexture = NULL;

int gScreenWidth = 640; // TODO: get rid of
int gScreenHeight = 480; // TODO: get rid of

int gLastFrameTime = 0;

int gDebug = 1;
int gPause = 0;

int gShaderSelection = 0;

int gIgnorePauses = 0;

// vvvvvvvvvvvvvvvvv Tracker vvvvvvvvvvvvv
int gPlayhead = 0;
int gFrameCount = 0;
long gTrackerSixteenth;
// ^^^^^^^^^^^^^^^^^^ Tracker ^^^^^^^^^^^^^

// vvvvvvvvvvvvvvvvvv OpenGL vvvvvvvvvvvvvvvvvvv

// OpenGL Globals

int gOpenGLDebugFlag = 1;


// Uniforms
float u_Offset = 0.0f;

// todo: array implementation

// vvvvvvvvvvv Function Declarations vvvvvvvvvvvvvvvvvvvvvvv
void processInput();

void pause() {
	if (!gIgnorePauses)
	{
		int pause = 1;
		SDL_Event e;
		printf("Paused\n");
		while (pause) {
			SDL_PollEvent(&e);
			if (e.type == SDL_KEYDOWN) {
				switch (e.key.keysym.sym) {
				case SDLK_SPACE:
					pause = 0;
					break;
				case SDLK_ESCAPE:
					pause = 0;
					gIgnorePauses = 1;
					printf("Ingoring further pauses\n");
				}
			}
		}
		printf("Continue\n");
	}
}


int random_number(int min, int max)
{
	// only seed once
	static int seeded = 0;
	if (seeded)
	{
		srand(time(NULL));
		seeded = 1;
	}
	return (rand() % (max - min + 1)) + min;
}

int initializeWindowSDL() {
	if (SDL_Init(SDL_INIT_EVERYTHING)) {
		fprintf(stderr, "Error initializing SDL.\n");
		return -1;
	}
	gWindow = SDL_CreateWindow(
		"P SYNTH",
		SDL_WINDOWPOS_CENTERED,
		SDL_WINDOWPOS_CENTERED,
		gScreenWidth,
		gScreenHeight,
		SDL_WINDOW_BORDERLESS
	);
	if (!gWindow)
	{
		fprintf(stderr, "Error creating SDL Window.\n");
		return -1;
	}
	return 0;
}



void frameClock() 
{
	// Sleep the execution until we reach the target frame time in milliseconds
	Uint32 ticks = SDL_GetTicks();
	int time_to_wait = FRAME_TARGET_TIME - (ticks - gLastFrameTime);
	// Only call delay if we are too fast to process this frame
	if (time_to_wait > 0 && time_to_wait <= FRAME_TARGET_TIME)
	{
		SDL_Delay(time_to_wait);
	}
	
	// Get a delta time factor converted to seconds to be used to update my objects
	float delta_time = (ticks - gLastFrameTime) / 1000.0f;
	gLastFrameTime = ticks;

	if (gPause) {
		return;
	}
}
void destroy_window()
{
	SDL_DestroyWindow(gWindow);
	//SDL_DestroyWindow(gOffScreenWindow);

	SDL_Quit();
}

void processInput()
{
	SDL_Event event;
	SDL_PollEvent(&event);

	if (event.type == SDL_WINDOWEVENT &&
		event.window.event == SDL_WINDOWEVENT_CLOSE) {
		gRunning = 0;
	}

	switch (event.type)
	{
	case SDL_QUIT:
		gRunning = 0;
		break;
	case SDL_KEYDOWN:
		if (event.key.keysym.sym == SDLK_SPACE) {
			gDebug = 0;
			gPause = 0;
		}

		if (event.key.keysym.sym == SDLK_s) {
			// loop through programs
			gShaderSelection++;
			if (gShaderSelection >= PROGRAM_COUNT) {
				gShaderSelection = 0;
			}
		}

		if (event.key.keysym.sym == SDLK_ESCAPE) {
			gRunning = 0;
			break;
		}
		break;
	}
	const Uint8* state = SDL_GetKeyboardState(NULL);
	if (state[SDL_SCANCODE_UP]) {
		u_Offset += 0.01f;
		printf("u_Offset: %.2f\n", u_Offset);
	}
	if (state[SDL_SCANCODE_DOWN]) {
		u_Offset -= 0.01f;
		printf("u_Offset: %.2f\n", u_Offset);
	}
}

void instrumentDebugPrint(Instrument* instrument) {

}

// Processes an envelope for the length of one sample.
void processEnvelope(Envelope* env) {
	switch (env->state) {
		case ATTACK:
			//printf("Attack: %f\n", voice->ampEnv.level);
			env->level += (env->attackLevel - 0.0)
				/ (SAMPLE_RATE * env->attackTime);
			if (env->level >= env->attackLevel) {
				env->level = env->attackLevel;
				env->state = DECAY;
				//printf("Switch to DECAY at %d\n", SDL_GetTicks());
			}

			break;
		case DECAY:
			// LINEAR DECAY
			//voice->ampEnv.level -=
			//	(voice->ampEnv.attackLevel - voice->ampEnv.sustainLevel)
			//	/ (SAMPLE_RATE * voice->ampEnv.decayTime);
			// LOGARITHMIC DECAY
			env->level *= env->decayFactor;
			if (env->level <= env->sustainLevel + 0.01) {
				env->level = env->sustainLevel;
				env->state = SUSTAIN;
				//printf("Switch to SUSTAIN at %d\n", SDL_GetTicks());
			}
			//printf("Decay: %f\n", voice->ampEnv.level);
			break;
		case SUSTAIN:
			//printf("SUSTAIN\t");
			// keep sustain level
			env->level = env->sustainLevel;
			break;
		case RELEASE:
			// LOGARITHMIC RELEASE 
			//env->level *= env->releaseFactor;
			//if (env->level <= 0.01)
			
			// LINEAR RELEASE
			/*env->level -= (env->sustainLevel - 0.0) / (SAMPLE_RATE * env->releaseTime);
			if (env->level <= 0.0) {*/

			// LOGARITHMIC RELEASE 
			env->level *= env->releaseFactor;
			if (env->level <= 0.01){
				env->level = 0.0;
				env->state = OFF;
			}
			break;
		case OFF:
			// don't do anything
			break;
	}
}


// Fills an audio buffer. Called every time audio device needs more data.
void audioCallbackV4(void* userdata, Uint8* stream, int len) {
	static int debugOnce = 1;
	AudioDataV2** ad = (AudioDataV2*)userdata; // Cast userdata to AudioData
	//printf("ADC\n");
	//double phase = audioData->phase;
	//double frequency = audioData->frequency;

	Sint16* buffer = (Sint16*)stream;


	int length = len / 2; // because buffer is in 16-bit samples

	// Clear the buffer
	for (int i = 0; i < length; i++) {
		buffer[i] = 0;
	}
	//printf("Active voices: ");

	// test stuff
	if (1) {
		if (debugOnce) {
			printf("AudioCallback:\n");
			/*printf("ADC PTR: %p\n", *ad);
			printf("ADC INSTRBLOCK: %p", &(*ad)->instruments);
			printf("ADC INSTR0: %p\n", &(*ad)->instruments[0]);
			debugOnce = 0;*/
			pv_synth_print_audiodata((*ad));
			debugOnce = 0;
		}
	}
	

	// loop through every sample
	for (int i = 0; i < length; i++) {
		long sample = 0;


		// loop through instruments (midi channels)
		for (int i_instruments = 0; i_instruments < (*ad)->n_instruments; i_instruments++)
		{
			//printf("instrumentloop\n");
			Instrument* instrument = &(*ad)->instruments[i_instruments];

			// loop through instrument's voices
			for (int i_voices = 0; i_voices < instrument->n_voices; i_voices++) 
			{
				Voice* voice = &instrument->voices[i_voices];

				if (voice->ampEnv.state == OFF) 
				{
					//printf("X");
					continue;
				}

				double phase = voice->phase;
				double frequency = voice->frequency;

				processEnvelope(&voice->ampEnv);

				phase += 2.0 * M_PI * frequency / SAMPLE_RATE;


				// limiter/clipper (prevent integer overflow)
				//long sample = (long)buffer[i];
				sample += (Sint16)((AMPLITUDE * voice->ampEnv.level * (*ad)->masterGain * instrument->instrumentGain) * sin(phase));
				if (sample > INT16_MAX - CLIP_EDGE) {
					sample = (long)INT16_MAX - CLIP_EDGE;
				}
				else if (sample < INT16_MIN + CLIP_EDGE) {
					sample = (long)INT16_MIN + CLIP_EDGE;
				}
				// update phase in audiodata
				voice->phase = phase;
			}



			//printf("\n");
		

			//printf("Envelope level at %f\n", audioData->AmpEnvelope->envelopeLevel);
		}

		
		buffer[i] = (Sint16)sample;
		visbuf_write((Sint16)sample);

		// kirjoita sample myös visbufferiin


	}


}


PortMidiStream* openMIDIStream() {
	PmError err = Pm_Initialize();
	if (err != pmNoError) {
		printf("PortMidi initialization error: %s\n", Pm_GetErrorText(err));
		// TODO: Handle error
		return 1;
	}

	int numDevices = Pm_CountDevices();
	const PmDeviceInfo* info;
	int inputDevice = -1;

	printf("Input Device Count: %d\n", numDevices);
	for (int i = 0; i < numDevices; i++) {
		info = Pm_GetDeviceInfo(i);
		if (info->input) {
			printf("Input device %d: %s, %s\n", i, info->interf, info->name);

		}
	}

	// MIDI device selection
	int midiDeviceSelection;
	if (1) {
		midiDeviceSelection = 2;
		printf("Autoselected MIDI device %d\n", midiDeviceSelection);
	}
	else {
		printf("Select a MIDI device: ");
		scanf_s("%d", &midiDeviceSelection, 1);
		printf("\n");
	}



	printf("Opening MIDI-stream on device %d\n", midiDeviceSelection);
	PortMidiStream* midiStream;
	err = Pm_OpenInput(&midiStream, midiDeviceSelection, NULL, 512, NULL, NULL);
	if (err != pmNoError) {
		printf("Error opening MIDI input: %s\n", Pm_GetErrorText(err));
		Pm_Terminate();

		//TODO: handle error
		//return 1;
	}
	return midiStream;
}


void drawSDL(SDL_Rect* vispoints, Sint16* displayBuffer) {

	int screen_center_y = gScreenHeight / 2;
	// clear screen (draw background)
	SDL_SetRenderDrawColor(gSDLRenderer, 0, 0, 0, 255);
	SDL_RenderClear(gSDLRenderer);

	for (int i = DISPLAY_BUFFER_SIZE; i >= 0; i--) {
		vispoints[i].y = (int)((double)screen_center_y / (double)AMPLITUDE * (double)displayBuffer[i]) + screen_center_y-vispoints[i].h/2;

		SDL_SetRenderDrawColor(gSDLRenderer, i*10, 255, 255, 255);
		SDL_RenderFillRect(gSDLRenderer, &vispoints[i]);

	}
	//printf("y: %d\n", vispoints[0].y);

	SDL_RenderPresent(gSDLRenderer);
}



// ringbuffer for internal midi messages
uint32_t internalMessages[UCHAR_MAX] = { 0 };
unsigned char im_readHead = 0;
unsigned char im_writeHead = 0;


// activate instruments based on internalMessages
void processMidi(uint32_t* internalMessages, AudioDataV2** audioData) {
	static debugOnce = 1;
	uint32_t msg = 0;
	if (debugOnce) {
		printf("Processmidi:\n");
		pv_synth_print_audiodata((*audioData));
		debugOnce = 0;
	}

	while (!readInternalMessage(internalMessages, &msg)) {
		unsigned char command = Pm_MessageStatus(msg) & 0xF0;
		//printf("processMidi - Command: %u\n", (uint32_t)command);
		unsigned char channel = Pm_MessageStatus(msg) & 0x0F;
		unsigned char data1 = Pm_MessageData1(msg);
		unsigned char data2 = Pm_MessageData2(msg);
		printf("processMidi: %u\t%u\t%u\n", Pm_MessageStatus(msg), data1, data2);
		// note on
		if (command == NOTE_ON && data2 > 0) {
			// loop through voices, set first envelope with state OFF to ATTACK
			//printf("AudioData PTR: %u\n", audioData);
			for (int i = 0; i < (*audioData)->instruments[channel].n_voices; i++) {
				if ((*audioData)->instruments[channel].voices[i].ampEnv.state != OFF) {
					continue; // skip already active voices
				}
				(*audioData)->instruments[channel].voices[i].note = data1;
				(*audioData)->instruments[channel].voices[i].frequency = FREQ_A4 * pow(2.0, ((double)data1 - 69.0) / 12.0);
				(*audioData)->instruments[channel].voices[i].ampEnv.state = ATTACK;
				(*audioData)->instruments[channel].voices[i].pitchEnv.state = ATTACK;
				break; // we've activated an unactivated voice, all done
			}
			printf("\n");
		}
		else if (command == NOTE_OFF || data2 == 0) {
			// loop to voice that has the corresponding note, set amp envelope to RELEASE
			for (int i = 0; i < MAX_VOICES; i++) {
				if (data1 == (*audioData)->instruments[channel].voices[i].note) {
					(*audioData)->instruments[channel].voices[i].note = 0;
					(*audioData)->instruments[channel].voices[i].ampEnv.state = RELEASE;
					(*audioData)->instruments[channel].voices[i].pitchEnv.state = RELEASE;
				}
			}
		}
	}
}

void pollExternalMidiInput(PortMidiStream* stream, uint32_t* internalMessages) {
	//printf("Polling midi stream: %d\n", stream);
	PmEvent event;
	if (Pm_Poll(stream)) {
		int numEvents = Pm_Read(stream, &event, 1);
		//printf("numEvents: %u\n", numEvents);
		if (numEvents > 0) {
			uint32_t msg = event.message;
			//printf("PollExternal msg: %u\n", msg);
			//printf("extMIDI: %u\n", msg);
			//printf("pollExternal: %u\t%u\t%u\n", Pm_MessageStatus(msg), Pm_MessageData1(msg), Pm_MessageData2(msg));
			writeInternalMessage(internalMessages, msg);
		}
	}
}

// returns 0 if write fails
int writeInternalMessage(uint32_t* internalMessages, uint32_t internalMessage) {
	// is the buffer full?
	if ((unsigned char)(im_writeHead + 1) == im_readHead) {
		return PV_RET_FAILURE;
	}
	internalMessages[im_writeHead] = internalMessage;
	//printf("IM - Message written to slot: %u \n", im_writeHead);
	im_writeHead++;
	return PV_RET_SUCCESS;
}
// returns 0 if read fails without modifying target
int readInternalMessage(uint32_t* internalMessages, uint32_t* target) {
	if (im_readHead == im_writeHead) { // is the buffer empty?
		return 1;
	}
	*target = internalMessages[im_readHead];
	//printf("IM - Message read from slot: %u\n", im_readHead);
	im_readHead++;
	return 0;
}

void asdf(int* arr) {
	printf("asdf &array[0] %p\n", arr);
}

int main(int argc, char* argv[]){

	// audioData test
	if (0) {
		AudioDataV2* adTest = NULL;
		pv_synth_init(&adTest, AMPLITUDE, 1.0, 2, 2);
		pv_synth_print_audiodata(adTest);
		pv_synth_destroy(adTest);

		int arr[3] = { 1, 2, 3 };

		printf("\n");
		printf("array: %p\n", (void*)arr);
		printf("&array[0]: %p\n", (void*)&arr[0]);
		asdf(&arr);
		return EXIT_SUCCESS;
	}


	// SDL initialization
	if (initializeWindowSDL() == 0) {
		gRunning = 1;
	}
	else {
		printf("Failed to initialize SDL Window\n");
	}
	gSDLRenderer = SDL_CreateRenderer(gWindow, -1, SDL_RENDERER_ACCELERATED);
	if (!gSDLRenderer)
	{
		fprintf(stderr, "Error creating SDL Renderer.\n");
		gRunning = 0;
	}


	// Init synthesizer (AudioData)
	AudioDataV2* ad = NULL;
	pv_synth_init(&ad, AMPLITUDE, 1.0,  N_INSTRUMENTS, N_VOICES);
	printf("MAIN PTR: %p\n", ad);
	if (!ad) {
		printf("Could not initialize audiodata\n");
		gRunning = 0;
	}
	pv_synth_print_audiodata(ad);

	if (ad) {
		printf("MAIN INSTR0: %p\n", &ad->instruments[0]);
	}
	

	// init visualizer
	int vp_height = 20;
	int vp_width = gScreenWidth / DISPLAY_BUFFER_SIZE;
	SDL_Rect vispoints[DISPLAY_BUFFER_SIZE];
	for (int i = 0; i < DISPLAY_BUFFER_SIZE; i++) {
		vispoints[i].h = vp_height;
		vispoints[i].w = vp_width;
		vispoints[i].x = vp_width * i;
		vispoints[i].y = gScreenHeight / 2 - vp_height / 2;
	}
	// Values for the visualizer rects.
	Sint16 displayBuffer[DISPLAY_BUFFER_SIZE] = { 0 };

	// Init MIDI (External interfaces)
	PortMidiStream* pMidiStream;
	pMidiStream = openMIDIStream();
	printf("main midi stream: %u\n", pMidiStream);

	// SDL Audio device specification
	SDL_AudioSpec audioSpec;
	audioSpec.freq = SAMPLE_RATE;
	audioSpec.format = AUDIO_S16SYS;
	audioSpec.channels = 1;
	audioSpec.samples = BUFFER_SIZE;
	audioSpec.callback = audioCallbackV4;
	audioSpec.userdata = &ad;	// BIND SYNTH (AudioData) TO AUDIO LOOPBACK


	// Open audio device
	if (SDL_OpenAudio(&audioSpec, NULL) < 0) {
		fprintf(stderr, "Could not open audio: %s\n", SDL_GetError());
	}

	long frameCount = 0;

	// Start audio device
	SDL_PauseAudioDevice(1, 0);

	while (gRunning) // MAIN LOOP, EVERY VISUAL FRAME
	{
		processInput();
			


		if (gPause) { continue; }

		pollExternalMidiInput(pMidiStream, &internalMessages);
		processMidi(&internalMessages, &ad);

		drawSDL(vispoints, visbuf);

		SDL_GL_SwapWindow(gWindow);
		frameClock();
		gFrameCount++;
		frameCount++;

	}
	SDL_PauseAudioDevice(1, 1);
	Pm_Close(pMidiStream);
	Pm_Terminate();
	SDL_CloseAudioDevice(1);
	destroy_window();
	pv_synth_destroy(ad);
	return 0;
}