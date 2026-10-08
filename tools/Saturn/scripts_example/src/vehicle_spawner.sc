void ApplyPlayerControl(bool bOn)
{
    SET_PLAYER_CONTROL(CONVERT_INT_TO_PLAYERINDEX(GET_PLAYER_ID()), bOn);
}

void AddChar(char name, char line, int key, char letter)
{
    if (IS_KEYBOARD_KEY_JUST_PRESSED(key) || IS_GAME_KEYBOARD_KEY_JUST_PRESSED(key))
    {
        if (GET_LENGTH_OF_LITERAL_STRING(name) < 16)
        {
            strcat(name, letter);
            strcat(line, letter);
        }
    }
}

void main()
{
    bool bShow;
    bool bHeld;
    bool bDown;
    bool bLoading;
    bool bChecked;
    int openWait;
    int length;
    int hash;
    Ped playerChar;
    float x;
    float y;
    float z;
    Vehicle veh;
    char name[32];
    char line[64];
    hash = GET_HASH_KEY("admiral");
    bShow = false;
    bHeld = false;
    bLoading = false;
    bChecked = false;
    openWait = 0;
    strcpy(name, "");
    strcpy(line, "Enter the vehicle name: ");
    while (true)
    {
        if (HAS_DEATHARREST_EXECUTED())
        {
            if (bShow)
            {
                ApplyPlayerControl(true);
            }
            if (bLoading)
            {
                MARK_MODEL_AS_NO_LONGER_NEEDED(hash);
            }
            TERMINATE_THIS_SCRIPT();
        }

        WAIT(0);

        if (IS_SCREEN_FADED_IN() && IS_PLAYER_PLAYING(GET_PLAYER_ID()))
        {
            bDown = (IS_GAME_KEYBOARD_KEY_PRESSED(17) || IS_KEYBOARD_KEY_PRESSED(17)) &&
                (IS_GAME_KEYBOARD_KEY_PRESSED(25) || IS_KEYBOARD_KEY_PRESSED(25));
            if (bDown)
            {
                if (bHeld)
                {
                }
                else
                {
                    if (bShow)
                    {
                        bShow = false;
                        bChecked = false;
                        if (bLoading)
                        {
                            MARK_MODEL_AS_NO_LONGER_NEEDED(hash);
                            bLoading = false;
                        }
                        strcpy(name, "");
                        strcpy(line, "Enter the vehicle name: ");
                        ApplyPlayerControl(true);
                    }
                    else
                    {
                        bShow = true;
                        bChecked = false;
                        openWait = 15;
                        strcpy(name, "");
                        strcpy(line, "Enter the vehicle name: ");
                        ApplyPlayerControl(false);
                    }
                }
            }
            bHeld = bDown;
            if (bShow)
            {
                if (bDown)
                {
                }
                else
                {
                    if (openWait > 0)
                    {
                        openWait = openWait - 1;
                    }
                    else
                    {
                        if (!bLoading)
                        {
                            AddChar(name, line, 30, "a");
                            AddChar(name, line, 48, "b");
                            AddChar(name, line, 46, "c");
                            AddChar(name, line, 32, "d");
                            AddChar(name, line, 18, "e");
                            AddChar(name, line, 33, "f");
                            AddChar(name, line, 34, "g");
                            AddChar(name, line, 35, "h");
                            AddChar(name, line, 23, "i");
                            AddChar(name, line, 36, "j");
                            AddChar(name, line, 37, "k");
                            AddChar(name, line, 38, "l");
                            AddChar(name, line, 50, "m");
                            AddChar(name, line, 49, "n");
                            AddChar(name, line, 24, "o");
                            AddChar(name, line, 25, "p");
                            AddChar(name, line, 16, "q");
                            AddChar(name, line, 19, "r");
                            AddChar(name, line, 31, "s");
                            AddChar(name, line, 20, "t");
                            AddChar(name, line, 22, "u");
                            AddChar(name, line, 47, "v");
                            AddChar(name, line, 17, "w");
                            AddChar(name, line, 45, "x");
                            AddChar(name, line, 21, "y");
                            AddChar(name, line, 44, "z");
                            AddChar(name, line, 2, "1");
                            AddChar(name, line, 3, "2");
                            AddChar(name, line, 4, "3");
                            AddChar(name, line, 5, "4");
                            AddChar(name, line, 6, "5");
                            AddChar(name, line, 7, "6");
                            AddChar(name, line, 8, "7");
                            AddChar(name, line, 9, "8");
                            AddChar(name, line, 10, "9");
                            AddChar(name, line, 11, "0");
                            if (IS_KEYBOARD_KEY_JUST_PRESSED(14) || IS_GAME_KEYBOARD_KEY_JUST_PRESSED(14))
                            {
                                length = GET_LENGTH_OF_LITERAL_STRING(name);
                                if (length > 0)
                                {
                                    strcpy(name, GET_FIRST_N_CHARACTERS_OF_LITERAL_STRING(name, length - 1));
                                    length = GET_LENGTH_OF_LITERAL_STRING(line);
                                    strcpy(line, GET_FIRST_N_CHARACTERS_OF_LITERAL_STRING(line, length - 1));
                                }
                            }
                            length = GET_LENGTH_OF_LITERAL_STRING(name);
                            if (length == 7)
                            {
                                if (!bChecked)
                                {
                                    bChecked = true;
                                    if (GET_HASH_KEY(name) == hash)
                                    {
                                        REQUEST_MODEL(hash);
                                        bLoading = true;
                                    }
                                }
                            }
                            else
                            {
                                bChecked = false;
                            }
                        }
                    }
                }
            }
            if (bLoading)
            {
                REQUEST_MODEL(hash);
                if (HAS_MODEL_LOADED(hash))
                {
                    GET_PLAYER_CHAR(CONVERT_INT_TO_PLAYERINDEX(GET_PLAYER_ID()), &playerChar);
                    if (DOES_CHAR_EXIST(playerChar) && !IS_CHAR_DEAD(playerChar))
                    {
                        GET_OFFSET_FROM_CHAR_IN_WORLD_COORDS(playerChar, 0.0, 5.0, 0.0, &x, &y, &z);
                        veh = 0;
                        CREATE_CAR(hash, x, y, z, &veh, true);
                        MARK_MODEL_AS_NO_LONGER_NEEDED(hash);
                        bLoading = false;
                        bShow = false;
                        bChecked = false;
                        strcpy(name, "");
                        strcpy(line, "Enter the vehicle name: ");
                        ApplyPlayerControl(true);
                    }
                }
            }
            if (bShow)
            {
                SET_TEXT_FONT(0);
                SET_TEXT_BACKGROUND(false);
                SET_TEXT_DROPSHADOW(true, 0, 0, 0, 255);
                SET_TEXT_EDGE(true, 0, 0, 0, 255);
                SET_TEXT_COLOUR(255, 255, 255, 255);
                SET_TEXT_SCALE(0.38, 0.38);
                SET_TEXT_PROPORTIONAL(true);
                SET_TEXT_CENTRE(true);
                SET_TEXT_WRAP(0.0, 1.0);
                DISPLAY_TEXT_WITH_LITERAL_STRING(0.5, 0.86, "STRING", line);
            }
        }
        else
        {
            if (bShow)
            {
                bShow = false;
                bChecked = false;
                if (bLoading)
                {
                    MARK_MODEL_AS_NO_LONGER_NEEDED(hash);
                    bLoading = false;
                }
                strcpy(name, "");
                strcpy(line, "Enter the vehicle name: ");
                ApplyPlayerControl(true);
            }
        }
    }
}
