#ifndef M_CORE_INTERNAL_H
#define M_CORE_INTERNAL_H

#include <mgba-util/common.h>

CXX_GUARD_START

#include <mgba/core/rewind.h>
#include <mgba/core/sync.h>
#include <mgba-util/threading.h>

enum mCoreThreadState {
	mTHREAD_INITIALIZED = -1,
	mTHREAD_RUNNING = 0,
	mTHREAD_REQUEST,

	mTHREAD_INTERRUPTED,
	mTHREAD_PAUSED,
	mTHREAD_CRASHED,

	mTHREAD_INTERRUPTING,
	mTHREAD_EXITING,

	mTHREAD_SHUTDOWN,

	mTHREAD_MIN_WAITING = mTHREAD_INTERRUPTED,
	mTHREAD_MAX_WAITING = mTHREAD_CRASHED
};

enum mCoreThreadRequest {
	mTHREAD_REQ_PAUSE = 1, // User-set pause
	mTHREAD_REQ_WAIT = 2, // Core-set pause
	mTHREAD_REQ_RESET = 4,
	mTHREAD_REQ_RUN_ON = 8,
	mTHREAD_REQ_CRASHED = 16,
	mTHREAD_REQ_REWIND_EMPTY = 32,
};

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

struct mCoreThreadInternal {
	Thread thread;
	enum mCoreThreadState state;
	bool rewinding;
	int requested;

	Mutex stateMutex;
	Condition stateOnThreadCond;
	Condition stateOffThreadCond;
	int interruptDepth;
	bool frameWasOn;

	struct mCoreSync sync;
	struct mCoreRewindContext rewind;
	struct mCore* core;
};

CXX_GUARD_END

#endif
