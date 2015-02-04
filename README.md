# VBIUtils
Collection of utilities for capturing laserdisc VBI data via DirectShow

These are some DirectShow plugins that I wrote to capture laserdisc vertical blank (VBI) data for the MAME project. There are several important bits of information contained in the VBI region, but video captures don't include it by default. 

What these plugins do is capture the VBI data separately and then append the data to the top of each frame.

The subdirectories in this repository are as follows:

* BaseClasses - contain an externally provided set of base classes for writing DirectShow plugins; these need to be compiled first.

* VBIAttach - this is the main plugin that samples and attaches the VBI data stream to the appropriate frame. I seem to recall some difficulty in matching the frame to the VBI data, so they might be out of sync at times. If no VBI data is provided, then a green bar is drawn at the top instead.

* VBIToVideo - I believe this is the original attempt at sampling the VBI data. It is vestigial at this point, but checked in for historical reasons.

* VirtualDubMods - These are some modifications I made to VirtualDub version 1.7.8 to automatically connect and invoke the VBIAttach plugin, based on a registry key setting. There are two .reg files included which can be double-clicked to enable/disable the appropriate key. Just search the source file for "VBIAttach" to see where the modifications are.

This setup was used with a particular laserdisc model to get some of the existing MAME laserdisc captures.
