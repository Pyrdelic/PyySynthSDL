#pragma once

#ifndef MAX_VOICES
#define MAX VOICES 16
#endif
#ifndef MAX_INSTRUMENTS
#define MAX_INSTRUMENTS 16
#endif
#ifndef SAMPLE_RATE
#define SAMPLE_RATE 44100
#endif

typedef enum ADSRState{
	OFF,
	ATTACK,
	DECAY,
	SUSTAIN,
	RELEASE
} ADSRState;

// TODO: implement
typedef enum Waveform {
	SINE,
	SQUARE,
	SAW
}Waveform;

// TODO: implement / use
typedef enum InstrumentPreset {
	INSTRUMENT_KICK,
	INSTRUMENT_ORGAN
}InstrumentPreset;

typedef struct Envelope{
	double level;			// current level of the envelope
	double attackTime;		// attack time (seconds)
	double attackLevel;		// target level of attack (0.0 - 1.0)
	double decayTime;		// decay time (seconds)
	double sustainLevel;	// target level of decay (0.0 - 1.0)
	double releaseTime;		// release time (seconds)
	ADSRState state;		// state of the envelope (ADSR)

	double decayFactor;
	double releaseFactor;
} Envelope;

typedef struct Voice {
	double phase;
	double frequency;
	int note;
	Envelope ampEnv;
	Envelope pitchEnv;
	double pitchOffset;		// multiplier (range) of pitch envelope
}Voice;

typedef enum SynthReturnValue {
	PV_RET_SUCCESS = 0,
	PV_RET_FAILURE,
}SynthReturnValue;

typedef struct Instrument {
	double instrumentGain;
	int MIDIChannel;
	//Voice voices[MAX_VOICES]; // TODO: deprecate
	int n_voices;
	Voice* voices;
}Instrument;

typedef struct AudioDataV2 {
	double amplitude;
	//double commonPhase;
	double masterGain;
	//Voice voices[MAX_VOICES];
	int n_instruments;
	Instrument* instruments;
	//RingBuffer* audioRB;
}AudioDataV2;


void pv_initEnvelope(	Envelope* envelope,
						double attackTime,
						double attackLevel,
						double decayTime,
						double sustainLevel,
						double releaseTime) {
	envelope->level = 0.0;
	envelope->attackTime = attackTime;
	envelope->attackLevel = attackLevel;
	envelope->decayTime = decayTime;
	envelope->sustainLevel = sustainLevel;
	envelope->releaseTime = releaseTime;
	envelope->state = OFF;
	envelope->decayFactor = pow(envelope->sustainLevel, 1.0 / (envelope->decayTime * SAMPLE_RATE));
	envelope->releaseFactor = pow(0.01, 1.0 / (envelope->releaseTime * SAMPLE_RATE));
}

void pv_initVoice(Voice* voice, InstrumentPreset preset) {
	//printf("initVoice: %u\n", voice);
	voice->phase = 0.0;		// will be overwritten
	voice->frequency = 0.0;	// will be overwritten
	voice->note = 0;		// will be overwritten


	switch (preset) {
	case INSTRUMENT_KICK:
		voice->pitchOffset = 10;
		pv_initEnvelope(&voice->ampEnv, 0.01, 1.0, 0.5, 0.1, 0.05);
		pv_initEnvelope(&voice->pitchEnv, 0.01, 1.0, 0.2, 0.0, 0.0);
		break;
	case INSTRUMENT_ORGAN:
		voice->pitchOffset = 0.0;
		pv_initEnvelope(&voice->ampEnv, 0.1, 1.0, 0.1, 0.6, 0.1);
		pv_initEnvelope(&voice->pitchEnv, 0, 0, 0, 0, 0, 0, 0);
	default:
		// INSTRUMENT_ORGAN
		voice->pitchOffset = 0.0;
		pv_initEnvelope(&voice->ampEnv, 0.1, 1.0, 0.1, 0.6, 0.1);
		pv_initEnvelope(&voice->pitchEnv, 0, 0, 0, 0, 0, 0, 0);
		break;
	}
}


