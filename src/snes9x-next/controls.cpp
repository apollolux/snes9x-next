/***********************************************************************************
  Snes9x - Portable Super Nintendo Entertainment System (TM) emulator.

  (c) Copyright 1996 - 2002  Gary Henderson (gary.henderson@ntlworld.com),
                             Jerremy Koot (jkoot@snes9x.com)

  (c) Copyright 2002 - 2004  Matthew Kendora

  (c) Copyright 2002 - 2005  Peter Bortas (peter@bortas.org)

  (c) Copyright 2004 - 2005  Joel Yliluoma (http://iki.fi/bisqwit/)

  (c) Copyright 2001 - 2006  John Weidman (jweidman@slip.net)

  (c) Copyright 2002 - 2006  funkyass (funkyass@spam.shaw.ca),
                             Kris Bleakley (codeviolation@hotmail.com)

  (c) Copyright 2002 - 2010  Brad Jorsch (anomie@users.sourceforge.net),
                             Nach (n-a-c-h@users.sourceforge.net),
                             zones (kasumitokoduck@yahoo.com)

  (c) Copyright 2006 - 2007  nitsuja

  (c) Copyright 2009 - 2010  BearOso,
                             OV2


  BS-X C emulator code
  (c) Copyright 2005 - 2006  Dreamer Nom,
                             zones

  C4 x86 assembler and some C emulation code
  (c) Copyright 2000 - 2003  _Demo_ (_demo_@zsnes.com),
                             Nach,
                             zsKnight (zsknight@zsnes.com)

  C4 C++ code
  (c) Copyright 2003 - 2006  Brad Jorsch,
                             Nach

  DSP-1 emulator code
  (c) Copyright 1998 - 2006  _Demo_,
                             Andreas Naive (andreasnaive@gmail.com),
                             Gary Henderson,
                             Ivar (ivar@snes9x.com),
                             John Weidman,
                             Kris Bleakley,
                             Matthew Kendora,
                             Nach,
                             neviksti (neviksti@hotmail.com)

  DSP-2 emulator code
  (c) Copyright 2003         John Weidman,
                             Kris Bleakley,
                             Lord Nightmare (lord_nightmare@users.sourceforge.net),
                             Matthew Kendora,
                             neviksti

  DSP-3 emulator code
  (c) Copyright 2003 - 2006  John Weidman,
                             Kris Bleakley,
                             Lancer,
                             z80 gaiden

  DSP-4 emulator code
  (c) Copyright 2004 - 2006  Dreamer Nom,
                             John Weidman,
                             Kris Bleakley,
                             Nach,
                             z80 gaiden

  OBC1 emulator code
  (c) Copyright 2001 - 2004  zsKnight,
                             pagefault (pagefault@zsnes.com),
                             Kris Bleakley
                             Ported from x86 assembler to C by sanmaiwashi

  SPC7110 and RTC C++ emulator code used in 1.39-1.51
  (c) Copyright 2002         Matthew Kendora with research by
                             zsKnight,
                             John Weidman,
                             Dark Force

  SPC7110 and RTC C++ emulator code used in 1.52+
  (c) Copyright 2009         byuu,
                             neviksti

  S-DD1 C emulator code
  (c) Copyright 2003         Brad Jorsch with research by
                             Andreas Naive,
                             John Weidman

  S-RTC C emulator code
  (c) Copyright 2001 - 2006  byuu,
                             John Weidman

  ST010 C++ emulator code
  (c) Copyright 2003         Feather,
                             John Weidman,
                             Kris Bleakley,
                             Matthew Kendora

  Super FX x86 assembler emulator code
  (c) Copyright 1998 - 2003  _Demo_,
                             pagefault,
                             zsKnight

  Super FX C emulator code
  (c) Copyright 1997 - 1999  Ivar,
                             Gary Henderson,
                             John Weidman

  Sound emulator code used in 1.5-1.51
  (c) Copyright 1998 - 2003  Brad Martin
  (c) Copyright 1998 - 2006  Charles Bilyue'

  Sound emulator code used in 1.52+
  (c) Copyright 2004 - 2007  Shay Green (gblargg@gmail.com)

  SH assembler code partly based on x86 assembler code
  (c) Copyright 2002 - 2004  Marcus Comstedt (marcus@mc.pp.se)

  2xSaI filter
  (c) Copyright 1999 - 2001  Derek Liauw Kie Fa

  HQ2x, HQ3x, HQ4x filters
  (c) Copyright 2003         Maxim Stepin (maxim@hiend3d.com)

  NTSC filter
  (c) Copyright 2006 - 2007  Shay Green

  GTK+ GUI code
  (c) Copyright 2004 - 2010  BearOso

  Win32 GUI code
  (c) Copyright 2003 - 2006  blip,
                             funkyass,
                             Matthew Kendora,
                             Nach,
                             nitsuja
  (c) Copyright 2009 - 2010  OV2

  Mac OS GUI code
  (c) Copyright 1998 - 2001  John Stiles
  (c) Copyright 2001 - 2010  zones


  Specific ports contains the works of other authors. See headers in
  individual files.


  Snes9x homepage: http://www.snes9x.com/

  Permission to use, copy, modify and/or distribute Snes9x in both binary
  and source form, for non-commercial purposes, is hereby granted without
  fee, providing that this license information and copyright notice appear
  with all copies and any derived work.

  This software is provided 'as-is', without any express or implied
  warranty. In no event shall the authors be held liable for any damages
  arising from the use of this software or it's derivatives.

  Snes9x is freeware for PERSONAL USE only. Commercial users should
  seek permission of the copyright holders first. Commercial use includes,
  but is not limited to, charging money for Snes9x or software derived from
  Snes9x, including Snes9x or derivatives in commercial game bundles, and/or
  using Snes9x as a promotion for your commercial product.

  The copyright holders request that bug fixes and improvements to the code
  should be forwarded to them so everyone can benefit from the modifications
  in future versions.

  Super NES and Super Nintendo Entertainment System are trademarks of
  Nintendo Co., Limited and its subsidiary companies.
 ***********************************************************************************/

#include <vector>
#include <string>
#include <algorithm>

#include "snes9x.h"
#include "memmap.h"
#include "apu/apu.h"
#include "snapshot.h"
#include "controls.h"
#include "crosshairs.h"
#include "display.h"

using namespace	std;

#define NONE				(-2)
#define MP5				(-1)
#define JOYPAD0				0
#define JOYPAD1				1
#define JOYPAD2				2
#define JOYPAD3				3
#define JOYPAD4				4
#define JOYPAD5				5
#define JOYPAD6				6
#define JOYPAD7				7
#define MOUSE0				8
#define MOUSE1				9
#define SUPERSCOPE			10
#define ONE_JUSTIFIER			11
#define TWO_JUSTIFIERS			12
#define NUMCTLS				13 // This must be LAST

#define POLL_ALL			NUMCTLS

#define SUPERSCOPE_FIRE			0x80
#define SUPERSCOPE_CURSOR		0x40
#define SUPERSCOPE_TURBO		0x20
#define SUPERSCOPE_PAUSE		0x10
#define SUPERSCOPE_OFFSCREEN		0x02

