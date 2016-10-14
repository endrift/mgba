# Copyright (c) 2013-2016 Jeffrey Pfau
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from _pylib import ffi, lib

class GBA:
    def __init__(self, native):
        self._native = ffi.cast("struct GBA*", native)
