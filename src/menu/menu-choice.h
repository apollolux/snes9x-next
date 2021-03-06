static void do_select_file(uint32_t menu_id)
{
	char extensions[256], title[256], object[256], comment[256], dir_path[MAX_PATH_LENGTH];
	switch(menu_id)
	{
		case GAME_AWARE_SHADER_CHOICE:
			strncpy(dir_path, GAME_AWARE_SHADER_DIR_PATH, sizeof(dir_path));
			strncpy(extensions, "cfg|CFG", sizeof(extensions));
			strncpy(title, "GAME AWARE SHADER SELECTION", sizeof(title));
			strncpy(object, "Game Aware Shader", sizeof(object));
			strncpy(comment, "INFO - Select a 'Game Aware Shader' script from the menu by pressing X.", sizeof(comment));
			break;
		case SHADER_CHOICE:
			strncpy(dir_path, SHADERS_DIR_PATH, sizeof(dir_path));
			strncpy(extensions, "cg|CG", sizeof(extensions));
			strncpy(title, "SHADER SELECTION", sizeof(title));
			strncpy(object, "Shader", sizeof(object));
			strncpy(comment, "INFO - Select a shader from the menu by pressing the X button.", sizeof(comment));
			break;
		case PRESET_CHOICE:
			strncpy(dir_path, PRESETS_DIR_PATH, sizeof(dir_path));
			strncpy(extensions, "conf|CONF", sizeof(extensions));
			strncpy(title, "SHADER PRESETS SELECTION", sizeof(title));
			strncpy(object, "Shader", sizeof(object));
			strncpy(object, "Shader preset", sizeof(object));
                        strncpy(comment, "INFO - Select a shader preset from the menu by pressing the X button. ", sizeof(comment));
			break;
		case BORDER_CHOICE:
			strncpy(dir_path, BORDERS_DIR_PATH, sizeof(dir_path));
			strncpy(extensions, "png|PNG|jpg|JPG|JPEG|jpeg", sizeof(extensions));
			strncpy(title, "BORDER SELECTION", sizeof(title));
			strncpy(object, "Border", sizeof(object));
			strncpy(object, "Border image file", sizeof(object));
			strncpy(comment, "INFO - Select a border image file from the menu by pressing the X button. ", sizeof(comment));
			break;
	}

	if (tmpBrowser == NULL)
		tmpBrowser = new FileBrowser(dir_path, extensions);

	char path[MAX_PATH_LENGTH];

	uint64_t state = cell_pad_input_poll_device(0);
	static uint64_t old_state = 0;
	uint64_t diff_state = old_state ^ state;
	uint64_t button_was_pressed = old_state & diff_state;

	UpdateBrowser(tmpBrowser);

	if (CTRL_START(button_was_pressed))
		tmpBrowser->ResetStartDirectory("/",extensions);

	if (CTRL_CROSS(button_was_pressed))
	{
		if(tmpBrowser->IsCurrentADirectory())
		{
			//if 'filename' is in fact '..' - then pop back directory instead of adding '..' to filename path
			if(tmpBrowser->GetCurrentEntryIndex() == 0)
			{
				old_state = state;
				tmpBrowser->PopDirectory();
			}
			else
			{
                                const char * separatorslash = (strcmp(tmpBrowser->get_current_directory_name(),"/") == 0) ? "" : "/";
				snprintf(path, sizeof(path), "%s%s%s", tmpBrowser->get_current_directory_name(), separatorslash, tmpBrowser->get_current_filename());
				tmpBrowser->PushDirectory(path, CELL_FS_TYPE_REGULAR | CELL_FS_TYPE_DIRECTORY, extensions);
			}
		}
		else if (tmpBrowser->IsCurrentAFile())
		{
			snprintf(path, sizeof(path), "%s/%s", tmpBrowser->get_current_directory_name(), tmpBrowser->get_current_filename());

			switch(menu_id)
			{
				case GAME_AWARE_SHADER_CHOICE:
					emulator_implementation_set_gameaware(path);
					strncpy(Settings.GameAwareShaderPath, path, sizeof(Settings.GameAwareShaderPath));
					break;
				case SHADER_CHOICE:
					ps3graphics_load_fragment_shader(path, set_shader);
					break;
				case PRESET_CHOICE:
					emulator_implementation_set_shader_preset(path);
					break;
				case BORDER_CHOICE:
					strncpy(Settings.PS3CurrentBorder, path, sizeof(Settings.PS3CurrentBorder));
					emulator_implementation_set_texture(path);
					break;
			}	

			menuStackindex--;
		}
	}

	if (CTRL_TRIANGLE(button_was_pressed))
		menuStackindex--;

        cellDbgFontPrintf(0.09f, 0.09f, Emulator_GetFontSize(), YELLOW, "PATH: %s", tmpBrowser->get_current_directory_name());
	cellDbgFontPuts	(0.09f,	0.05f,	Emulator_GetFontSize(),	RED,	title);
	cellDbgFontPrintf(0.09f, 0.92f, 0.92, YELLOW, "X - Select %s  /\\ - return to settings  START - Reset Startdir", object);
	cellDbgFontPrintf(0.09f, 0.83f, 0.91f, LIGHTBLUE, "%s", comment);
	cellDbgFontDraw();

	RenderBrowser(tmpBrowser);
	old_state = state;
}

