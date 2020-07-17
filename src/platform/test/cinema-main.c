/* Copyright (c) 2013-2020 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/core/config.h>
#include <mgba/core/core.h>
#include <mgba/core/log.h>
#include <mgba/core/version.h>
#include <mgba/feature/commandline.h>
#include <mgba/feature/video-logger.h>

#include <mgba-util/png-io.h>
#include <mgba-util/table.h>
#include <mgba-util/vector.h>
#include <mgba-util/vfs.h>

#ifdef _MSC_VER
#include <mgba-util/platform/windows/getopt.h>
#else
#include <getopt.h>
#endif

#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>

#define MAX_TEST 200

static const struct option longOpts[] = {
	{ "base",       required_argument, 0, 'b' },
	{ "diffs",      no_argument, 0, 'd' },
	{ "help",       no_argument, 0, 'h' },
	{ "dry-run",    no_argument, 0, 'n' },
	{ "outdir",     required_argument, 0, 'o' },
	{ "quiet",      no_argument, 0, 'q' },
	{ "rebaseline", no_argument, 0, 'r' },
	{ "verbose",    no_argument, 0, 'v' },
	{ "version",    no_argument, 0, '\0' },
	{ 0, 0, 0, 0 }
};

static const char shortOpts[] = "b:dhno:qrv";

enum CInemaStatus {
	CI_PASS,
	CI_FAIL,
	CI_XPASS,
	CI_XFAIL,
	CI_ERROR,
	CI_SKIP
};

struct CInemaTest {
	char directory[MAX_TEST];
	char filename[MAX_TEST];
	char name[MAX_TEST];
	enum CInemaStatus status;
	unsigned failedFrames;
	uint64_t failedPixels;
	unsigned totalFrames;
	uint64_t totalDistance;
	uint64_t totalPixels;
};

struct CInemaImage {
	void* data;
	unsigned width;
	unsigned height;
	unsigned stride;
};

DECLARE_VECTOR(CInemaTestList, struct CInemaTest)
DEFINE_VECTOR(CInemaTestList, struct CInemaTest)

DECLARE_VECTOR(ImageList, void*)
DEFINE_VECTOR(ImageList, void*)

static bool showVersion = false;
static bool showUsage = false;
static char base[PATH_MAX] = {0};
static char outdir[PATH_MAX] = {'.'};
static bool dryRun = false;
static bool diffs = false;
static bool rebaseline = false;
static int verbosity = 0;

bool CInemaTestInit(struct CInemaTest*, const char* directory, const char* filename);
void CInemaTestRun(struct CInemaTest*, struct Table* configTree);

bool CInemaConfigGetUInt(struct Table* configTree, const char* testName, const char* key, unsigned* value);
void CInemaConfigLoad(struct Table* configTree, const char* testName, struct mCore* core);

static void _log(struct mLogger* log, int category, enum mLogLevel level, const char* format, va_list args);

ATTRIBUTE_FORMAT(printf, 2, 3) void CIlog(int minlevel, const char* format, ...) {
	if (verbosity < minlevel) {
		return;
	}
	va_list args;
	va_start(args, format);
	vprintf(format, args);
	va_end(args);
}

ATTRIBUTE_FORMAT(printf, 2, 3) void CIerr(int minlevel, const char* format, ...) {
	if (verbosity < minlevel) {
		return;
	}
	va_list args;
	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
}

static bool parseCInemaArgs(int argc, char* const* argv) {
	int ch;
	int index = 0;
	while ((ch = getopt_long(argc, argv, shortOpts, longOpts, &index)) != -1) {
		const struct option* opt = &longOpts[index];
		switch (ch) {
		case '\0':
			if (strcmp(opt->name, "version") == 0) {
				showVersion = true;
			} else {
				return false;
			}
			break;
		case 'b':
			strncpy(base, optarg, sizeof(base));
			// TODO: Verify path exists
			break;
		case 'd':
			diffs = true;
			break;
		case 'h':
			showUsage = true;
			break;
		case 'n':
			dryRun = true;
			break;
		case 'o':
			strncpy(outdir, optarg, sizeof(outdir));
			// TODO: Make directory
			break;
		case 'q':
			--verbosity;
			break;
		case 'r':
			rebaseline = true;
			break;
		case 'v':
			++verbosity;
			break;
		default:
			return false;
		}
	}

	return true;
}

static void usageCInema(const char* arg0) {
	printf("usage: %s [-dhnqv] [-b BASE] [-o DIR] [--version] [test...]\n", arg0);
	puts("  -b, --base [BASE]          Path to the CInema base directory");
	puts("  -d, --diffs                Output image diffs from failures");
	puts("  -h, --help                 Print this usage and exit");
	puts("  -n, --dry-run              List all collected tests instead of running them");
	puts("  -o, --output [DIR]         Path to output applicable results");
	puts("  -q, --quiet                Decrease log verbosity (can be repeated)");
	puts("  -r, --rebaseline           Rewrite the baseline for failing tests");
	puts("  -v, --verbose              Increase log verbosity (can be repeated)");
	puts("  --version                  Print version and exit");
}

static bool determineBase(int argc, char* const* argv) {
	// TODO: Better dynamic detection
	separatePath(__FILE__, base, NULL, NULL);
	strncat(base, PATH_SEP ".." PATH_SEP ".." PATH_SEP ".." PATH_SEP "cinema", sizeof(base) - strlen(base) - 1);
	return true;
}

static bool collectTests(struct CInemaTestList* tests, const char* path) {
	CIerr(2, "Considering path %s\n", path);
	struct VDir* dir = VDirOpen(path);
	if (!dir) {
		return false;
	}
	struct VDirEntry* entry = dir->listNext(dir);
	while (entry) {
		char subpath[PATH_MAX];
		snprintf(subpath, sizeof(subpath), "%s" PATH_SEP "%s", path, entry->name(entry));
		if (entry->type(entry) == VFS_DIRECTORY && strncmp(entry->name(entry), ".", 2) != 0 && strncmp(entry->name(entry), "..", 3) != 0) {
			if (!collectTests(tests, subpath)) {
				dir->close(dir);
				return false;
			}
		} else if (entry->type(entry) == VFS_FILE && strncmp(entry->name(entry), "test.", 5) == 0) {
			CIerr(3, "Found potential test %s\n", subpath);
			struct VFile* vf = dir->openFile(dir, entry->name(entry), O_RDONLY);
			if (vf) {
				if (mCoreIsCompatible(vf) != PLATFORM_NONE || mVideoLogIsCompatible(vf) != PLATFORM_NONE) {
					struct CInemaTest* test = CInemaTestListAppend(tests);
					if (!CInemaTestInit(test, path, entry->name(entry))) {
						CIerr(3, "Failed to create test\n");
						CInemaTestListResize(tests, -1);
					} else {
						CIerr(2, "Found test %s\n", test->name);
					}
				} else {
					CIerr(3, "Not a compatible file\n");
				}
				vf->close(vf);
			} else {
				CIerr(3, "Failed to open file\n");
			}
		}
		entry = dir->listNext(dir);
	}
	dir->close(dir);
	return true;
}

static int _compareNames(const void* a, const void* b) {
	const struct CInemaTest* ta = a;
	const struct CInemaTest* tb = b;

	return strncmp(ta->name, tb->name, sizeof(ta->name));
}

static void reduceTestList(struct CInemaTestList* tests) {
	qsort(CInemaTestListGetPointer(tests, 0), CInemaTestListSize(tests), sizeof(struct CInemaTest), _compareNames);

	size_t i;
	for (i = 1; i < CInemaTestListSize(tests);) {
		struct CInemaTest* cur = CInemaTestListGetPointer(tests, i);
		struct CInemaTest* prev = CInemaTestListGetPointer(tests, i - 1);
		if (strncmp(cur->name, prev->name, sizeof(cur->name)) != 0) {
			++i;
			continue;
		}
		CInemaTestListShift(tests, i, 1);
	}
}

static void testToPath(const char* testName, char* path) {
	strncpy(path, base, PATH_MAX);

	bool dotSeen = true;
	size_t i;
	for (i = strlen(path); testName[0] && i < PATH_MAX; ++testName) {
		if (testName[0] == '.') {
			dotSeen = true;
		} else {
			if (dotSeen) {
				strncpy(&path[i], PATH_SEP, PATH_MAX - i);
				i += strlen(PATH_SEP);
				dotSeen = false;
				if (!i) {
					break;
				}
			}
			path[i] = testName[0];
			++i;
		}
	}
}

static void _loadConfigTree(struct Table* configTree, const char* testName) {
	char key[MAX_TEST];
	strncpy(key, testName, sizeof(key) - 1);

	struct mCoreConfig* config;
	while (!(config = HashTableLookup(configTree, key))) {
		char path[PATH_MAX];
		config = malloc(sizeof(*config));
		mCoreConfigInit(config, "cinema");
		testToPath(key, path);
		strncat(path, PATH_SEP, sizeof(path) - 1);
		strncat(path, "config.ini", sizeof(path) - 1);
		mCoreConfigLoadPath(config, path);
		HashTableInsert(configTree, key, config);
		char* pos = strrchr(key, '.');
		if (pos) {
			pos[0] = '\0';
		} else if (key[0]) {
			key[0] = '\0';
		} else {
			break;
		}
	}
}

static void _unloadConfigTree(const char* key, void* value, void* user) {
	UNUSED(key);
	UNUSED(user);
	mCoreConfigDeinit(value);
}

static const char* _lookupValue(struct Table* configTree, const char* testName, const char* key) {
	_loadConfigTree(configTree, testName);

	char testKey[MAX_TEST];
	strncpy(testKey, testName, sizeof(testKey) - 1);

	struct mCoreConfig* config;
	while (true) {
		config = HashTableLookup(configTree, testKey);
		if (!config) {
			continue;
		}
		const char* str = ConfigurationGetValue(&config->configTable, "testinfo", key);
		if (str) {
			return str;
		}
		char* pos = strrchr(testKey, '.');
		if (pos) {
			pos[0] = '\0';
		} else if (testKey[0]) {
			testKey[0] = '\0';
		} else {
			break;
		}
	}
	return NULL;
}

bool CInemaConfigGetUInt(struct Table* configTree, const char* testName, const char* key, unsigned* out) {
	const char* charValue = _lookupValue(configTree, testName, key);
	if (!charValue) {
		return false;
	}
	char* end;
	unsigned long value = strtoul(charValue, &end, 10);
	if (*end) {
		return false;
	}
	*out = value;
	return true;
}

void CInemaConfigLoad(struct Table* configTree, const char* testName, struct mCore* core) {
	_loadConfigTree(configTree, testName);

	char testKey[MAX_TEST] = {0};
	char* keyEnd = testKey;
	const char* pos;
	while (true) {
		pos = strchr(testName, '.');
		size_t maxlen = sizeof(testKey) - (keyEnd - testKey) - 1;
		size_t len;
		if (pos) {
			len = pos - testName;
		} else {
			len = strlen(testName);
		}
		if (len > maxlen) {
			len = maxlen;
		}
		strncpy(keyEnd, testName, len);
		keyEnd += len;

		struct mCoreConfig* config = HashTableLookup(configTree, testKey);
		if (config) {
			core->loadConfig(core, config);
		}
		if (!pos) {
			break;
		}
		testName = pos + 1;
		keyEnd[0] = '.';
		++keyEnd;
	}
}

bool CInemaTestInit(struct CInemaTest* test, const char* directory, const char* filename) {
	if (strncmp(base, directory, strlen(base)) != 0) {
		return false;
	}
	memset(test, 0, sizeof(*test));
	strncpy(test->directory, directory, sizeof(test->directory) - 1);
	strncpy(test->filename, filename, sizeof(test->filename) - 1);
	directory += strlen(base) + 1;
	strncpy(test->name, directory, sizeof(test->name) - 1);
	char* str = strstr(test->name, PATH_SEP);
	while (str) {
		str[0] = '.';
		str = strstr(str, PATH_SEP);
	}
	return true;
}

static bool _loadBaseline(struct VDir* dir, struct CInemaImage* image, size_t frame, enum CInemaStatus* status) {
	char baselineName[32];
	snprintf(baselineName, sizeof(baselineName), "baseline_%04" PRIz "u.png", frame);
	struct VFile* baselineVF = dir->openFile(dir, baselineName, O_RDONLY);
	if (!baselineVF) {
		if (*status == CI_PASS) {
			*status = CI_FAIL;
		}
		return false;
	}

	png_structp png = PNGReadOpen(baselineVF, 0);
	png_infop info = png_create_info_struct(png);
	png_infop end = png_create_info_struct(png);
	if (!png || !info || !end || !PNGReadHeader(png, info)) {
		PNGReadClose(png, info, end);
		baselineVF->close(baselineVF);
		CIerr(1, "Failed to load %s\n", baselineName);
		*status = CI_ERROR;
		return false;
	}

	unsigned pwidth = png_get_image_width(png, info);
	unsigned pheight = png_get_image_height(png, info);
	if (pheight != image->height || pwidth != image->width) {
		PNGReadClose(png, info, end);
		baselineVF->close(baselineVF);
		CIerr(1, "Size mismatch for %s, expected %ux%u, got %ux%u\n", baselineName, pwidth, pheight, image->width, image->height);
		if (*status == CI_PASS) {
			*status = CI_FAIL;
		}
		return false;
	}

	image->data = malloc(pwidth * pheight * BYTES_PER_PIXEL);
	if (!image->data) {
		CIerr(1, "Failed to allocate baseline buffer\n");
		*status = CI_ERROR;
		PNGReadClose(png, info, end);
		baselineVF->close(baselineVF);
		return false;
	}
	if (!PNGReadPixels(png, info, image->data, pwidth, pheight, pwidth) || !PNGReadFooter(png, end)) {
		CIerr(1, "Failed to read %s\n", baselineName);
		*status = CI_ERROR;
		free(image->data);
		return false;
	}
	PNGReadClose(png, info, end);
	baselineVF->close(baselineVF);
	image->stride = pwidth;
	return true;
}

static struct VDir* _makeOutDir(const char* testName) {
	char path[PATH_MAX] = {0};
	strncpy(path, outdir, sizeof(path) - 1);
	char* pathEnd = path + strlen(path);
	const char* pos;
	while (true) {
		pathEnd[0] = PATH_SEP[0];
		++pathEnd;
		pos = strchr(testName, '.');
		size_t maxlen = sizeof(path) - (pathEnd - path) - 1;
		size_t len;
		if (pos) {
			len = pos - testName;
		} else {
			len = strlen(testName);
		}
		if (len > maxlen) {
			len = maxlen;
		}
		strncpy(pathEnd, testName, len);
		pathEnd += len;

		mkdir(path, 0777);

		if (!pos) {
			break;
		}
		testName = pos + 1;
	}
	return VDirOpen(path);
}

static void _writeImage(struct VFile* vf, const struct CInemaImage* image) {
	png_structp png = PNGWriteOpen(vf);
	png_infop info = PNGWriteHeader(png, image->width, image->height);
	if (!PNGWritePixels(png, image->width, image->height, image->stride, image->data)) {
		CIerr(0, "Could not write output image\n");
	}
	PNGWriteClose(png, info);

	vf->close(vf);
}

static void _writeDiff(const char* testName, const struct CInemaImage* image, size_t frame, const char* type) {
	struct VDir* dir = _makeOutDir(testName);
	if (!dir) {
		CIerr(0, "Could not open directory for %s\n", testName);
		return;
	}
	char name[32];
	snprintf(name, sizeof(name), "%s_%04" PRIz "u.png", type, frame);
	struct VFile* vf = dir->openFile(dir, name, O_CREAT | O_TRUNC | O_WRONLY);
	if (!vf) {
		CIerr(0, "Could not open output file %s\n", name);
		dir->close(dir);
		return;
	}
	_writeImage(vf, image);
	dir->close(dir);
}

static void _writeBaseline(struct VDir* dir, const struct CInemaImage* image, size_t frame) {
	char baselineName[32];
	snprintf(baselineName, sizeof(baselineName), "baseline_%04" PRIz "u.png", frame);
	struct VFile* baselineVF = dir->openFile(dir, baselineName, O_CREAT | O_TRUNC | O_WRONLY);
	if (baselineVF) {
		_writeImage(baselineVF, image);
	} else {
		CIerr(0, "Could not open output file %s\n", baselineName);
	}
}

void CInemaTestRun(struct CInemaTest* test, struct Table* configTree) {
	unsigned ignore = 0;
	CInemaConfigGetUInt(configTree, test->name, "ignore", &ignore);
	if (ignore) {
		test->status = CI_SKIP;
		return;
	}

	struct VDir* dir = VDirOpen(test->directory);
	if (!dir) {
		CIerr(0, "Failed to open test directory\n");
		test->status = CI_ERROR;
		return;
	}
	struct VFile* rom = dir->openFile(dir, test->filename, O_RDONLY);
	if (!rom) {
		CIerr(0, "Failed to open test\n");
		test->status = CI_ERROR;
		return;
	}
	struct mCore* core = mCoreFindVF(rom);
	if (!core) {
		CIerr(0, "Failed to load test\n");
		test->status = CI_ERROR;
		rom->close(rom);
		return;
	}
	if (!core->init(core)) {
		CIerr(0, "Failed to init test\n");
		test->status = CI_ERROR;
		core->deinit(core);
		return;
	}
	struct CInemaImage image;
	core->desiredVideoDimensions(core, &image.width, &image.height);
	ssize_t bufferSize = image.width * image.height * BYTES_PER_PIXEL;
	image.data = malloc(bufferSize);
	image.stride = image.width;
	if (!image.data) {
		CIerr(0, "Failed to allocate video buffer\n");
		test->status = CI_ERROR;
		core->deinit(core);
	}
	core->setVideoBuffer(core, image.data, image.stride);
	mCoreConfigInit(&core->config, "cinema");

	unsigned limit = 9999;
	unsigned skip = 0;
	unsigned fail = 0;

	CInemaConfigGetUInt(configTree, test->name, "frames", &limit);
	CInemaConfigGetUInt(configTree, test->name, "skip", &skip);
	CInemaConfigGetUInt(configTree, test->name, "fail", &fail);
	CInemaConfigLoad(configTree, test->name, core);

	core->loadROM(core, rom);
	core->reset(core);

	test->status = CI_PASS;

	unsigned minFrame = core->frameCounter(core);
	size_t frame;
	for (frame = 0; frame < skip; ++frame) {
		core->runFrame(core);
	}
	for (frame = 0; limit; ++frame, --limit) {
		core->runFrame(core);
		++test->totalFrames;
		unsigned frameCounter = core->frameCounter(core);
		if (frameCounter <= minFrame) {
			break;
		}
		CIerr(3, "Test frame: %u\n", frameCounter);
		core->desiredVideoDimensions(core, &image.width, &image.height);
		uint8_t* diff = NULL;
		struct CInemaImage expected = {
			.data = NULL,
			.width = image.width,
			.height = image.height,
			.stride = image.width,
		};
		if (_loadBaseline(dir, &expected, frame, &test->status)) {
			uint8_t* testPixels = image.data;
			uint8_t* expectPixels = expected.data;
			size_t x;
			size_t y;
			int max = 0;
			bool failed = false;
			for (y = 0; y < image.height; ++y) {
				for (x = 0; x < image.width; ++x) {
					size_t pix = expected.stride * y + x;
					size_t tpix = image.stride * y + x;
					int testR = testPixels[tpix * 4 + 0];
					int testG = testPixels[tpix * 4 + 1];
					int testB = testPixels[tpix * 4 + 2];
					int expectR = expectPixels[pix * 4 + 0];
					int expectG = expectPixels[pix * 4 + 1];
					int expectB = expectPixels[pix * 4 + 2];
					int r = expectR - testR;
					int g = expectG - testG;
					int b = expectB - testB;
					if (r | g | b) {
						failed = true;
						if (diffs && !diff) {
							diff = calloc(expected.width * expected.height, BYTES_PER_PIXEL);
						}
						CIerr(3, "Frame %u failed at pixel %" PRIz "ux%" PRIz "u with diff %i,%i,%i (expected %02x%02x%02x, got %02x%02x%02x)\n",
						    frameCounter, x, y, r, g, b,
						    expectR, expectG, expectB,
						    testR, testG, testB);
						test->status = CI_FAIL;
						if (r < 0) {
							r = -r;
						}
						if (g < 0) {
							g = -g;
						}
						if (b < 0) {
							b = -b;
						}

						if (diff) {
							if (r > max) {
								max = r;
							}
							if (g > max) {
								max = g;
							}
							if (b > max) {
								max = b;
							}
							diff[pix * 4 + 0] = r;
							diff[pix * 4 + 1] = g;
							diff[pix * 4 + 2] = b;
						}

						test->totalDistance += r + g + b;
						++test->failedPixels;
					}
				}
			}
			if (failed) {
				++test->failedFrames;
			}
			test->totalPixels += image.height * image.width;
			if (rebaseline && failed) {
				_writeBaseline(dir, &image, frame);
			}
			if (diff) {
				if (failed) {
					struct CInemaImage outdiff = {
						.data = diff,
						.width = image.width,
						.height = image.height,
						.stride = image.width,
					};

					_writeDiff(test->name, &image, frame, "result");
					_writeDiff(test->name, &expected, frame, "expected");
					_writeDiff(test->name, &outdiff, frame, "diff");

					for (y = 0; y < outdiff.height; ++y) {
						for (x = 0; x < outdiff.width; ++x) {
							size_t pix = outdiff.stride * y + x;
							diff[pix * 4 + 0] = diff[pix * 4 + 0] * 255 / max;
							diff[pix * 4 + 1] = diff[pix * 4 + 1] * 255 / max;
							diff[pix * 4 + 2] = diff[pix * 4 + 2] * 255 / max;
						}
					}
					_writeDiff(test->name, &outdiff, frame, "normalized");
				}
				free(diff);
			}
			free(expected.data);
		} else if (test->status == CI_ERROR) {
			break;
		} else if (rebaseline) {
			_writeBaseline(dir, &image, frame);
		}
	}

	if (fail) {
		if (test->status == CI_FAIL) {
			test->status = CI_XFAIL;
		} else if (test->status == CI_PASS) {
			test->status = CI_XPASS;
		}
	}

	free(image.data);
	mCoreConfigDeinit(&core->config);
	core->deinit(core);
	dir->close(dir);
}

void _log(struct mLogger* log, int category, enum mLogLevel level, const char* format, va_list args) {
	// TODO: Write
}

int main(int argc, char** argv) {
	int status = 0;
	if (!parseCInemaArgs(argc, argv)) {
		status = 1;
		goto cleanup;
	}

	if (showVersion) {
		version(argv[0]);
		goto cleanup;
	}

	if (showUsage) {
		usageCInema(argv[0]);
		goto cleanup;
	}

	argc -= optind;
	argv += optind;

	if (!base[0] && !determineBase(argc, argv)) {
		CIerr(0, "Could not determine CInema test base. Please specify manually.");
		status = 1;
		goto cleanup;
	}
#ifndef _WIN32
	char* rbase = realpath(base, NULL);
	strncpy(base, rbase, PATH_MAX);
	free(rbase);
#endif

	struct CInemaTestList tests;
	CInemaTestListInit(&tests, 0);

	struct mLogger logger = { .log = _log };
	mLogSetDefaultLogger(&logger);

	if (argc > 0) {
		size_t i;
		for (i = 0; i < (size_t) argc; ++i) {
			char path[PATH_MAX + 1] = {0};
			testToPath(argv[i], path);

			if (!collectTests(&tests, path)) {
				status = 1;
				break;
			}
		}
	} else if (!collectTests(&tests, base)) {
		status = 1;
	}

	if (CInemaTestListSize(&tests) == 0) {
		CIerr(1, "No tests found.");
		status = 1;
	} else {
		reduceTestList(&tests);
	}

	struct Table configTree;
	HashTableInit(&configTree, 0, free);

	size_t i;
	for (i = 0; i < CInemaTestListSize(&tests); ++i) {
		struct CInemaTest* test = CInemaTestListGetPointer(&tests, i);
		if (dryRun) {
			CIlog(-1, "%s\n", test->name);
		} else {
			CIerr(1, "%s: ", test->name);
			CInemaTestRun(test, &configTree);
			switch (test->status) {
			case CI_PASS:
				CIerr(1, "pass");
				break;
			case CI_FAIL:
				status = 1;
				CIerr(1, "fail");
				break;
			case CI_XPASS:
				CIerr(1, "xpass");
				break;
			case CI_XFAIL:
				CIerr(1, "xfail");
				break;
			case CI_SKIP:
				CIerr(1, "skip");
				break;
			case CI_ERROR:
				status = 1;
				CIerr(1, "error");
				break;
			}
			if (test->failedFrames) {
				CIerr(2, "\n\tfailed frames: %u/%u (%1.3g%%)", test->failedFrames, test->totalFrames, test->failedFrames / (test->totalFrames * 0.01));
				CIerr(2, "\n\tfailed pixels: %" PRIu64 "/%" PRIu64 " (%1.3g%%)", test->failedPixels, test->totalPixels, test->failedPixels / (test->totalPixels * 0.01));
				CIerr(2, "\n\tdistance: %" PRIu64 "/%" PRIu64 " (%1.3g%%)", test->totalDistance, test->totalPixels * 765, test->totalDistance / (test->totalPixels * 7.65));
			}

			CIerr(1, "\n");
		}
	}

	HashTableEnumerate(&configTree, _unloadConfigTree, NULL);
	HashTableDeinit(&configTree);
	CInemaTestListDeinit(&tests);

cleanup:
	return status;
}