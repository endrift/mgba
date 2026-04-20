/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef M_CORE_SYNC_H
#define M_CORE_SYNC_H

#include <mgba-util/common.h>

CXX_GUARD_START

#include <mgba-util/threading.h>

struct mCoreSync {
	Mutex mutex;
	Condition readyCond;
	bool interrupt;
	bool busy;
	bool active;

	bool audioSync;
	bool videoSync;

	int videoFramePending;
	Condition videoFrameAvailableCond;

	size_t audioHighWater;
	const struct mAudioBuffer* audioBuffer;

	bool clockSync;
	int32_t lastCycles;
	double clockOffset;
#ifdef _WIN32
	uint64_t currentClock;
	double qpcFreq;
#else
	uint64_t currentClock;
#endif

	int32_t coreFrequency;
	float systemFps;
	float fpsTarget;
	float speedMultiplier;
	float clockRatio;
};

void mCoreSyncInit(struct mCoreSync* sync);
void mCoreSyncDeinit(struct mCoreSync* sync);

void mCoreSyncSetFpsTarget(struct mCoreSync* sync, float fpsTarget);
void mCoreSyncSetActive(struct mCoreSync* sync, bool active);
void mCoreSyncSetSpeedMultiplier(struct mCoreSync* sync, float speedMultiplier);

struct mCoreOptions;
void mCoreSyncLoadCoreOpts(struct mCoreSync* sync, const struct mCoreOptions* opts);

struct mCore;
void mCoreSyncSetCoreParams(struct mCoreSync* sync, struct mCore* core);
void mCoreSyncSetFpsTarget(struct mCoreSync* sync, float fpsTarget);

void mCoreSyncLock(struct mCoreSync* sync);
void mCoreSyncUnlock(struct mCoreSync* sync);
bool mCoreSyncWait(struct mCoreSync* sync, int32_t currentCycles, int timeoutMs);

void mCoreSyncInterrupt(struct mCoreSync* sync);
void mCoreSyncResume(struct mCoreSync* sync);

void mCoreSyncPostFrame(struct mCoreSync* sync);
void mCoreSyncForceFrame(struct mCoreSync* sync);
bool mCoreSyncWaitFrameStart(struct mCoreSync* sync);
void mCoreSyncWaitFrameEnd(struct mCoreSync* sync);
void mCoreSyncSetVideoSync(struct mCoreSync* sync, bool wait);

struct mAudioBuffer;
bool mCoreSyncProduceAudio(struct mCoreSync* sync, const struct mAudioBuffer*);
void mCoreSyncConsumeAudio(struct mCoreSync* sync);
void mCoreSyncSetAudioSync(struct mCoreSync* sync, bool wait);

void mCoreSyncResetClock(struct mCoreSync* sync);
void mCoreSyncSetClockSync(struct mCoreSync* sync, bool wait);

CXX_GUARD_END

#endif