static void do_pathChoice()
{
        if (tmpBrowser == NULL)
                tmpBrowser = new FileBrowser("/\0", "empty");

        char path[1024];
        char newpath[1024];

        uint64_t state = cell_pad_input_poll_device(0);
        static uint64_t old_state = 0;
        uint64_t diff_state = old_state ^ state;
        uint64_t button_was_pressed = old_state & diff_state;

        UpdateBrowser(tmpBrowser);

        if (CTRL_START(button_was_pressed))
                tmpBrowser->ResetStartDirectory("/","empty");

        if (CTRL_SQUARE(button_was_pressed))
        {
                if(tmpBrowser->IsCurrentADirectory())
                {
                        snprintf(path, sizeof(path), "%s/%s", tmpBrowser->get_current_directory_name(), tmpBrowser->get_current_filename());
                        switch(menu_pathsettings.selected)
                        {
                                case SETTING_PATH_SAVESTATES_DIRECTORY:
                                        strcpy(Settings.PS3PathSaveStates, path);
                                        break;
                                case SETTING_PATH_SRAM_DIRECTORY:
                                        strcpy(Settings.PS3PathSRAM, path);
                                        break;
                                case SETTING_PATH_DEFAULT_ROM_DIRECTORY:
                                        strcpy(Settings.PS3PathROMDirectory, path);
                                        break;
                        }
                        menuStackindex--;
                }
        }
        if (CTRL_TRIANGLE(button_was_pressed))
        {
                strcpy(path, usrDirPath);
                switch(menu_pathsettings.selected)
                {
                        case SETTING_PATH_SAVESTATES_DIRECTORY:
                                strcpy(Settings.PS3PathSaveStates, path);
                                break;
                        case SETTING_PATH_SRAM_DIRECTORY:
                                strcpy(Settings.PS3PathSRAM, path);
                                break;
                        case SETTING_PATH_DEFAULT_ROM_DIRECTORY:
                                strcpy(Settings.PS3PathROMDirectory, path);
                                break;
                }
                menuStackindex--;
        }
        if (CTRL_CROSS(button_was_pressed))
        {
                if(tmpBrowser->IsCurrentADirectory())
                {
                        //if 'filename' is in fact '..' - then pop back directory instead of adding '..' to filename path
                        if(tmpBrowser->GetCurrentEntryIndex() == 0)
                        {
                                old_state = state;
                                tmpBrowser->PopDirectory();
                        }
                        else
                        {
                                const char * separatorslash = (strcmp(tmpBrowser->get_current_directory_name(),"/") == 0) ? "" : "/";
                                snprintf(newpath, sizeof(newpath), "%s%s%s", tmpBrowser->get_current_directory_name(), separatorslash, tmpBrowser->get_current_filename());
                                tmpBrowser->PushDirectory(newpath, CELL_FS_TYPE_REGULAR | CELL_FS_TYPE_DIRECTORY, "empty");
                        }
                }
        }

        cellDbgFontPrintf (0.09f,  0.09f, Emulator_GetFontSize(), YELLOW,  "PATH: %s", tmpBrowser->get_current_directory_name());
        cellDbgFontPuts (0.09f, 0.05f,  Emulator_GetFontSize(), RED,    "DIRECTORY SELECTION");
        cellDbgFontPuts(0.09f, 0.93f, 0.92f, YELLOW,"X - Enter dir  /\\ - return to settings  START - Reset Startdir");
        cellDbgFontPrintf(0.09f, 0.83f, 0.91f, LIGHTBLUE, "%s",
                        "INFO - Browse to a directory and assign it as the path by pressing SQUARE button.");
        cellDbgFontDraw();

        RenderBrowser(tmpBrowser);
        old_state = state;
}