#define JUSTIFIER_TRIGGER		0x80
#define JUSTIFIER_START			0x20
#define JUSTIFIER_SELECT		0x08

#define MAP_UNKNOWN			(-1)
#define MAP_NONE			0
#define MAP_BUTTON			1
#define MAP_AXIS			2
#define MAP_POINTER			3

#define FLAG_IOBIT0			(Memory.FillRAM[0x4213] & 0x40)
#define FLAG_IOBIT1			(Memory.FillRAM[0x4213] & 0x80)
#define FLAG_IOBIT(n)			((n) ? (FLAG_IOBIT1) : (FLAG_IOBIT0))

bool8	pad_read = 0, pad_read_last = 0;
uint8	read_idx[2 /* ports */][2 /* per port */];

struct crosshair
{
	uint8				set;
	uint8				img;
	uint8				fg, bg;
};

static struct
{
	uint8				delta_x, delta_y;
	int16				old_x, old_y;
	int16				cur_x, cur_y;
	uint8				buttons;
	uint32				ID;
	struct crosshair	crosshair;
}	mouse[2];

static struct
{
	int16				x, y;
	uint8				phys_buttons;
	uint8				next_buttons;
	uint8				read_buttons;
	uint32				ID;
	struct crosshair	crosshair;
}	superscope;

static struct
{
	int16				x[2], y[2];
	uint8				buttons;
	bool8				offscreen[2];
	uint32				ID[2];
	struct crosshair	crosshair[2];
}	justifier;

static int8 mp5[2][4];
extern s9xcommand_t			keymap[1024];
static bool8				FLAG_LATCH = FALSE;
static int32				curcontrollers[2] = { NONE,    NONE };
static int32				newcontrollers[2] = { JOYPAD0, NONE };
static char				buf[256];
extern uint16_t joypad[8];

static const char	*color_names[32] =
{
	"Trans",
	"Black",
	"25Grey",
	"50Grey",
	"75Grey",
	"White",
	"Red",
	"Orange",
	"Yellow",
	"Green",
	"Cyan",
	"Sky",
	"Blue",
	"Violet",
	"MagicPink",
	"Purple",
	NULL,
	"tBlack",
	"t25Grey",
	"t50Grey",
	"t75Grey",
	"tWhite",
	"tRed",
	"tOrange",
	"tYellow",
	"tGreen",
	"tCyan",
	"tSky",
	"tBlue",
	"tViolet",
	"tMagicPink",
	"tPurple"
};

// Note: these should be in asciibetical order!
#define THE_COMMANDS \
	S(ExitEmu), \
	S(ExitToMenu), \
	S(LoadFreezeFile), \
	S(Reset), \
	S(SaveFreezeFile), \
	S(SoftReset), \
	S(SwapJoypads), \
	S(ToggleEmuTurbo) \

#define S(x)	x

enum command_numbers
{
	THE_COMMANDS,
	LAST_COMMAND
};

#undef S
#define S(x)	#x

static const char	*command_names[LAST_COMMAND + 1] =
{
	THE_COMMANDS,
	NULL
};

#undef S
#undef THE_COMMANDS

static void DoGunLatch (int x, int y)
{
	x += 40;

	if (x > 295)
		x = 295;
	else
	if (x < 40)
		x = 40;

	if (y > PPU.ScreenHeight - 1)
		y = PPU.ScreenHeight - 1;
	else
	if (y < 0)
		y = 0;

	PPU.GunVLatch = (uint16) (y + 1);
	PPU.GunHLatch = (uint16) x;
}

static int maptype (int t)
{
	switch (t)
	{
		case S9xNoMapping:
			return (MAP_NONE);

		case S9xButtonJoypad:
		case S9xButtonMouse:
		case S9xButtonSuperscope:
		case S9xButtonJustifier:
		case S9xButtonCommand:
			return (MAP_BUTTON);
		case S9xPointer:
			return (MAP_POINTER);
		default:
			return (MAP_UNKNOWN);
	}
}

void S9xControlsReset (void)
{
	S9xControlsSoftReset();
	mouse[0].buttons  &= ~0x30;
	mouse[1].buttons  &= ~0x30;
	justifier.buttons &= ~JUSTIFIER_SELECT;
}

void S9xControlsSoftReset (void)
{
	for (int i = 0; i < 2; i++)
		for (int j = 0; j < 2; j++)
			read_idx[i][j]=0;

	FLAG_LATCH = FALSE;
}

void S9xUnmapAllControls (void)
{
	S9xControlsReset();

	for (int i = 0; i < 2; i++)
	{
		mouse[i].old_x = mouse[i].old_y = 0;
		mouse[i].cur_x = mouse[i].cur_y = 0;
		mouse[i].buttons = 1;
		mouse[i].ID = InvalidControlID;

		if (!(mouse[i].crosshair.set & 1))
			mouse[i].crosshair.img = 0; // no image for mouse because its only logical position is game-specific, not known by the emulator
		if (!(mouse[i].crosshair.set & 2))
			mouse[i].crosshair.fg  = 5;
		if (!(mouse[i].crosshair.set & 4))
			mouse[i].crosshair.bg  = 1;

		justifier.x[i] = justifier.y[i] = 0;
		justifier.offscreen[i] = 0;
		justifier.ID[i] = InvalidControlID;

		if (!(justifier.crosshair[i].set & 1))
			justifier.crosshair[i].img = 4;
		if (!(justifier.crosshair[i].set & 2))
			justifier.crosshair[i].fg  = i ? 14 : 12;
		if (!(justifier.crosshair[i].set & 4))
			justifier.crosshair[i].bg  = 1;
	}

	justifier.buttons = 0;

	superscope.x = superscope.y = 0;
	superscope.phys_buttons = 0;
	superscope.next_buttons = 0;
	superscope.read_buttons = 0;
	superscope.ID = InvalidControlID;

	if (!(superscope.crosshair.set & 1))
		superscope.crosshair.img = 2;
	if (!(superscope.crosshair.set & 2))
		superscope.crosshair.fg  = 5;
	if (!(superscope.crosshair.set & 4))
		superscope.crosshair.bg  = 1;
}

