#!/usr/bin/env python3

# Copyright 2021 Hein-Pieter van Braam-Stewart
#
# This file is part of ppelib (Portable Portable Executable LIBrary)
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import os
import sys
from generator import generate

outdir = sys.argv[1]
mydir = os.path.dirname(os.path.abspath(__file__))

dos_header = [
        ["public_header.h", "ppelib-dos-header.h" ],
        ["public_header_lowlevel.h", "ppelib-dos-header-lowlevel.h"],
]

header = [
        ["public_header.h", "ppelib-header.h" ],
        ["public_header_lowlevel.h", "ppelib-header-lowlevel.h"],
]

section = [
        ["public_header.h", "ppelib-section.h" ],
        ["public_header_lowlevel.h", "ppelib-section-lowlevel.h"],
]

for file in dos_header:
    generate(f"{mydir}/structures/dos_header.yaml", f"{mydir}/templates/{file[0]}", f"{outdir}/{file[1]}")

for file in header:
    generate(f"{mydir}/structures/header.yaml", f"{mydir}/templates/{file[0]}", f"{outdir}/{file[1]}")

for file in section:
    generate(f"{mydir}/structures/section.yaml", f"{mydir}/templates/{file[0]}", f"{outdir}/{file[1]}")
