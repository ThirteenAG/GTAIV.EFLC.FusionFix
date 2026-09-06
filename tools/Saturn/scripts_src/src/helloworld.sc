void main()
{
    if (HAS_DEATHARREST_EXECUTED())
    {
        TERMINATE_THIS_SCRIPT();
    }

    if (IS_SCREEN_FADED_IN() && IS_PLAYER_PLAYING(GET_PLAYER_ID()))
    {
        PRINT_STRING_WITH_LITERAL_STRING_NOW("STRING", "Hello world!", 5000, 1);
        TERMINATE_THIS_SCRIPT();
    }
}