void S9xSetController (int port, enum controllers controller, int8 id1, int8 id2, int8 id3, int8 id4)
{
	if (port < 0 || port > 1)
		return;

	switch (controller)
	{
		case CTL_NONE:
			break;

		case CTL_JOYPAD:
			if (id1 < 0 || id1 > 7)
				break;

			newcontrollers[port] = JOYPAD0 + id1;
			return;

		case CTL_MOUSE:
			if (id1 < 0 || id1 > 1)
				break;
			if (!Settings.MouseMaster)
			{
				S9xMessage(S9X_CONFIG_INFO, S9X_ERROR, "Cannot select SNES Mouse: MouseMaster disabled");
				break;
			}

			newcontrollers[port] = MOUSE0 + id1;
			return;

		case CTL_SUPERSCOPE:
			if (!Settings.SuperScopeMaster)
			{
				S9xMessage(S9X_CONFIG_INFO, S9X_ERROR, "Cannot select SNES Superscope: SuperScopeMaster disabled");
				break;
			}

			newcontrollers[port] = SUPERSCOPE;
			return;

		case CTL_JUSTIFIER:
			if (id1 < 0 || id1 > 1)
				break;
			if (!Settings.JustifierMaster)
			{
				S9xMessage(S9X_CONFIG_INFO, S9X_ERROR, "Cannot select Konami Justifier: JustifierMaster disabled");
				break;
			}

			newcontrollers[port] = ONE_JUSTIFIER + id1;
			return;

		case CTL_MP5:
			if (id1 < -1 || id1 > 7)
				break;
			if (id2 < -1 || id2 > 7)
				break;
			if (id3 < -1 || id3 > 7)
				break;
			if (id4 < -1 || id4 > 7)
				break;
			if (!Settings.MultiPlayer5Master)
			{
				S9xMessage(S9X_CONFIG_INFO, S9X_ERROR, "Cannot select MP5: MultiPlayer5Master disabled");
				break;
			}

			newcontrollers[port] = MP5;
			mp5[port][0] = (id1 < 0) ? NONE : JOYPAD0 + id1;
			mp5[port][1] = (id2 < 0) ? NONE : JOYPAD0 + id2;
			mp5[port][2] = (id3 < 0) ? NONE : JOYPAD0 + id3;
			mp5[port][3] = (id4 < 0) ? NONE : JOYPAD0 + id4;
			return;

		default:
			fprintf(stderr, "Unknown controller type %d\n", controller);
			break;
	}

	newcontrollers[port] = NONE;
}

bool S9xVerifyControllers (void)
{
	bool	ret = false;
	int		port, i, used[NUMCTLS];

	for (i = 0; i < NUMCTLS; used[i++] = 0) ;

	for (port = 0; port < 2; port++)
	{
		switch (i = newcontrollers[port])
		{
			case MOUSE0:
			case MOUSE1:
				if (!Settings.MouseMaster)
				{
					S9xMessage(S9X_CONFIG_INFO, S9X_ERROR, "Cannot select SNES Mouse: MouseMaster disabled");
					newcontrollers[port] = NONE;
					ret = true;
					break;
				}

				if (used[i]++ > 0)
				{
					snprintf(buf, sizeof(buf), "Mouse%d used more than once! Disabling extra instances", i - MOUSE0 + 1);
					S9xMessage(S9X_CONFIG_INFO, S9X_ERROR, buf);
					newcontrollers[port] = NONE;
					ret = true;
					break;
				}

				break;

			case SUPERSCOPE:
				if (!Settings.SuperScopeMaster)
				{
					S9xMessage(S9X_CONFIG_INFO, S9X_ERROR, "Cannot select SNES Superscope: SuperScopeMaster disabled");
					newcontrollers[port] = NONE;
					ret = true;
					break;
				}

				if (used[i]++ > 0)
				{
					snprintf(buf, sizeof(buf), "Superscope used more than once! Disabling extra instances");
					S9xMessage(S9X_CONFIG_INFO, S9X_ERROR, buf);
					newcontrollers[port] = NONE;
					ret = true;
					break;
				}

				break;

			case ONE_JUSTIFIER:
			case TWO_JUSTIFIERS:
				if (!Settings.JustifierMaster)
				{
					S9xMessage(S9X_CONFIG_INFO, S9X_ERROR, "Cannot select Konami Justifier: JustifierMaster disabled");
					newcontrollers[port] = NONE;
					ret = true;
					break;
				}

				if (used[ONE_JUSTIFIER]++ > 0)
				{
					snprintf(buf, sizeof(buf), "Justifier used more than once! Disabling extra instances");
					S9xMessage(S9X_CONFIG_INFO, S9X_ERROR, buf);
					newcontrollers[port] = NONE;
					ret = true;
					break;
				}

				break;

			case MP5:
				if (!Settings.MultiPlayer5Master)
				{
					S9xMessage(S9X_CONFIG_INFO, S9X_ERROR, "Cannot select MP5: MultiPlayer5Master disabled");
					newcontrollers[port] = NONE;
					ret = true;
					break;
				}

				for (i = 0; i < 4; i++)
				{
					if (mp5[port][i] != NONE)
					{
						if (used[mp5[port][i] - JOYPAD0]++ > 0)
						{
							snprintf(buf, sizeof(buf), "Joypad%d used more than once! Disabling extra instances", mp5[port][i] - JOYPAD0 + 1);
							S9xMessage(S9X_CONFIG_INFO, S9X_ERROR, buf);
							mp5[port][i] = NONE;
							ret = true;
							break;
						}
					}
				}

				break;

			case JOYPAD0:
			case JOYPAD1:
			case JOYPAD2:
			case JOYPAD3:
			case JOYPAD4:
			case JOYPAD5:
			case JOYPAD6:
			case JOYPAD7:
				if (used[i - JOYPAD0]++ > 0)
				{
					snprintf(buf, sizeof(buf), "Joypad%d used more than once! Disabling extra instances", i - JOYPAD0 + 1);
					S9xMessage(S9X_CONFIG_INFO, S9X_ERROR, buf);
					newcontrollers[port] = NONE;
					ret = true;
					break;
				}

				break;

			default:
				break;
		}
	}

	return (ret);
}

static bool strless (const char *a, const char *b)
{
	return (strcmp(a, b) < 0);
}

static int findstr (const char *needle, const char **haystack, int numstr)
{
	const char	**r;

	r = lower_bound(haystack, haystack + numstr, needle, strless);
	if (r >= haystack + numstr || strcmp(needle, *r))
		return (-1);

	return (r - haystack);
}

