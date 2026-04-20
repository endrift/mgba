/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/core/sync.h>

#include <mgba/core/config.h>
#include <mgba-util/audio-buffer.h>

static const float _defaultFPSTarget = 60.f;

void mCoreSyncInit(struct mCoreSync* sync) {
	MutexInit(&sync->mutex);
	ConditionInit(&sync->readyCond);
	ConditionInit(&sync->videoFrameAvailableCond);

	sync->fpsTarget = _defaultFPSTarget;
	sync->audioHighWater = 512;
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
	sync->active = active;
	mCoreSyncUnlock(sync);
}

void mCoreSyncSetFpsTarget(struct mCoreSync* sync, float fpsTarget) {
	if (!sync) {
		return;
	}

	mCoreSyncLock(sync);
	sync->fpsTarget = fpsTarget;
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

bool mCoreSyncWait(struct mCoreSync* sync, int timeoutMs) {
	if (!sync || !sync->active || !sync->busy) {
		return true;
	}

	bool done = false;

	MutexLock(&sync->mutex);
	size_t produced = 0;
	while (!done && !sync->interrupt) {
		done = true;
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
