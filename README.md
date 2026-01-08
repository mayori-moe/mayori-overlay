# Mayori-Overlay

Mayori's Gentoo test overlay for some personal stuff.

Experimental changes to several ebuild from official gentoo repo.

I'm running Gentoo with `musl` libc. If you are not using musl, be careful about some patches in this repo named as `musl-compat` , which may not compile on non-musl systems.

## Package Notes:

[AMD ROCm](https://rocm.docs.amd.com) patches for LLVM 21

[Cherry-KDE](https://github.com/nullxception/cherry-kde) theme

[Fluent-icon-theme](https://github.com/vinceliuice/Fluent-icon-theme) red variant

[Sunshine](https://github.com/LizardByte/Sunshine) fixes for musl

[Valgrind](https://valgrind.org/) fixes for musl based on Chimera Linux [patch](https://github.com/chimera-linux/cports/blob/382e497f40f80e5528d714e6075a00b225459dfa/experimental/valgrind/patches/rtlib.patch)