void pv_synth_initInstrument(Instrument* instrument, int MIDIChannel, uint32_t n_voices, InstrumentPreset preset) {
	instrument->instrumentGain = 0.3;
	instrument->MIDIChannel = MIDIChannel;
	for (int i = 0; i < n_voices; i++) {
		//printf("\tSetting up voice %u\n", i);
		pv_initVoice(&instrument->voices[i], preset);
	}
}



// Initializes the synth struct to be used with in an audio loobpack function.
// Succesfully initialized struct must be destroyed using synth_destroy().
// Returns PV_RET_SUCCESS (0) on success.
// Returns PV_RET_FAILURE (1) on failure, destroys incomplete struct.
int pv_synth_init(AudioDataV2** ad, double amplitude, double masterGain, uint32_t n_instruments, uint32_t n_voices) {
	// allocate base struct
	*ad = malloc(sizeof(AudioDataV2));
	if (!*ad) {
		return PV_RET_FAILURE;
	}

	(*ad)->amplitude = amplitude;
	(*ad)->masterGain = masterGain;

	//printf("BASE ALLOCATED at\t%p\n", (*ad));
	// allocate instruments to base struct
	(*ad)->instruments = malloc(sizeof(Instrument) * n_instruments);
	if (!(*ad)->instruments) {
		free(ad);
		return PV_RET_FAILURE;
	}
	(*ad)->n_instruments = n_instruments;
	//printf("INSTRUMENTS (%u) ALLOCATED starting at\t%p\n", (*ad)->n_instruments, &(*ad)->instruments);
	// for each instrument, allocate voices
	for (int i = 0; i < n_instruments; i++) {
		(*ad)->instruments[i].voices = malloc(sizeof(Voice) * n_voices);
		if (!(*ad)->instruments[i].voices) {
			free((*ad)->instruments);
			free(ad);
			return PV_RET_FAILURE;
		}
		(*ad)->instruments[i].n_voices = n_voices;
		// memory allocated, set up voices
	}
	//printf("VOICES ALLOCATED\n");

	// memory allocated, set up instruments
	for (int i = 0; i < n_instruments; i++) {
		//printf("Setting up instrument %u\n", i);
		pv_synth_initInstrument(&(*ad)->instruments[i], i, n_voices, INSTRUMENT_ORGAN);
	}
	//printf("ALLOCATION COMPLETE!!!\n");
	//printf("PTR: %u\n", *ad);
	return PV_RET_SUCCESS;
	
}

// Destroys a synth struct.
void pv_synth_destroy(AudioDataV2* ad) {
	printf("DESTROY PTR: %u\n", ad);
	if (!ad) {
		printf("pv_synth_destroy: !ad\n");
		return;
	}
	if (!ad->instruments) {
		printf("pv_synth_destroy: !ad->instruments\n");
		free(ad);
		return;
	}
	for (int i = 0; i < ad->n_instruments; i++) {
		if (!ad->instruments[i].voices) {
			continue;
		}
		free(ad->instruments[i].voices);
	}
	free(ad->instruments);
	free(ad);
	printf("SYNTH DESTROYED\n");
}

void pv_synth_print_audiodata(AudioDataV2* ad) {
	if (!ad) {
		printf("Audiodata not initialized\n");
		return;
	}
	printf("Audiodata: %p\n", ad);
	if (!ad->instruments) {
		printf("Instruments not initialized\n");
		return;
	}
	printf("INSTRUMENTS BLOCK AT\t%p\n", &ad->instruments);
	for (int i = 0; i < ad->n_instruments; i++) {
		printf("Instrument %u\t%p:\n", i, &ad->instruments[i]);
		if (!ad->instruments[i].voices) {
			printf("\tVoices not intialized\n");
			continue;
		}
		for (int j = 0; j < ad->n_instruments; j++) {
			printf("\t\tVoice ampEnv decayTime: %f\n", ad->instruments[i].voices[j].ampEnv.decayTime);
		}
	}
}