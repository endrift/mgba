/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef GBA_AUDIO_H
#define GBA_AUDIO_H

#include <mgba-util/common.h>

CXX_GUARD_START

#include <mgba/core/cpu.h>
#include <mgba/core/log.h>
#include <mgba/internal/gb/audio.h>
#include <mgba-util/circle-buffer.h>

#define MKS4AGB_MAGIC 0x68736D53
#define MKS4AGB_MAX_SOUND_CHANNELS 12

mLOG_DECLARE_CATEGORY(GBA_AUDIO);

struct GBADMA;

extern const unsigned GBA_AUDIO_SAMPLES;
extern const int GBA_AUDIO_VOLUME_MAX;

struct GBAAudioFIFO {
	struct CircleBuffer fifo;
	int dmaSource;
	int8_t sample;
};

DECL_BITFIELD(GBARegisterSOUNDCNT_HI, uint16_t);
DECL_BITS(GBARegisterSOUNDCNT_HI, Volume, 0, 2);
DECL_BIT(GBARegisterSOUNDCNT_HI, VolumeChA, 2);
DECL_BIT(GBARegisterSOUNDCNT_HI, VolumeChB, 3);
DECL_BIT(GBARegisterSOUNDCNT_HI, ChARight, 8);
DECL_BIT(GBARegisterSOUNDCNT_HI, ChALeft, 9);
DECL_BIT(GBARegisterSOUNDCNT_HI, ChATimer, 10);
DECL_BIT(GBARegisterSOUNDCNT_HI, ChAReset, 11);
DECL_BIT(GBARegisterSOUNDCNT_HI, ChBRight, 12);
DECL_BIT(GBARegisterSOUNDCNT_HI, ChBLeft, 13);
DECL_BIT(GBARegisterSOUNDCNT_HI, ChBTimer, 14);
DECL_BIT(GBARegisterSOUNDCNT_HI, ChBReset, 15);

DECL_BITFIELD(GBARegisterSOUNDBIAS, uint16_t);
DECL_BITS(GBARegisterSOUNDBIAS, Bias, 0, 10);
DECL_BITS(GBARegisterSOUNDBIAS, Resolution, 14, 2);

struct GBAAudioMixer;
struct GBAAudio {
	struct GBA* p;

	struct GBAudio psg;
	struct GBAAudioFIFO chA;
	struct GBAAudioFIFO chB;

	int16_t lastLeft;
	int16_t lastRight;
	int clock;

	uint8_t volume;
	bool volumeChA;
	bool volumeChB;
	bool chARight;
	bool chALeft;
	bool chATimer;
	bool chBRight;
	bool chBLeft;
	bool chBTimer;
	bool enable;

	size_t samples;
	unsigned sampleRate;

	GBARegisterSOUNDBIAS soundbias;

	struct GBAAudioMixer* mixer;
	bool externalMixing;
	int32_t sampleInterval;

	bool forceDisableChA;
	bool forceDisableChB;
	int masterVolume;

	struct mTimingEvent sampleEvent;
};

struct GBAStereoSample {
	int16_t left;
	int16_t right;
};

struct GBAMKS4AGBADSR {
	uint8_t attack;
	uint8_t decay;
	uint8_t sustain;
	uint8_t release;
};

struct GBAMKS4AGBSoundChannel {
	uint8_t status;
	uint8_t type;
	uint8_t rightVolume;
	uint8_t leftVolume;
	struct GBAMKS4AGBADSR adsr;
	uint8_t ky;
	uint8_t envelopeV;
	uint8_t envelopeRight;
	uint8_t envelopeLeft;
	uint8_t echoVolume;
	uint8_t echoLength;
	uint8_t d1;
	uint8_t d2;
	uint8_t gt;
	uint8_t mk;
	uint8_t ve;
	uint8_t pr;
	uint8_t rp;
	uint8_t d3[3];
	uint32_t ct;
	uint32_t fw;
	uint32_t freq;
	uint32_t waveData;
	uint32_t cp;
	uint32_t track;
	uint32_t pp;
	uint32_t np;
	uint32_t d4;
	uint16_t xpi;
	uint16_t xpc;
};

struct GBAMKS4AGBContext {
	uint32_t magic;
	uint8_t pcmDmaCounter;
	uint8_t reverb;
	uint8_t maxChans;
	uint8_t masterVolume;
	uint8_t freq;
	uint8_t mode;
	uint8_t c15;
	uint8_t pcmDmaPeriod;
	uint8_t maxLines;
	uint8_t gap[3];
	int32_t pcmSamplesPerVBlank;
	int32_t pcmFreq;
	int32_t divFreq;
	uint32_t cgbChans;
	uint32_t func;
	uint32_t intp;
	uint32_t cgbSound;
	uint32_t cgbOscOff;
	uint32_t midiKeyToCgbFreq;
	uint32_t mPlayJumpTable;
	uint32_t plynote;
	uint32_t extVolPit;
	uint8_t gap2[16];
	struct GBAMKS4AGBSoundChannel chans[MKS4AGB_MAX_SOUND_CHANNELS];
};

