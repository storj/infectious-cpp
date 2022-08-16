# infectious-cpp

This is a c++ port of the [infectious](https://github.com/vivint/infectious) 
library for Golang. It currently makes use of the C++20 standard, but could
probably be backported to C++17 or earlier without too much trouble.

Infectious implements
[Reed-Solomon forward error correction](https://en.wikipedia.org/wiki/Reed%E2%80%93Solomon_error_correction).
It uses the
[Berlekamp-Welch error correction algorithm](https://en.wikipedia.org/wiki/Berlekamp%E2%80%93Welch_algorithm)
to achieve the ability to actually correct errors.

[Some bright folks at Vivint wrote a blog post about how this library works!](https://innovation.vivint.com/introduction-to-reed-solomon-bc264d0794f8)

### Important note

The optimized versions of the 'multiply and add a row' function are not present
(yet) in this port. Nearly all operations will be significantly slower than the
Go library until that is remedied. All that is needed is a good way to query
and handle CPUID information without excessive clutter, so that the right
optimized addmul() for the current CPU can be called.

See the Botan project for examples of SIMD-optimized addmul() functions for
several different processor families.

### Example

(Todo)

### LICENSE

 * Copyright (C) 2022 Storj Labs, Inc.
 * Copyright (C) 2016-2017 Vivint, Inc.
 * Copyright (c) 2015 Klaus Post
 * Copyright (c) 2015 Backblaze
 * Copyright (C) 2011 Billy Brumley (billy.brumley@aalto.fi)
 * Copyright (C) 2009-2010,2021 Jack Lloyd (lloyd@randombit.net)
 * Copyright (C) 1996-1998 Luigi Rizzo (luigi@iet.unipi.it)

Portions derived from code by Phil Karn (karn@ka9q.ampr.org),
Robert Morelos-Zaragoza (robert@spectra.eng.hawaii.edu) and Hari
Thirumoorthy (harit@spectra.eng.hawaii.edu), Aug 1995

**Portions of this project (labeled in each file) are licensed under this
license:**

```
Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the
   distribution.

THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT,
INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.
```

**All other portions of this project are licensed under this license:**

```
The MIT License (MIT)

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```