s9xcommand_t S9xGetCommandT (const char *name)
{
	s9xcommand_t	cmd;
	int				i;
	const char		*s;

	ZeroMemory(&cmd, sizeof(cmd));
	cmd.type         = S9xBadMapping;
	cmd.multi_press  = 0;
	cmd.button_norpt = 0;

	if (!strcmp(name, "None"))
		cmd.type = S9xNoMapping;
	else
	if (!strncmp(name, "Joypad", 6))
	{
		if (name[6] < '1' || name[6] > '8' || name[7] != ' ')
			return (cmd);

		cmd.button.joypad.idx = name[6] - '1';
		s = name + 8;
		i = 0;

		if (i)
		{
			if (*s != ' ')
				return (cmd);
			s++;
		}

		i = 0;

		if (!strncmp(s, "Up",     2))	{ i |= SNES_UP_MASK;     s += 2; if (*s == '+') s++; }
		if (!strncmp(s, "Down",   4))	{ i |= SNES_DOWN_MASK;   s += 4; if (*s == '+') s++; }
		if (!strncmp(s, "Left",   4))	{ i |= SNES_LEFT_MASK;   s += 4; if (*s == '+') s++; }
		if (!strncmp(s, "Right",  5))	{ i |= SNES_RIGHT_MASK;  s += 5; if (*s == '+') s++; }

		if (*s == 'A')	{ i |= SNES_A_MASK;  s++; if (*s == '+') s++; }
		if (*s == 'B')	{ i |= SNES_B_MASK;  s++; if (*s == '+') s++; }
		if (*s == 'X')	{ i |= SNES_X_MASK;  s++; if (*s == '+') s++; }
		if (*s == 'Y')	{ i |= SNES_Y_MASK;  s++; if (*s == '+') s++; }
		if (*s == 'L')	{ i |= SNES_TL_MASK; s++; if (*s == '+') s++; }
		if (*s == 'R')	{ i |= SNES_TR_MASK; s++; if (*s == '+') s++; }

		if (!strncmp(s, "Start",  5))	{ i |= SNES_START_MASK;  s += 5; if (*s == '+') s++; }
		if (!strncmp(s, "Select", 6))	{ i |= SNES_SELECT_MASK; s += 6; }

		if (i == 0 || *s != 0 || *(s - 1) == '+')
			return (cmd);

		cmd.button.joypad.buttons = i;

		cmd.type = S9xButtonJoypad;
	}
	else
	if (!strncmp(name, "Mouse", 5))
	{
		if (name[5] < '1' || name[5] > '2' || name[6] != ' ')
			return (cmd);

		cmd.button.mouse.idx = name[5] - '1';
		s = name + 7;
		i = 0;

		if ((cmd.button.mouse.left  = (*s == 'L')))	s += i = 1;
		if ((cmd.button.mouse.right = (*s == 'R')))	s += i = 1;

		if (i == 0 || *s != 0)
			return (cmd);

		cmd.type = S9xButtonMouse;
	}
	else
	if (!strncmp(name, "Superscope ", 11))
	{
		s = name + 11;
		i = 0;

		if ((cmd.button.scope.aim_offscreen     = strncmp(s, "AimOffscreen", 12) ? 0 : 1))	{ s += i = 12; if (*s == ' ') s++; else if (*s != 0) return (cmd); }
		if ((cmd.button.scope.fire              = strncmp(s, "Fire",          4) ? 0 : 1))	{ s += i =  4; if (*s == '+') s++; }
		if ((cmd.button.scope.cursor            = strncmp(s, "Cursor",        6) ? 0 : 1))	{ s += i =  6; if (*s == '+') s++; }
		if ((cmd.button.scope.turbo             = strncmp(s, "ToggleTurbo",  11) ? 0 : 1))	{ s += i = 11; if (*s == '+') s++; }
		if ((cmd.button.scope.pause             = strncmp(s, "Pause",         5) ? 0 : 1))	{ s += i =  5; }

		if (i == 0 || *s != 0 || *(s - 1) == '+')
			return (cmd);

		cmd.type = S9xButtonSuperscope;
	}
	else
	if (!strncmp(name, "Justifier", 9))
	{
		if (name[9] < '1' || name[9] > '2' || name[10] != ' ')
			return (cmd);

		cmd.button.justifier.idx = name[9] - '1';
		s = name + 11;
		i = 0;

		if ((cmd.button.justifier.aim_offscreen = strncmp(s, "AimOffscreen", 12) ? 0 : 1))	{ s += i = 12; if (*s == ' ') s++; else if (*s != 0) return (cmd); }
		if ((cmd.button.justifier.trigger       = strncmp(s, "Trigger",       7) ? 0 : 1))	{ s += i =  7; if (*s == '+') s++; }
		if ((cmd.button.justifier.start         = strncmp(s, "Start",         5) ? 0 : 1))	{ s += i =  5; }

		if (i == 0 || *s != 0 || *(s - 1) == '+')
			return (cmd);

		cmd.type = S9xButtonJustifier;
	}
	else
	if (!strncmp(name, "Pointer ", 8))
	{
		s = name + 8;
		i = 0;

		if ((cmd.pointer.aim_mouse0     = strncmp(s, "Mouse1",      6) ? 0 : 1))	{ s += i =  6; if (*s == '+') s++; }
		if ((cmd.pointer.aim_mouse1     = strncmp(s, "Mouse2",      6) ? 0 : 1))	{ s += i =  6; if (*s == '+') s++; }
		if ((cmd.pointer.aim_scope      = strncmp(s, "Superscope", 10) ? 0 : 1))	{ s += i = 10; if (*s == '+') s++; }
		if ((cmd.pointer.aim_justifier0 = strncmp(s, "Justifier1", 10) ? 0 : 1))	{ s += i = 10; if (*s == '+') s++; }
		if ((cmd.pointer.aim_justifier1 = strncmp(s, "Justifier2", 10) ? 0 : 1))	{ s += i = 10; }

		if (i == 0 || *s != 0 || *(s - 1) == '+')
			return (cmd);

		cmd.type = S9xPointer;
	}
	else
	{
		i = findstr(name, command_names, LAST_COMMAND);
		if (i < 0)
			return (cmd);

		cmd.type = S9xButtonCommand;
		cmd.button.command = i;
	}

	return (cmd);
}

s9xcommand_t S9xGetMapping (uint32 id)
{
	return (keymap[id]);
}

static const char * maptypename (int t)
{
	switch (t)
	{
		case MAP_NONE:		return ("unmapped");
		case MAP_BUTTON:	return ("button");
		case MAP_POINTER:	return ("pointer");
		default:			return ("unknown");
	}
}

void S9xUnmapID (uint32 id)
{
	if (mouse[0].ID     == id)	mouse[0].ID     = InvalidControlID;
	if (mouse[1].ID     == id)	mouse[1].ID     = InvalidControlID;
	if (superscope.ID   == id)	superscope.ID   = InvalidControlID;
	if (justifier.ID[0] == id)	justifier.ID[0] = InvalidControlID;
	if (justifier.ID[1] == id)	justifier.ID[1] = InvalidControlID;
}

bool S9xMapButton (uint32 id, s9xcommand_t mapping, bool poll)
{
	int	t;

	if (id == InvalidControlID)
	{
		fprintf(stderr, "Cannot map InvalidControlID\n");
		return (false);
	}

	t = maptype(mapping.type);

	if (t == MAP_NONE)
	{
		S9xUnmapID(id);
		return (true);
	}

	if (t != MAP_BUTTON)
		return (false);

	t = maptype(S9xGetMapping(id).type);

	if (t != MAP_NONE && t != MAP_BUTTON)
		fprintf(stderr, "WARNING: Remapping ID 0x%08x from %s to button\n", id, maptypename(t));

	t = -1;

	S9xUnmapID(id);

	keymap[id] = mapping;

	return (true);
}

void S9xReportButton (uint32 id, bool pressed)
{
	if (keymap[id].type == S9xNoMapping)
		return;

	if (maptype(keymap[id].type) != MAP_BUTTON)
	{
		//fprintf(stderr, "ERROR: S9xReportButton called on %s ID 0x%08x\n", maptypename(maptype(keymap[id].type)), id);
		return;
	}

	keymap[id].button_norpt = pressed;

	S9xApplyCommand(keymap[id], pressed, 0);
}

