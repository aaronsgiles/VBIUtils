//------------------------------------------------------------------------------
// File: VBIFont.h
//
// Copyright (c) Aaron Giles.  All rights reserved.
//------------------------------------------------------------------------------


#define ____ 0x0
#define ___X 0x1
#define __X_ 0x2
#define __XX 0x3
#define _X__ 0x4
#define _X_X 0x5
#define _XX_ 0x6
#define _XXX 0x7
#define X___ 0x8
#define X__X 0x9
#define X_X_ 0xa
#define X_XX 0xb
#define XX__ 0xc
#define XX_X 0xd
#define XXX_ 0xe
#define XXXX 0xf

const WCHAR *CVBIAttach::FontChars = L" 0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
const BYTE CVBIAttach::Font[][5] = 
{
	{	____,
		____,
		____,
		____,
		____ 
	},
	{	_XX_,
		X__X,
		X__X,
		X__X,
		_XX_ 
	},
	{	__X_,
		_XX_,
		__X_,
		__X_,
		_XXX 
	},
	{	XXX_,
		___X,
		_XX_,
		X___,
		XXXX 
	},
	{	XXX_,
		___X,
		_XX_,
		___X,
		XXX_ 
	},
	{	___X,
		X__X,
		XXXX,
		___X,
		___X 
	},
	{	XXXX,
		X___,
		XXX_,
		___X,
		XXX_ 
	},
	{	_XXX,
		X___,
		XXX_,
		X__X,
		_XX_ 
	},
	{	XXXX,
		___X,
		__X_,
		_X__,
		_X__ 
	},
	{	_XX_,
		X__X,
		_XX_,
		X__X,
		_XX_ 
	},
	{	_XX_,
		X__X,
		_XXX,
		___X,
		XXX_ 
	},
	{	_XX_,
		X__X,
		XXXX,
		X__X,
		X__X 
	},
	{	XXX_,
		X__X,
		XXX_,
		X__X,
		XXX_ 
	},
	{	_XXX,
		X___,
		X___,
		X___,
		_XXX 
	},
	{	XXX_,
		X__X,
		X__X,
		X__X,
		XXX_ 
	},
	{	XXXX,
		X___,
		XX__,
		X___,
		XXXX 
	},
	{	XXXX,
		X___,
		XX__,
		X___,
		X___ 
	},
	{	_XXX,
		X___,
		X_XX,
		X__X,
		_XXX 
	},
	{	X__X,
		X__X,
		XXXX,
		X__X,
		X__X 
	},
	{	_XXX,
		__X_,
		__X_,
		__X_,
		_XXX 
	},
	{	___X,
		___X,
		___X,
		X__X,
		_XX_ 
	},
	{	X__X,
		X_X_,
		XX__,
		X_X_,
		X__X 
	},
	{	X___,
		X___,
		X___,
		X___,
		XXXX 
	},
	{	X__X,
		XXXX,
		XXXX,
		X__X,
		X__X 
	},
	{	X__X,
		XX_X,
		X_XX,
		X__X,
		X__X 
	},
	{	_XX_,
		X__X,
		X__X,
		X__X,
		_XX_ 
	},
	{	XXX_,
		X__X,
		XXX_,
		X___,
		X___ 
	},
	{	_XX_,
		X__X,
		X__X,
		X_X_,
		_X_X 
	},
	{	XXX_,
		X__X,
		XXX_,
		X_X_,
		X__X 
	},
	{	_XXX,
		X___,
		_XX_,
		___X,
		XXX_ 
	},
	{	_XXX,
		__X_,
		__X_,
		__X_,
		__X_ 
	},
	{	X__X,
		X__X,
		X__X,
		X__X,
		_XX_ 
	},
	{	X__X,
		X__X,
		X__X,
		_XX_,
		_XX_ 
	},
	{	X__X,
		X__X,
		XXXX,
		XXXX,
		X__X 
	},
	{	X__X,
		X__X,
		_XX_,
		X__X,
		X__X 
	},
	{	X__X,
		X__X,
		_XXX,
		___X,
		_XX_ 
	},
	{	XXXX,
		___X,
		_XX_,
		X___,
		XXXX 
	}
};

