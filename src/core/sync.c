/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/core/sync.h>

#include <mgba/core/core.h>
#include <mgba/core/config.h>
#include <mgba-util/audio-buffer.h>

static const float _defaultFPSTarget = 60.f;

void mCoreSyncInit(struct mCoreSync* sync) {
	MutexInit(&sync->mutex);
	ConditionInit(&sync->readyCond);
	ConditionInit(&sync->videoFrameAvailableCond);

	sync->clockRatio = 1.f;
	sync->fpsTarget = _defaultFPSTarget;
	sync->speedMultiplier = 1.f;
	sync->audioHighWater = 512;

#ifdef _WIN32
	int64_t freq = 0;
	if (QueryPerformanceFrequency((PLARGE_INTEGER) &freq)) {
		sync->qpcFreq = 1.0 / freq;
	} else {
		sync->qpcFreq = 0;
	}
#endif
	mCoreSyncResetClock(sync);
}

void mCoreSyncDeinit(struct mCoreSync* sync) {
	MutexDeinit(&sync->mutex);
	ConditionWake(&sync->readyCond);
	ConditionDeinit(&sync->readyCond);
	ConditionWake(&sync->videoFrameAvailableCond);
	ConditionDeinit(&sync->videoFrameAvailableCond);
}

void mCoreSyncSetActive(struct mCoreSync* sync, bool active) {
	if (!sync) {
		return;
	}

	mCoreSyncLock(sync);
	if (sync->active != active) {
		mCoreSyncResetClock(sync);
		sync->active = active;
	}
	mCoreSyncUnlock(sync);
}

static void _recalculateClockRatio(struct mCoreSync* sync) {
	sync->clockRatio = sync->systemFps / (sync->fpsTarget * sync->speedMultiplier * sync->coreFrequency);
	mCoreSyncResetClock(sync);
}

void mCoreSyncSetCoreParams(struct mCoreSync* sync, struct mCore* core) {
	if (!sync) {
		return;
	}

	mCoreSyncLock(sync);
	sync->coreFrequency = core->timingFrequency(core);
	int32_t frameTime = core->frameCycles(core);
	int32_t frequency = core->frequency(core);
	sync->systemFps = frequency / (float) frameTime;
	_recalculateClockRatio(sync);
	mCoreSyncUnlock(sync);
}

void mCoreSyncSetFpsTarget(struct mCoreSync* sync, float fpsTarget) {
	if (!sync) {
		return;
	}

	mCoreSyncLock(sync);
	if (sync->fpsTarget != fpsTarget) {
		sync->fpsTarget = fpsTarget;
		_recalculateClockRatio(sync);
	}
	mCoreSyncUnlock(sync);
}

void mCoreSyncSetSpeedMultiplier(struct mCoreSync* sync, float speedMultiplier) {
	if (!sync) {
		return;
	}

	mCoreSyncLock(sync);
	if (sync->speedMultiplier != speedMultiplier) {
		sync->speedMultiplier = speedMultiplier;
		_recalculateClockRatio(sync);
	}
	mCoreSyncUnlock(sync);
}

void mCoreSyncLock(struct mCoreSync* sync) {
	if (!sync) {
		return;
	}

	MutexLock(&sync->mutex);
}

void mCoreSyncUnlock(struct mCoreSync* sync) {
	if (!sync) {
		return;
	}

	MutexUnlock(&sync->mutex);
}

bool mCoreSyncWait(struct mCoreSync* sync, int32_t currentCycles, int timeoutMs) {
	if (!sync) {
		return true;
	}

	bool done = false;

	MutexLock(&sync->mutex);
	if (!sync->active) {
		done = true;
	}

	int32_t cycleAdvance = currentCycles - sync->lastCycles;
	sync->clockOffset += cycleAdvance * sync->clockRatio;
	sync->lastCycles = currentCycles;

	size_t produced = 0;
	while (!done && !sync->interrupt) {
		done = true;
		if (sync->speedMultiplier == 1 && !sync->busy) {
			// Audio and video sync only make sense when the speed multiplier is 1
			// otherwise, force clock sync
			if (sync->videoSync && sync->videoFramePending) {
				ConditionWake(&sync->videoFrameAvailableCond);
				done = false;
			}

			if (sync->audioBuffer) {
				produced = mAudioBufferAvailable(sync->audioBuffer);
			}
			if (sync->audioSync && sync->audioHighWater && produced >= sync->audioHighWater) {
				done = false;
			}
		}

		if (sync->clockSync || sync->speedMultiplier != 1) {
			double clockAdvance = 0;
#ifndef _WIN32
			uint64_t usec = 0;
			struct timespec ts;
			if (timespec_get(&ts, TIME_MONOTONIC)) {
				usec = ts.tv_nsec / 1000;
				usec += ts.tv_sec * 1000000LL;
				uint64_t usecAdvance = usec - sync->currentClock;
				sync->currentClock = usec;
				clockAdvance = usecAdvance / 1000000.0;
			}
#else
			uint64_t qpc = 0;
			if (sync->qpcFreq && QueryPerformanceCounter((PLARGE_INTEGER) &qpc)) {
				uint64_t qpcAdvance = qpc - sync->currentClock;
				sync->currentClock = qpc;
				clockAdvance = qpcAdvance * sync->qpcFreq;
			}
#endif
			sync->clockOffset -= clockAdvance;

			if (sync->clockOffset > 0) {
				done = false;
			}
		}

		if (!done) {
			if (ConditionWaitTimed(&sync->readyCond, &sync->mutex, timeoutMs)) {
				break;
			}
		}
	}
	MutexUnlock(&sync->mutex);

	sync->busy = !done;
	return done;
}