bool S9xMapPointer (uint32 id, s9xcommand_t mapping, bool poll)
{
	int	t;

	if (id == InvalidControlID)
	{
		fprintf(stderr, "Cannot map InvalidControlID\n");
		return (false);
	}

	t = maptype(mapping.type);

	if (t == MAP_NONE)
	{
		S9xUnmapID(id);
		return (true);
	}

	if (t != MAP_POINTER)
		return (false);

	t = maptype(S9xGetMapping(id).type);

	if (t != MAP_NONE && t != MAP_POINTER)
		fprintf(stderr, "WARNING: Remapping ID 0x%08x from %s to pointer\n", id, maptypename(t));

	if (mapping.type == S9xPointer)
	{
		if (mapping.pointer.aim_mouse0 && mouse[0].ID != InvalidControlID && mouse[0].ID != id)
		{
			fprintf(stderr, "ERROR: Rejecting attempt to control Mouse1 with two pointers\n");
			return (false);
		}

		if (mapping.pointer.aim_mouse1 && mouse[1].ID != InvalidControlID && mouse[1].ID != id)
		{
			fprintf(stderr, "ERROR: Rejecting attempt to control Mouse2 with two pointers\n");
			return (false);
		}

		if (mapping.pointer.aim_scope && superscope.ID != InvalidControlID && superscope.ID != id)
		{
			fprintf(stderr, "ERROR: Rejecting attempt to control SuperScope with two pointers\n");
			return (false);
		}

		if (mapping.pointer.aim_justifier0 && justifier.ID[0] != InvalidControlID && justifier.ID[0] != id)
		{
			fprintf(stderr, "ERROR: Rejecting attempt to control Justifier1 with two pointers\n");
			return (false);
		}

		if (mapping.pointer.aim_justifier1 && justifier.ID[1] != InvalidControlID && justifier.ID[1] != id)
		{
			fprintf(stderr, "ERROR: Rejecting attempt to control Justifier2 with two pointers\n");
			return (false);
		}
	}

	S9xUnmapID(id);

	keymap[id] = mapping;

	if (mapping.pointer.aim_mouse0    )	mouse[0].ID     = id;
	if (mapping.pointer.aim_mouse1    )	mouse[1].ID     = id;
	if (mapping.pointer.aim_scope     )	superscope.ID   = id;
	if (mapping.pointer.aim_justifier0)	justifier.ID[0] = id;
	if (mapping.pointer.aim_justifier1)	justifier.ID[1] = id;

	return (true);
}

void S9xReportPointer (uint32 id, int16 x, int16 y)
{
	if (keymap[id].type == S9xNoMapping)
		return;

	if (maptype(keymap[id].type) != MAP_POINTER)
	{
		fprintf(stderr, "ERROR: S9xReportPointer called on %s ID 0x%08x\n", maptypename(maptype(keymap[id].type)), id);
		return;
	}

	S9xApplyCommand(keymap[id], x, y);
}

void S9xApplyCommand (s9xcommand_t cmd, int16 data1, int16 data2)
{
	int	i;

	switch (cmd.type)
	{
		case S9xNoMapping:
			return;
		case S9xButtonMouse:
			i = 0;
			if (cmd.button.mouse.left )	i |= 0x40;
			if (cmd.button.mouse.right)	i |= 0x80;

			if (data1)
				mouse[cmd.button.mouse.idx].buttons |=  i;
			else 
				mouse[cmd.button.mouse.idx].buttons &= ~i;

			return;

		case S9xButtonSuperscope:
			i = 0;
			if (cmd.button.scope.fire         )	i |= SUPERSCOPE_FIRE;
			if (cmd.button.scope.cursor       )	i |= SUPERSCOPE_CURSOR;
			if (cmd.button.scope.pause        )	i |= SUPERSCOPE_PAUSE;
			if (cmd.button.scope.aim_offscreen)	i |= SUPERSCOPE_OFFSCREEN;

			if (data1)
			{
				superscope.phys_buttons |= i;

				if (cmd.button.scope.turbo)
				{
					superscope.phys_buttons ^= SUPERSCOPE_TURBO;

					if (superscope.phys_buttons & SUPERSCOPE_TURBO)
						superscope.next_buttons |= superscope.phys_buttons & (SUPERSCOPE_FIRE | SUPERSCOPE_CURSOR);
					else
						superscope.next_buttons &= ~(SUPERSCOPE_FIRE | SUPERSCOPE_CURSOR);
				}

				superscope.next_buttons |= i & (SUPERSCOPE_FIRE | SUPERSCOPE_CURSOR | SUPERSCOPE_PAUSE);

					if ((superscope.next_buttons & (SUPERSCOPE_FIRE | SUPERSCOPE_CURSOR)) && curcontrollers[1] == SUPERSCOPE && !(superscope.phys_buttons & SUPERSCOPE_OFFSCREEN))
						DoGunLatch(superscope.x, superscope.y);
			}
			else
			{
				superscope.phys_buttons &= ~i;
				superscope.next_buttons &= SUPERSCOPE_OFFSCREEN | ~i;
			}

			return;

		case S9xButtonJustifier:
			i = 0;
			if (cmd.button.justifier.trigger)	i |= JUSTIFIER_TRIGGER;
			if (cmd.button.justifier.start  )	i |= JUSTIFIER_START;
			if (cmd.button.justifier.aim_offscreen)	justifier.offscreen[cmd.button.justifier.idx] = data1 ? 1 : 0;
			i >>= cmd.button.justifier.idx;

			if (data1)
				justifier.buttons |=  i;
			else
				justifier.buttons &= ~i;

			return;

		case S9xButtonCommand:
			if (data1)
			{
				switch ((enum command_numbers) (i = cmd.button.command))
				{
					case ExitEmu:
						S9xExit();
						break;
					case ExitToMenu:
						S9xExitToMenu();
						break;
					case Reset:
						S9xReset();
						break;

					case SoftReset:
						S9xSoftReset();
						break;
					case ToggleEmuTurbo:
						Settings.Throttled = !Settings.Throttled;
						S9xDoThrottling(Settings.Throttled);
						break;
					case LoadFreezeFile:
						S9xUnfreezeGame(S9xChooseFilename(TRUE));
						break;

					case SaveFreezeFile:
						S9xFreezeGame(S9xChooseFilename(FALSE));
						break;
					case SwapJoypads:
						if ((curcontrollers[0] != NONE && !(curcontrollers[0] >= JOYPAD0 && curcontrollers[0] <= JOYPAD7)))
						{
							S9xSetInfoString("Cannot swap pads: port 1 is not a joypad");
							break;
						}

						if ((curcontrollers[1] != NONE && !(curcontrollers[1] >= JOYPAD0 && curcontrollers[1] <= JOYPAD7)))
						{
							S9xSetInfoString("Cannot swap pads: port 2 is not a joypad");
							break;
						}

						newcontrollers[1] = curcontrollers[0];
						newcontrollers[0] = curcontrollers[1];

						strcpy(buf, "Swap pads: P1=");
						i = 14;
						if (newcontrollers[0] == NONE)
						{
							strcpy(buf + i, "<none>");
							i += 6;
						}
						else
						{
							sprintf(buf + i, "Joypad%d", newcontrollers[0] - JOYPAD0 + 1);
							i += 7;
						}

						strcpy(buf + i, " P2=");
						i += 4;
						if (newcontrollers[1] == NONE)
							strcpy(buf + i, "<none>");
						else
							sprintf(buf + i, "Joypad%d", newcontrollers[1] - JOYPAD0 + 1);

						S9xSetInfoString(buf);
						break;
					case LAST_COMMAND:
						break;
				}
			}

			return;

		case S9xPointer:
			if (cmd.pointer.aim_mouse0)
			{
				mouse[0].cur_x = data1;
				mouse[0].cur_y = data2;
			}

			if (cmd.pointer.aim_mouse1)
			{
				mouse[1].cur_x = data1;
				mouse[1].cur_y = data2;
			}

			if (cmd.pointer.aim_scope)
			{
				superscope.x   = data1;
				superscope.y   = data2;
			}

			if (cmd.pointer.aim_justifier0)
			{
				justifier.x[0] = data1;
				justifier.y[0] = data2;
			}

			if (cmd.pointer.aim_justifier1)
			{
				justifier.x[1] = data1;
				justifier.y[1] = data2;
			}

			return;
		default:
			fprintf(stderr, "WARNING: Unknown command type %d\n", cmd.type);
			return;
	}
}

