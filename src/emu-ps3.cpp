/******************************************************************************* 
 * emu-ps3.cpp - SNES9xNext PS3
 *
 *  Created on: May 4, 2011
********************************************************************************/
#include <sys/return_code.h>

#include <sys/process.h>
#include <cell/sysmodule.h>
#include <sysutil/sysutil_sysparam.h>
#include <sysutil/sysutil_screenshot.h>
#include <sysutil/sysutil_msgdialog.h>
#include <cell/cell_fs.h>

#if(CELL_SDK_VERSION > 0x340000)
#include <sysutil/sysutil_bgmplayback.h>
#endif

#include <sys/spu_initialize.h>

#ifndef __SNC__
#include <stdio.h>
#endif

#include <pthread.h>
#include "snes9x/snes9x.h"
#include "snes9x/memmap.h"
#include "snes9x/apu/apu.h"
#include "snes9x/ppu.h"
#include "snes9x/controls.h"
#include "snes9x/cheats.h"
#include "snes9x/display.h"

#include "snes_state/config_file.h"

#include "snes9x/snapshot.h"

#ifdef PS3_PROFILING
#include "cellframework/network-stdio/net_stdio.h"
#endif

#include "cellframework2/input/mouse_input.h"
#include "cellframework2/input/pad_input.h"
#include "emu-ps3.hpp"
#include "menu.hpp"

SYS_PROCESS_PARAM(1001, 0x100000)

/* PS3 frontend variables */

char contentInfoPath[MAX_PATH_LENGTH];
char usrDirPath[MAX_PATH_LENGTH];
char DEFAULT_PRESET_FILE[MAX_PATH_LENGTH];
char DEFAULT_BORDER_FILE[MAX_PATH_LENGTH];
char DEFAULT_MENU_BORDER_FILE[MAX_PATH_LENGTH];
char GAME_AWARE_SHADER_DIR_PATH[MAX_PATH_LENGTH];
char PRESETS_DIR_PATH[MAX_PATH_LENGTH];
char BORDERS_DIR_PATH[MAX_PATH_LENGTH];
char SHADERS_DIR_PATH[MAX_PATH_LENGTH];
char DEFAULT_SHADER_FILE[MAX_PATH_LENGTH];
char DEFAULT_MENU_SHADER_FILE[MAX_PATH_LENGTH];
char SYS_CONFIG_FILE[MAX_PATH_LENGTH];
#ifdef MULTIMAN_SUPPORT
char MULTIMAN_GAME_TO_BOOT[MAX_PATH_LENGTH];
#endif

pthread_mutex_t audio_lock = PTHREAD_MUTEX_INITIALIZER;
PS3Graphics* Graphics;
cell_audio_handle_t audio_handle;
const struct cell_audio_driver *audio_driver = &cell_audio_audioport;
oskutil_params oskutil_handle;
PS3InputList PS3Input;

uint64_t mode_switch = MODE_MENU;						// mode the main loop is in

static uint32_t is_running;							// is the ROM currently running in the emulator?
static bool is_ingame_menu_running;						// is the ingame menu currently running?
bool return_to_MM = false;							// launch multiMAN on exit if ROM is passed
bool emulator_initialized = false;						// is the emulator loaded?
bool need_load_rom = false;							// need to load the current rom
char* current_rom = NULL;							// current filename of the ROM being emulated
bool dialog_is_running;								// is a dialog screen currently running?
char special_action_msg[256];				
static uint32_t special_action_msg_expired;		// time at which the message no longer needs to be overlaid onscreen

uint64_t ingame_menu_item = 0;
bool need_load_settings = true;							// needs settings loaded
static uint32_t controller_settings = NO_JOYSTICKS;					// controller mode to run emulator in

//emulator-specific
s9xcommand_t keymap[1024];
extern uint16_t joypad[8];

/* PS3 frontend - save state/emulator SRAM related functions */

#define emulator_decrement_current_save_state_slot() \
	if (Settings.CurrentSaveStateSlot != MIN_SAVE_STATE_SLOT) \
	{ \
		Settings.CurrentSaveStateSlot--; \
	} \
	snprintf(special_action_msg, sizeof(special_action_msg), "Save state slot changed to: #%d", Settings.CurrentSaveStateSlot); \
	special_action_msg_expired = Graphics->SetTextMessageSpeed();

#define emulator_increment_current_save_state_slot() \
	Settings.CurrentSaveStateSlot++; \
	\
	snprintf(special_action_msg, sizeof(special_action_msg), "Save state slot changed to: #%d", Settings.CurrentSaveStateSlot); \
	special_action_msg_expired = Graphics->SetTextMessageSpeed();

#define emulator_load_current_save_state_slot() \
	/* emulator-specific */ \
	int ret = S9xUnfreezeGame(S9xChooseFilename(FALSE)); \
	if(ret) \
		snprintf(special_action_msg, sizeof(special_action_msg), "Loaded save state slot #%d", Settings.CurrentSaveStateSlot); \
	else \
		snprintf(special_action_msg, sizeof(special_action_msg), "Can't load from save state slot #%d", Settings.CurrentSaveStateSlot); \
	special_action_msg_expired = Graphics->SetTextMessageSpeed();

#define emulator_save_current_save_state_slot() \
	/* emulator-specific */ \
	S9xFreezeGame(S9xChooseFilename(FALSE)); \
	snprintf(special_action_msg, sizeof(special_action_msg), "Saved to save state slot #%d", Settings.CurrentSaveStateSlot); \
	special_action_msg_expired = Graphics->SetTextMessageSpeed();

/* emulator-specific */

#define S9xReportButton(id, pressed) \
      if(keymap[id].type == S9xButtonJoypad) \
      joypad[keymap[id].button.joypad.idx] = ((joypad[keymap[id].button.joypad.idx] | keymap[id].button.joypad.buttons) & (((pressed) | -(pressed)) >> 31)) | ((joypad[keymap[id].button.joypad.idx] & ~keymap[id].button.joypad.buttons) & ~(((pressed) | -(pressed)) >> 31)); \
      else if(pressed)\
         S9xApplyCommand_Button(keymap[id], pressed);

#define S9xReportButton_Mouse(id, pressed) \
      if(keymap[id].type == S9xButtonJoypad) \
      joypad[keymap[id].button.joypad.idx] = ((joypad[keymap[id].button.joypad.idx] | keymap[id].button.joypad.buttons) & (((pressed) | -(pressed)) >> 31)) | ((joypad[keymap[id].button.joypad.idx] & ~keymap[id].button.joypad.buttons) & ~(((pressed) | -(pressed)) >> 31)); \
      else if(keymap[id].type == S9xButtonMouse || pressed) \
         S9xApplyCommand_Button(keymap[id], pressed);

