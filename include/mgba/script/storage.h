/* Copyright (c) 2013-2023 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef M_SCRIPT_STORAGE_H
#define M_SCRIPT_STORAGE_H

#include <mgba-util/common.h>

CXX_GUARD_START

#include <mgba/script/context.h>
#include <mgba/script/macros.h>

struct VFile;
void mScriptContextAttachStorage(struct mScriptContext* context);

CXX_GUARD_END

#endif