struct GBAMKS4AGBMusicPlayerInfo {
	uint32_t songHeader;
	uint32_t status;
	uint8_t trackCount;
	uint8_t priority;
	uint8_t cmd;
	uint8_t unk_B;
	uint32_t clock;
	uint8_t gap[8];
	uint32_t memAccArea;
	uint16_t tempoD;
	uint16_t tempoU;
	uint16_t tempoI;
	uint16_t tempoC;
	uint16_t fadeOI;
	uint16_t fadeOC;
	uint16_t fadeOV;
	uint32_t tracks;
	uint32_t tone;
	uint32_t magic;
	uint32_t func;
	uint32_t intp;
};

struct GBAMKS4AGBInstrument {
	uint8_t type;
	uint8_t key;
	uint8_t length;
	union {
		uint8_t pan;
		uint8_t sweep;
	};
	union {
		uint32_t waveData;
		uint32_t subTable;
	};
	union {
		struct GBAMKS4AGBADSR adsr;
		uint32_t map;
	};
};

struct GBAMKS4AGBMusicPlayerTrack {
	uint8_t flags;
	uint8_t wait;
	uint8_t patternLevel;
	uint8_t repN;
	uint8_t gateTime;
	uint8_t key;
	uint8_t velocity;
	uint8_t runningStatus;
	uint8_t keyM;
	uint8_t pitM;
	int8_t keyShift;
	int8_t keyShiftX;
	int8_t tune;
	uint8_t pitX;
	int8_t bend;
	uint8_t bendRange;
	uint8_t volMR;
	uint8_t volML;
	uint8_t vol;
	uint8_t volX;
	int8_t pan;
	int8_t panX;
	int8_t modM;
	uint8_t mod;
	uint8_t modT;
	uint8_t lfoSpeed;
	uint8_t lfoSpeedC;
	uint8_t lfoDelay;
	uint8_t lfoDelayC;
	uint8_t priority;
	uint8_t echoVolume;
	uint8_t echoLength;
	uint32_t chan;
	struct GBAMKS4AGBInstrument instrument;
	uint8_t gap[10];
	uint16_t unk_3A;
	uint32_t unk_3C;
	uint32_t cmdPtr;
	uint32_t patternStack[3];
};

struct GBAMKS4AGBTrack {
	struct GBAMKS4AGBMusicPlayerTrack track;
	uint8_t lastCommand;
};

struct GBAAudioMixer {
	struct mCPUComponent d;
	struct GBAAudio* p;

	uint32_t contextAddress;

	bool (*engage)(struct GBAAudioMixer* mixer, uint32_t address);
	void (*vblank)(struct GBAAudioMixer* mixer);

	struct mTimingEvent stepEvent;

	struct GBAMKS4AGBContext context;
	struct GBAMKS4AGBMusicPlayerInfo player;
	struct GBAMKS4AGBTrack activeTracks[MKS4AGB_MAX_SOUND_CHANNELS];
};

void GBAAudioInit(struct GBAAudio* audio, size_t samples);
void GBAAudioReset(struct GBAAudio* audio);
void GBAAudioDeinit(struct GBAAudio* audio);

void GBAAudioResizeBuffer(struct GBAAudio* audio, size_t samples);

void GBAAudioScheduleFifoDma(struct GBAAudio* audio, int number, struct GBADMA* info);

void GBAAudioWriteSOUND1CNT_LO(struct GBAAudio* audio, uint16_t value);
void GBAAudioWriteSOUND1CNT_HI(struct GBAAudio* audio, uint16_t value);
void GBAAudioWriteSOUND1CNT_X(struct GBAAudio* audio, uint16_t value);
void GBAAudioWriteSOUND2CNT_LO(struct GBAAudio* audio, uint16_t value);
void GBAAudioWriteSOUND2CNT_HI(struct GBAAudio* audio, uint16_t value);
void GBAAudioWriteSOUND3CNT_LO(struct GBAAudio* audio, uint16_t value);
void GBAAudioWriteSOUND3CNT_HI(struct GBAAudio* audio, uint16_t value);
void GBAAudioWriteSOUND3CNT_X(struct GBAAudio* audio, uint16_t value);
void GBAAudioWriteSOUND4CNT_LO(struct GBAAudio* audio, uint16_t value);
void GBAAudioWriteSOUND4CNT_HI(struct GBAAudio* audio, uint16_t value);
void GBAAudioWriteSOUNDCNT_LO(struct GBAAudio* audio, uint16_t value);
void GBAAudioWriteSOUNDCNT_HI(struct GBAAudio* audio, uint16_t value);
void GBAAudioWriteSOUNDCNT_X(struct GBAAudio* audio, uint16_t value);
void GBAAudioWriteSOUNDBIAS(struct GBAAudio* audio, uint16_t value);

void GBAAudioWriteWaveRAM(struct GBAAudio* audio, int address, uint32_t value);
void GBAAudioWriteFIFO(struct GBAAudio* audio, int address, uint32_t value);
void GBAAudioSampleFIFO(struct GBAAudio* audio, int fifoId, int32_t cycles);

struct GBASerializedState;
void GBAAudioSerialize(const struct GBAAudio* audio, struct GBASerializedState* state);
void GBAAudioDeserialize(struct GBAAudio* audio, const struct GBASerializedState* state);

float GBAAudioCalculateRatio(float inputSampleRate, float desiredFPS, float desiredSampleRatio);

CXX_GUARD_END

#endif