#define create_msg_dialog(text, callback) \
	dialog_is_running = true; \
	cellMsgDialogOpen2(CELL_MSGDIALOG_DIALOG_TYPE_NORMAL|CELL_MSGDIALOG_TYPE_BG_VISIBLE|\
	CELL_MSGDIALOG_TYPE_BUTTON_TYPE_YESNO|CELL_MSGDIALOG_TYPE_DISABLE_CANCEL_OFF|\
	CELL_MSGDIALOG_TYPE_DEFAULT_CURSOR_YES,\
	""#text, callback,NULL,NULL);

#define create_error_dialog(text, callback) \
	dialog_is_running = true; \
	cellMsgDialogOpen2(CELL_MSGDIALOG_TYPE_SE_TYPE_ERROR|CELL_MSGDIALOG_TYPE_BG_VISIBLE|\
	CELL_MSGDIALOG_TYPE_BUTTON_TYPE_NONE|CELL_MSGDIALOG_TYPE_DISABLE_CANCEL_OFF|\
	CELL_MSGDIALOG_TYPE_DEFAULT_CURSOR_OK,\
	""#text, callback,NULL,NULL);

void callback_sysutil_exit(uint64_t status, uint64_t param, void *userdata)
{
	(void) param;
	(void) userdata;

	switch (status)
	{
		case CELL_SYSUTIL_REQUEST_EXITGAME:
			menu_is_running = 0;
			is_running = 0;
			is_ingame_menu_running = 0;
			return_to_MM = false;
			mode_switch = MODE_EXIT;
			break;
		case CELL_SYSUTIL_DRAWING_BEGIN:
		case CELL_SYSUTIL_DRAWING_END:
			break;
		case CELL_SYSUTIL_OSKDIALOG_LOADED:
			break;
		case CELL_SYSUTIL_OSKDIALOG_FINISHED:
			oskutil_stop(&oskutil_handle);
			break;
		case CELL_SYSUTIL_OSKDIALOG_UNLOADED:
			oskutil_close(&oskutil_handle);
			break;
		default:
			break;
	}
}

uint32_t emulator_audio_callback(int16_t *out)
{
	pthread_mutex_lock(&audio_lock);
	S9xMixSamples((uint32_t*)out);
	pthread_mutex_unlock(&audio_lock);
	return 512;
}

#define audio_default_params() \
   cell_audio_params params; \
   memset(&params, 0, sizeof(params)); \
   params.channels=2; \
   params.samplerate=48000; \
   params.buffer_size=8192; \
   params.sample_cb = emulator_audio_callback; \
   params.userdata = NULL;

static void callback_rsound_dialog_ok(int button_type, void *userdata)
{
	switch(button_type)
	{
		case CELL_MSGDIALOG_BUTTON_ESCAPE:
			dialog_is_running = false;
			audio_default_params();
			audio_driver = &cell_audio_audioport;
			audio_handle = audio_driver->init(&params);
			Settings.SoundMode = SOUND_MODE_NORMAL;
			break;
	}
}

#define create_callback(setting, mute_sound) \
	switch(button_type) \
	{ \
		case CELL_MSGDIALOG_BUTTON_YES: \
		   setting = true; \
		   break; \
		case CELL_MSGDIALOG_BUTTON_ESCAPE: \
		case CELL_MSGDIALOG_BUTTON_NO: \
		   setting = false; \
		   break; \
	} \
   if(mute_sound) \
      S9xSetSoundMute(false); \
   dialog_is_running = false; \
   cellMsgDialogClose(0.0f);


#define create_callback_loop(mute_sound) \
   do \
   { \
      if(mute_sound) \
         S9xSetSoundMute(true); \
      glClear(GL_COLOR_BUFFER_BIT); \
      psglSwap(); \
      cellSysutilCheckCallback(); \
   }while(dialog_is_running & is_running);

#ifdef CELL_DEBUG_CONSOLE
#define create_osk_loop() \
	while(OSK_IS_RUNNING(oskutil_handle) & is_running) \
	{ \
		/* OSK Util gets updated */ \
		S9xMainLoop(); \
		cellConsolePoll(); \
		cellSysutilCheckCallback(); \
	}
#else
#define create_osk_loop() \
	while(OSK_IS_RUNNING(oskutil_handle) & is_running) \
	{ \
		/* OSK Util gets updated */ \
		S9xMainLoop(); \
		cellSysutilCheckCallback(); \
	}
#endif

#define create_emulator_loop(input_loop, main_loop) \
   do{ \
      main_loop(); \
      input_loop(); \
      cell_console_poll(); \
	   cellSysutilCheckCallback(); \
	}while(is_running);

static void callback_multitap_compatible(int button_type, void *userdata)
{
	create_callback(Settings.CurrentROMisMultitapCompatible, 1);
}

static void callback_mouse_compatible(int button_type, void *userdata)
{
	create_callback(Settings.CurrentROMisMouseCompatible, 1);
}

static void callback_superscope_compatible(int button_type, void *userdata)
{
	create_callback(Settings.CurrentROMisSuperScopeCompatible, 1);
}

static void callback_justifier_compatible(int button_type, void *userdata)
{
	create_callback(Settings.CurrentROMisJustifierCompatible, 1);
}

static void callback_save_custom_controls(int button_type, void *userdata)
{
	create_callback(Settings.SaveCustomControlScheme, 0);
}

void emulator_stop_rom_running(void)
{
	is_running = 0;
}

void emulator_implementation_add_cheat(uint32_t address, uint8_t byte, const char* description)
{
	S9xAddCheat(false, true, address, byte);
	S9xEnableCheat(Cheat.num_cheats - 1);
	strncpy(Cheat.c[Cheat.num_cheats - 1].name, description, 22);
}

void emulator_change_cheat_description(uint32 cheatposition, const char *description)
{
	strncpy(Cheat.c[cheatposition].name, description, 22);
	Cheat.c[cheatposition].saved = TRUE;
}

const char * emulator_input_cheatlabel(void)
{
	const char * newlabel = new char[22];
	// (1) Let the user enter a label name for currently selected cheat code

	oskutil_write_initial_message(&oskutil_handle, L"");
	oskutil_write_message(&oskutil_handle, L"Enter cheat label");
	oskutil_start(&oskutil_handle);

	create_osk_loop();

	newlabel = strdup(OUTPUT_TEXT_STRING(oskutil_handle));

	// (2) add cheat label
	emulator_change_cheat_description(Settings.CurrentCheatPosition, newlabel);

	// (3) Close OSKUtil
	oskutil_close(&oskutil_handle);

	// (4) Return message text string
	return "Cheat label added.";	
}

const char * emulator_input_cheat(void)
{
	// (1) Let the user enter his cheat code
	oskutil_write_initial_message(&oskutil_handle, L"");
	oskutil_write_message(&oskutil_handle, L"Enter cheat code (Game Genie/Action Replay/GoldFinger Pro format)");
	oskutil_start(&oskutil_handle);


	create_osk_loop();

	// (2) Save the code to a char variable
	const char * newcode = new char[128];
	newcode = strdup(OUTPUT_TEXT_STRING(oskutil_handle));

	// Close OSKUtil
	oskutil_close(&oskutil_handle);

	// (3) Check the cheat format of the cheat code
	uint32 address;
	uint8 byte;
	uint8 bytes [3];
	bool8 sram;
	uint8 num_bytes;

	int cheatformat = 0;

	if (!S9xGameGenieToRaw (newcode, address, byte))
	{
		// (4) add Game Genie cheat code
		emulator_implementation_add_cheat(address, byte, newcode);
		cheatformat = 1;
	}
	else if (!S9xProActionReplayToRaw(newcode, address, byte))
	{
		// (4) add Pro Action Replay cheat code
		emulator_implementation_add_cheat(address, byte, newcode);
		cheatformat = 2;
	}
	else if (!S9xGoldFingerToRaw(newcode, address, sram, num_bytes, bytes))
	{
		// (4) add Gold Finger cheat code
		for (int c = 0; c < num_bytes; c++)
			emulator_implementation_add_cheat(address + c, bytes[c], newcode);
		cheatformat = 3;
	}
	else
		cheatformat = 4;

	// (5) Return message text string
	switch(cheatformat)
	{
		case 1:  //Game Genie
			return "Game Genie code added.";	
		case 2:  //Action Replay
			return "Pro Action Replay code added.";	
		case 3:  //Gold Finger
			return "Gold Finger code added.";	
		case 4:  //else
			return "ERROR: Code entered was incorrect.";
	}
	return "";
}

static bool emulator_is_super_scope_compatible(void)
{
	if	(
			Memory.match_na("BATTLE CLASH") || // Battle Clash (EU/US) (*) / Space Bazooka (JP)

			Memory.match_na("SPACE BAZOOKA") || // Space Bazooka (JP) (*) / Battle Clash (EU/US)
			//FIXME: TODO: Add The Hunt for Red October - used for bonus game
			//FIXME: TODO: Add Lamborghini American Challenge - if needed
			Memory.match_na("METAL COMBAT") ||			// Metal Combat: Falcon's Revenge
			Memory.match_na("OPERATION THUNDERBOLT1") ||		// Operation Thunderbolt
			Memory.match_na("SUPER SCOPE 6") ||			// Super Scope 6
			//Memory.match_na("TERMINATOR2 THE MOVIE0") ||		// Terminator 2: The Arcade Game
			Memory.match_na("TINSTAR") ||				// Tin Star
			Memory.match_id("9N") ||
			Memory.match_na("X ZONE") ||				// X-Zone
			//FIXME: TODO: Add Yoshi no Road Hunting - CRC32: 52948F3C
			Memory.match_na("YOSHI'S SAFARI")			// Yoshi's Safari
			)
			{
				return true;
			}
	else
		return false;

}

static bool emulator_implementation_is_justifier_compatible(void)
{
	if(Memory.match_nc("LETHAL ENFORCERS"))			// Lethal Enforcers
		return true;
	else
		return false;
}

static bool emulator_is_mouse_compatible(void)
{
	if	(
			Memory.match_nc("ACME ANIMATION FACTOR") ||		// ACME Animation Factory
			Memory.match_id("ACM") ||
			Memory.match_nc("ALICE PAINT") ||			// Alice no Paint Adventure
			Memory.match_id("AALJ") ||
			Memory.match_nc("ARKANOID DOH IT AGAIN0") ||		// Arkanoid: Doh It Again
			Memory.match_id("A6") ||
			Memory.match_na("SFC SAILORMOON S") ||			// Bishojou Senshi Sailor Moon S: Kondowa Puzzle de
										// Oshiokiyo!

			Memory.match_na("BRANDISH2 EXPERT") ||			// Brandish 2: Expert
			Memory.match_id("AQIJ") ||				

			Memory.match_na("BREAKTRHU!") ||			// BreakThru!
			Memory.match_id("ABXE") ||

			Memory.match_na("CIVILIZATION") ||			// Civilization
			Memory.match_id("EQ") ||

			Memory.match_na("CAMELTRY") ||				// Cameltry (JP) (*)
										// On The Ball (EU/US)

			Memory.match_na("CANNON FODDER") ||			// Cannon Fodder
			Memory.match_id("ACNP") ||				

			Memory.match_id("M Ve") ||				// Dai-3-ji Super Robot Taisen

			Memory.match_id("AR4J") ||				// Dai-4-ji Super Robot Taisen
			Memory.match_na("SUPER ROBOT WARS 4") ||

			Memory.match_na("DOOM") ||				// Doom
			Memory.match_id("AD8") ||				

			Memory.match_id("A88J") ||				// Doukyuusei 2 (JP) (NP)

			Memory.match_na("DRAGON KNIGHT 4") ||			// Dragon Knight 4
			Memory.match_id("A87J") ||				

			Memory.match_na("Eye of the Beholder") ||		// Eye of the Beholder

			Memory.match_na("FARLANDSTORY2") ||			// Farland Story 2
			Memory.match_id("A2FJ") ||

			Memory.match_na("Fun 'N Games") ||			// Fun 'N Games (EU/US)
			Memory.match_na("GALAXY ROBO") ||			// Galaxy Robo
			Memory.match_na("HIOUDEN") ||				// Hiouden: Mamono-tachi tono Chikai

			Memory.match_na("JIGSAW PARTY") ||			// Jigsaw Party (US) (*)
										// Pieces (JP)

			//Memory.match_na("JURASSIC PARK") ||			// Jurassic Park

			Memory.match_na("King Arthurs World") ||		// King Arthur's World (EU/US) (*)
										// Royal Conquest (JP)
			Memory.match_na("KOUTETSU-NO KISHI") ||			// Koutetsu No Kishi
			Memory.match_na("KOUTETSU-NO KISHI 2") ||		// Koutetsu No Kishi 2

			Memory.match_na("KOUTETSU-NO KISHI 3") ||		// Koutetsu No Kishi 3
			Memory.match_id("APZJ") ||

			Memory.match_na("LAMBORGHINI AMERICAN") ||		// Lamborghini American Challenge (EU/US)

			Memory.match_na("LAS VEGAS DREAM") ||			// Las Vegas Dream in Golden Paradise (JP) (*)
										// Vegas Stakes (EU/US)

			Memory.match_na("Lemmings 2,The Tribes") ||		// Lemmings 2: The Tribes
			Memory.match_id("L2") ||

			Memory.match_na("LORD MONARCH") ||			// Lord Monarch
			Memory.match_na("Mario&Wario") ||			// Mario & Wario

			Memory.match_na("SUPER PICROSS") ||			// Mario no Super Picross
			Memory.match_id("ACXJ") ||

			Memory.match_nc("MARIOPAINT") ||			// Mario Paint

			Memory.match_na("MEY  Preschool Fun") ||		// Mario's Early Years: Preschool Fun
			Memory.match_id("AMEE") ||

			Memory.match_na("MEGA LO MANIA") ||			// Mega-Lo Mania
			Memory.match_na("MIGHT  AND MAGIC III") ||		// Might And Magic III - Isles of Terra

			Memory.match_na("WONDER KITCHEN") ||			// Motoko-chan no Wonder Kitchen

			Memory.match_na("Nobunaga's Ambition") ||		// Nobunaga's Ambition (US) (*)
										// Super Nobunaga no Yabou: Zenkokuban (JP)

			Memory.match_na("ON THE BALL") ||			// On The Ball (EU/US) (*)
										// Cameltry (JP)

			Memory.match_na("OPERATION THUNDERBOLT1") ||		// Operation Thunderbolt

			Memory.match_na("PIECES") ||
			Memory.match_id("Z5") ||				// Pieces (JP) (*)
										// Jigsaw Party (US)

			Memory.match_nc("POPULOUS II") ||			// Populous II
			Memory.match_na("POWERMONGER") ||			// Power Monger

			Memory.match_na("Revolution X") ||			// Revolution X
			Memory.match_id("AXR") ||

			Memory.match_na("Royal Conquest") ||			// Royal Conquest (JP) (*)
										// King Arthur's World (EU/US)

			Memory.match_na("TENBU SPIRIT") ||			// San Goku Shi Seishi: Tenbu Spirits

			Memory.match_na("SHIEN THE BLADE CHASE0") ||		// Shien The Blade Chaser (JP) (*)
			Memory.match_na("SHIEN'S REVENGE") ||			// Shien's Revenge (*)

			Memory.match_nc("SimAnt") ||				// SimAnt
			Memory.match_na("SNOOPY CONCERT") ||			// Snoopy Concert
			Memory.match_na("SUPER CAESARS PALACE") ||		// Super Caesar's Palace

			Memory.match_na("SUPER CASTLES") ||			// Super Castles
			Memory.match_id("AVIJ") ||

			//FIXME: TODO: Super Noah's Ark 3D: - bad checksum - CRC32: A2315A14

			Memory.match_na("SuperPachiSlotMahjong") ||		// Super Pachi Slot Mahjong
			Memory.match_na("SUPER SOLITAIRE") ||			// Super Solitaire

			Memory.match_na("SUPER ZENKOKUBAN") ||			// Super Nobunaga no Yabou: Zenkokuban (JP) (*)
										// Nobunaga's Ambition (US)
			Memory.match_na("TERMINATOR2 THE MOVIE0") ||		// Terminator 2: The Arcade Game

			Memory.match_na("TINSTAR") ||				// Tin Star
			Memory.match_id("9N") ||

			Memory.match_na("TOKIMEKI MEMORIAL") ||			// Tokimeki Memorial
			Memory.match_id("AM8J") ||

			Memory.match_na("Troddlers") ||				// Troddlers
			Memory.match_na("UTOPIA") ||				// Utopia

			Memory.match_na("VEGAS STAKES") ||			// Vegas Stakes (EU/US) (*)
										// Las Vegas Dream in Golden Paradise (JP)
			Memory.match_nc("WOLFENSTEIN 3D") ||			// Wolfenstein 3D

			Memory.match_na("WONDERPROJECT J") ||			// Wonder Project J
			Memory.match_id("APJJ") ||				

			Memory.match_na("ZAN2 SPIRITS") ||			// Zan 2: Spirits
			Memory.match_na("ZAN3 SFC")				// Zan 3: Spirits
			)
			{
				return true;
			}
	else
		return false;
}

static bool emulator_is_multitap_compatible(void)
{
	if 	(
			Memory.match_nn("BARKLEY") ||	// Barkley Shut Up and Jam!
			Memory.match_id("ABCJ") ||	// Battle Cross
			Memory.match_na("Bill Walsh College FB1") || // Bill Walsh College Football
			Memory.match_id("ANYE") || // College Slam
			Memory.match_id("AC3J") || // Crystal Beans from Dungeon Explorer
			Memory.match_nc("Bruce Lee") ||	// Dragon - Bruce Lee Story
			Memory.match_id("AVS") || // Fever Pitch Soccer (EU) / Head-On Soccer (US) (*)
			Memory.match_na("FURI-FURI GIRLS") || // Furi-Furi Girls
			Memory.match_na("HAT TRICK HERO 2") || // Hat Trick Hero 2
			Memory.match_na("HEBEREKE NO PUZZLE") || // Hebereke no Oishii Puzzle wa Irimasenka
			Memory.match_id("AWJ") || // International Superstar Soccer Deluxe (EU/US) / Jikkyou World Soccer 2 - Fighting Eleven (JP) (*)
			Memory.match_na("J-LEAGUE SUPER SOCCER0") || // J-League Super Soccer (JP) / Virtual Soccer (EU)
			Memory.match_na("JIGSAW PARTY") || // Jigsaw Party (US) (*) / Pieces (JP)
			Memory.match_na("JIMMY CONNORS TENNIS") || // Jimmy Connors Pro Tennis Tour
			Memory.match_na("LAS VEGAS DREAM") || // Las Vegas Dream in Golden Paradise (JP) (*) / Vegas Stakes (EU/US)
			Memory.match_id("ALT") || // Looney Tunes B-Ball (US) (*) / Looney Tunes Basketball (EU) (*)
			Memory.match_id("AYHJ") || // Mizuki Shigeru no Youkai Hyakkiyakou
			Memory.match_id("A3VJ") || // Multi Play Volleyball
			Memory.match_na("MUSCLE BOMBER") || // Muscle Bomber: The Body Explosion (JP) (*) // Saturday Night Slam Masters (US)
			Memory.match_id("CHAMP WRESTLING") || // Natsume Championship Wrestling
			Memory.match_id("ANJ") || // NBA Give 'n Go
			Memory.match_id("AXG") || // NBA Hangtime
			Memory.match_id("AJT") || // NBA Jam Tournament Edition (EU/US/JP)
			Memory.match_na("NBA JAM") || // NBA Jam
			Memory.match_id("AFIE") || // NCAA Final Four Basketball
			Memory.match_id("AFBE") || // NCAA Football
			Memory.match_id("Q9") || // NFL Quarterback Club / NFL Quarterback Club '95
			Memory.match_id("AQB") || // NFL Quarterback Club '96
			Memory.match_na("PIECES") ||
			Memory.match_id("Z5") || // Pieces (JP) (*) / Jigsaw Party (US)
			Memory.match_id("ARVE") || // Rap Jam - Volume One
			Memory.match_na("RUSHING BEAT SYURA") || // Rushing Beat Shura (JP) (*) / The Peace Keepers (US)
			Memory.match_na("S.Night SLAM MASTERS") || // Saturday Night Slam Masters (US) (*) / Muscle Bomber: The Body Explosion (JP)
			Memory.match_na("Secret of MANA") || // Secret of Mana (EU/US) (*)
			Memory.match_na("SeikenDensetsu 2") || // Seiken Densetsu 2 (JP) (*) / 
			Memory.match_nn("SeikenDensetsu3") || // Seiken Densetsu 3 (in case you use 3-player IPS patch)
			Memory.match_na("SMASH TENNIS") || // Smash Tennis (EU) (*) / / Super Family Tennis (JP)
			//FIXME: TODO - Add CRC32 for Super Family Tennis - CRC32: 2BCBFF26
			Memory.match_id("ASR") || // Street Racer
			Memory.match_na("SUGOI HEBEREKE") || // Sugoi Hebereke
			Memory.match_na("Sugoro Quest++") || // Sugoro Quest++: Dicenics
			Memory.match_id("AO9") || // Summer Olympics
			Memory.match_id("AS6") || // Super Bomberman 3
			Memory.match_id("A4BJ") || // Super Bomberman 4
			Memory.match_id("APBJ") || // Super Bomberman Panic Bomber W
			Memory.match_id("AF5J") || // Super Fire Pro Wrestling X
			Memory.match_id("AP4J") || // Super Fire Pro Wrestling Special
			Memory.match_id("AQQJ") || // Super Fire Pro Wrestling - Queen's Special
			Memory.match_id("A7PJ") || // Super Puyo Puyo Tsuu Remix
			Memory.match_id("AT3J") || // Super Tetris 3
			Memory.match_id("AFY") || // Syndicate

			Memory.match_na("THE PEACE KEEPERS") ||	// The Peace Keepers (US) / Rushing Beat Shura (JP)

			Memory.match_na("Tiny Toon Sports") ||	// Tiny Toon Adventures: Dotabata Daiundoukai (JP) / Wacky Sports Challenge (US) (*) / Wild & Wacky Sports (EU) (*)
			Memory.match_id("A3T") || // Top Gear 3000 (EU/US) / Planet's Champ TG 3000 (JP) (*)
			Memory.match_na("VEGAS STAKES") || // Vegas Stakes (EU/US) (*) / Las Vegas Dream in Golden Paradise (JP)
			Memory.match_na("Virtual Soccer") || // Virtual Soccer (EU) / J-League Super Soccer (JP)
			Memory.match_id("AWF") || // WWF RAW
			//Catch-alls
			Memory.match_nc("SUPER BOMBERMAN") ||	// All Bomberman games (FIXME: remove
			Memory.match_nc("FIFA") ||		// All FIFA games
			Memory.match_nn("NHL") ||		// All NHL games
			Memory.match_nc("MADDEN") ||		// All Madden games
			Memory.match_nc("MICRO MACHINES") ||	// All Micro Machines games
			Memory.match_id("Q4")			// ?
			)
			{
				return true;
			}
	else
		return false;
}

static void emulator_odd_screen_height_jump_fix(void)
{
	if(Memory.match_nc("CHRONO TRIGGER") ||					// Chrono Trigger
			Memory.match_id("ACT") ||
			Memory.match_id("AC9J")					// Chrono Trigger (Sample)
	  )
	{
		Settings.OddScreenHeightJumpFix = 1;
	}
	else
		Settings.OddScreenHeightJumpFix = 0;
}


void emulator_toggle_sound(uint64_t soundmode)
{
	audio_default_params();
	switch(soundmode)
	{
		case SOUND_MODE_RSOUND:
			params.device = Settings.RSoundServerIPAddress;
			params.headset = 0;
			break;
		case SOUND_MODE_HEADSET:
			params.device = NULL;
			params.headset = 1;
			break;
		case SOUND_MODE_NORMAL: 
			params.device = NULL;
			params.headset = 0;
			break;
	}

	if(audio_handle)
	{
		audio_driver->free(audio_handle);
		audio_handle = NULL; 
	}
	if(soundmode == SOUND_MODE_RSOUND)
	{
		audio_driver = &cell_audio_rsound;
		audio_handle =  audio_driver->init(&params);
		if(!audio_handle || !(strlen(Settings.RSoundServerIPAddress) > 0))
		{
			create_error_dialog("Couldn't connect to RSound server at specified IP address. Falling back to regular audio.",callback_rsound_dialog_ok);
			create_callback_loop(0);
		}
	}
	else
	{
		audio_driver = &cell_audio_audioport;
		audio_handle =  audio_driver->init(&params);
	}

}

void emulator_save_sram(void)
{
	if(!Settings.SRAMWriteProtect)
		Memory.SaveSRAM(S9xGetFilename(".srm", SRAM_DIR));

	S9xSaveCheatFile(S9xGetFilename(".cht", CHEAT_DIR));
}

void emulator_switch_pal_60hz(bool pal60Hz)
{
	Settings.PS3PALTemporalMode60Hz = pal60Hz;
	Graphics->SetPAL60Hz(Settings.PS3PALTemporalMode60Hz);
	Graphics->SwitchResolution(Graphics->GetCurrentResolution(), Settings.PS3PALTemporalMode60Hz, Settings.TripleBuffering, Settings.ScaleEnabled, Settings.ScaleFactor);
}

#define EMULATOR_IMPLEMENTATION_INPUT_LOOP_MOUSE() \
   static uint64_t old_state; \
   static int mouse_old_x; \
   static int mouse_old_y; \
   static uint8_t old_mouse_buttons; \
   uint32_t pads_connected = cell_pad_input_pads_connected(); \
   const CellMouseData mouse_state = cell_mouse_input_poll_device(0); \
   const uint64_t state = cell_pad_input_poll_device(0); \
   const uint64_t button_was_pressed = old_state & (old_state ^ state); \
   const uint64_t button_was_held = old_state & state; \
   const uint64_t button_was_not_held = ~(old_state & state); \
   const uint64_t button_was_not_pressed = ~(state); \
   const uint64_t pad = 1; \
   const uint64_t new_state_mouse_button_1 = (mouse_state.buttons & CELL_MOUSE_BUTTON_1); \
   const uint64_t new_state_mouse_button_2 = (mouse_state.buttons & CELL_MOUSE_BUTTON_2); \
   const uint64_t new_state_mouse_button_3 = (mouse_state.buttons & CELL_MOUSE_BUTTON_3); \
   const uint64_t new_state_mouse_button_4 = (mouse_state.buttons & CELL_MOUSE_BUTTON_4); \
   const uint64_t new_state_mouse_button_5 = (mouse_state.buttons & CELL_MOUSE_BUTTON_5); \
   uint64_t used_mouse_button_1; \
   uint64_t used_mouse_button_2; \
   uint64_t used_mouse_button_3; \
   uint64_t used_mouse_button_4; \
   uint64_t used_mouse_button_5; \
   if(mouse_state.update == CELL_MOUSE_DATA_UPDATE) \
   { \
      used_mouse_button_1 = new_state_mouse_button_1; \
      used_mouse_button_2 = new_state_mouse_button_2; \
      used_mouse_button_3 = new_state_mouse_button_3; \
      used_mouse_button_4 = new_state_mouse_button_4; \
      used_mouse_button_5 = new_state_mouse_button_5; \
      old_mouse_buttons   = mouse_state.buttons; \
   } \
   else \
   { \
      used_mouse_button_1 = old_mouse_buttons & CELL_MOUSE_BUTTON_1; \
      used_mouse_button_2 = old_mouse_buttons & CELL_MOUSE_BUTTON_2; \
      used_mouse_button_3 = old_mouse_buttons & CELL_MOUSE_BUTTON_3; \
      used_mouse_button_4 = old_mouse_buttons & CELL_MOUSE_BUTTON_4; \
      used_mouse_button_5 = old_mouse_buttons & CELL_MOUSE_BUTTON_5; \
   } \
   S9xReportButton_Mouse(MAKE_BUTTON(pad, PS3Input.ButtonCircle[0]), (used_mouse_button_3)); \
   S9xReportButton_Mouse(MAKE_BUTTON(pad, PS3Input.ButtonCross[0]), (used_mouse_button_4)); \
   S9xReportButton_Mouse(MAKE_BUTTON(pad, PS3Input.ButtonTriangle[0]), (used_mouse_button_2)); \
   S9xReportButton_Mouse(MAKE_BUTTON(pad, PS3Input.ButtonSquare[0]), (used_mouse_button_1)); \
   S9xReportButton_Mouse(MAKE_BUTTON(pad, PS3Input.ButtonSelect[0]), (CTRL_SELECT(state))); \
   S9xReportButton_Mouse(MAKE_BUTTON(pad, PS3Input.ButtonStart[0]), (used_mouse_button_5)); \
   S9xReportButton_Mouse(MAKE_BUTTON(pad, PS3Input.ButtonL1[0]), (CTRL_L1(state))); \
   S9xReportButton_Mouse(MAKE_BUTTON(pad, PS3Input.ButtonL2[0]), (CTRL_L2(state) && CTRL_R2(button_was_not_pressed))); \
   S9xReportButton_Mouse(MAKE_BUTTON(pad, PS3Input.ButtonR1[0]), (CTRL_R1(state))); \
   S9xReportButton_Mouse(MAKE_BUTTON(pad, PS3Input.ButtonR2[0]), (CTRL_R2(state) && CTRL_L2(button_was_not_pressed))); \
   S9xReportButton_Mouse(MAKE_BUTTON(pad, PS3Input.DPad_Up[0]),   (CTRL_UP(state)   | CTRL_LSTICK_UP(state)) != 0); \
   S9xReportButton_Mouse(MAKE_BUTTON(pad, PS3Input.DPad_Down[0]), (CTRL_DOWN(state) | CTRL_LSTICK_DOWN(state)) != 0); \
   S9xReportButton_Mouse(MAKE_BUTTON(pad, PS3Input.DPad_Left[0]), (CTRL_LEFT(state) | CTRL_LSTICK_LEFT(state)) != 0); \
   S9xReportButton_Mouse(MAKE_BUTTON(pad, PS3Input.DPad_Right[0]),(CTRL_RIGHT(state) | CTRL_LSTICK_RIGHT(state)) != 0); \
   S9xReportButton_Mouse(MAKE_BUTTON(pad, PS3Input.ButtonR2_ButtonR3[0]), (CTRL_R2(state) && CTRL_R3(state))); \
   S9xReportButton_Mouse(MAKE_BUTTON(pad, PS3Input.ButtonL2_ButtonR3[0]), (CTRL_L2(state) && CTRL_R3(state))); \
   S9xReportButton_Mouse(MAKE_BUTTON(pad, PS3Input.ButtonL2_ButtonL3[0]), (CTRL_L2(state) && CTRL_L3(state))); \
   S9xReportButton_Mouse(MAKE_BUTTON(pad, PS3Input.ButtonL2_ButtonR2[0]), (CTRL_L2(button_was_held) && CTRL_R2(button_was_held))); \
   S9xReportButton_Mouse(MAKE_BUTTON(pad, PS3Input.ButtonL3[0]), (CTRL_L3(state) && CTRL_R3(button_was_not_held))); \
   S9xReportButton_Mouse(MAKE_BUTTON(pad, PS3Input.ButtonR3[0]), (CTRL_R3(state) && CTRL_L3(button_was_not_held))); \
   S9xReportButton_Mouse(MAKE_BUTTON(pad, PS3Input.AnalogR_Right[0]), CTRL_RSTICK_RIGHT(state) && CTRL_R2(button_was_not_held) && CTRL_L2(button_was_not_held)); \
   S9xReportButton_Mouse(MAKE_BUTTON(pad, PS3Input.AnalogR_Left[0]), CTRL_RSTICK_LEFT(state) && CTRL_R2(button_was_not_held) && CTRL_L2(button_was_not_held)); \
   S9xReportButton_Mouse(MAKE_BUTTON(pad, PS3Input.AnalogR_Up[0]), CTRL_RSTICK_UP(state) && CTRL_R2(button_was_not_held) && CTRL_L2(button_was_not_held)); \
   S9xReportButton_Mouse(MAKE_BUTTON(pad, PS3Input.AnalogR_Down[0]), CTRL_RSTICK_DOWN(state) && CTRL_R2(button_was_not_held) && CTRL_L2(button_was_not_held)); \
   S9xReportButton_Mouse(MAKE_BUTTON(pad, PS3Input.ButtonL2_AnalogR_Right[0]), (CTRL_L2(button_was_held) && CTRL_RSTICK_RIGHT(button_was_pressed))); \
   S9xReportButton_Mouse(MAKE_BUTTON(pad, PS3Input.ButtonL2_AnalogR_Left[0]), (CTRL_L2(button_was_held) && CTRL_RSTICK_LEFT(button_was_pressed))); \
   S9xReportButton_Mouse(MAKE_BUTTON(pad, PS3Input.ButtonL2_AnalogR_Up[0]), (CTRL_L2(state) && CTRL_RSTICK_UP(button_was_pressed))); \
   S9xReportButton_Mouse(MAKE_BUTTON(pad, PS3Input.ButtonL2_AnalogR_Down[0]), (CTRL_L2(state) && CTRL_R2(button_was_not_pressed) && CTRL_RSTICK_DOWN(button_was_pressed))); \
   S9xReportButton_Mouse(MAKE_BUTTON(pad, PS3Input.ButtonL2_AnalogR_Right[0]), (CTRL_L2(state) && CTRL_RSTICK_RIGHT(button_was_pressed))); \
   S9xReportButton_Mouse(MAKE_BUTTON(pad, PS3Input.ButtonR2_AnalogR_Left[0]), (CTRL_R2(state) && CTRL_RSTICK_LEFT(button_was_pressed))); \
   S9xReportButton_Mouse(MAKE_BUTTON(pad, PS3Input.ButtonR2_AnalogR_Right[0]), (CTRL_R2(state) && CTRL_RSTICK_RIGHT(button_was_pressed))); \
   S9xReportButton_Mouse(MAKE_BUTTON(pad, PS3Input.ButtonR2_AnalogR_Up[0]), (CTRL_R2(state) && CTRL_RSTICK_UP(button_was_pressed))); \
   S9xReportButton_Mouse(MAKE_BUTTON(pad, PS3Input.ButtonR2_AnalogR_Down[0]), (CTRL_R2(state) && CTRL_RSTICK_DOWN(button_was_pressed) && CTRL_L2(button_was_not_pressed))); \
   S9xReportButton_Mouse(MAKE_BUTTON(pad, PS3Input.ButtonL2_ButtonR2_AnalogR_Down[0]), (CTRL_R2(state) && CTRL_L2(state) && CTRL_RSTICK_DOWN(button_was_pressed))); \
   S9xReportButton_Mouse(MAKE_BUTTON(pad, PS3Input.ButtonR3_ButtonL3[0]), (CTRL_R3(state) && CTRL_L3(state))); \
   if(mouse_state.update == CELL_MOUSE_DATA_UPDATE) \
   { \
      mouse_old_x += mouse_state.x_axis; \
      mouse_old_y += mouse_state.y_axis;  \
	   S9xReportPointer(BTN_POINTER1, mouse_old_x, mouse_old_y, keymap); \
      S9xReportPointer(BTN_SCOPE_POINTER, mouse_old_x, mouse_old_y, keymap); \
   } \
   /* old_mouse_state = mouse_state; */ \
   old_state = state;

#define EMULATOR_IMPLEMENTATION_INPUT_LOOP_MOUSE_SCOPE() \
   static uint64_t old_state; \
   /*static CellMouseData old_mouse_state; */ \
   static int mouse_old_x; \
   static int mouse_old_y; \
   static uint8_t old_mouse_buttons; \
   uint32_t pads_connected = cell_pad_input_pads_connected(); \
   const CellMouseData mouse_state = cell_mouse_input_poll_device(0); \
   const uint64_t state = cell_pad_input_poll_device(0); \
   const uint64_t button_was_pressed = old_state & (old_state ^ state); \
   const uint64_t button_was_held = old_state & state; \
   const uint64_t button_was_not_held = ~(old_state & state); \
   const uint64_t button_was_not_pressed = ~(state); \
   const uint64_t pad = 1; \
   const uint64_t new_state_mouse_button_1 = (mouse_state.buttons & CELL_MOUSE_BUTTON_1); \
   const uint64_t new_state_mouse_button_2 = (mouse_state.buttons & CELL_MOUSE_BUTTON_2); \
   const uint64_t new_state_mouse_button_3 = (mouse_state.buttons & CELL_MOUSE_BUTTON_3); \
   const uint64_t new_state_mouse_button_4 = (mouse_state.buttons & CELL_MOUSE_BUTTON_4); \
   const uint64_t new_state_mouse_button_5 = (mouse_state.buttons & CELL_MOUSE_BUTTON_5); \
   uint64_t used_mouse_button_1; \
   uint64_t used_mouse_button_2; \
   uint64_t used_mouse_button_3; \
   uint64_t used_mouse_button_4; \
   uint64_t used_mouse_button_5; \
   if(mouse_state.update == CELL_MOUSE_DATA_UPDATE) \
   { \
      used_mouse_button_1 = new_state_mouse_button_1; \
      used_mouse_button_2 = new_state_mouse_button_2; \
      used_mouse_button_3 = new_state_mouse_button_3; \
      used_mouse_button_4 = new_state_mouse_button_4; \
      used_mouse_button_5 = new_state_mouse_button_5; \
      old_mouse_buttons = mouse_state.buttons; \
   } \
   else \
   { \
      used_mouse_button_1 = old_mouse_buttons & CELL_MOUSE_BUTTON_1; \
      used_mouse_button_2 = old_mouse_buttons & CELL_MOUSE_BUTTON_2; \
      used_mouse_button_3 = old_mouse_buttons & CELL_MOUSE_BUTTON_3; \
      used_mouse_button_4 = old_mouse_buttons & CELL_MOUSE_BUTTON_4; \
      used_mouse_button_5 = old_mouse_buttons & CELL_MOUSE_BUTTON_5; \
   } \
   S9xReportButton_Mouse(MAKE_BUTTON(pad, PS3Input.ButtonCircle[0]), (used_mouse_button_3)); \
   S9xReportButton_Mouse(MAKE_BUTTON(pad, PS3Input.ButtonCross[0]), (used_mouse_button_4)); \
   S9xReportButton_Mouse(MAKE_BUTTON(pad, PS3Input.ButtonTriangle[0]), (used_mouse_button_2)); \
   S9xReportButton_Mouse(MAKE_BUTTON(pad, PS3Input.ButtonSquare[0]), (used_mouse_button_1)); \
   S9xReportButton_Mouse(MAKE_BUTTON(pad, PS3Input.ButtonSelect[0]), (CTRL_SELECT(state))); \
   S9xReportButton_Mouse(MAKE_BUTTON(pad, PS3Input.ButtonStart[0]), (used_mouse_button_5)); \
   S9xReportButton_Mouse(MAKE_BUTTON(pad, PS3Input.ButtonL1[0]), (CTRL_L1(state))); \
   S9xReportButton_Mouse(MAKE_BUTTON(pad, PS3Input.ButtonL2[0]), (CTRL_L2(state) && CTRL_R2(button_was_not_pressed))); \
   S9xReportButton_Mouse(MAKE_BUTTON(pad, PS3Input.ButtonR1[0]), (CTRL_R1(state))); \
   S9xReportButton_Mouse(MAKE_BUTTON(pad, PS3Input.ButtonR2[0]), (CTRL_R2(state) && CTRL_L2(button_was_not_pressed))); \
   S9xReportButton_Mouse(MAKE_BUTTON(pad, PS3Input.DPad_Up[0]),   (CTRL_UP(state)   | CTRL_LSTICK_UP(state)) != 0); \
   S9xReportButton_Mouse(MAKE_BUTTON(pad, PS3Input.DPad_Down[0]), (CTRL_DOWN(state) | CTRL_LSTICK_DOWN(state)) != 0); \
   S9xReportButton_Mouse(MAKE_BUTTON(pad, PS3Input.DPad_Left[0]), (CTRL_LEFT(state) | CTRL_LSTICK_LEFT(state)) != 0); \
   S9xReportButton_Mouse(MAKE_BUTTON(pad, PS3Input.DPad_Right[0]),(CTRL_RIGHT(state) | CTRL_LSTICK_RIGHT(state)) != 0); \
   S9xReportButton_Mouse(MAKE_BUTTON(pad, PS3Input.ButtonR2_ButtonR3[0]), (CTRL_R2(state) && CTRL_R3(state))); \
   S9xReportButton_Mouse(MAKE_BUTTON(pad, PS3Input.ButtonL2_ButtonR3[0]), (CTRL_L2(state) && CTRL_R3(state))); \
   S9xReportButton_Mouse(MAKE_BUTTON(pad, PS3Input.ButtonL2_ButtonL3[0]), (CTRL_L2(state) && CTRL_L3(state))); \
   S9xReportButton_Mouse(MAKE_BUTTON(pad, PS3Input.ButtonL2_ButtonR2[0]), (CTRL_L2(button_was_held) && CTRL_R2(button_was_held))); \
   S9xReportButton_Mouse(MAKE_BUTTON(pad, PS3Input.ButtonL3[0]), (CTRL_L3(state) && CTRL_R3(button_was_not_held))); \
   S9xReportButton_Mouse(MAKE_BUTTON(pad, PS3Input.ButtonR3[0]), (CTRL_R3(state) && CTRL_L3(button_was_not_held))); \
   S9xReportButton_Mouse(MAKE_BUTTON(pad, PS3Input.AnalogR_Right[0]), CTRL_RSTICK_RIGHT(state) && CTRL_R2(button_was_not_held) && CTRL_L2(button_was_not_held)); \
   S9xReportButton_Mouse(MAKE_BUTTON(pad, PS3Input.AnalogR_Left[0]), CTRL_RSTICK_LEFT(state) && CTRL_R2(button_was_not_held) && CTRL_L2(button_was_not_held)); \
   S9xReportButton_Mouse(MAKE_BUTTON(pad, PS3Input.AnalogR_Up[0]), CTRL_RSTICK_UP(state) && CTRL_R2(button_was_not_held) && CTRL_L2(button_was_not_held)); \
   S9xReportButton_Mouse(MAKE_BUTTON(pad, PS3Input.AnalogR_Down[0]), CTRL_RSTICK_DOWN(state) && CTRL_R2(button_was_not_held) && CTRL_L2(button_was_not_held)); \
   S9xReportButton_Mouse(MAKE_BUTTON(pad, PS3Input.ButtonL2_AnalogR_Right[0]), (CTRL_L2(button_was_held) && CTRL_RSTICK_RIGHT(button_was_pressed))); \
   S9xReportButton_Mouse(MAKE_BUTTON(pad, PS3Input.ButtonL2_AnalogR_Left[0]), (CTRL_L2(button_was_held) && CTRL_RSTICK_LEFT(button_was_pressed))); \
   S9xReportButton_Mouse(MAKE_BUTTON(pad, PS3Input.ButtonL2_AnalogR_Up[0]), (CTRL_L2(state) && CTRL_RSTICK_UP(button_was_pressed))); \
   S9xReportButton_Mouse(MAKE_BUTTON(pad, PS3Input.ButtonL2_AnalogR_Down[0]), (CTRL_L2(state) && CTRL_RSTICK_DOWN(button_was_pressed) && CTRL_R2(button_was_not_pressed))); \
   S9xReportButton_Mouse(MAKE_BUTTON(pad, PS3Input.ButtonL2_AnalogR_Right[0]), (CTRL_L2(state) && CTRL_RSTICK_RIGHT(button_was_pressed))); \
   S9xReportButton_Mouse(MAKE_BUTTON(pad, PS3Input.ButtonR2_AnalogR_Left[0]), (CTRL_R2(state) && CTRL_RSTICK_LEFT(button_was_pressed))); \
   S9xReportButton_Mouse(MAKE_BUTTON(pad, PS3Input.ButtonR2_AnalogR_Right[0]), (CTRL_R2(state) && CTRL_RSTICK_RIGHT(button_was_pressed))); \
   S9xReportButton_Mouse(MAKE_BUTTON(pad, PS3Input.ButtonR2_AnalogR_Up[0]), (CTRL_R2(state) && CTRL_RSTICK_UP(button_was_pressed))); \
   S9xReportButton_Mouse(MAKE_BUTTON(pad, PS3Input.ButtonR2_AnalogR_Down[0]), (CTRL_R2(state) && CTRL_RSTICK_DOWN(button_was_pressed) && CTRL_L2(button_was_not_pressed))); \
   S9xReportButton_Mouse(MAKE_BUTTON(pad, PS3Input.ButtonL2_ButtonR2_AnalogR_Down[0]), (CTRL_R2(state) && CTRL_L2(state) && CTRL_RSTICK_DOWN(button_was_pressed))); \
   S9xReportButton_Mouse(MAKE_BUTTON(pad, PS3Input.ButtonR3_ButtonL3[0]), (CTRL_R3(state) && CTRL_L3(state))); \
   if(mouse_state.update == CELL_MOUSE_DATA_UPDATE) \
   { \
      mouse_old_x += mouse_state.x_axis; \
      mouse_old_y += mouse_state.y_axis; \
      if(mouse_old_x > 258) \
         mouse_old_x = 258; \
      if(mouse_old_x < -3) \
         mouse_old_x = -3; \
      if(mouse_old_y > 239) \
         mouse_old_y = 239; \
      if(mouse_old_y < -3) \
         mouse_old_y = -3; \
	   S9xReportPointer(BTN_POINTER1, mouse_old_x, mouse_old_y, keymap); \
      S9xReportPointer(BTN_SCOPE_POINTER, mouse_old_x, mouse_old_y, keymap); \
   } \
   /* old_mouse_state = mouse_state; */ \
   old_state = state;

#define EMULATOR_IMPLEMENTATION_INPUT_LOOP_ANALOG() \
   static uint64_t old_state[MAX_PADS]; \
	uint32_t pads_connected = cell_pad_input_pads_connected(); \
	for (uint32_t i = 0; i < pads_connected; ++i) \
	{ \
		const uint64_t state = cell_pad_input_poll_device(i); \
		const uint64_t button_was_pressed = old_state[i] & (old_state[i] ^ state); \
		const uint64_t button_was_held = old_state[i] & state; \
		const uint64_t button_was_not_held = ~(old_state[i] & state); \
		const uint64_t button_was_not_pressed = ~(state); \
		const uint64_t pad = i + 1; \
		S9xReportButton_Mouse(MAKE_BUTTON(pad, PS3Input.ButtonCircle[i]), (CTRL_CIRCLE(state))); \
		S9xReportButton_Mouse(MAKE_BUTTON(pad, PS3Input.ButtonCross[i]), (CTRL_CROSS(state))); \
		S9xReportButton_Mouse(MAKE_BUTTON(pad, PS3Input.ButtonTriangle[i]), (CTRL_TRIANGLE(state))); \
		S9xReportButton_Mouse(MAKE_BUTTON(pad, PS3Input.ButtonSquare[i]), (CTRL_SQUARE(state))); \
		S9xReportButton_Mouse(MAKE_BUTTON(pad, PS3Input.ButtonSelect[i]), (CTRL_SELECT(state))); \
		S9xReportButton_Mouse(MAKE_BUTTON(pad, PS3Input.ButtonStart[i]), (CTRL_START(state))); \
		S9xReportButton_Mouse(MAKE_BUTTON(pad, PS3Input.ButtonL1[i]), (CTRL_L1(state))); \
		S9xReportButton_Mouse(MAKE_BUTTON(pad, PS3Input.ButtonL2[i]), (CTRL_L2(state) && CTRL_R2(button_was_not_pressed))); \
		S9xReportButton_Mouse(MAKE_BUTTON(pad, PS3Input.ButtonR1[i]), (CTRL_R1(state))); \
		S9xReportButton_Mouse(MAKE_BUTTON(pad, PS3Input.ButtonR2[i]), (CTRL_R2(state) && CTRL_L2(button_was_not_pressed))); \
 		S9xReportButton_Mouse(MAKE_BUTTON(pad, PS3Input.DPad_Up[i]),   (CTRL_UP(state)   | CTRL_LSTICK_UP(state)) != 0); \
		S9xReportButton_Mouse(MAKE_BUTTON(pad, PS3Input.DPad_Down[i]), (CTRL_DOWN(state) | CTRL_LSTICK_DOWN(state)) != 0); \
		S9xReportButton_Mouse(MAKE_BUTTON(pad, PS3Input.DPad_Left[i]), (CTRL_LEFT(state) | CTRL_LSTICK_LEFT(state)) != 0); \
		S9xReportButton_Mouse(MAKE_BUTTON(pad, PS3Input.DPad_Right[i]),(CTRL_RIGHT(state) | CTRL_LSTICK_RIGHT(state)) != 0); \
		S9xReportButton_Mouse(MAKE_BUTTON(pad, PS3Input.ButtonR2_ButtonR3[i]), (CTRL_R2(state) && CTRL_R3(state))); \
		S9xReportButton_Mouse(MAKE_BUTTON(pad, PS3Input.ButtonL2_ButtonR3[i]), (CTRL_L2(state) && CTRL_R3(state))); \
		S9xReportButton_Mouse(MAKE_BUTTON(pad, PS3Input.ButtonL2_ButtonL3[i]), (CTRL_L2(state) && CTRL_L3(state))); \
		S9xReportButton_Mouse(MAKE_BUTTON(pad, PS3Input.ButtonL2_ButtonR2[i]), (CTRL_L2(button_was_held) && CTRL_R2(button_was_held))); \
		S9xReportButton_Mouse(MAKE_BUTTON(pad, PS3Input.ButtonL3[0]), (CTRL_L3(state) && CTRL_R3(button_was_not_held))); \
		S9xReportButton_Mouse(MAKE_BUTTON(pad, PS3Input.ButtonR3[0]), (CTRL_R3(state) && CTRL_L3(button_was_not_held))); \
		S9xReportButton_Mouse(MAKE_BUTTON(pad, PS3Input.AnalogR_Right[i]), CTRL_RSTICK_RIGHT(state) && CTRL_R2(button_was_not_held) && CTRL_L2(button_was_not_held)); \
		S9xReportButton_Mouse(MAKE_BUTTON(pad, PS3Input.AnalogR_Left[i]), CTRL_RSTICK_LEFT(state) && CTRL_R2(button_was_not_held) && CTRL_L2(button_was_not_held)); \
		S9xReportButton_Mouse(MAKE_BUTTON(pad, PS3Input.AnalogR_Up[i]), CTRL_RSTICK_UP(state) && CTRL_R2(button_was_not_held) && CTRL_L2(button_was_not_held)); \
		S9xReportButton_Mouse(MAKE_BUTTON(pad, PS3Input.AnalogR_Down[i]), CTRL_RSTICK_DOWN(state) && CTRL_R2(button_was_not_held) && CTRL_L2(button_was_not_held)); \
		S9xReportButton_Mouse(MAKE_BUTTON(pad, PS3Input.ButtonL2_AnalogR_Right[i]), (CTRL_L2(button_was_held) && CTRL_RSTICK_RIGHT(button_was_pressed))); \
		S9xReportButton_Mouse(MAKE_BUTTON(pad, PS3Input.ButtonL2_AnalogR_Left[i]), (CTRL_L2(button_was_held) && CTRL_RSTICK_LEFT(button_was_pressed))); \
		S9xReportButton_Mouse(MAKE_BUTTON(pad, PS3Input.ButtonL2_AnalogR_Up[i]), (CTRL_L2(state) && CTRL_RSTICK_UP(button_was_pressed))); \
		S9xReportButton_Mouse(MAKE_BUTTON(pad, PS3Input.ButtonL2_AnalogR_Down[i]), (CTRL_L2(state) && CTRL_RSTICK_DOWN(button_was_pressed) && CTRL_R2(button_was_not_pressed))); \
		S9xReportButton_Mouse(MAKE_BUTTON(pad, PS3Input.ButtonL2_AnalogR_Right[i]), (CTRL_L2(state) && CTRL_RSTICK_RIGHT(button_was_pressed))); \
		S9xReportButton_Mouse(MAKE_BUTTON(pad, PS3Input.ButtonR2_AnalogR_Left[i]), (CTRL_R2(state) && CTRL_RSTICK_LEFT(button_was_pressed))); \
		S9xReportButton_Mouse(MAKE_BUTTON(pad, PS3Input.ButtonR2_AnalogR_Right[i]), (CTRL_R2(state) && CTRL_RSTICK_RIGHT(button_was_pressed))); \
		S9xReportButton_Mouse(MAKE_BUTTON(pad, PS3Input.ButtonR2_AnalogR_Up[i]), (CTRL_R2(state) && CTRL_RSTICK_UP(button_was_pressed))); \
		S9xReportButton_Mouse(MAKE_BUTTON(pad, PS3Input.ButtonR2_AnalogR_Down[i]), (CTRL_R2(state) && CTRL_L2(button_was_not_pressed) && CTRL_RSTICK_DOWN(button_was_pressed))); \
		S9xReportButton_Mouse(MAKE_BUTTON(pad, PS3Input.ButtonL2_ButtonR2_AnalogR_Down[i]), (CTRL_R2(state) && CTRL_L2(state) && CTRL_RSTICK_DOWN(button_was_pressed))); \
		S9xReportButton_Mouse(MAKE_BUTTON(pad, PS3Input.ButtonR3_ButtonL3[i]), (CTRL_R3(state) && CTRL_L3(state))); \
		S9xReportPointer(BTN_POINTER1, CTRL_AXIS_LSTICK_X(state), CTRL_AXIS_LSTICK_Y(state), 1, keymap); \
		S9xReportPointer(BTN_SCOPE_POINTER, CTRL_AXIS_LSTICK_X(state), CTRL_AXIS_LSTICK_Y(state), 0, keymap); \
		old_state[i] = state; \
	}

#define EMULATOR_IMPLEMENTATION_INPUT_LOOP() \
	static uint64_t old_state[MAX_PADS]; \
	const uint32_t pads_connected = cell_pad_input_pads_connected(); \
	for (uint32_t i = 0; i < pads_connected; ++i) \
	{ \
		const uint64_t state = cell_pad_input_poll_device(i); \
		const uint64_t button_was_pressed = old_state[i] & (old_state[i] ^ state); \
		const uint64_t button_was_held = old_state[i] & state; \
		const uint64_t button_was_not_held = ~(old_state[i] & state); \
		const uint64_t button_was_not_pressed = ~(state); \
		const uint64_t pad = i + 1; \
		\
		S9xReportButton(MAKE_BUTTON(pad, PS3Input.ButtonCircle[i]), (CTRL_CIRCLE(state))); \
		S9xReportButton(MAKE_BUTTON(pad, PS3Input.ButtonCross[i]), (CTRL_CROSS(state))); \
		S9xReportButton(MAKE_BUTTON(pad, PS3Input.ButtonTriangle[i]), (CTRL_TRIANGLE(state))); \
		S9xReportButton(MAKE_BUTTON(pad, PS3Input.ButtonSquare[i]), (CTRL_SQUARE(state))); \
		S9xReportButton(MAKE_BUTTON(pad, PS3Input.ButtonSelect[i]), (CTRL_SELECT(state))); \
		S9xReportButton(MAKE_BUTTON(pad, PS3Input.ButtonStart[i]), (CTRL_START(state))); \
		S9xReportButton(MAKE_BUTTON(pad, PS3Input.ButtonL1[i]), (CTRL_L1(state))); \
		S9xReportButton(MAKE_BUTTON(pad, PS3Input.ButtonL2[i]), (CTRL_L2(state) && CTRL_R2(button_was_not_pressed))); \
		S9xReportButton(MAKE_BUTTON(pad, PS3Input.ButtonR1[i]), (CTRL_R1(state))); \
		S9xReportButton(MAKE_BUTTON(pad, PS3Input.ButtonR2[i]), (CTRL_R2(state) && CTRL_L2(button_was_not_pressed))); \
 		S9xReportButton(MAKE_BUTTON(pad, PS3Input.DPad_Up[i]), (CTRL_UP(state) | CTRL_LSTICK_UP(state)) != 0); \
		S9xReportButton(MAKE_BUTTON(pad, PS3Input.DPad_Down[i]), (CTRL_DOWN(state) | CTRL_LSTICK_DOWN(state)) != 0); \
		S9xReportButton(MAKE_BUTTON(pad, PS3Input.DPad_Left[i]), (CTRL_LEFT(state) | CTRL_LSTICK_LEFT(state)) != 0); \
		S9xReportButton(MAKE_BUTTON(pad, PS3Input.DPad_Right[i]), (CTRL_RIGHT(state) | CTRL_LSTICK_RIGHT(state)) != 0); \
		S9xReportButton(MAKE_BUTTON(pad, PS3Input.ButtonR2_ButtonR3[i]), (CTRL_R2(state) && CTRL_R3(state))); \
		S9xReportButton(MAKE_BUTTON(pad, PS3Input.ButtonL2_ButtonR3[i]), (CTRL_L2(state) && CTRL_R3(state))); \
		S9xReportButton(MAKE_BUTTON(pad, PS3Input.ButtonL2_ButtonL3[i]), (CTRL_L2(state) && CTRL_L3(state))); \
		S9xReportButton(MAKE_BUTTON(pad, PS3Input.ButtonL2_ButtonR2[i]), (CTRL_L2(button_was_held) && CTRL_R2(button_was_held))); \
		S9xReportButton(MAKE_BUTTON(pad, PS3Input.ButtonL3[0]), (CTRL_L3(state) && CTRL_R3(button_was_not_held))); \
		S9xReportButton(MAKE_BUTTON(pad, PS3Input.ButtonR3[0]), (CTRL_R3(state) && CTRL_L3(button_was_not_held))); \
		S9xReportButton(MAKE_BUTTON(pad, PS3Input.AnalogR_Right[i]), CTRL_RSTICK_RIGHT(state) && CTRL_R2(button_was_not_held) && CTRL_L2(button_was_not_held)); \
		S9xReportButton(MAKE_BUTTON(pad, PS3Input.AnalogR_Left[i]), CTRL_RSTICK_LEFT(state) && CTRL_R2(button_was_not_held) && CTRL_L2(button_was_not_held)); \
		S9xReportButton(MAKE_BUTTON(pad, PS3Input.AnalogR_Up[i]), CTRL_RSTICK_UP(state) && CTRL_R2(button_was_not_held) && CTRL_L2(button_was_not_held)); \
		S9xReportButton(MAKE_BUTTON(pad, PS3Input.AnalogR_Down[i]), CTRL_RSTICK_DOWN(state) && CTRL_R2(button_was_not_held) && CTRL_L2(button_was_not_held)); \
		S9xReportButton(MAKE_BUTTON(pad, PS3Input.ButtonL2_AnalogR_Right[i]), (CTRL_L2(button_was_held) && CTRL_RSTICK_RIGHT(button_was_pressed))); \
		S9xReportButton(MAKE_BUTTON(pad, PS3Input.ButtonL2_AnalogR_Left[i]), (CTRL_L2(button_was_held) && CTRL_RSTICK_LEFT(button_was_pressed))); \
		S9xReportButton(MAKE_BUTTON(pad, PS3Input.ButtonL2_AnalogR_Up[i]), (CTRL_L2(state) && CTRL_RSTICK_UP(button_was_pressed))); \
		S9xReportButton(MAKE_BUTTON(pad, PS3Input.ButtonL2_AnalogR_Down[i]), (CTRL_L2(state) && CTRL_R2(button_was_not_pressed) && CTRL_RSTICK_DOWN(button_was_pressed))); \
		S9xReportButton(MAKE_BUTTON(pad, PS3Input.ButtonL2_AnalogR_Right[i]), (CTRL_L2(state) && CTRL_RSTICK_RIGHT(button_was_pressed))); \
		S9xReportButton(MAKE_BUTTON(pad, PS3Input.ButtonR2_AnalogR_Left[i]), (CTRL_R2(state) && CTRL_RSTICK_LEFT(button_was_pressed))); \
		S9xReportButton(MAKE_BUTTON(pad, PS3Input.ButtonR2_AnalogR_Right[i]), (CTRL_R2(state) && CTRL_RSTICK_RIGHT(button_was_pressed))); \
		S9xReportButton(MAKE_BUTTON(pad, PS3Input.ButtonR2_AnalogR_Up[i]), (CTRL_R2(state) && CTRL_RSTICK_UP(button_was_pressed))); \
		S9xReportButton(MAKE_BUTTON(pad, PS3Input.ButtonR2_AnalogR_Down[i]), (CTRL_R2(state) && CTRL_L2(button_was_not_pressed) && CTRL_RSTICK_DOWN(button_was_pressed))); \
		S9xReportButton(MAKE_BUTTON(pad, PS3Input.ButtonL2_ButtonR2_AnalogR_Down[i]), (CTRL_R2(state) && CTRL_L2(state) && CTRL_RSTICK_DOWN(button_was_pressed))); \
		S9xReportButton(MAKE_BUTTON(pad, PS3Input.ButtonR3_ButtonL3[i]), (CTRL_R3(state) && CTRL_L3(state))); \
		old_state[i] = state; \
	}

#define string_concat(padno, string) "Joypad"#padno" "#string""

#define map_snes9x_standard_controls(padno) \
   MAP_BUTTON(MAKE_BUTTON(padno, BTN_A), string_concat(padno, A)); \
   MAP_BUTTON(MAKE_BUTTON(padno, BTN_B), string_concat(padno, B)); \
   MAP_BUTTON(MAKE_BUTTON(padno, BTN_X), string_concat(padno, X)); \
   MAP_BUTTON(MAKE_BUTTON(padno, BTN_Y), string_concat(padno, Y)); \
   MAP_BUTTON(MAKE_BUTTON(padno, BTN_SELECT), string_concat(padno, Select)); \
   MAP_BUTTON(MAKE_BUTTON(padno, BTN_START), string_concat(padno, Start)); \
   MAP_BUTTON(MAKE_BUTTON(padno, BTN_L), string_concat(padno,  L)); \
   MAP_BUTTON(MAKE_BUTTON(padno, BTN_R), string_concat(padno, R)); \
   MAP_BUTTON(MAKE_BUTTON(padno, BTN_LEFT), string_concat(padno, Left)); \
   MAP_BUTTON(MAKE_BUTTON(padno, BTN_RIGHT), string_concat(padno, Right)); \
   MAP_BUTTON(MAKE_BUTTON(padno, BTN_UP), string_concat(padno, Up)); \
   MAP_BUTTON(MAKE_BUTTON(padno, BTN_DOWN), string_concat(padno, Down));

#define map_snes9x_special_controls(padno) \
   MAP_BUTTON(MAKE_BUTTON(padno, BTN_QUICKSAVE), "SaveFreezeFile"); \
   MAP_BUTTON(MAKE_BUTTON(padno, BTN_QUICKLOAD), "LoadFreezeFile"); \
   MAP_BUTTON(MAKE_BUTTON(padno, BTN_INCREMENTSAVE), "IncSave"); \
   MAP_BUTTON(MAKE_BUTTON(padno, BTN_DECREMENTSAVE), "DecSave"); \
   MAP_BUTTON(MAKE_BUTTON(padno, BTN_INCREMENTCHEAT), "IncCheat"); \
   MAP_BUTTON(MAKE_BUTTON(padno, BTN_DECREMENTCHEAT), "DecCheat"); \
   MAP_BUTTON(MAKE_BUTTON(padno, BTN_CHEATENABLE), "CheatEnable"); \
   MAP_BUTTON(MAKE_BUTTON(padno, BTN_CHEATTOGGLE), "CheatToggle"); \
   MAP_BUTTON(MAKE_BUTTON(padno, BTN_CHEATDISABLE), "CheatDisable"); \
   MAP_BUTTON(MAKE_BUTTON(padno, BTN_CHEATINPUT), "CheatInput"); \
   MAP_BUTTON(MAKE_BUTTON(padno, BTN_CHEATINPUTLABEL), "CheatInputLabel"); \
   MAP_BUTTON(MAKE_BUTTON(padno, BTN_EXITTOMENU), "ExitToMenu"); \
   MAP_BUTTON(MAKE_BUTTON(padno, BTN_PAUSE), "Pause"); \
   MAP_BUTTON(MAKE_BUTTON(padno, BTN_FASTFORWARD), "ToggleEmuTurbo"); \
   MAP_BUTTON(MAKE_BUTTON(padno, BTN_RESET), "Reset"); \
   MAP_BUTTON(MAKE_BUTTON(padno, BTN_SOFTRESET), "SoftReset"); \
   MAP_BUTTON(MAKE_BUTTON(padno, BTN_INCREMENTTURBO), "IncEmuTurbo"); \
   MAP_BUTTON(MAKE_BUTTON(padno, BTN_DECREMENTTURBO), "DecEmuTurbo"); \
   MAP_BUTTON(MAKE_BUTTON(padno, BTN_SWAPJOYPADS), "SwapJoypads"); \
   MAP_BUTTON(MAKE_BUTTON(padno, BTN_SRAM_WRITEPROTECT), "SramWriteProtect"); \
   MAP_BUTTON(MAKE_BUTTON(padno, BTN_INGAME_MENU), "IngameMenu");


static bool emulator_init_system(void)
{
	if(Settings.AccessoryType)
		cell_mouse_input_init();
	else
		cell_mouse_input_deinit();

	pthread_mutex_lock(&audio_lock);

	if(emulator_initialized)
		Memory.Deinit();

	Memory.Init();

	if(emulator_initialized)
		S9xDeinitAPU();

	S9xInitAPU();

	S9xInitSound(64, 0);

	//S9xSetSamplesAvailableCallback(emulator_s9x_audio_callback, NULL);
	//audio_active = true;
	pthread_mutex_unlock(&audio_lock);

	S9xSetSoundMute(true);
	S9xSetRenderPixelFormat(RGB555);

	if (current_rom == NULL)
		return false; //No ROM to load

	if (!Memory.LoadROM(current_rom))
		return false; //Load ROM failed

	if(emulator_is_multitap_compatible())
	{
		if(Settings.AccessoryAutoDetection == ACCESSORY_AUTODETECTION_CONFIRM)
		{
			create_msg_dialog("This game supports the [Multitap] accessory. Do you want to enable [Multitap] support?", callback_multitap_compatible);
			create_callback_loop(1);
		}
		else if(Settings.AccessoryAutoDetection == ACCESSORY_AUTODETECTION_ENABLED)
			Settings.CurrentROMisMultitapCompatible = true;
		else if(Settings.AccessoryAutoDetection == ACCESSORY_AUTODETECTION_NONE)
			Settings.CurrentROMisMultitapCompatible = false;

		if(Settings.CurrentROMisMultitapCompatible)
			Emulator_SetControllerMode(MULTITAP);
	}
	else if(emulator_is_mouse_compatible())
	{
		if(Settings.AccessoryAutoDetection == ACCESSORY_AUTODETECTION_CONFIRM)
		{
			create_msg_dialog(Settings.AccessoryType ? "This game supports the [SNES Mouse] accessory. Do you want to enable [SNES Mouse] support? (NOTE: [SNES Mouse] will be mapped to the USB/Bluetooth mouse)" : "This game supports the [SNES Mouse] accessory. Do you want to enable [SNES Mouse] support? (NOTE: [SNES Mouse] will be mapped to the left analog stick)",callback_mouse_compatible);
			create_callback_loop(1);
		}
		else if(Settings.AccessoryAutoDetection == ACCESSORY_AUTODETECTION_ENABLED)
			Settings.CurrentROMisMouseCompatible = true;
		else if(Settings.AccessoryAutoDetection == ACCESSORY_AUTODETECTION_NONE)
			Settings.CurrentROMisMouseCompatible = false;

		if(Settings.CurrentROMisMouseCompatible)
			Emulator_SetControllerMode(MOUSE);
	}
	else if(emulator_is_super_scope_compatible())
	{
		if(Settings.AccessoryAutoDetection == ACCESSORY_AUTODETECTION_CONFIRM)
		{
			create_msg_dialog(Settings.AccessoryType ? "This game supports the [Super Scope] accessory. Do you want to enable [Super Scope] support? (NOTE: [Super Scope] will be mapped to the USB/Bluetooth mouse)" : "This game supports the [Super Scope] accessory. Do you want to enable [Super Scope] support? (NOTE: [Super Scope] will be mapped to the left analog stick)",callback_superscope_compatible);
			create_callback_loop(1);
		}
		else if(Settings.AccessoryAutoDetection == ACCESSORY_AUTODETECTION_ENABLED)
			Settings.CurrentROMisSuperScopeCompatible = true;
		else if(Settings.AccessoryAutoDetection == ACCESSORY_AUTODETECTION_NONE)
			Settings.CurrentROMisSuperScopeCompatible = false;

		if(Settings.CurrentROMisSuperScopeCompatible)
			Emulator_SetControllerMode(SUPERSCOPE);
	}
	else if(emulator_implementation_is_justifier_compatible())
	{
		if(Settings.AccessoryAutoDetection == ACCESSORY_AUTODETECTION_CONFIRM)
		{
			create_msg_dialog(Settings.AccessoryType ? "This game supports the [Justifier] accessory. Do you want to enable [Justifier] support? (NOTE: [Justifier] will be mapped to the USB/Bluetooth mouse)" : "This game supports the [Justifier] accessory. Do you want to enable [Justifier] support? (NOTE: [Justifier] will be mapped to the left analog stick)",callback_justifier_compatible);
			create_callback_loop(1);
		}
		else if(Settings.AccessoryAutoDetection == ACCESSORY_AUTODETECTION_ENABLED)
			Settings.CurrentROMisJustifierCompatible = true;
		else if(Settings.AccessoryAutoDetection == ACCESSORY_AUTODETECTION_NONE)
			Settings.CurrentROMisJustifierCompatible = false;

		if(Settings.CurrentROMisJustifierCompatible)
			Emulator_SetControllerMode(TWO_JUSTIFIERS);
	}
	//this last rule is in case Multitap has been set manually by the user by pressing TRIANGLE on a ROM
	else if (controller_settings != MULTITAP)
		Emulator_SetControllerMode(TWO_JOYSTICKS);

	// Check if Chrono Trigger is loaded, if so, we need to set a variable to true
	// to get rid of an annoying mid-frame resolution switch to 256x239 which can cause
	// an undesirable flicker/breakup of the screen for a split second - this happens
	// whenever the game switches from normal mode to battle mode and vice versa
	emulator_odd_screen_height_jump_fix();

	Memory.LoadSRAM(S9xGetFilename(".srm", SRAM_DIR));

	GFX.Pitch = 1024;
	GFX.Screen = (uint16_t *)memalign(1024, 489472);

	if(emulator_initialized)
	{
		S9xGraphicsDeinit();
		S9xControlsSoftReset();
	}
	S9xGraphicsInit();


#define MAP_BUTTON(id, name) S9xMapButton((id), S9xGetCommandT((name)), false, keymap)
#define MAP_POINTER(id, name) S9xMapPointer((id), S9xGetCommandT((name)), false, keymap)

	// Controller initialization
	switch(controller_settings)
	{
		case TWO_JOYSTICKS:
			S9xSetController(0, CTL_JOYPAD,		0,	0,	0,	0);
			S9xSetController(1, CTL_JOYPAD,		1,	0,	0,	0);
			break;
		case MULTITAP:
			S9xSetController(0, CTL_JOYPAD,		0,	0,	0,	0);
			S9xSetController(1, CTL_MP5,		1,	2,	3,	4);
			break;
		case MOUSE:
			S9xSetController(0, CTL_MOUSE,		0,	0,	0,	0);
			S9xSetController(1, CTL_JOYPAD,		1,	0,	0,	0);
			break;
		case SUPERSCOPE:
			S9xSetController(0, CTL_JOYPAD,		0,	0,	0,	0);
			S9xSetController(1, CTL_SUPERSCOPE,	0,	0,	0,	0);
			break;
		case JUSTIFIER:
			S9xSetController(0, CTL_JOYPAD,		0,	0,	0,	0);
			S9xSetController(1, CTL_JUSTIFIER,	0,	0,	0,	0);
			break;
		case TWO_JUSTIFIERS:
			S9xSetController(0, CTL_JOYPAD,		0,	0,	0,	0);
			S9xSetController(1, CTL_JUSTIFIER,	1,	0,	0,	0);	
			break;
		default:
			break;
	}

	S9xUnmapAllControls();

	switch(controller_settings)
	{
		case TWO_JOYSTICKS:
			map_snes9x_standard_controls(PAD_1);      // Player 1
			map_snes9x_standard_controls(PAD_2);      // Player 2
			break;
		case MULTITAP:
			map_snes9x_standard_controls(PAD_1);      // Player 1
			map_snes9x_standard_controls(PAD_2);      // Player 2
			map_snes9x_standard_controls(PAD_3);      // Player 3
			map_snes9x_standard_controls(PAD_4);      // Player 4
			map_snes9x_standard_controls(PAD_5);      // Player 5

			map_snes9x_special_controls(PAD_3);       
			map_snes9x_special_controls(PAD_4);
			map_snes9x_special_controls(PAD_5);
			break;
		case MOUSE:
			MAP_BUTTON(MAKE_BUTTON(PAD_1, BTN_X), "Mouse1 R");
			MAP_BUTTON(MAKE_BUTTON(PAD_1, BTN_Y), "Mouse1 L");
			MAP_BUTTON(MAKE_BUTTON(PAD_2, BTN_X), "Mouse2 R");
			MAP_BUTTON(MAKE_BUTTON(PAD_2, BTN_Y), "Mouse2 L");
			MAP_POINTER(BTN_POINTER1, "Pointer Mouse1");
			MAP_POINTER(BTN_POINTER2, "Pointer Mouse2");
			break;
		case SUPERSCOPE:
			MAP_BUTTON(MAKE_BUTTON(PAD_1, BTN_A), "Superscope ToggleTurbo");
			MAP_BUTTON(MAKE_BUTTON(PAD_1, BTN_B), "Superscope Cursor");
			MAP_BUTTON(MAKE_BUTTON(PAD_1, BTN_X), "Superscope AimOffscreen");
			MAP_BUTTON(MAKE_BUTTON(PAD_1, BTN_Y), "Superscope Fire");
			MAP_BUTTON(MAKE_BUTTON(PAD_1, BTN_START), "Superscope Pause");
			MAP_BUTTON(MAKE_BUTTON(PAD_2, BTN_X), "Superscope AimOffscreen");
			MAP_BUTTON(MAKE_BUTTON(PAD_2, BTN_Y), "Superscope Fire");
			MAP_BUTTON(MAKE_BUTTON(PAD_1, BTN_START), "Justifier Pause");
			MAP_POINTER(BTN_SCOPE_POINTER, "Pointer Superscope");
			break;
		case JUSTIFIER:
		case TWO_JUSTIFIERS:
		default:
			break;
	}

	map_snes9x_special_controls(PAD_1);
	map_snes9x_special_controls(PAD_2);

	return true;
}

#define init_setting_uint(charstring, setting, defaultvalue) \
	if(!(config_get_uint(currentconfig, charstring, &setting))) \
		setting = defaultvalue; 

#define init_setting_int(charstring, setting, defaultvalue) \
	if(!(config_get_int(currentconfig, charstring, &setting))) \
		setting = defaultvalue; 

#define init_setting_bool(charstring, setting, defaultvalue) \
	if(!(config_get_bool(currentconfig, charstring, &setting))) \
		setting = defaultvalue; 

#define init_setting_bool(charstring, setting, defaultvalue) \
	if(!(config_get_bool(currentconfig, charstring, &setting))) \
		setting =	defaultvalue;

#define init_setting_char(charstring, setting, defaultvalue) \
	if(!(config_get_char_array(currentconfig, charstring, setting, sizeof(setting)))) \
		strncpy(setting,defaultvalue, sizeof(setting));

static bool file_exists(const char * filename)
{
	CellFsStat sb;
	if(cellFsStat(filename,&sb) == CELL_FS_SUCCEEDED)
		return true;
	else
		return false;
}

void emulator_init_settings(void)
{
	memset(&Settings, 0, sizeof(Settings));

	if(!file_exists(SYS_CONFIG_FILE))
	{
		FILE * f;
		f = fopen(SYS_CONFIG_FILE, "w");
		fclose(f);
	}

	config_file_t * currentconfig = config_file_new(SYS_CONFIG_FILE);

	/* emulator-specific settings */

#if 0
	Settings.DumpStreamsMaxFrames = -1;
	Settings.CartAName[0] = 0;
	Settings.CartBName[0] = 0;
#endif

	// ROM

	init_setting_bool("ROM::Interleaved2", Settings.ForceInterleaved2, false);
	init_setting_bool("ROM::InterleaveGD24", Settings.ForceInterleaveGD24, false);
	init_setting_uint("ROM::Cheat", Settings.ApplyCheats, false);
	init_setting_uint("ROM::NoPatch", Settings.NoPatch, false);

	init_setting_bool("ROM::LoROM", Settings.ForceLoROM, false);
	init_setting_bool("ROM::HiROM", Settings.ForceHiROM, false);
	if (Settings.ForceLoROM)
	{
		Settings.ForceHiROM = false;
	}

	init_setting_uint("ROM::PAL", Settings.ForcePAL,  0);
	init_setting_uint("ROM::NTSC", Settings.ForceNTSC, 0);

	if (Settings.ForcePAL)
	{
		Settings.ForceNTSC = false;
	}

	init_setting_bool("ROM::Header", Settings.ForceHeader, false);
	Settings.ForceNoHeader = !Settings.ForceHeader;

	init_setting_bool("ROM::Interleaved", Settings.ForceInterleaved, false);
	Settings.ForceNotInterleaved = !Settings.ForceInterleaved;

	//rom_filename = conf.GetStringDup("ROM::Filename", NULL);

	// Sound

	init_setting_bool("Sound::Sync", Settings.SoundSync, true);
	init_setting_bool("Sound::16BitSound", Settings.SixteenBitSound, true);
	init_setting_bool("Sound::Stereo", Settings.Stereo, true);
	init_setting_bool("Sound::Mute", Settings.Mute, false);

	// Display

	init_setting_bool("Display::HiRes", Settings.SupportHiRes, true);
	init_setting_uint("Display::MessagesInImage", Settings.AutoDisplayMessages, true);
	init_setting_uint("Display::MessageDisplayTime", Settings.InitialInfoStringTimeout, 120);

	// Settings

	init_setting_bool("Settings::BSXBootup", Settings.BSXBootup, false);
	init_setting_bool("Settings::TurboMode", Settings.TurboMode, false);
	init_setting_uint("Settings::StretchScreenshots", Settings.StretchScreenshots, 1);
	init_setting_bool("Settings::SnapshotScreenshots", Settings.SnapshotScreenshots, true);
	Settings.DontSaveOopsSnapshot = true;
	init_setting_int("Settings::AutoSaveDelay", Settings.AutoSaveDelay, 0);

	/*
	   if (conf.Exists("BSX::CartAName"))
	   {
	   Settings.CartAName[0]			= conf.GetString("BSX::CartAName");
	   }
	   if (conf.Exists("BSX::CartBName"))
	   {
	   Settings.CartBName[0]			= conf.GetString("BSX::CartBName");
	   }
	 */

	init_setting_uint("Settings::FrameTime", Settings.FrameTimePAL, 1667);
	init_setting_uint("Settings::FrameTime", Settings.FrameTimeNTSC, 1667);

#if 0
	if (!strcasecmp(conf.GetString("Settings::FrameSkip", "Auto"), "Auto"))
	{
		Settings.SkipFrames = AUTO_FRAMERATE;
	}
	else
	{
		Settings.SkipFrames = conf.GetUInt("Settings::FrameSkip", 0) + 1;
	}
#endif

	// Controls

	init_setting_bool("Controls::MouseMaster", Settings.MouseMaster, true);
	init_setting_bool("Controls::SuperscopeMaster", Settings.SuperScopeMaster, true);
	init_setting_bool("Controls::JustifierMaster", Settings.JustifierMaster, true);
	init_setting_bool("Controls::MP5Master", Settings.MultiPlayer5Master, true);
	//Settings.UpAndDown                  =  conf.GetBool("Controls::AllowLeftRight",            false);

#if 0
	if (conf.Exists("Controls::Port1"))
	{
		parse_controller_spec(0, conf.GetString("Controls::Port1"));
	}
	if (conf.Exists("Controls::Port2"))
		parse_controller_spec(1, conf.GetString("Controls::Port2"));

	if (conf.Exists("Controls::Mouse1Crosshair"))
	{
		parse_crosshair_spec(X_MOUSE1,     conf.GetString("Controls::Mouse1Crosshair"));
	}
	if (conf.Exists("Controls::Mouse2Crosshair"))
	{
		parse_crosshair_spec(X_MOUSE2,     conf.GetString("Controls::Mouse2Crosshair"));
	}
	if (conf.Exists("Controls::SuperscopeCrosshair"))
	{
		parse_crosshair_spec(X_SUPERSCOPE, conf.GetString("Controls::SuperscopeCrosshair"));
	}
	if (conf.Exists("Controls::Justifier1Crosshair"))
	{
		parse_crosshair_spec(X_JUSTIFIER1, conf.GetString("Controls::Justifier1Crosshair"));
	}
	if (conf.Exists("Controls::Justifier2Crosshair"))
	{
		parse_crosshair_spec(X_JUSTIFIER2, conf.GetString("Controls::Justifier2Crosshair"));
	}
#endif

	// Hack

	init_setting_bool("Hack::DisableGameSpecificHacks", Settings.DisableGameSpecificHacks, false);
	init_setting_bool("Hack::BlockInvalidVRAMAccess", Settings.BlockInvalidVRAMAccessMaster, true)
		init_setting_bool("Hack::SpeedHacks", Settings.ShutdownMaster, false);
	init_setting_bool("Hack::DisableIRQ", Settings.DisableIRQ, false);
	init_setting_bool("Hack::DisableHDMA", Settings.DisableHDMA, false);
	init_setting_int("Hack::HDMATiming", Settings.HDMATimingHack, 100);

	// Netplay

#ifdef NETPLAY_SUPPORT
	Settings.NetPlay = conf.GetBool("Netplay::Enable");

	Settings.Port = NP_DEFAULT_PORT;
	if (conf.Exists("Netplay::Port"))
	{
		Settings.Port = -(int) conf.GetUInt("Netplay::Port");
	}

	Settings.ServerName[0] = '\0';
	if (conf.Exists("Netplay::Server"))
	{
		conf.GetString("Netplay::Server", Settings.ServerName, 128);
	}
#endif
	S9xVerifyControllers();

	int tmp_int;
	double tmp_double;
	bool tmp_bool;
	char *tmp_str;

	//Some additional settings that are not properly set by SNES9x's LoadConfigFiles
	init_setting_uint("Settings::FrameTimePAL", Settings.FrameTimePAL, 20000);

	//PS3 - General Settings
	init_setting_uint("PS3General::ApplyShaderPresetOnStartup", Settings.ApplyShaderPresetOnStartup, 0);
	init_setting_uint("PS3General::KeepAspect", Settings.PS3KeepAspect, ASPECT_RATIO_4_3);
	init_setting_uint("PS3General::Smooth", Settings.PS3Smooth, 1);
	init_setting_uint("PS3General::Smooth2", Settings.PS3Smooth2, 1);
	init_setting_char("PS3General::PS3CurrentShader", Settings.PS3CurrentShader, DEFAULT_SHADER_FILE);
	init_setting_char("PS3General::PS3CurrentShader2", Settings.PS3CurrentShader2, DEFAULT_SHADER_FILE);
	init_setting_char("PS3General::Border", Settings.PS3CurrentBorder, DEFAULT_BORDER_FILE);
	init_setting_uint("PS3General::PS3TripleBuffering",Settings.TripleBuffering, 1);
	init_setting_char("PS3General::ShaderPresetPath", Settings.ShaderPresetPath, DEFAULT_PRESET_FILE);
	init_setting_char("PS3General::ShaderPresetTitle", Settings.ShaderPresetTitle, "Custom");
	init_setting_uint("PS3General::ScaleFactor", Settings.ScaleFactor, 2);
	init_setting_uint("PS3General::ViewportX", Settings.ViewportX, 0);
	init_setting_uint("PS3General::ViewportY", Settings.ViewportY, 0);
	init_setting_uint("PS3General::ViewportWidth", Settings.ViewportWidth, 0);
	init_setting_uint("PS3General::ViewportHeight", Settings.ViewportHeight, 0);
	init_setting_uint("PS3General::ScaleEnabled", Settings.ScaleEnabled, 1);
	init_setting_uint("PS3General::PS3CurrentResolution", Settings.PS3CurrentResolution, NULL);
	init_setting_uint("PS3General::OverscanEnabled", Settings.PS3OverscanEnabled, 0);
	init_setting_int("PS3General::OverscanAmount", Settings.PS3OverscanAmount, 0);
	init_setting_uint("PS3General::PS3PALTemporalMode60Hz", Settings.PS3PALTemporalMode60Hz, 0);
	init_setting_uint("Sound::SoundMode", Settings.SoundMode, SOUND_MODE_NORMAL);
	init_setting_char("RSound::RSoundServerIPAddress",  Settings.RSoundServerIPAddress, "0.0.0.0");
	init_setting_uint("PS3General::Throttled", Settings.Throttled, 1);
	init_setting_uint("PS3General::PS3FontSize", Settings.PS3FontSize, 100);

	if (config_get_char_array(currentconfig,"PS3Paths::PathSaveStates", tmp_str, sizeof(tmp_str)))
		config_get_char_array(currentconfig, "PS3Paths::PathSaveStates", Settings.PS3PathSaveStates, sizeof(Settings.PS3PathSaveStates));
	else
		strcpy(Settings.PS3PathSaveStates, usrDirPath);

	if (config_get_char_array(currentconfig,"PS3Paths::PathCheats", tmp_str, sizeof(tmp_str)))
		config_get_char_array(currentconfig, "PS3Paths::PathCheats", Settings.PS3PathCheats, sizeof(Settings.PS3PathCheats));
	else
		strncpy(Settings.PS3PathCheats, usrDirPath, sizeof(Settings.PS3PathCheats));

	init_setting_char("PS3Paths::PathSRAM", Settings.PS3PathSRAM, usrDirPath);
	init_setting_char("PS3Paths::PathROMDirectory", Settings.PS3PathROMDirectory, "/");
	init_setting_int("PS3General::ControlScheme", Settings.ControlScheme, CONTROL_SCHEME_DEFAULT);
	init_setting_uint("PS3General::CurrentSaveStateSlot",  Settings.CurrentSaveStateSlot, 0);
	init_setting_uint("PS3General::ScreenshotsEnabled", Settings.ScreenshotsEnabled, 0);
	if (config_get_char_array(currentconfig,"PS3General::GameAwareShaderPath", tmp_str, sizeof(tmp_str)))
		config_get_char_array(currentconfig, "PS3General::GameAwareShaderPath", Settings.GameAwareShaderPath, sizeof(Settings.GameAwareShaderPath));

	//Emulator-specific Settings
	init_setting_uint("PS3SNES9x::ResetBeforeRecordingMovie", Settings.ResetBeforeRecordingMovie, 0);
	init_setting_int("PS3SNES9x::AccessoryAutoDetection", Settings.AccessoryAutoDetection, ACCESSORY_AUTODETECTION_CONFIRM);
	init_setting_uint("PS3SNES9x::AccessoryType", Settings.AccessoryType, 0);
	init_setting_uint("PS3SNES9x::SRAMWriteProtect", Settings.SRAMWriteProtect, 0);
	init_setting_uint("Settings::TurboFrameSkip", Settings.TurboSkipFrames, 15);
	init_setting_uint("Sound::Rate", Settings.SoundPlaybackRate, 48000);
	init_setting_uint("Sound::InputRate", Settings.SoundInputRate, 31950);

	emulator_implementation_switch_control_scheme();
}

void emulator_implementation_set_texture(const char * fname)
{
	strcpy(Settings.PS3CurrentBorder, fname);
	Graphics->LoadMenuTexture(PS3Graphics::TEXTURE_BACKDROP, fname);
	Graphics->LoadMenuTexture(PS3Graphics::TEXTURE_MENU, DEFAULT_MENU_BORDER_FILE);
}


void emulator_implementation_set_shader_preset(const char * fname)
{
	config_file_t * currentconfig = config_file_new(fname);

	init_setting_uint("ScaleFactor", Settings.ScaleFactor, Settings.ScaleFactor);
	init_setting_uint("Smooth", Settings.PS3Smooth, Settings.PS3Smooth);
	init_setting_uint("Smooth2", Settings.PS3Smooth2, Settings.PS3Smooth2);
	init_setting_uint("ScaleEnabled", Settings.ScaleEnabled, Settings.ScaleEnabled);
	if(config_string_exists(currentconfig, "PS3CurrentShader", Settings.PS3CurrentShader))
	{
		char shadertemp[MAX_PATH_LENGTH];
		init_setting_char("PS3CurrentShader", shadertemp, DEFAULT_SHADER_FILE);
		snprintf(Settings.PS3CurrentShader, sizeof(Settings.PS3CurrentShader), "%s/%s", SHADERS_DIR_PATH, shadertemp);
	}
	if(config_string_exists(currentconfig, "PS3CurrentShader2", Settings.PS3CurrentShader2))
	{
		char shadertemp2[MAX_PATH_LENGTH];
		init_setting_char("PS3CurrentShader2", shadertemp2, DEFAULT_SHADER_FILE);
		snprintf(Settings.PS3CurrentShader2, sizeof(Settings.PS3CurrentShader2), "%s/%s", SHADERS_DIR_PATH, shadertemp2);
	}

	strncpy(Settings.ShaderPresetPath, fname, sizeof(Settings.ShaderPresetPath));

	init_setting_char("ShaderPresetTitle", Settings.ShaderPresetTitle, "Custom/None");
	printf("Gets to ShaderPresetTitle: %s\n", Settings.ShaderPresetTitle);

	//FIXME: This does not yet work
#if 0
	if (currentconfig.Exists("PS3General::Border"))
	{
		Settings.PS3CurrentBorder = currentconfig.GetString("PS3General::Border");
		//need_border_reload_hack = true;
	}
#endif
	init_setting_uint("KeepAspect", Settings.PS3KeepAspect, Settings.PS3KeepAspect);
	init_setting_uint("OverscanEnabled", Settings.PS3OverscanEnabled, Settings.PS3OverscanEnabled);
	init_setting_int("OverscanAmount", Settings.PS3OverscanAmount, Settings.PS3OverscanAmount);
	init_setting_uint("ViewportX", Settings.ViewportX, Settings.ViewportX);
	init_setting_uint("ViewportY", Settings.ViewportY, Settings.ViewportY);
	init_setting_uint("ViewportWidth", Settings.ViewportWidth, Settings.ViewportWidth);
	init_setting_uint("ViewportHeight", Settings.ViewportHeight, Settings.ViewportHeight);
	Graphics->LoadFragmentShader(Settings.PS3CurrentShader);
	Graphics->LoadFragmentShader(Settings.PS3CurrentShader2, 1);
	Graphics->SetFBOScale(Settings.ScaleEnabled,Settings.ScaleFactor);
	Graphics->set_aspect_ratio(Settings.PS3KeepAspect, SCREEN_RENDER_TEXTURE_WIDTH, SCREEN_RENDER_TEXTURE_HEIGHT, 1);
	Graphics->SetOverscan(Settings.PS3OverscanEnabled, (float)Settings.PS3OverscanAmount/100);
	Graphics->SetSmooth(Settings.PS3Smooth);
	Graphics->SetSmooth(Settings.PS3Smooth2, 1);
}

void emulator_implementation_set_gameaware(const char * fname)
{
	Graphics->InitStateUniforms(fname);
	strcpy(Settings.PS3CurrentShader, Graphics->GetFragmentShaderPath(0));
	strcpy(Settings.PS3CurrentShader2, Graphics->GetFragmentShaderPath(1));
}

#define string_concat_ps3_controls(padno, string) "PS3Player"#padno"::"#string""

#define map_ps3_standard_controls(padno) \
   config_set_uint(currentconfig, string_concat_ps3_controls(padno, DPad_Up),PS3Input.DPad_Up[padno]); \
   config_set_uint(currentconfig, string_concat_ps3_controls(padno, DPad_Down),PS3Input.DPad_Down[padno]); \
   config_set_uint(currentconfig, string_concat_ps3_controls(padno, DPad_Left),PS3Input.DPad_Left[padno]); \
   config_set_uint(currentconfig, string_concat_ps3_controls(padno, DPad_Right),PS3Input.DPad_Right[padno]); \
   config_set_uint(currentconfig, string_concat_ps3_controls(padno, ButtonCircle),PS3Input.ButtonCircle[padno]); \
   config_set_uint(currentconfig, string_concat_ps3_controls(padno, ButtonCross), PS3Input.ButtonCross[padno]); \
   config_set_uint(currentconfig, string_concat_ps3_controls(padno, ButtonTriangle), PS3Input.ButtonTriangle[padno]); \
   config_set_uint(currentconfig, string_concat_ps3_controls(padno, ButtonSquare), PS3Input.ButtonSquare[padno]); \
   config_set_uint(currentconfig, string_concat_ps3_controls(padno, ButtonSelect), PS3Input.ButtonSelect[padno]); \
   config_set_uint(currentconfig, string_concat_ps3_controls(padno, ButtonStart), PS3Input.ButtonStart[padno]); \
   config_set_uint(currentconfig, string_concat_ps3_controls(padno, ButtonL1), PS3Input.ButtonL1[padno]); \
   config_set_uint(currentconfig, string_concat_ps3_controls(padno, ButtonR1), PS3Input.ButtonR1[padno]); \
   config_set_uint(currentconfig, string_concat_ps3_controls(padno, ButtonL2), PS3Input.ButtonL2[padno]); \
   config_set_uint(currentconfig, string_concat_ps3_controls(padno, ButtonR2), PS3Input.ButtonR2[padno]); \
   config_set_uint(currentconfig, string_concat_ps3_controls(padno, ButtonL2_ButtonL3), PS3Input.ButtonL2_ButtonL3[padno]); \
   config_set_uint(currentconfig, string_concat_ps3_controls(padno, ButtonL2_ButtonR3),PS3Input.ButtonL2_ButtonR3[padno]); \
   config_set_uint(currentconfig, string_concat_ps3_controls(padno, ButtonR3),PS3Input.ButtonR3[padno]); \
   config_set_uint(currentconfig, string_concat_ps3_controls(padno, ButtonL3), PS3Input.ButtonL3[padno]); \
   config_set_uint(currentconfig, string_concat_ps3_controls(padno, ButtonL2_ButtonR2),PS3Input.ButtonL2_ButtonR2[padno]); \
   config_set_uint(currentconfig, string_concat_ps3_controls(padno, ButtonL2_AnalogR_Right), PS3Input.ButtonL2_AnalogR_Right[padno]); \
   config_set_uint(currentconfig, string_concat_ps3_controls(padno, ButtonL2_AnalogR_Left),PS3Input.ButtonL2_AnalogR_Left[padno]); \
   config_set_uint(currentconfig, string_concat_ps3_controls(padno, ButtonL2_AnalogR_Up), PS3Input.ButtonL2_AnalogR_Up[padno]); \
   config_set_uint(currentconfig, string_concat_ps3_controls(padno, ButtonL2_AnalogR_Down), PS3Input.ButtonL2_AnalogR_Down[padno]); \
   config_set_uint(currentconfig, string_concat_ps3_controls(padno, ButtonR2_AnalogR_Right),PS3Input.ButtonR2_AnalogR_Right[padno]); \
   config_set_uint(currentconfig, string_concat_ps3_controls(padno, ButtonR2_AnalogR_Left), PS3Input.ButtonR2_AnalogR_Left[padno]); \
   config_set_uint(currentconfig, string_concat_ps3_controls(padno, ButtonR2_AnalogR_Up), PS3Input.ButtonR2_AnalogR_Up[padno]); \
   config_set_uint(currentconfig, string_concat_ps3_controls(padno, ButtonR2_AnalogR_Down),PS3Input.ButtonR2_AnalogR_Down[padno]); \
   config_set_uint(currentconfig, string_concat_ps3_controls(padno, ButtonL2_ButtonR2_AnalogR_Down), PS3Input.ButtonL2_ButtonR2_AnalogR_Down[padno]); \
   config_set_uint(currentconfig, string_concat_ps3_controls(padno, ButtonR2_ButtonR3), PS3Input.ButtonR2_ButtonR3[padno]); \
   config_set_uint(currentconfig, string_concat_ps3_controls(padno, ButtonR3_ButtonL3), PS3Input.ButtonR3_ButtonL3[padno]); \
   config_set_uint(currentconfig, string_concat_ps3_controls(padno, AnalogR_Up), PS3Input.AnalogR_Up[padno]); \
   config_set_uint(currentconfig, string_concat_ps3_controls(padno, AnalogR_Down), PS3Input.AnalogR_Down[padno]); \
   config_set_uint(currentconfig, string_concat_ps3_controls(padno, AnalogR_Left), PS3Input.AnalogR_Left[padno]); \
   config_set_uint(currentconfig, string_concat_ps3_controls(padno, AnalogR_Right),PS3Input.AnalogR_Right[padno]); \
   \
   config_set_uint(currentconfig, string_concat_ps3_controls(padno, AnalogR_Up_Type), PS3Input.AnalogR_Up_Type[padno]); \
   config_set_uint(currentconfig, string_concat_ps3_controls(padno, AnalogR_Down_Type), PS3Input.AnalogR_Down_Type[padno]); \
   config_set_uint(currentconfig, string_concat_ps3_controls(padno, AnalogR_Left_Type), PS3Input.AnalogR_Left_Type[padno]); \
   config_set_uint(currentconfig, string_concat_ps3_controls(padno, AnalogR_Right_Type), PS3Input.AnalogR_Right_Type[padno]);

#define get_ps3_standard_controls(padno) \
init_setting_uint(string_concat_ps3_controls(padno, DPad_Up),PS3Input.DPad_Up[padno],BTN_UP); \
init_setting_uint(string_concat_ps3_controls(padno, DPad_Down),PS3Input.DPad_Down[padno],BTN_DOWN); \
init_setting_uint(string_concat_ps3_controls(padno, DPad_Left),PS3Input.DPad_Left[padno],BTN_LEFT); \
init_setting_uint(string_concat_ps3_controls(padno, DPad_Right),PS3Input.DPad_Right[padno],BTN_RIGHT); \
init_setting_uint(string_concat_ps3_controls(padno, ButtonCircle),PS3Input.ButtonCircle[padno],BTN_A); \
init_setting_uint(string_concat_ps3_controls(padno, ButtonCross),PS3Input.ButtonCross[padno],BTN_B); \
init_setting_uint(string_concat_ps3_controls(padno, ButtonTriangle),PS3Input.ButtonTriangle[padno],BTN_X); \
init_setting_uint(string_concat_ps3_controls(padno, ButtonSquare),PS3Input.ButtonSquare[padno],BTN_Y); \
init_setting_uint(string_concat_ps3_controls(padno, ButtonSelect),PS3Input.ButtonSelect[padno],BTN_SELECT); \
init_setting_uint(string_concat_ps3_controls(padno, ButtonStart),PS3Input.ButtonStart[padno],BTN_START); \
init_setting_uint(string_concat_ps3_controls(padno, ButtonL1), PS3Input.ButtonL1[padno], BTN_L); \
init_setting_uint(string_concat_ps3_controls(padno, ButtonR1), PS3Input.ButtonR1[padno], BTN_R); \
init_setting_uint(string_concat_ps3_controls(padno, ButtonL2), PS3Input.ButtonL2[padno], BTN_NONE); \
init_setting_uint(string_concat_ps3_controls(padno, ButtonR2), PS3Input.ButtonR2[padno], BTN_NONE); \
init_setting_uint(string_concat_ps3_controls(padno, ButtonL2_ButtonL3), PS3Input.ButtonL2_ButtonL3[padno], BTN_NONE); \
init_setting_uint(string_concat_ps3_controls(padno, ButtonL2_ButtonR3), PS3Input.ButtonL2_ButtonR3[padno], BTN_QUICKLOAD); \
init_setting_uint(string_concat_ps3_controls(padno, ButtonR3), PS3Input.ButtonR3[padno], BTN_NONE); \
init_setting_uint(string_concat_ps3_controls(padno, ButtonL3), PS3Input.ButtonL3[padno], BTN_NONE); \
init_setting_uint(string_concat_ps3_controls(padno, ButtonL2_ButtonR2), PS3Input.ButtonL2_ButtonR2[padno], BTN_NONE); \
init_setting_uint(string_concat_ps3_controls(padno, ButtonL2_AnalogR_Right), PS3Input.ButtonL2_AnalogR_Right[padno], BTN_INCREMENTCHEAT); \
init_setting_uint(string_concat_ps3_controls(padno, ButtonL2_AnalogR_Left), PS3Input.ButtonL2_AnalogR_Left[padno], BTN_DECREMENTCHEAT); \
init_setting_uint(string_concat_ps3_controls(padno, ButtonL2_AnalogR_Up), PS3Input.ButtonL2_AnalogR_Up[padno], BTN_NONE); \
init_setting_uint(string_concat_ps3_controls(padno, ButtonL2_AnalogR_Down), PS3Input.ButtonL2_AnalogR_Down[padno], BTN_NONE); \
init_setting_uint(string_concat_ps3_controls(padno, ButtonR2_AnalogR_Right), PS3Input.ButtonL2_AnalogR_Right[padno], BTN_NONE); \
init_setting_uint(string_concat_ps3_controls(padno, ButtonR2_AnalogR_Left), PS3Input.ButtonR2_AnalogR_Left[padno], BTN_NONE); \
init_setting_uint(string_concat_ps3_controls(padno, ButtonR2_AnalogR_Up), PS3Input.ButtonR2_AnalogR_Up[padno], BTN_NONE); \
init_setting_uint(string_concat_ps3_controls(padno, ButtonR2_AnalogR_Down), PS3Input.ButtonR2_AnalogR_Down[padno], BTN_NONE); \
init_setting_uint(string_concat_ps3_controls(padno, ButtonL2_ButtonR2_AnalogR_Down), PS3Input.ButtonL2_ButtonR2_AnalogR_Down[padno], BTN_NONE); \
init_setting_uint(string_concat_ps3_controls(padno, ButtonR2_ButtonR3), PS3Input.ButtonR2_ButtonR3[padno], BTN_QUICKSAVE); \
init_setting_uint(string_concat_ps3_controls(padno, ButtonR3_ButtonL3), PS3Input.ButtonR3_ButtonL3[padno], BTN_EXITTOMENU); \
init_setting_uint(string_concat_ps3_controls(padno, AnalogR_Up), PS3Input.AnalogR_Up[padno], BTN_CHEATENABLE); \
init_setting_uint(string_concat_ps3_controls(padno, AnalogR_Down), PS3Input.AnalogR_Down[padno], BTN_CHEATDISABLE); \
init_setting_uint(string_concat_ps3_controls(padno, AnalogR_Left), PS3Input.AnalogR_Left[padno], BTN_DECREMENTSAVE); \
init_setting_uint(string_concat_ps3_controls(padno, AnalogR_Right), PS3Input.AnalogR_Right[padno], BTN_INCREMENTSAVE); \
init_setting_uint(string_concat_ps3_controls(padno, AnalogR_Up_Type), PS3Input.AnalogR_Up_Type[padno], 0); \
init_setting_uint(string_concat_ps3_controls(padno, AnalogR_Down_Type), PS3Input.AnalogR_Down_Type[padno], 0); \
init_setting_uint(string_concat_ps3_controls(padno, AnalogR_Left_Type), PS3Input.AnalogR_Left_Type[padno], 0); \
init_setting_uint(string_concat_ps3_controls(padno, AnalogR_Right_Type), PS3Input.AnalogR_Right_Type[padno], 0);

#define map_ps3_button_array(buttonarray) \
	int tmp_int; \
	for(int i = 0; i < MAX_PADS; i++) \
	{ \
	Input_MapButton(PS3Input.DPad_Up[i],false,              buttonarray[0]); \
	Input_MapButton(PS3Input.DPad_Down[i],false,            buttonarray[1]); \
	Input_MapButton(PS3Input.DPad_Left[i],false,            buttonarray[2]); \
	Input_MapButton(PS3Input.DPad_Right[i],false,           buttonarray[3]); \
	Input_MapButton(PS3Input.ButtonCircle[i],false,         buttonarray[4]); \
	Input_MapButton(PS3Input.ButtonCross[i],false,          buttonarray[5]); \
	Input_MapButton(PS3Input.ButtonTriangle[i],false,       buttonarray[6]); \
	Input_MapButton(PS3Input.ButtonSquare[i],false,         buttonarray[7]); \
	Input_MapButton(PS3Input.ButtonSelect[i],false,         buttonarray[8]); \
	Input_MapButton(PS3Input.ButtonStart[i],false,          buttonarray[9]); \
	Input_MapButton(PS3Input.ButtonL1[i],false,             buttonarray[10]); \
	Input_MapButton(PS3Input.ButtonL2[i],false,             buttonarray[11]); \
	Input_MapButton(PS3Input.ButtonR2[i],false,             buttonarray[12]); \
	Input_MapButton(PS3Input.ButtonL3[i],false,             buttonarray[13]); \
	Input_MapButton(PS3Input.ButtonR3[i],false,             buttonarray[14]); \
	Input_MapButton(PS3Input.ButtonR1[i],false,             buttonarray[15]); \
	Input_MapButton(PS3Input.ButtonL2_ButtonL3[i],false,    buttonarray[16]); \
	Input_MapButton(PS3Input.ButtonL2_ButtonR2[i],false,    buttonarray[17]); \
	Input_MapButton(PS3Input.ButtonL2_ButtonR3[i],false,    buttonarray[18]); \
	Input_MapButton(PS3Input.ButtonR2_ButtonR3[i],false,    buttonarray[19]); \
	Input_MapButton(PS3Input.AnalogR_Up[i],false,           buttonarray[20]); \
	Input_MapButton(PS3Input.AnalogR_Down[i],false,         buttonarray[21]); \
	Input_MapButton(PS3Input.AnalogR_Left[i],false,         buttonarray[22]); \
	Input_MapButton(PS3Input.AnalogR_Right[i],false,        buttonarray[23]); \
	Input_MapButton(PS3Input.ButtonL2_AnalogR_Right[i],false, buttonarray[24]); \
	Input_MapButton(PS3Input.ButtonL2_AnalogR_Left[i],false, buttonarray[25]); \
	Input_MapButton(PS3Input.ButtonL2_AnalogR_Up[i],false, buttonarray[26]); \
	Input_MapButton(PS3Input.ButtonL2_AnalogR_Down[i],false, buttonarray[27]); \
	Input_MapButton(PS3Input.ButtonR2_AnalogR_Right[i],false, buttonarray[28]); \
	Input_MapButton(PS3Input.ButtonR2_AnalogR_Left[i],false, buttonarray[29]); \
	Input_MapButton(PS3Input.ButtonR2_AnalogR_Up[i],false, buttonarray[30]); \
	Input_MapButton(PS3Input.ButtonR2_AnalogR_Down[i],false, buttonarray[31]); \
	Input_MapButton(PS3Input.ButtonL2_ButtonR2_AnalogR_Down[i],false, buttonarray[32]); \
	Input_MapButton(PS3Input.ButtonR3_ButtonL3[i],false, buttonarray[33]); \
	PS3Input.AnalogR_Up_Type[i] = buttonarray[34]; \
	PS3Input.AnalogR_Down_Type[i] = buttonarray[35]; \
	PS3Input.AnalogR_Left_Type[i] = buttonarray[36]; \
	PS3Input.AnalogR_Right_Type[i] = buttonarray[37]; \
	}
   

void emulator_implementation_button_mapping_settings(int map_button_option_enum)
{
	config_file_t * currentconfig = config_file_new(SYS_CONFIG_FILE);

	switch(map_button_option_enum)
	{
		case MAP_BUTTONS_OPTION_SETTER:
			map_ps3_standard_controls(0);
			map_ps3_standard_controls(1);
			map_ps3_standard_controls(2);
			map_ps3_standard_controls(3);
			map_ps3_standard_controls(4);
			map_ps3_standard_controls(5);
			map_ps3_standard_controls(6);
			map_ps3_standard_controls(MAX_PADS);
			config_file_write(currentconfig, SYS_CONFIG_FILE);
			break;
		case MAP_BUTTONS_OPTION_GETTER:
			get_ps3_standard_controls(0);
			get_ps3_standard_controls(1);
			get_ps3_standard_controls(2);
			get_ps3_standard_controls(3);
			get_ps3_standard_controls(4);
			get_ps3_standard_controls(5);
			get_ps3_standard_controls(6);
			get_ps3_standard_controls(MAX_PADS);
			break;
		case MAP_BUTTONS_OPTION_DEFAULT:
			{
				uint32_t array_btn[] = {BTN_UP, BTN_DOWN, BTN_LEFT, BTN_RIGHT, BTN_A, BTN_B, BTN_X, BTN_Y, BTN_SELECT, BTN_START, BTN_L, BTN_NONE, BTN_NONE, BTN_NONE, BTN_INGAME_MENU, BTN_R, BTN_NONE, BTN_NONE, BTN_QUICKLOAD, BTN_QUICKSAVE, BTN_CHEATENABLE, BTN_CHEATDISABLE, BTN_DECREMENTSAVE, BTN_INCREMENTSAVE, BTN_INCREMENTCHEAT, BTN_DECREMENTCHEAT, BTN_NONE, BTN_NONE, BTN_NONE, BTN_NONE, BTN_NONE, BTN_NONE, BTN_NONE, BTN_EXITTOMENU, 0, 0, 0, 0};
				map_ps3_button_array(array_btn);
				break;
			}
		case MAP_BUTTONS_OPTION_NEW:
			{
				uint32_t array_btn[] = {BTN_UP, BTN_DOWN, BTN_LEFT, BTN_RIGHT, BTN_A, BTN_B, BTN_X, BTN_Y, BTN_SELECT, BTN_START, BTN_L, BTN_NONE, BTN_NONE, BTN_NONE, BTN_INGAME_MENU, BTN_R, BTN_NONE, BTN_NONE, BTN_NONE, BTN_SRAM_WRITEPROTECT, BTN_NONE, BTN_FASTFORWARD, BTN_DECREMENTTURBO, BTN_INCREMENTTURBO, BTN_INCREMENTCHEAT, BTN_DECREMENTCHEAT, BTN_CHEATTOGGLE, BTN_CHEATINPUT, BTN_INCREMENTSAVE, BTN_DECREMENTSAVE, BTN_QUICKLOAD, BTN_QUICKSAVE, BTN_CHEATINPUTLABEL, BTN_EXITTOMENU, 0, 0, 0, 0};
				map_ps3_button_array(array_btn);
				break;
			}
	}
}


void emulator_implementation_save_custom_controls(bool showdialog)
{
	if(Settings.ControlScheme == CONTROL_SCHEME_CUSTOM)
	{
		if(showdialog)
		{
			create_msg_dialog("Do you want to save the custom controller settings?",callback_save_custom_controls);
			create_callback_loop(0);
		}
		if(!showdialog || Settings.SaveCustomControlScheme)
			emulator_implementation_button_mapping_settings(MAP_BUTTONS_OPTION_SETTER);
	}
}

#define CONFIG_FILE 0
#define SHADER_PRESET_FILE 1


void emulator_save_settings(uint64_t filetosave)
{
	config_file_t * currentconfig = config_file_new(SYS_CONFIG_FILE);
	char filepath[MAX_PATH_LENGTH];

	switch(filetosave)
	{
		case CONFIG_FILE:
			strcpy(filepath, SYS_CONFIG_FILE);

			config_set_uint(currentconfig, "PS3General::ControlScheme",Settings.ControlScheme);
			config_set_uint(currentconfig, "PS3General::CurrentSaveStateSlot",Settings.CurrentSaveStateSlot);
			config_set_uint(currentconfig, "PS3General::KeepAspect",Settings.PS3KeepAspect);
			config_set_uint(currentconfig, "PS3General::ApplyShaderPresetOnStartup", Settings.ApplyShaderPresetOnStartup);
			config_set_uint(currentconfig, "PS3General::ViewportX", Graphics->get_viewport_x());
			config_set_uint(currentconfig, "PS3General::ViewportY", Graphics->get_viewport_y());
			config_set_uint(currentconfig, "PS3General::ViewportWidth", Graphics->get_viewport_width());
			config_set_uint(currentconfig, "PS3General::ViewportHeight", Graphics->get_viewport_height());
			config_set_uint(currentconfig, "PS3General::Smooth", Settings.PS3Smooth);
			config_set_uint(currentconfig, "PS3General::Smooth2", Settings.PS3Smooth2);
			config_set_uint(currentconfig, "PS3General::ScaleFactor", Settings.ScaleFactor);
			config_set_uint(currentconfig, "PS3General::OverscanEnabled", Settings.PS3OverscanEnabled);
			config_set_uint(currentconfig, "PS3General::OverscanAmount",Settings.PS3OverscanAmount);
			config_set_uint(currentconfig, "PS3General::PS3FontSize",Settings.PS3FontSize);
			config_set_uint(currentconfig, "PS3General::Throttled",Settings.Throttled);
			config_set_uint(currentconfig, "PS3General::PS3PALTemporalMode60Hz",Settings.PS3PALTemporalMode60Hz);
			config_set_uint(currentconfig, "PS3General::PS3TripleBuffering",Settings.TripleBuffering);
			config_set_uint(currentconfig, "PS3General::ScreenshotsEnabled",Settings.ScreenshotsEnabled);
			config_set_uint(currentconfig, "Sound::SoundMode",Settings.SoundMode);
			config_set_uint(currentconfig, "PS3General::PS3CurrentResolution",Graphics->GetCurrentResolution());
			config_set_string(currentconfig, "PS3General::ShaderPresetPath", Settings.ShaderPresetPath);
			config_set_string(currentconfig, "PS3General::ShaderPresetTitle", Settings.ShaderPresetTitle);
			config_set_string(currentconfig, "PS3General::PS3CurrentShader",Graphics->GetFragmentShaderPath());
			config_set_string(currentconfig, "PS3General::PS3CurrentShader2", Graphics->GetFragmentShaderPath(1));
			config_set_string(currentconfig, "PS3General::Border", Settings.PS3CurrentBorder);
			config_set_string(currentconfig, "PS3General::GameAwareShaderPath", Settings.GameAwareShaderPath);
			config_set_string(currentconfig, "PS3Paths::PathSaveStates", Settings.PS3PathSaveStates);
			config_set_string(currentconfig, "PS3Paths::PathSRAM", Settings.PS3PathSRAM);
			config_set_string(currentconfig, "PS3Paths::PathROMDirectory", Settings.PS3PathROMDirectory);
			config_set_string(currentconfig, "PS3Paths::PathCheats", Settings.PS3PathCheats);
			config_set_string(currentconfig, "RSound::RSoundServerIPAddress", Settings.RSoundServerIPAddress);
			config_set_uint(currentconfig, "PS3General::ScaleEnabled", Settings.ScaleEnabled);

			//Emulator-specific settings
			config_set_uint(currentconfig, "Sound::InputRate",Settings.SoundInputRate);
			config_set_uint(currentconfig, "PS3SNES9x::ResetBeforeRecordingMovie",Settings.ResetBeforeRecordingMovie);
			config_set_uint(currentconfig, "PS3SNES9x::AccessoryAutoDetection",Settings.AccessoryAutoDetection);
			config_set_uint(currentconfig, "PS3SNES9x::AccessoryType",Settings.AccessoryType);
			config_set_uint(currentconfig, "PS3SNES9x::SRAMWriteProtect",Settings.SRAMWriteProtect);
			config_set_bool(currentconfig, "Sound::Sync",Settings.SoundSync);
			config_set_uint(currentconfig, "ROM::PAL",Settings.ForcePAL);
			config_set_uint(currentconfig, "ROM::NTSC",Settings.ForceNTSC);
			config_set_uint(currentconfig, "ROM::Cheat",Settings.ApplyCheats);
			config_set_uint(currentconfig, "ROM::Patch", !Settings.NoPatch);
			config_set_uint(currentconfig, "Display::MessagesInImage",Settings.AutoDisplayMessages);
			config_set_bool(currentconfig, "Settings::BSXBootup",Settings.BSXBootup);
			config_set_uint(currentconfig, "Settings::TurboFrameSkip",Settings.TurboSkipFrames);

			config_file_write(currentconfig, filepath);
			emulator_implementation_button_mapping_settings(MAP_BUTTONS_OPTION_SETTER);
			break;
		case SHADER_PRESET_FILE:
			snprintf(filepath, sizeof(filepath), "%s/test.conf", usrDirPath);
			currentconfig = config_file_new(filepath);

			config_set_string(currentconfig, "PS3General::PS3CurrentShader", Graphics->GetFragmentShaderPath());
			config_set_string(currentconfig, "PS3General::PS3CurrentShader2", Graphics->GetFragmentShaderPath(1));
			config_set_string(currentconfig, "PS3General::Border", Settings.PS3CurrentBorder);
			config_set_uint(currentconfig, "PS3General::Smooth", Settings.PS3Smooth);
			config_set_uint(currentconfig, "PS3General::Smooth2", Settings.PS3Smooth2);
			char tmp[10] = "Test";
			config_set_string(currentconfig, "PS3General::ShaderPresetTitle", tmp);
			config_set_uint(currentconfig, "PS3General::ViewportX", Graphics->get_viewport_x());
			config_set_uint(currentconfig, "PS3General::ViewportY", Graphics->get_viewport_y());
			config_set_uint(currentconfig, "PS3General::ViewportWidth", Graphics->get_viewport_width());
			config_set_uint(currentconfig, "PS3General::ViewportHeight", Graphics->get_viewport_height());
			config_set_uint(currentconfig, "PS3General::ScaleFactor", Settings.ScaleFactor);
			config_set_uint(currentconfig, "PS3General::ScaleEnabled", Settings.ScaleEnabled);
			config_set_uint(currentconfig, "PS3General::OverscanEnabled", Settings.PS3OverscanEnabled);
			config_set_uint(currentconfig, "PS3General::OverscanAmount", Settings.PS3OverscanAmount);

			config_file_write(currentconfig, filepath);
			break;
	}
}

void emulator_implementation_switch_control_scheme(void)
{
	switch(Settings.ControlScheme)
	{
		case CONTROL_SCHEME_DEFAULT:
			emulator_implementation_button_mapping_settings(MAP_BUTTONS_OPTION_DEFAULT);
			break;
		case CONTROL_SCHEME_NEW:
			emulator_implementation_button_mapping_settings(MAP_BUTTONS_OPTION_NEW);
			break;
		case CONTROL_SCHEME_CUSTOM:
			emulator_implementation_button_mapping_settings(MAP_BUTTONS_OPTION_GETTER);
			break;
	}
}

const char *S9xStringInput (const char *message)
{
	static char	buffer[256];
	
	printf("%s: ", message);
	fflush(stdout);

	if (fgets(buffer, sizeof(buffer) - 2, stdin))
		return (buffer);

	return (NULL);
}

bool8 S9xOpenSnapshotFile (const char *filepath, bool8 read_only, STREAM *file)
{
	if(read_only)
	{
		if((*file = OPEN_STREAM(filepath, "rb")) != 0)
			return (TRUE);
	}
	else
	{
		if((*file = OPEN_STREAM(filepath, "wb")) != 0)
			return (TRUE);
	}
	return (FALSE);
}

void S9xCloseSnapshotFile (STREAM file)
{
	CLOSE_STREAM (file);
}

void S9xExit (void)
{
}

void S9xMessage (int type, int number, char const *message)
{
	S9xSetInfoString(message);
}

const char * S9xGetFilename (const char *ex, enum s9x_getdirtype dirtype)
{
	static char	s[PATH_MAX + 1];
	char		drive[_MAX_DRIVE + 1];
	char		dir[_MAX_DIR + 1];
	char		fname[_MAX_FNAME + 1];
	char		ext[_MAX_EXT + 1];

	_splitpath(Memory.ROMFilename, drive, dir, fname, ext);
	snprintf(s, PATH_MAX + 1, "%s%s%s%s", S9xGetDirectory(dirtype), SLASH_STR, fname, ex);

	return (s);
}

const char * S9xGetFilenameInc (const char *ex, enum s9x_getdirtype dirtype)
{
	static char	s[PATH_MAX + 1];
	char		drive[_MAX_DRIVE + 1];
	char		dir[_MAX_DIR + 1];
	char		fname[_MAX_FNAME + 1];
	char		ext[_MAX_EXT + 1];

	unsigned int	i = 0;
	const char	*d;

	_splitpath(Memory.ROMFilename, drive, dir, fname, ext);
	d = S9xGetDirectory(dirtype);

	snprintf(s, PATH_MAX + 1, "%s%s%s.%03d%s", d, SLASH_STR, fname, i++, ex);

	return (s);
}

const char * S9xBasename (const char *f)
{
	const char	*p;

	if ((p = strrchr(f, '/')) != NULL || (p = strrchr(f, '\\')) != NULL)
		return (p + 1);

	return (f);
}

const char * S9xGetDirectory (enum s9x_getdirtype dirtype)
{
	static char s[PATH_MAX + 1];

	switch (dirtype)
	{
		case SNAPSHOT_DIR:
			return Settings.PS3PathSaveStates;
		case CHEAT_DIR:
			return Settings.PS3PathCheats;
		case SRAM_DIR:
			return Settings.PS3PathSRAM;
		case ROMFILENAME_DIR:
			strncpy(s, Memory.ROMFilename, PATH_MAX + 1);
			s[PATH_MAX] = 0;

			for (unsigned int i = strlen(s); i >= 0; --i)
			{
				if (s[i] == SLASH_CHAR)
				{
					s[i] = 0;
					break;
				}
			}

			return s;
		default:
			return usrDirPath;
	}
}

const char *S9xChooseFilename (bool8 read_only)
{
	static char	filename[PATH_MAX + 1];
	static char	drive[_MAX_DRIVE + 1];
	static char	dir[_MAX_DIR + 1];
	static char	def[_MAX_FNAME + 1];
	static char	ext[_MAX_EXT + 1];

	_splitpath(Memory.ROMFilename, drive, dir, def, ext);
	snprintf(filename, PATH_MAX + 1, "%s%s%s.%03d", S9xGetDirectory(SNAPSHOT_DIR), SLASH_STR, def, Settings.CurrentSaveStateSlot);

	return filename;
}

#if 0
const char * S9xChooseMovieFilename (bool8 read_only)
{
	char	drive[_MAX_DRIVE + 1], dir[_MAX_DIR + 1], fname[_MAX_FNAME + 1], ext[_MAX_EXT + 1];

	char	title[64];

	_splitpath(Memory.ROMFilename, drive, dir, fname, ext);
	sprintf(title, "Choose movie %s filename", read_only ? "playback" : "record");
	const char	*filename;

	S9xSetSoundMute(TRUE);
	filename = S9xGetFilename(".smv", SNAPSHOT_DIR);
	S9xSetSoundMute(FALSE);

	return (filename);
}
#endif

#if 0
void S9xToggleSoundChannel (int c)
{
	static uint8	sound_switch = 255;
	
	if (c == 8)
		sound_switch = 255;
	else
		sound_switch ^= 1 << c;

	S9xSetSoundControl(sound_switch);
}
#endif

void _splitpath (const char *path, char *drive, char *dir, char *fname, char *ext)
{
	*drive = 0;

	const char	*slash = strrchr(path, SLASH_CHAR);
	const char	*dot   = strrchr(path, '.');

	if (dot && slash && dot < slash)
		dot = NULL;

	if (!slash)
	{
		*dir = 0;

		strcpy(fname, path);

		if (dot)
		{
			fname[dot - path] = 0;
			strcpy(ext, dot + 1);
		}
		else
			*ext = 0;
	}
	else
	{
		strcpy(dir, path);
		dir[slash - path] = 0;

		strcpy(fname, slash + 1);

		if (dot)
		{
			fname[dot - slash - 1] = 0;
			strcpy(ext, dot + 1);
		}
		else
			*ext = 0;
	}
}

void _makepath (char *path, const char *, const char *dir, const char *fname, const char *ext)
{
	if (dir && *dir)
	{
		strcpy(path, dir);
		strcat(path, SLASH_STR);
	}
	else
		*path = 0;

	strcat(path, fname);

	if (ext && *ext)
	{
		strcat(path, ".");
		strcat(path, ext);
	}
}

#define emulator_s9x_mainloop() create_emulator_loop(EMULATOR_IMPLEMENTATION_INPUT_LOOP, S9xMainLoop);

#define calculate_aspect_ratio_before_load() \
   if (Graphics->calculate_aspect_ratio_before_game_load()) \
   { \
      Graphics->set_aspect_ratio(Settings.PS3KeepAspect, IPPU.RenderedScreenWidth, IPPU.RenderedScreenHeight, 1); \
   }

static void Emulator_Initialize(void)
{
	if(emulator_init_system())
	{
		if(!emulator_initialized)
			emulator_initialized = 1;

		need_load_rom = false;
	}
	else
		need_load_rom = true;
}

static void emulator_start(void)
{
	if(need_load_rom)
		Emulator_Initialize();

	S9xSetSoundMute(false);

	if (Graphics->GetCurrentResolution() == CELL_VIDEO_OUT_RESOLUTION_576)
	{
		if(Graphics->CheckResolution(CELL_VIDEO_OUT_RESOLUTION_576))
		{
			if(Settings.PAL)
			{
				//Game is PAL
				if(Graphics->GetPAL60Hz())
					//PAL60 is ON, turn it off for PAL
					emulator_switch_pal_60hz(false);
			}
			else
			{
				//Game is NTSC
				if(!Graphics->GetPAL60Hz())
					//PAL60 is OFF, turn it on for NTSC
					emulator_switch_pal_60hz(true);
			}
		}
	}

	calculate_aspect_ratio_before_load();

	if(Settings.Throttled)
		audio_driver->unpause(audio_handle);


	//Game - standard controls
	emulator_s9x_mainloop();

	emulator_save_sram();
	S9xSetSoundMute(true);
}

float Emulator_GetFontSize(void)
{
	return Settings.PS3FontSize/100.0;
}

void Emulator_SetControllerMode(int m)
{
	controller_settings = m;
}

bool Emulator_IsROMLoaded(void)
{
	return current_rom != NULL && need_load_rom == false;
}

static void emulator_shutdown(void)
{
	emulator_save_settings(CONFIG_FILE);

#ifdef PS3_PROFILING
	// When profiling we have to do exit(), so that gcov can be hooked properly. sys_process_exit() bypasses all this.
	net_stdio_enable(1);
	gfx_deinit();

	cellSysmoduleUnloadModule(CELL_SYSMODULE_FS);
	cellSysmoduleUnloadModule(CELL_SYSMODULE_IO);
	cellSysmoduleUnloadModule(CELL_SYSMODULE_AVCONF_EXT);
	cellSysmoduleUnloadModule(CELL_SYSMODULE_PNGDEC);
	cellSysmoduleUnloadModule(CELL_SYSMODULE_SYSUTIL_SCREENSHOT);
	cellSysutilUnregisterCallback(0);

	exit(0);
#else
	cellSysutilUnregisterCallback(0);
#ifdef MULTIMAN_SUPPORT
	if(return_to_MM)
	{
		//audio_active = false;
		pthread_mutex_unlock(&audio_lock);

		if(audio_handle)
		{
			audio_driver->free(audio_handle);
			audio_handle = NULL; 
		}

		cellSysmoduleUnloadModule(CELL_SYSMODULE_AVCONF_EXT);
		sys_spu_initialize(6, 0);
		char multiMAN[512];
		snprintf(multiMAN, sizeof(multiMAN), "%s", "/dev_hdd0/game/BLES80608/USRDIR/RELOAD.SELF");
		sys_game_process_exitspawn2((char*) multiMAN, NULL, NULL, NULL, 0, 2048, SYS_PROCESS_PRIMARY_STACK_SIZE_64K);		
		sys_process_exit(0);
	}
	else
#endif
		sys_process_exit(0);
#endif
}

void Emulator_RequestLoadROM(const char* rom)
{
	if (current_rom == NULL || strcmp(rom, current_rom) != 0)
	{
		if (current_rom != NULL)
			free(current_rom);

		current_rom = strdup(rom);
		need_load_rom = true;
	}
	else
	{
		need_load_rom = false;
		S9xSoftReset();
	}
}

void Emulator_StartROMRunning(uint32_t set_is_running)
{
	if(set_is_running)
		is_running = 1;
	mode_switch = MODE_EMULATION;
}



void ingame_menu_enable (int enable)
{
	is_running = false;
	is_ingame_menu_running = enable;
}


//FIXME: Turn GREEN into WHITE and RED into LIGHTBLUE once the overlay is in
#define ingame_menu_reset_entry_colors(ingame_menu_item) \
{ \
   for(int i = 0; i < MENU_ITEM_LAST; i++) \
      menuitem_colors[i] = GREEN; \
   menuitem_colors[ingame_menu_item] = RED; \
}


static void ingame_menu(void)
{
	uint64_t menuitem_colors[MENU_ITEM_LAST];
	char comment[256];
	char aspectratio[256];

	do
	{
		uint64_t state = cell_pad_input_poll_device(0);
		static uint64_t old_state = 0;
		uint64_t stuck_in_loop = 1;
		const uint64_t button_was_pressed = old_state & (old_state ^ state);
		const uint64_t button_was_held = old_state & state;

		if(CTRL_CIRCLE(state))
		{
			is_running = 1;
			ingame_menu_item = 0;
			is_ingame_menu_running = 0;
			Emulator_StartROMRunning(0);
		}

		Graphics->Draw(IPPU.RenderedScreenWidth, IPPU.RenderedScreenHeight, GFX.Screen);

		switch(ingame_menu_item)
		{
			case MENU_ITEM_LOAD_STATE:
				if(CTRL_CROSS(button_was_pressed))
				{
					emulator_load_current_save_state_slot();
					is_running = 1;
					ingame_menu_item = 0;
					is_ingame_menu_running = 0;
					Emulator_StartROMRunning(0);
				}
				if(CTRL_LEFT(button_was_pressed) || CTRL_LSTICK_LEFT(button_was_pressed))
				{
					emulator_decrement_current_save_state_slot();
				}
				if(CTRL_RIGHT(button_was_pressed) || CTRL_LSTICK_RIGHT(button_was_pressed))
				{
					emulator_increment_current_save_state_slot();
				}

				ingame_menu_reset_entry_colors(ingame_menu_item);
				strcpy(comment, "Press LEFT or RIGHT to change the current save state slot.\nPress CROSS to load the state from the currently selected save state slot.");
				break;
			case MENU_ITEM_SAVE_STATE:
				if(CTRL_CROSS(button_was_pressed))
				{
					emulator_save_current_save_state_slot();
					is_running = 1;
					ingame_menu_item = 0;
					is_ingame_menu_running = 0;
					Emulator_StartROMRunning(0);
				}
				if(CTRL_LEFT(button_was_pressed) || CTRL_LSTICK_LEFT(button_was_pressed))
				{
					emulator_decrement_current_save_state_slot();
				}
				if(CTRL_RIGHT(button_was_pressed) || CTRL_LSTICK_RIGHT(button_was_pressed))
				{
					emulator_increment_current_save_state_slot();
				}

				ingame_menu_reset_entry_colors (ingame_menu_item);
				strcpy(comment, "Press LEFT or RIGHT to change the current save state slot.\nPress CROSS to save the state to the currently selected save state slot.");
				break;
			case MENU_ITEM_KEEP_ASPECT_RATIO:
				if(CTRL_LEFT(button_was_pressed) || CTRL_LSTICK_LEFT(button_was_pressed))
				{
					if(Settings.PS3KeepAspect > 0)
					{
						Settings.PS3KeepAspect--;
						Graphics->set_aspect_ratio(Settings.PS3KeepAspect, IPPU.RenderedScreenWidth, IPPU.RenderedScreenHeight, 1);
						calculate_aspect_ratio_before_load();
					}
				}
				if(CTRL_RIGHT(button_was_pressed)  || CTRL_LSTICK_RIGHT(button_was_pressed))
				{
					if(Settings.PS3KeepAspect < LAST_ASPECT_RATIO)
					{
						Settings.PS3KeepAspect++;
						Graphics->set_aspect_ratio(Settings.PS3KeepAspect, IPPU.RenderedScreenWidth, IPPU.RenderedScreenHeight, 1);
						calculate_aspect_ratio_before_load();
					}
				}
				if(CTRL_START(button_was_pressed))
				{
					Settings.PS3KeepAspect = ASPECT_RATIO_4_3;
					Graphics->set_aspect_ratio(Settings.PS3KeepAspect, IPPU.RenderedScreenWidth, IPPU.RenderedScreenHeight, 1);
				}
				ingame_menu_reset_entry_colors (ingame_menu_item);
				strcpy(comment, "Press LEFT or RIGHT to change the [Aspect Ratio].\nPress START to reset back to default values.");
				break;
			case MENU_ITEM_OVERSCAN_AMOUNT:
				if(CTRL_LEFT(button_was_pressed)  ||  CTRL_LSTICK_LEFT(button_was_pressed) || CTRL_CROSS(button_was_pressed) || CTRL_LSTICK_LEFT(button_was_held))
				{
					Settings.PS3OverscanAmount--;
					Settings.PS3OverscanEnabled = 1;

					if(Settings.PS3OverscanAmount == 0)
						Settings.PS3OverscanEnabled = 0;

					Graphics->SetOverscan(Settings.PS3OverscanEnabled, (float)Settings.PS3OverscanAmount/100);
				}
				if(CTRL_RIGHT(button_was_pressed) || CTRL_LSTICK_RIGHT(button_was_pressed) || CTRL_CROSS(button_was_pressed) || CTRL_LSTICK_RIGHT(button_was_held))
				{
					Settings.PS3OverscanAmount++;
					Settings.PS3OverscanEnabled = 1;
					if(Settings.PS3OverscanAmount == 0)
						Settings.PS3OverscanEnabled = 0;

					Graphics->SetOverscan(Settings.PS3OverscanEnabled, (float)Settings.PS3OverscanAmount/100);
				}
				if(CTRL_START(button_was_pressed))
				{
					Settings.PS3OverscanAmount = 0;
					Settings.PS3OverscanEnabled = 0;
					Graphics->SetOverscan(Settings.PS3OverscanEnabled, (float)Settings.PS3OverscanAmount/100);
				}
				ingame_menu_reset_entry_colors (ingame_menu_item);
				strcpy(comment, "Press LEFT or RIGHT to change the [Overscan] settings.\nPress START to reset back to default values.");
				break;
			case MENU_ITEM_FRAME_ADVANCE:
				if(CTRL_CROSS(state) || CTRL_R2(state) || CTRL_L2(state))
				{
					is_running = 0;
					ingame_menu_item = MENU_ITEM_FRAME_ADVANCE;
					is_ingame_menu_running = 0;
					Emulator_StartROMRunning(0);
				}
				ingame_menu_reset_entry_colors (ingame_menu_item);
				strcpy(comment, "Press 'CROSS', 'L2' or 'R2' button to step one frame.\nNOTE: Pressing the button rapidly will advance the frame more slowly\nand prevent buttons from being input.");
				break;
			case MENU_ITEM_RESIZE_MODE:
				if(CTRL_CROSS(state))
				{
					Graphics->set_aspect_ratio(ASPECT_RATIO_CUSTOM, IPPU.RenderedScreenWidth, IPPU.RenderedScreenWidth, 1);
					do
					{
						Graphics->Draw(IPPU.RenderedScreenWidth, IPPU.RenderedScreenHeight, GFX.Screen);
						state = cell_pad_input_poll_device(0);
						Graphics->resize_aspect_mode_input_loop(state);

						if(CTRL_CIRCLE(state))
						{
							sys_timer_usleep(FILEBROWSER_DELAY);
							stuck_in_loop = 0;
						}

						psglSwap();
						cellSysutilCheckCallback();
						old_state = state;
					}while(stuck_in_loop && is_ingame_menu_running);
				}
				ingame_menu_reset_entry_colors (ingame_menu_item);
				strcpy(comment, "Allows you to resize the screen by moving around the two analog sticks.\nPress TRIANGLE to reset to default values, and CIRCLE to go back to the\nin-game menu.");
				break;
			case MENU_ITEM_SCREENSHOT_MODE:
				if(CTRL_CROSS(state))
				{
					while(stuck_in_loop && is_ingame_menu_running)
					{
						state = cell_pad_input_poll_device(0);
						if(CTRL_CIRCLE(state))
						{
							sys_timer_usleep(FILEBROWSER_DELAY);
							stuck_in_loop = 0;
						}

						Graphics->Draw(IPPU.RenderedScreenWidth, IPPU.RenderedScreenHeight, GFX.Screen);
						psglSwap();
						cellSysutilCheckCallback();
						old_state = state;
					}
				}

				ingame_menu_reset_entry_colors (ingame_menu_item);
				strcpy(comment, "Allows you to take a screenshot without any text clutter.\nPress CIRCLE to go back to the in-game menu while in 'Screenshot Mode'.");
				break;
			case MENU_ITEM_RETURN_TO_GAME:
				if(CTRL_CROSS(button_was_pressed))
				{
					is_running = 1;
					ingame_menu_item = 0;
					is_ingame_menu_running = 0;
					Emulator_StartROMRunning(0);
				}
				ingame_menu_reset_entry_colors (ingame_menu_item);
				strcpy(comment, "Press 'CROSS' to return back to the game.");
				break;
			case MENU_ITEM_RESET:
				if(CTRL_CROSS(button_was_pressed))
				{
					S9xSoftReset();
					is_running = 1;
					ingame_menu_item = 0;
					is_ingame_menu_running = 0;
					Emulator_StartROMRunning(0);
				}
				ingame_menu_reset_entry_colors (ingame_menu_item);
				strcpy(comment, "Press 'CROSS' to reset the game.");
				break;
			case MENU_ITEM_RESET_FORCE_NTSC_PAL:
				if(CTRL_CROSS(button_was_pressed))
				{
					Settings.ForceNTSC = false;
					Settings.ForcePAL = true;
					need_load_rom = 1;
					is_running = 1;
					is_ingame_menu_running = 0;
					Emulator_StartROMRunning(0);
					ingame_menu_item = 0;
				}
				ingame_menu_reset_entry_colors (ingame_menu_item);
				strcpy(comment, "Press 'CROSS' to reset the game and force it into PAL videomode.");
				break;
			case MENU_ITEM_RESET_FORCE_PAL_NTSC:
				if(CTRL_CROSS(button_was_pressed))
				{
					Settings.ForceNTSC = true;
					Settings.ForcePAL = false;
					need_load_rom = 1;
					is_running = 1;
					is_ingame_menu_running = 0;
					Emulator_StartROMRunning(0);
					ingame_menu_item = 0;
				} 
				ingame_menu_reset_entry_colors (ingame_menu_item);
				strcpy(comment, "Press 'CROSS' to reset the game and force it into NTSC videomode.");
				break;
			case MENU_ITEM_RETURN_TO_MENU:
				if(CTRL_CROSS(button_was_pressed))
				{
					is_running = 1;
					ingame_menu_item = 0;
					is_ingame_menu_running = 0;
					mode_switch = MODE_EMULATION;
				}

				ingame_menu_reset_entry_colors (ingame_menu_item);
				strcpy(comment, "Press 'CROSS' to return to the ROM Browser menu.");
				break;
#ifdef MULTIMAN_SUPPORT
			case MENU_ITEM_RETURN_TO_MULTIMAN:
				if(CTRL_CROSS(button_was_pressed))
				{
					is_running = 0;
					is_ingame_menu_running = 0;
					mode_switch = MODE_EXIT; 
				}

				ingame_menu_reset_entry_colors (ingame_menu_item);
				strcpy(comment, "Press 'CROSS' to quit the emulator and return to multiMAN.");
				break;
#endif
			case MENU_ITEM_RETURN_TO_XMB:
				if(CTRL_CROSS(button_was_pressed))
				{
					is_running = 0;
					is_ingame_menu_running = 0;
#ifdef MULTIMAN_SUPPORT
					return_to_MM = false;
#endif
					mode_switch = MODE_EXIT; 
				}

				ingame_menu_reset_entry_colors (ingame_menu_item);
				strcpy(comment, "Press 'CROSS' to quit the emulator and return to the XMB.");
				break;
		}

		if(CTRL_UP(button_was_pressed) || CTRL_LSTICK_UP(button_was_pressed))
		{
			if(ingame_menu_item > 0)
				ingame_menu_item--;
		}

		if(CTRL_DOWN(button_was_pressed) || CTRL_LSTICK_DOWN(button_was_pressed))
		{
			if(ingame_menu_item < MENU_ITEM_LAST)
				ingame_menu_item++;
		}

		float x_position = 0.3f;
		float font_size = 1.1f;
		float ypos = 0.19f;
		float ypos_increment = 0.04f;

		cellDbgFontPrintf	(x_position,	0.10f,	1.4f+0.01f,	BLUE,               "Quick Menu");
		cellDbgFontPrintf(x_position,	0.10f,	1.4f,	WHITE,               "Quick Menu");

		cellDbgFontPrintf	(x_position,	ypos,	font_size+0.01f,	BLUE,	"Load State #%d", Settings.CurrentSaveStateSlot);
		cellDbgFontPrintf(x_position,	ypos,	font_size,	menuitem_colors[MENU_ITEM_LOAD_STATE],	"Load State #%d", Settings.CurrentSaveStateSlot);

		cellDbgFontPrintf	(x_position,	ypos+(ypos_increment*MENU_ITEM_SAVE_STATE),	font_size+0.01f,	BLUE,	"Save State #%d", Settings.CurrentSaveStateSlot);
		cellDbgFontPrintf(x_position,	ypos+(ypos_increment*MENU_ITEM_SAVE_STATE),	font_size,	menuitem_colors[MENU_ITEM_SAVE_STATE],	"Save State #%d", Settings.CurrentSaveStateSlot);
		cellDbgFontDraw();

		cellDbgFontPrintf	(x_position,	(ypos+(ypos_increment*MENU_ITEM_KEEP_ASPECT_RATIO)),	font_size+0.01f,	BLUE,	"Aspect Ratio: %s %s %d:%d", Graphics->calculate_aspect_ratio_before_game_load() ?"(Auto)" : "", Settings.PS3KeepAspect == LAST_ASPECT_RATIO ? "Custom" : "", (int)Graphics->get_aspect_ratio_int(0), (int)Graphics->get_aspect_ratio_int(1));
		cellDbgFontPrintf(x_position,	(ypos+(ypos_increment*MENU_ITEM_KEEP_ASPECT_RATIO)),	font_size,	menuitem_colors[MENU_ITEM_KEEP_ASPECT_RATIO],	"Aspect Ratio: %s %s %d:%d", Graphics->calculate_aspect_ratio_before_game_load() ?"(Auto)" : "", Settings.PS3KeepAspect == LAST_ASPECT_RATIO ? "Custom" : "", (int)Graphics->get_aspect_ratio_int(0), (int)Graphics->get_aspect_ratio_int(1));

		cellDbgFontPrintf(x_position,	(ypos+(ypos_increment*MENU_ITEM_OVERSCAN_AMOUNT)),	font_size,	menuitem_colors[MENU_ITEM_OVERSCAN_AMOUNT],	"Overscan: %f", (float)Settings.PS3OverscanAmount/100);

		cellDbgFontPrintf	(x_position,	(ypos+(ypos_increment*MENU_ITEM_RESIZE_MODE)),	font_size+0.01f,	BLUE,	"Resize Mode");
		cellDbgFontPrintf(x_position,	(ypos+(ypos_increment*MENU_ITEM_RESIZE_MODE)),	font_size,	menuitem_colors[MENU_ITEM_RESIZE_MODE],	"Resize Mode");

		cellDbgFontPuts	(x_position,	(ypos+(ypos_increment*MENU_ITEM_FRAME_ADVANCE)),	font_size+0.01f,	BLUE,	"Frame Advance");
		cellDbgFontPuts(x_position,	(ypos+(ypos_increment*MENU_ITEM_FRAME_ADVANCE)),	font_size,	menuitem_colors[MENU_ITEM_FRAME_ADVANCE],	"Frame Advance");

		cellDbgFontPuts	(x_position,	(ypos+(ypos_increment*MENU_ITEM_SCREENSHOT_MODE)),	font_size+0.01f,	BLUE,	"Screenshot Mode");
		cellDbgFontPuts(x_position,	(ypos+(ypos_increment*MENU_ITEM_SCREENSHOT_MODE)),	font_size,	menuitem_colors[MENU_ITEM_SCREENSHOT_MODE],	"Screenshot Mode");

		cellDbgFontDraw();

		cellDbgFontPuts	(x_position,	(ypos+(ypos_increment*MENU_ITEM_RESET)),	font_size+0.01f,	BLUE,	"Reset");
		cellDbgFontPuts(x_position,	(ypos+(ypos_increment*MENU_ITEM_RESET)),	font_size,	menuitem_colors[MENU_ITEM_RESET],	"Reset");

		cellDbgFontPuts(x_position,	(ypos+(ypos_increment*MENU_ITEM_RESET_FORCE_NTSC_PAL)),	font_size+0.01f, BLUE,	"Reset - Force NTSC to PAL");
		cellDbgFontPuts(x_position,	(ypos+(ypos_increment*MENU_ITEM_RESET_FORCE_NTSC_PAL)),	font_size,	menuitem_colors[MENU_ITEM_RESET_FORCE_NTSC_PAL],	"Reset - Force NTSC to PAL");

		cellDbgFontPuts(x_position,	(ypos+(ypos_increment*MENU_ITEM_RESET_FORCE_PAL_NTSC)),	font_size+0.01f,	BLUE,	"Reset - Force PAL to NTSC");
		cellDbgFontPuts(x_position,	(ypos+(ypos_increment*MENU_ITEM_RESET_FORCE_PAL_NTSC)),	font_size,	menuitem_colors[MENU_ITEM_RESET_FORCE_PAL_NTSC],	"Reset - Force PAL to NTSC");

		cellDbgFontPuts   (x_position,   (ypos+(ypos_increment*MENU_ITEM_RETURN_TO_GAME)),   font_size+0.01f,  BLUE,  "Return to Game");
		cellDbgFontPuts(x_position,   (ypos+(ypos_increment*MENU_ITEM_RETURN_TO_GAME)),   font_size,  menuitem_colors[MENU_ITEM_RETURN_TO_GAME],  "Return to Game");

		cellDbgFontPuts	(x_position,	(ypos+(ypos_increment*MENU_ITEM_RETURN_TO_MENU)),	font_size+0.01f,	BLUE,	"Return to Menu");
		cellDbgFontPuts(x_position,	(ypos+(ypos_increment*MENU_ITEM_RETURN_TO_MENU)),	font_size,	menuitem_colors[MENU_ITEM_RETURN_TO_MENU],	"Return to Menu");
#ifdef MULTIMAN_SUPPORT
		cellDbgFontPuts	(x_position,	(ypos+(ypos_increment*MENU_ITEM_RETURN_TO_MULTIMAN)),	font_size+0.01f,	BLUE,	"Return to multiMAN");
		cellDbgFontPuts(x_position,	(ypos+(ypos_increment*MENU_ITEM_RETURN_TO_MULTIMAN)),	font_size,	menuitem_colors[MENU_ITEM_RETURN_TO_MULTIMAN],	"Return to multiMAN");
#endif
		cellDbgFontDraw();

		cellDbgFontPuts	(x_position,	(ypos+(ypos_increment*MENU_ITEM_RETURN_TO_XMB)),	font_size+0.01f,	BLUE,	"Return to XMB");
		cellDbgFontPuts(x_position,	(ypos+(ypos_increment*MENU_ITEM_RETURN_TO_XMB)),	font_size,	menuitem_colors[MENU_ITEM_RETURN_TO_XMB],	"Return to XMB");

		if(Graphics->frame_count < special_action_msg_expired)
		{
			cellDbgFontPrintf (0.09f, 0.90f, 1.51f, BLUE,	special_action_msg);
			cellDbgFontPrintf (0.09f, 0.90f, 1.50f, WHITE,	special_action_msg);
			cellDbgFontDraw();
		}
		else
		{
			special_action_msg_expired = 0;
			cellDbgFontPrintf (0.09f,   0.90f,   0.98f+0.01f,      BLUE,           comment);
			cellDbgFontPrintf (0.09f,   0.90f,   0.98f,      LIGHTBLUE,           comment);
		}
		cellDbgFontDraw();
		psglSwap();
		old_state = state;
		cellSysutilCheckCallback();
	}while(is_ingame_menu_running);
}

void get_path_settings(bool multiman_support)
{
	unsigned int get_type;
	unsigned int get_attributes;
	CellGameContentSize size;
	char dirName[CELL_GAME_DIRNAME_SIZE];

	memset(&size, 0x00, sizeof(CellGameContentSize));

	int ret = cellGameBootCheck(&get_type, &get_attributes, &size, dirName); 
	if(ret < 0)
	{
		printf("cellGameBootCheck() Error: 0x%x\n", ret);
	}
	else
	{
		printf("cellGameBootCheck() OK\n");
		printf("  get_type = [%d] get_attributes = [0x%08x] dirName = [%s]\n", get_type, get_attributes, dirName);
		printf("  hddFreeSizeKB = [%d] sizeKB = [%d] sysSizeKB = [%d]\n", size.hddFreeSizeKB, size.sizeKB, size.sysSizeKB);

		ret = cellGameContentPermit(contentInfoPath, usrDirPath);

		if(multiman_support)
		{
			snprintf(contentInfoPath, sizeof(contentInfoPath), "/dev_hdd0/game/%s", EMULATOR_CONTENT_DIR);
			snprintf(usrDirPath, sizeof(usrDirPath), "/dev_hdd0/game/%s/USRDIR", EMULATOR_CONTENT_DIR);
		}

		if(ret < 0)
		{
			printf("cellGameContentPermit() Error: 0x%x\n", ret);
		}
		else
		{
			printf("cellGameContentPermit() OK\n");
			printf("contentInfoPath:[%s]\n", contentInfoPath);
			printf("usrDirPath:[%s]\n",  usrDirPath);
		}

		/* now we fill in all the variables */
		snprintf(DEFAULT_PRESET_FILE, sizeof(DEFAULT_PRESET_FILE), "%s/presets/stock.conf", usrDirPath);
		snprintf(DEFAULT_BORDER_FILE, sizeof(DEFAULT_BORDER_FILE), "%s/borders/Centered-1080p/super-metroid.png", usrDirPath);
		snprintf(DEFAULT_MENU_BORDER_FILE, sizeof(DEFAULT_MENU_BORDER_FILE), "%s/borders/Menu/main-menu.jpg", usrDirPath);
		snprintf(GAME_AWARE_SHADER_DIR_PATH, sizeof(GAME_AWARE_SHADER_DIR_PATH), "%s/gameaware", usrDirPath);
		snprintf(PRESETS_DIR_PATH, sizeof(PRESETS_DIR_PATH), "%s/presets", usrDirPath); 
		snprintf(BORDERS_DIR_PATH, sizeof(BORDERS_DIR_PATH), "%s/borders", usrDirPath); 
		snprintf(SHADERS_DIR_PATH, sizeof(SHADERS_DIR_PATH), "%s/shaders", usrDirPath);
		snprintf(DEFAULT_SHADER_FILE, sizeof(DEFAULT_SHADER_FILE), "%s/shaders/stock.cg", usrDirPath);
		snprintf(DEFAULT_MENU_SHADER_FILE, sizeof(DEFAULT_MENU_SHADER_FILE), "%s/shaders/Borders/Menu/border-only.cg", usrDirPath);
	}
}


int main(int argc, char **argv)
{
	sys_spu_initialize(4, 3);
	cellSysmoduleLoadModule(CELL_SYSMODULE_IO);
	cellSysmoduleLoadModule(CELL_SYSMODULE_AVCONF_EXT);
	cellSysmoduleLoadModule(CELL_SYSMODULE_PNGDEC);
	cellSysmoduleLoadModule(CELL_SYSMODULE_JPGDEC);
	cellSysmoduleLoadModule(CELL_SYSMODULE_SYSUTIL_GAME);

	cellSysutilRegisterCallback(0, callback_sysutil_exit, NULL);

#ifdef PS3_PROFILING
	net_stdio_set_target(PS3_PROFILING_IP, PS3_PROFILING_PORT);
	net_stdio_set_paths("/", 2);
	net_stdio_enable(0);
#endif

	// parse input roms
#ifdef MULTIMAN_SUPPORT
	return_to_MM = true;

	if(argc > 1)
	{
		mode_switch = MODE_MULTIMAN_STARTUP;
		strncpy(MULTIMAN_GAME_TO_BOOT, argv[1], sizeof(MULTIMAN_GAME_TO_BOOT));
	}
#endif

	get_path_settings(return_to_MM);

	if (need_load_settings)
	{
		emulator_init_settings();
		need_load_settings =  false;
	}


#if(CELL_SDK_VERSION > 0x340000)
	if (Settings.ScreenshotsEnabled)
	{
		cellSysmoduleLoadModule(CELL_SYSMODULE_SYSUTIL_SCREENSHOT);
		CellScreenShotSetParam  screenshot_param = {0, 0, 0, 0};

		screenshot_param.photo_title = "SNES9x PS3";
		screenshot_param.game_title = "SNES9x PS3";
		cellScreenShotSetParameter (&screenshot_param);
		cellScreenShotEnable();
	}
#endif

	Graphics = new PS3Graphics((uint32_t)Settings.PS3CurrentResolution, Settings.PS3KeepAspect, Settings.PS3Smooth, Settings.PS3Smooth2, Settings.PS3CurrentShader, Settings.PS3CurrentShader2, DEFAULT_MENU_SHADER_FILE, Settings.PS3OverscanEnabled, (float)Settings.PS3OverscanAmount/100, Settings.PS3PALTemporalMode60Hz, Settings.Throttled, Settings.TripleBuffering, Settings.ViewportX, Settings.ViewportY, Settings.ViewportWidth, Settings.ViewportHeight);
	Graphics->Init(Settings.ScaleEnabled, Settings.ScaleFactor);

	if(Settings.ApplyShaderPresetOnStartup)
		emulator_implementation_set_shader_preset(Settings.ShaderPresetPath); 

	cell_pad_input_init();

	oskutil_init(&oskutil_handle, 0);

#if 0
	if (strcmp(Settings.GameAwareShaderPath,"") != 0)
	{
		emulator_implementation_set_gameaware(Settings.GameAwareShaderPath);
	}
#endif

	Graphics->ResetFrameCounter();
	Graphics->InitDbgFont();

#ifdef CELL_DEBUG_CONSOLE
	cellConsoleInit();
	cellConsoleNetworkInitialize();
	cellConsoleNetworkServerInit(-1);
	cellConsoleScreenShotPluginInit();
#endif

	emulator_implementation_set_texture(Settings.PS3CurrentBorder);
	emulator_toggle_sound(Settings.SoundMode);

#if(CELL_SDK_VERSION > 0x340000)
	cellSysutilEnableBgmPlayback();
#endif

	do{
		switch(mode_switch)
		{
			case MODE_MENU:
				MenuMainLoop();
				break;
			case MODE_EMULATION:
				if(ingame_menu_item != 0)
					is_ingame_menu_running = 1;

				emulator_start();

				if(Settings.Throttled)
					audio_driver->pause(audio_handle);

				if(is_ingame_menu_running)
				{
					ingame_menu();
				}
				break;
#ifdef MULTIMAN_SUPPORT
			case MODE_MULTIMAN_STARTUP:
				Emulator_StartROMRunning();
				Emulator_RequestLoadROM(MULTIMAN_GAME_TO_BOOT);
				break;
#endif
			case MODE_EXIT:
				emulator_shutdown();
				return(0);
		}
	}while(1);
}
