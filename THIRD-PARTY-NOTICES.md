# Third-party notices

MEDUSA itself is distributed under the MIT License (see `LICENSE`). It builds
against the third-party components listed below, each of which remains under
its own license. These notices must be reproduced in redistributions,
including binary ones.

## GMP — GNU Multiple Precision Arithmetic Library

* Upstream: https://gmplib.org/
* License: dual-licensed, **LGPL-3.0-or-later OR GPL-2.0-or-later** (you may
  choose either); MEDUSA relies on the LGPL-3.0-or-later option
* Copyright: Free Software Foundation, Inc.
* Used by: all build configurations, linked dynamically (`libgmp.so`)

GMP provides the exact integer arithmetic behind the algebraic complex
amplitudes. Because it is linked dynamically, users can replace or rebuild
their own GMP, which is what the LGPL requires.

## MoToBuddy (and BuDDy, from which it derives)

* Upstream: https://github.com/VeriFIT/MoToBuddy
* License: permissive, BSD-like (BuDDy license)
* Copyright (C) 1996-2002 by Jorn Lind-Nielsen; substantial modifications
  copyright their respective authors
* Used by: the default MoToBuddy backends, linked statically (`libbuddy.a`)

The BuDDy license requires that its copyright notice and disclaimer appear in
all copies of the source code, and that redistributions — **including
binaries** — reproduce these notices in the supporting documentation:

> Permission is hereby granted, without written agreement and without license
> or royalty fees, to use, reproduce, prepare derivative works, distribute, and
> display this software and its documentation for any purpose, provided that
> (1) the above copyright notice and the following two paragraphs appear in all
> copies of the source code and (2) redistributions, including without
> limitation binaries, reproduce these notices in the supporting documentation.
> Substantial modifications to this software may be copyrighted by their
> authors and need not follow the licensing terms described here, provided that
> the new terms are clearly indicated in all files where they apply.
>
> IN NO EVENT SHALL JORN LIND-NIELSEN, OR DISTRIBUTORS OF THIS SOFTWARE BE
> LIABLE TO ANY PARTY FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR
> CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS
> DOCUMENTATION, EVEN IF THE AUTHORS OR ANY OF THE ABOVE PARTIES HAVE BEEN
> ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
>
> JORN LIND-NIELSEN SPECIFICALLY DISCLAIM ANY WARRANTIES, INCLUDING, BUT NOT
> LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
> PARTICULAR PURPOSE. THE SOFTWARE PROVIDED HEREUNDER IS ON AN "AS IS" BASIS,
> AND THE AUTHORS AND DISTRIBUTORS HAVE NO OBLIGATION TO PROVIDE MAINTENANCE,
> SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.

## Sylvan

* Upstream: https://github.com/trolando/sylvan
* License: Apache-2.0
* Copyright: Tom van Dijk and others
* Used by: the optional Sylvan backends (`make init-sylvan`), linked statically

## Lace

* Upstream: https://github.com/trolando/lace
* License: Apache-2.0
* Copyright: Tom van Dijk and others
* Used by: pulled in with Sylvan as its work-stealing framework

## Components used only on the `flint-eval` branch

* **FLINT** — https://github.com/flintlib/flint — LGPL-3.0
* **MPFR** — https://www.mpfr.org/ — LGPL-3.0-or-later

Note that the `flint-eval` Makefile configures FLINT with
`--enable-static --disable-shared`. Distributing a *binary* built that way
carries LGPL relinking obligations; distributing source does not. If prebuilt
binaries of that configuration are ever published, link FLINT dynamically
instead.

## Components not linked into MEDUSA

* **SliQSim** — https://github.com/NTU-ALComLab/SliQSim — no license declared.
  Cloned only by `make make-sliqsim` for benchmark comparison and executed as a
  separate process, outside this repository. It is not linked into MEDUSA and
  no part of it is redistributed here.