static void UpdatePolledMouse (int i)
{
	int16	j;

	j = mouse[i - MOUSE0].cur_x - mouse[i - MOUSE0].old_x;

	if (j < -127)
	{
		mouse[i - MOUSE0].delta_x = 0xff;
		mouse[i - MOUSE0].old_x -= 127;
	}
	else
	if (j < 0)
	{
		mouse[i - MOUSE0].delta_x = 0x80 | -j;
		mouse[i - MOUSE0].old_x = mouse[i - MOUSE0].cur_x;
	}
	else
	if (j > 127)
	{
		mouse[i - MOUSE0].delta_x = 0x7f;
		mouse[i - MOUSE0].old_x += 127;
	}
	else
	{
		mouse[i - MOUSE0].delta_x = (uint8) j;
		mouse[i - MOUSE0].old_x = mouse[i - MOUSE0].cur_x;
	}

	j = mouse[i - MOUSE0].cur_y - mouse[i - MOUSE0].old_y;

	if (j < -127)
	{
		mouse[i - MOUSE0].delta_y = 0xff;
		mouse[i - MOUSE0].old_y -= 127;
	}
	else
	if (j < 0)
	{
		mouse[i - MOUSE0].delta_y = 0x80 | -j;
		mouse[i - MOUSE0].old_y = mouse[i - MOUSE0].cur_y;
	}
	else
	if (j > 127)
	{
		mouse[i - MOUSE0].delta_y = 0x7f;
		mouse[i - MOUSE0].old_y += 127;
	}
	else
	{
		mouse[i - MOUSE0].delta_y = (uint8) j;
		mouse[i - MOUSE0].old_y = mouse[i - MOUSE0].cur_y;
	}
}

void S9xSetJoypadLatch (bool latch)
{
	if (!latch && FLAG_LATCH)
	{
		// 1 written, 'plug in' new controllers now
		curcontrollers[0] = newcontrollers[0];
		curcontrollers[1] = newcontrollers[1];
	}

	if (latch && !FLAG_LATCH)
	{
		int	i;

		for (int n = 0; n < 2; n++)
		{
			read_idx[n][0] = 0;
			read_idx[n][1] = 0;

			switch (i = curcontrollers[n])
			{
				case MP5:
				case JOYPAD0:
				case JOYPAD1:
				case JOYPAD2:
				case JOYPAD3:
				case JOYPAD4:
				case JOYPAD5:
				case JOYPAD6:
				case JOYPAD7:
					break;
				case MOUSE0:
				case MOUSE1:
					UpdatePolledMouse(i);
					break;

				case SUPERSCOPE:
					if (superscope.next_buttons & SUPERSCOPE_FIRE)
					{
						superscope.next_buttons &= ~SUPERSCOPE_TURBO;
						superscope.next_buttons |= superscope.phys_buttons & SUPERSCOPE_TURBO;
					}

					if (superscope.next_buttons & (SUPERSCOPE_FIRE | SUPERSCOPE_CURSOR))
					{
						superscope.next_buttons &= ~SUPERSCOPE_OFFSCREEN;
						superscope.next_buttons |= superscope.phys_buttons & SUPERSCOPE_OFFSCREEN;
					}

					superscope.read_buttons = superscope.next_buttons;

					superscope.next_buttons &= ~SUPERSCOPE_PAUSE;
					if (!(superscope.phys_buttons & SUPERSCOPE_TURBO))
						superscope.next_buttons &= ~(SUPERSCOPE_CURSOR | SUPERSCOPE_FIRE);

					break;

				case TWO_JUSTIFIERS:
					// fall through
				case ONE_JUSTIFIER:
					justifier.buttons ^= JUSTIFIER_SELECT;
					break;

				default:
					break;
			}
		}
	}

	FLAG_LATCH = latch;
}


