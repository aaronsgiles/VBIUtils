VBIUtils
========
Collection of utilities for capturing laserdisc VBI data via DirectShow.

These are some DirectShow plugins that I wrote to capture laserdisc vertical blank (VBI) data for the [MAME project](http://mamedev.org). There are several important bits of information contained in the VBI region, but video captures don't include it by default. 

What these plugins do is capture the VBI data separately and then append the data to the top of each frame.

Build Tools
===========
All the code has been built with Visual Studio 2008 under Windows 8.1. You must build the BaseClasses solution first. Make sure you select the non-MBCS versions to build.

Contents
========
The subdirectories in this repository are as follows:

* BaseClasses - contain an externally provided set of base classes for writing DirectShow plugins; these need to be compiled first.

* VBIAttach - this is the main plugin that samples and attaches the VBI data stream to the appropriate frame. I seem to recall some difficulty in matching the frame to the VBI data, so they might be out of sync at times. If no VBI data is provided, then a green bar is drawn at the top instead.

* VBIToVideo - I believe this is the original attempt at sampling the VBI data. It is vestigial at this point, but checked in for historical reasons.

* VirtualDubMods - These are some modifications I made to VirtualDub version 1.7.8 to automatically connect and invoke the VBIAttach plugin, based on a registry key setting. There are two .reg files included which can be double-clicked to enable/disable the appropriate key. Just search the source file for "VBIAttach" to see where the modifications are.

This setup was used with a particular laserdisc model to get some of the existing MAME laserdisc captures.

License
=======
Copyright (c) 2015, Aaron Giles
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
