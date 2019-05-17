/* Copyright (c) 2013-2019 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef VIDEO_GL_H
#define VIDEO_GL_H

#include <mgba-util/common.h>

CXX_GUARD_START

#include <mgba/core/core.h>
#include <mgba/gba/interface.h>
#include <mgba/internal/gba/io.h>
#include <mgba/internal/gba/renderers/common.h>
#include <mgba/internal/gba/video.h>

#if defined(BUILD_GLES2) || defined(BUILD_GLES3)

#ifdef USE_EPOXY
#include <epoxy/gl.h>
#elif defined(BUILD_GL)
#ifdef __APPLE__
#include <OpenGL/gl3.h>
#else
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>
#endif
#else
#include <GLES3/gl3.h>
#endif

struct GBAVideoGLAffine {
	int16_t dx;
	int16_t dmx;
	int16_t dy;
	int16_t dmy;
	int32_t sx;
	int32_t sy;
};

struct GBAVideoGLBackground {
	GLuint fbo;
	GLuint tex;
	GLuint flags;

	unsigned index;
	int enabled;
	unsigned priority;
	uint32_t charBase;
	int mosaic;
	int multipalette;
	uint32_t screenBase;
	int overflow;
	int size;
	int target1;
	int target2;
	uint16_t x;
	uint16_t y;
	int32_t refx;
	int32_t refy;

	struct GBAVideoGLAffine affine[4];
};

enum {
	GBA_GL_FBO_OBJ = 0,
	GBA_GL_FBO_WINDOW = 1,
	GBA_GL_FBO_OUTPUT = 2,
	GBA_GL_FBO_MAX
};

enum {
	GBA_GL_TEX_OBJ_COLOR = 0,
	GBA_GL_TEX_OBJ_FLAGS = 1,
	GBA_GL_TEX_WINDOW = 6,
	GBA_GL_TEX_MAX
};

enum {
	GBA_GL_VS_LOC = 0,
	GBA_GL_VS_MAXPOS,

	GBA_GL_BG_VRAM = 2,
	GBA_GL_BG_PALETTE,
	GBA_GL_BG_SCREENBASE,
	GBA_GL_BG_CHARBASE,
	GBA_GL_BG_SIZE,
	GBA_GL_BG_OFFSET,
	GBA_GL_BG_INFLAGS,
	GBA_GL_BG_TRANSFORM,

	GBA_GL_OBJ_VRAM = 2,
	GBA_GL_OBJ_PALETTE,
	GBA_GL_OBJ_CHARBASE,
	GBA_GL_OBJ_STRIDE,
	GBA_GL_OBJ_LOCALPALETTE,
	GBA_GL_OBJ_INFLAGS,
	GBA_GL_OBJ_TRANSFORM,
	GBA_GL_OBJ_DIMS,
	GBA_GL_OBJ_OBJWIN,

	GBA_GL_FINALIZE_SCALE = 2,
	GBA_GL_FINALIZE_LAYERS,
	GBA_GL_FINALIZE_FLAGS,
	GBA_GL_FINALIZE_WINDOW,
	GBA_GL_FINALIZE_BACKDROP,
	GBA_GL_FINALIZE_BACKDROPFLAGS,

	GBA_GL_UNIFORM_MAX = 12
};

struct GBAVideoGLShader {
	GLuint program;
	GLuint vao;
	GLuint uniforms[GBA_GL_UNIFORM_MAX];
};

struct GBAVideoGLRenderer {
	struct GBAVideoRenderer d;

	uint32_t* temporaryBuffer;

	struct GBAVideoGLBackground bg[4];

	int oamMax;
	bool oamDirty;
	struct GBAVideoRendererSprite sprites[128];

	GLuint fbo[GBA_GL_FBO_MAX];
	GLuint layers[GBA_GL_TEX_MAX];
	GLuint vbo;

	GLuint outputTex;

#ifdef BUILD_GLES3
	uint16_t shadowPalette[512];
#endif
	GLuint paletteTex;
	bool paletteDirty;

	GLuint vramTex;
	unsigned vramDirty;

	struct GBAVideoGLShader bgShader[6];
	struct GBAVideoGLShader objShader[2];
	struct GBAVideoGLShader finalizeShader;

	GBARegisterDISPCNT dispcnt;

	unsigned target1Obj;
	unsigned target1Bd;
	unsigned target2Obj;
	unsigned target2Bd;
	enum GBAVideoBlendEffect blendEffect;
	uint16_t blda;
	uint16_t bldb;
	uint16_t bldy;

	GBAMosaicControl mosaic;

	struct GBAVideoGLWindowN {
		struct GBAVideoWindowRegion h;
		struct GBAVideoWindowRegion v;
		GBAWindowControl control;
	} winN[2];

	GBAWindowControl winout;
	GBAWindowControl objwin;

	int firstAffine;

	int scale;
};

void GBAVideoGLRendererCreate(struct GBAVideoGLRenderer* renderer);

#endif

CXX_GUARD_END

#endif