uint8 S9xReadJOYSERn (int n)
{
	int	i, j, r;

	if (n > 1)
		n -= 0x4016;

	uint8	bits = (OpenBus & ~3) | ((n == 1) ? 0x1c : 0);

	if (FLAG_LATCH)
	{
		switch (i = curcontrollers[n])
		{
			case MP5:
				return (bits | 2);

			case JOYPAD0:
			case JOYPAD1:
			case JOYPAD2:
			case JOYPAD3:
			case JOYPAD4:
			case JOYPAD5:
			case JOYPAD6:
			case JOYPAD7:
				return (bits | ((joypad[i - JOYPAD0] & 0x8000) ? 1 : 0));

			case MOUSE0:
			case MOUSE1:
				mouse[i - MOUSE0].buttons += 0x10;
				if ((mouse[i - MOUSE0].buttons & 0x30) == 0x30)
					mouse[i - MOUSE0].buttons &= 0xcf;
				return (bits);

			case SUPERSCOPE:
				return (bits | ((superscope.read_buttons & 0x80) ? 1 : 0));

			case ONE_JUSTIFIER:
			case TWO_JUSTIFIERS:
				return (bits);

			default:
				return (bits);
		}
	}
	else
	{
		switch (i = curcontrollers[n])
		{
			case MP5:
				r = read_idx[n][FLAG_IOBIT(n) ? 0 : 1]++;
				j = FLAG_IOBIT(n) ? 0 : 2;

				for (i = 0; i < 2; i++, j++)
				{
					if (mp5[n][j] == NONE)
						continue;
					if (r >= 16)
						bits |= 1 << i;
					else
						bits |= ((joypad[mp5[n][j] - JOYPAD0] & (0x8000 >> r)) ? 1 : 0) << i;
				}

				return (bits);

			case JOYPAD0:
			case JOYPAD1:
			case JOYPAD2:
			case JOYPAD3:
			case JOYPAD4:
			case JOYPAD5:
			case JOYPAD6:
			case JOYPAD7:
				if (read_idx[n][0] >= 16)
				{
					read_idx[n][0]++;
					return (bits | 1);
				}
				else
					return (bits | ((joypad[i - JOYPAD0] & (0x8000 >> read_idx[n][0]++)) ? 1 : 0));

			case MOUSE0:
			case MOUSE1:
				if (read_idx[n][0] < 8)
				{
					read_idx[n][0]++;
					return (bits);
				}
				else
				if (read_idx[n][0] < 16)
					return (bits | ((mouse[i - MOUSE0].buttons & (0x8000     >> read_idx[n][0]++)) ? 1 : 0));
				else
				if (read_idx[n][0] < 24)
					return (bits | ((mouse[i - MOUSE0].delta_y & (0x800000   >> read_idx[n][0]++)) ? 1 : 0));
				else
				if (read_idx[n][0] < 32)
					return (bits | ((mouse[i - MOUSE0].delta_x & (0x80000000 >> read_idx[n][0]++)) ? 1 : 0));
				else
				{
					read_idx[n][0]++;
					return (bits | 1);
				}

			case SUPERSCOPE:
				if (read_idx[n][0] < 8)
					return (bits | ((superscope.read_buttons & (0x80 >> read_idx[n][0]++)) ? 1 : 0));
				else
				{
					read_idx[n][0]++;
					return (bits | 1);
				}

			case ONE_JUSTIFIER:
				if (read_idx[n][0] < 24)
					return (bits | ((0xaa7000 >> read_idx[n][0]++) & 1));
				else
				if (read_idx[n][0] < 32)
					return (bits | ((justifier.buttons & (JUSTIFIER_TRIGGER | JUSTIFIER_START | JUSTIFIER_SELECT) & (0x80000000 >> read_idx[n][0]++)) ? 1 : 0));
				else
				{
					read_idx[n][0]++;
					return (bits | 1);
				}

			case TWO_JUSTIFIERS:
				if (read_idx[n][0] < 24)
					return (bits | ((0xaa7000 >> read_idx[n][0]++) & 1));
				else
				if (read_idx[n][0] < 32)
					return (bits | ((justifier.buttons & (0x80000000 >> read_idx[n][0]++)) ? 1 : 0));
				else
				{
					read_idx[n][0]++;
					return (bits | 1);
				}

			default:
				read_idx[n][0]++;
				return (bits);
		}
	}
}

void S9xDoAutoJoypad (void)
{
	int	i, j;

	S9xSetJoypadLatch(1);
	S9xSetJoypadLatch(0);

	for (int n = 0; n < 2; n++)
	{
		switch (i = curcontrollers[n])
		{
			case MP5:
				j = FLAG_IOBIT(n) ? 0 : 2;
				for (i = 0; i < 2; i++, j++)
				{
					if (mp5[n][j] == NONE)
						WRITE_WORD(Memory.FillRAM + 0x4218 + n * 2 + i * 4, 0);
					else
						WRITE_WORD(Memory.FillRAM + 0x4218 + n * 2 + i * 4, joypad[mp5[n][j] - JOYPAD0]);
				}

				read_idx[n][FLAG_IOBIT(n) ? 0 : 1] = 16;
				break;

			case JOYPAD0:
			case JOYPAD1:
			case JOYPAD2:
			case JOYPAD3:
			case JOYPAD4:
			case JOYPAD5:
			case JOYPAD6:
			case JOYPAD7:
				read_idx[n][0] = 16;
				WRITE_WORD(Memory.FillRAM + 0x4218 + n * 2, joypad[i - JOYPAD0]);
				WRITE_WORD(Memory.FillRAM + 0x421c + n * 2, 0);
				break;

			case MOUSE0:
			case MOUSE1:
				read_idx[n][0] = 16;
				WRITE_WORD(Memory.FillRAM + 0x4218 + n * 2, mouse[i - MOUSE0].buttons);
				WRITE_WORD(Memory.FillRAM + 0x421c + n * 2, 0);
				break;

			case SUPERSCOPE:
				read_idx[n][0] = 16;
				Memory.FillRAM[0x4218 + n * 2] = 0xff;
				Memory.FillRAM[0x4219 + n * 2] = superscope.read_buttons;
				WRITE_WORD(Memory.FillRAM + 0x421c + n * 2, 0);
				break;

			case ONE_JUSTIFIER:
			case TWO_JUSTIFIERS:
				read_idx[n][0] = 16;
				WRITE_WORD(Memory.FillRAM + 0x4218 + n * 2, 0x000e);
				WRITE_WORD(Memory.FillRAM + 0x421c + n * 2, 0);
				break;

			default:
				WRITE_WORD(Memory.FillRAM + 0x4218 + n * 2, 0);
				WRITE_WORD(Memory.FillRAM + 0x421c + n * 2, 0);
				break;
		}
	}
}

void S9xControlEOF (void)
{
	PPU.GunVLatch = 1000; // i.e., never latch
	PPU.GunHLatch = 0;

	if(pad_read)
	{
		struct crosshair	*c;
		int			i;

		for (int n = 0; n < 2; n++)
		{
			switch (i = curcontrollers[n])
			{
				case MP5:
				case JOYPAD0:
				case JOYPAD1:
				case JOYPAD2:
				case JOYPAD3:
				case JOYPAD4:
				case JOYPAD5:
				case JOYPAD6:
				case JOYPAD7:
					break;
				case MOUSE0:
				case MOUSE1:
					c = &mouse[i - MOUSE0].crosshair;
					S9xDrawCrosshair(S9xGetCrosshair(c->img), c->fg, c->bg, mouse[i - MOUSE0].cur_x, mouse[i - MOUSE0].cur_y);
					break;

				case SUPERSCOPE:
					if (n == 1 && !(superscope.phys_buttons & SUPERSCOPE_OFFSCREEN))
					{
						if (superscope.next_buttons & (SUPERSCOPE_FIRE | SUPERSCOPE_CURSOR))
							DoGunLatch(superscope.x, superscope.y);

						c = &superscope.crosshair;
						S9xDrawCrosshair(S9xGetCrosshair(c->img), c->fg, c->bg, superscope.x, superscope.y);
					}

					break;

				case TWO_JUSTIFIERS:
					if (n == 1 && !justifier.offscreen[1])
					{
						c = &justifier.crosshair[1];
						S9xDrawCrosshair(S9xGetCrosshair(c->img), c->fg, c->bg, justifier.x[1], justifier.y[1]);
					}

					i = (justifier.buttons & JUSTIFIER_SELECT) ?  1 : 0;
					goto do_justifier;

				case ONE_JUSTIFIER:
					i = (justifier.buttons & JUSTIFIER_SELECT) ? -1 : 0;

do_justifier:
					if (n == 1)
					{
						if (i >= 0 && !justifier.offscreen[i])
							DoGunLatch(justifier.x[i], justifier.y[i]);

						if (!justifier.offscreen[0])
						{
							c = &justifier.crosshair[0];
							S9xDrawCrosshair(S9xGetCrosshair(c->img), c->fg, c->bg, justifier.x[0], justifier.y[0]);
						}
					}

					break;

				default:
					break;
			}
		}
	}

	pad_read_last = pad_read;
	pad_read      = false;
}

