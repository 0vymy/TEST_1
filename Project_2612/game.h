#ifndef GAME_H
#define GAME_H
#define SIZE 11
#define PLAYER_COUNT 2
#define UNIT_COUNT 3
#define HAND_SIZE 3
#define CARD_COUNT 16
#define MAX_MOVES 4
#define MINE_COUNT 11
#define MAX_ROUNDS 20

#define MAX_LIVES 2

typedef enum { VOID_CELL, EMPTY, MINE, BASE, GOAL } Terrain;
typedef struct {
    Terrain terrain;
    int owner;         // -1 중립, 0 P1, 1 P2
    int ownerUnit;     // 최초 점령한 말 번호 (0~2), -1 중립
    int protectedCell;
} Cell;
typedef struct {
    int row, col;
    int baseRow, baseCol;
    int inBase;
    int carryMine;
    int lives;         // 0이면 영구 사망
} Unit;
typedef struct { Unit units[UNIT_COUNT]; int hand[HAND_SIZE]; } Player;
typedef struct {
    int row, col;
    int carrierPlayer, carrierUnit, deliveredBy;
} Mine;
extern Cell board[SIZE][SIZE];
extern Player players[PLAYER_COUNT];
extern Mine mines[MINE_COUNT];
extern int currentTurn, roundCount, publicCard, winner;
extern int uiMoveTargets[SIZE][SIZE];
extern int uiCursorRow, uiCursorCol, uiSelectedUnit;
extern char gameMessage[120];
int inBoard(int r, int c);
int isPlayable(int r, int c);
int findUnitAt(int r, int c, int* p, int* u);
void initGame(void);
void playGame(void);
void placeStartingUnits(void);
void printBoard(void);
void clearVisualMarks(void);
void markMoveTarget(int r, int c);
void claimPath(int p, int u, int sr, int sc, int er, int ec);
void initCards(void);
void refillCard(int p, int slot);
void drawHandCards(void);
int playUnitTurn(int p);
void initMines(void);
void onUnitArrived(int p, int u);
void checkDeliveries(void);
int deliveredCount(int p);
int resourceValue(int r, int c);
int playerScore(int p);
void setFG(int r, int g, int b);
void setBG(int r, int g, int b);
void resetColor(void);
extern int rgbEnabled;
void initConsoleColors(void);
void captureUnit(int p, int u, int attacker, int attackRow, int attackCol);
#endif
