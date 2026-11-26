#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <Windows.h>
#include "game.h"

int main(void) {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    setvbuf(stdout, NULL, _IONBF, 0);
    initConsoleColors();

    srand((unsigned int)time(NULL));

    initGame();
    playGame();

    return 0;
}