void S9xSetControllerCrosshair (enum crosscontrols ctl, int8 idx, const char *fg, const char *bg)
{
	struct crosshair	*c;
	int8				fgcolor = -1, bgcolor = -1;
	int					i, j;

	if (idx < -1 || idx > 31)
	{
		fprintf(stderr, "S9xSetControllerCrosshair() called with invalid index\n");
		return;
	}

	switch (ctl)
	{
		case X_MOUSE1:		c = &mouse[0].crosshair;		break;
		case X_MOUSE2:		c = &mouse[1].crosshair;		break;
		case X_SUPERSCOPE:	c = &superscope.crosshair;		break;
		case X_JUSTIFIER1:	c = &justifier.crosshair[0];	break;
		case X_JUSTIFIER2:	c = &justifier.crosshair[1];	break;
		default:
			fprintf(stderr, "S9xSetControllerCrosshair() called with an invalid controller ID %d\n", ctl);
			return;
	}

	if (fg)
	{
		fgcolor = 0;
		if (*fg == 't')
		{
			fg++;
			fgcolor = 16;
		}

		for (i = 0; i < 16; i++)
		{
			for (j = 0; color_names[i][j] && fg[j] == color_names[i][j]; j++) ;
				if (isalnum(fg[j]))
					continue;

			if (!color_names[i][j])
				break;
		}

		fgcolor |= i;
		if (i > 15 || fgcolor == 16)
		{
			fprintf(stderr, "S9xSetControllerCrosshair() called with invalid fgcolor\n");
			return;
		}
	}

	if (bg)
	{
		bgcolor = 0;
		if (*bg == 't')
		{
			bg++;
			bgcolor = 16;
		}

		for (i = 0; i < 16; i++)
		{
			for (j = 0; color_names[i][j] && bg[j] == color_names[i][j]; j++) ;
				if (isalnum(bg[j]))
					continue;

			if (!color_names[i][j])
				break;
		}

		bgcolor |= i;
		if (i > 15 || bgcolor == 16)
		{
			fprintf(stderr, "S9xSetControllerCrosshair() called with invalid bgcolor\n");
			return;
		}
	}

	if (idx != -1)
	{
		c->set |= 1;
		c->img = idx;
	}

	if (fgcolor != -1)
	{
		c->set |= 2;
		c->fg = fgcolor;
	}

	if (bgcolor != -1)
	{
		c->set |= 4;
		c->bg = bgcolor;
	}
}

void S9xControlPreSaveState (struct SControlSnapshot *s)
{
	ZeroMemory(s, sizeof(*s));
	s->ver = 3;

	s->port1_read_idx[0] = read_idx[0][0];
	s->port2_read_idx[0] = read_idx[1][0];

	s->port1_read_idx[1] = read_idx[0][1];
	s->port2_read_idx[1] = read_idx[1][1];

	s->mouse_speed[0] = (mouse[0].buttons & 0x30) >> 4;
	s->mouse_speed[1] = (mouse[1].buttons & 0x30) >> 4;

	s->justifier_select = ((justifier.buttons & JUSTIFIER_SELECT) ? 1 : 0);

#define COPY(x)	{ memcpy((char *) s->internal + i, &(x), sizeof(x)); i += sizeof(x); }

	int	i = 0;

	for (int j = 0; j < 8; j++)
		COPY(joypad[j]);

	for (int j = 0; j < 2; j++)
	{
		COPY(mouse[j].delta_x);
		COPY(mouse[j].delta_y);
		COPY(mouse[j].old_x);
		COPY(mouse[j].old_y);
		COPY(mouse[j].cur_x);
		COPY(mouse[j].cur_y);
		COPY(mouse[j].buttons);
	}

	COPY(superscope.x);
	COPY(superscope.y);
	COPY(superscope.phys_buttons);
	COPY(superscope.next_buttons);
	COPY(superscope.read_buttons);

	COPY(justifier.x[0]);
	COPY(justifier.x[1]);
	COPY(justifier.y[0]);
	COPY(justifier.y[1]);
	COPY(justifier.buttons);

	COPY(justifier.offscreen[0]);
	COPY(justifier.offscreen[1]);

	COPY(mp5[0][0]);
	COPY(mp5[0][1]);

	COPY(mp5[1][0]);
	COPY(mp5[1][1]);

#undef COPY

	s->pad_read      = pad_read;
	s->pad_read_last = pad_read_last;
}

void S9xControlPostLoadState (struct SControlSnapshot *s)
{
	if (curcontrollers[0] == MP5 && s->ver < 1)
	{
		// Crap. Old snes9x didn't support this.
		S9xMessage(S9X_WARNING, S9X_FREEZE_FILE_INFO, "Old savestate has no support for MP5 in port 1.");
		newcontrollers[0] = curcontrollers[0];
		curcontrollers[0] = mp5[0][0];
	}

	read_idx[0][0] = s->port1_read_idx[0];
	read_idx[1][0] = s->port2_read_idx[0];

	read_idx[0][1] = s->port1_read_idx[1];
	read_idx[1][1] = s->port2_read_idx[1];

	mouse[0].buttons |= (s->mouse_speed[0] & 3) << 4;
	mouse[1].buttons |= (s->mouse_speed[1] & 3) << 4;

	if (s->justifier_select & 1)
		justifier.buttons |=  JUSTIFIER_SELECT;
	else
		justifier.buttons &= ~JUSTIFIER_SELECT;

	FLAG_LATCH = (Memory.FillRAM[0x4016] & 1) == 1;

	if (s->ver > 1)
	{
	#define COPY(x)	{ memcpy(&(x), (char *) s->internal + i, sizeof(x)); i += sizeof(x); }

		int	i = 0;

		for (int j = 0; j < 8; j++)
			COPY(joypad[j]);

		for (int j = 0; j < 2; j++)
		{
			COPY(mouse[j].delta_x);
			COPY(mouse[j].delta_y);
			COPY(mouse[j].old_x);
			COPY(mouse[j].old_y);
			COPY(mouse[j].cur_x);
			COPY(mouse[j].cur_y);
			COPY(mouse[j].buttons);
		}

		COPY(superscope.x);
		COPY(superscope.y);
		COPY(superscope.phys_buttons);
		COPY(superscope.next_buttons);
		COPY(superscope.read_buttons);

		COPY(justifier.x[0]);
		COPY(justifier.x[1]);
		COPY(justifier.y[0]);
		COPY(justifier.y[1]);
		COPY(justifier.buttons);

		COPY(justifier.offscreen[0]);
		COPY(justifier.offscreen[1]);

		COPY(mp5[0][0]);
		COPY(mp5[0][1]);

		COPY(mp5[1][0]);
		COPY(mp5[1][1]);

	#undef COPY
	}

	if (s->ver > 2)
	{
		pad_read      = s->pad_read;
		pad_read_last = s->pad_read_last;
	}
}