void mCoreSyncInterrupt(struct mCoreSync* sync) {
	MutexLock(&sync->mutex);
	sync->interrupt = true;
	ConditionWake(&sync->readyCond);
	MutexUnlock(&sync->mutex);
}

void mCoreSyncResume(struct mCoreSync* sync) {
	MutexLock(&sync->mutex);
	sync->interrupt = false;
	MutexUnlock(&sync->mutex);
}

void mCoreSyncLoadCoreOpts(struct mCoreSync* sync, const struct mCoreOptions* opts) {
	sync->audioSync = opts->audioSync;
	sync->videoSync = opts->videoSync;
	sync->clockSync = opts->clockSync;
	if (opts->fpsTarget) {
		sync->fpsTarget = opts->fpsTarget;
	}
}

void mCoreSyncPostFrame(struct mCoreSync* sync) {
	if (!sync) {
		return;
	}

	MutexLock(&sync->mutex);
	++sync->videoFramePending;
	MutexUnlock(&sync->mutex);
	sync->busy = true;
}

void mCoreSyncForceFrame(struct mCoreSync* sync) {
	if (!sync) {
		return;
	}

	MutexLock(&sync->mutex);
	ConditionWake(&sync->videoFrameAvailableCond);
	MutexUnlock(&sync->mutex);
}

bool mCoreSyncWaitFrameStart(struct mCoreSync* sync) {
	if (!sync) {
		return true;
	}

	MutexLock(&sync->mutex);

	if (sync->videoSync && !sync->videoFramePending) {
		ConditionWake(&sync->readyCond);
		ConditionWaitTimed(&sync->videoFrameAvailableCond, &sync->mutex, 50);
	}
	if (sync->videoFramePending) {
		sync->videoFramePending = 0;
		return true;
	}
	return false;
}

void mCoreSyncWaitFrameEnd(struct mCoreSync* sync) {
	if (!sync) {
		return;
	}

	ConditionWake(&sync->readyCond);
	MutexUnlock(&sync->mutex);
}

void mCoreSyncSetVideoSync(struct mCoreSync* sync, bool wait) {
	if (wait == sync->videoSync) {
		return;
	}
	sync->videoSync = wait;
	sync->videoFramePending = 0;
	ConditionWake(&sync->videoFrameAvailableCond);
}

bool mCoreSyncProduceAudio(struct mCoreSync* sync, const struct mAudioBuffer* buf) {
	if (!sync) {
		return true;
	}

	sync->audioBuffer = buf;
	size_t produced = mAudioBufferAvailable(buf);
	bool full = sync->audioSync && sync->audioHighWater && produced >= sync->audioHighWater;
	MutexUnlock(&sync->mutex);
	sync->busy = full;
	return !full;
}

void mCoreSyncConsumeAudio(struct mCoreSync* sync) {
	if (!sync) {
		return;
	}

	if (sync->audioSync) {
		ConditionWake(&sync->readyCond);
	}
	MutexUnlock(&sync->mutex);
}

void mCoreSyncSetAudioSync(struct mCoreSync* sync, bool wait) {
	sync->audioSync = wait;
}

void mCoreSyncResetClock(struct mCoreSync* sync) {
#ifndef _WIN32
	uint64_t usec = 0;
	struct timespec ts;
	if (timespec_get(&ts, TIME_MONOTONIC)) {
		usec = ts.tv_nsec / 1000;
		usec += ts.tv_sec * 1000000LL;
	}
	sync->currentClock = usec;
#else
	QueryPerformanceCounter((PLARGE_INTEGER) &sync->currentClock);
#endif
	sync->clockOffset = 0;
}

void mCoreSyncSetClockSync(struct mCoreSync* sync, bool wait) {
	if (wait == sync->clockSync) {
		return;
	}

	sync->clockSync = wait;
	mCoreSyncResetClock(sync);
}
