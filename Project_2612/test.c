#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <conio.h>
#include <Windows.h>
#include "test.h"

#define INITIAL_ROUNDS 40
#define MAX_ROUNDS 49
#define COLOR_DEFAULT 7
#define COLOR_WALL 8
#define COLOR_GOLD 14
#define COLOR_P1 12
#define COLOR_P2 9
#define COLOR_MOVE 10
#define COLOR_SEALED 13
#define COLOR_VISITED 11

Cell board[SIZE][SIZE];
Player players[PLAYER_COUNT];
int currentTurn = 0;
int roundCount = 1;

static int explorationLimit = INITIAL_ROUNDS;
static int majorMineFound = 0; // 3단계 붕괴 시스템과 연결할 값
static int temporaryCard = 0;
static int hasTemporaryCard = 0;
static char statusMessage[256] = "탐험을 시작하세요!";

static int absValue(int n) { return n < 0 ? -n : n; }
static void color(int n) {
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), (WORD)n);
}
static void cursorAt(short x, short y) {
    COORD xy;
    xy.X = x;
    xy.Y = y;
    SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), xy);
}
static void message(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vsnprintf(statusMessage, sizeof(statusMessage), format, args);
    va_end(args);
}

int inBoard(int r, int c) {
    return r >= 0 && r < SIZE && c >= 0 && c < SIZE;
}
static int isOccupied(int r, int c) {
    for (int p = 0; p < PLAYER_COUNT; p++) {
        if (players[p].row == r && players[p].col == c) return 1;
    }
    return 0;
}
static int randomCard(void) { return rand() % (CARD_COUNT - 1) + 1; }

static void putEvent(int kind, int amount) {
    int placed = 0;
    int tries = 0;
    while (placed < amount && tries < 10000) {
        int r = rand() % (SIZE - 2) + 1;
        int c = rand() % (SIZE - 2) + 1;
        Cell* tile = &board[r][c];
        tries++;
        if (tile->terrain != EMPTY || tile->event != EVENT_NONE) continue;
        if (r >= 10 && c >= 4 && c <= 8) continue; // 시작 지점은 안전하게
        tile->event = kind;
        placed++;
    }
}

void initBoard(void) {
    for (int r = 0; r < SIZE; r++) {
        for (int c = 0; c < SIZE; c++) {
            Cell* tile = &board[r][c];
            tile->terrain = EMPTY;
            tile->visited = 0;
            tile->revealed = 0;
            tile->event = EVENT_NONE;
            tile->resolved = 0;
            tile->mineType = MINE_UNSET;
        }
    }
    for (int r = 0; r < SIZE; r++) {
        board[r][0].terrain = WALL;
        board[r][SIZE - 1].terrain = WALL;
    }
    for (int c = 0; c < SIZE; c++) {
        board[0][c].terrain = WALL;
        board[SIZE - 1][c].terrain = WALL;
    }
    board[12][6].terrain = ENTRANCE;
    board[6][6].terrain = SEALED;

    const int walls[][2] = {
        {2,3}, {2,9}, {4,4}, {4,8}, {5,2}, {5,10},
        {7,2}, {7,10}, {8,4}, {8,8}, {10,3}, {10,9}
    };
    for (int i = 0; i < (int)(sizeof(walls) / sizeof(walls[0])); i++) {
        board[walls[i][0]][walls[i][1]].terrain = WALL;
    }

    // 후보 금광 네 곳에 대형/중형/소형/가짜 금광을 무작위 배정
    const int sites[4][2] = { {3,6}, {6,3}, {6,9}, {9,6} };
    int kinds[4] = { MINE_MAJOR, MINE_MEDIUM, MINE_SMALL, MINE_FAKE };
    for (int i = 3; i > 0; i--) {
        int j = rand() % (i + 1);
        int tmp = kinds[i]; kinds[i] = kinds[j]; kinds[j] = tmp;
    }
    for (int i = 0; i < 4; i++) {
        Cell* tile = &board[sites[i][0]][sites[i][1]];
        tile->terrain = MINE;
        tile->mineType = kinds[i];
    }

    // 빈 칸에 숨겨진 이벤트를 무작위 배치
    putEvent(EVENT_TRAP, 9);
    putEvent(EVENT_PICKAXE, 4);
    putEvent(EVENT_BAG, 4);
    putEvent(EVENT_SCANNER, 4);
    putEvent(EVENT_HOURGLASS, 3);
    putEvent(EVENT_EXTRA_CARD, 4);
    putEvent(EVENT_REROLL, 4);
    putEvent(EVENT_GEM, 10);
    putEvent(EVENT_HEAL, 4);
}

const char* getCharacterName(int type) {
    switch (type) {
    case 1: return "광부";
    case 2: return "정찰병";
    case 3: return "운반꾼";
    case 4: return "생존가";
    default: return "알 수 없음";
    }
}

void selectCharacters(void) {
    for (int p = 0; p < PLAYER_COUNT; p++) {
        int choice = 0;
        while (choice < 1 || choice > 4) {
            system("cls");
            color(COLOR_GOLD);
            printf("========== 광산 탈출 : 캐릭터 선택 ==========\n\n");
            color(COLOR_DEFAULT);
            printf("플레이어 %d\n\n", p + 1);
            printf("1. 광부   : 금광 채굴률 +20%%\n");
            printf("2. 정찰병 : 주변 1칸의 숨겨진 이벤트 탐지\n");
            printf("3. 운반꾼 : 금광 채굴률 +20%%, 보석 +10G\n");
            printf("4. 생존가 : 함정 피해 최초 2회 무효\n\n");
            printf("선택 (1~4) : ");
            int key = _getch();
            if (key >= '1' && key <= '4') choice = key - '0';
        }
        players[p].character = choice;
        players[p].trapGuards = choice == 4 ? 2 : 0;
    }
}

const char* getCardName(int type) {
    switch (type) {
    case CARD_STEP: return "기본 이동";
    case CARD_DASH: return "질주";
    case CARD_DIAGONAL: return "대각선";
    case CARD_LONG_DIAGONAL: return "긴 대각선";
    case CARD_JUMP: return "도약";
    default: return "선택 안 함";
    }
}
static const char* cardHelp(int type) {
    switch (type) {
    case CARD_STEP: return "상하좌우 1칸";
    case CARD_DASH: return "직선 2칸 (중간 칸 통과)";
    case CARD_DIAGONAL: return "대각선 1칸";
    case CARD_LONG_DIAGONAL: return "대각선 2칸 (중간 칸 통과)";
    case CARD_JUMP: return "직선 2칸 (중간 칸 점프)";
    default: return "";
    }
}
void refillCard(int playerIndex, int slot) {
    players[playerIndex].hand[slot] = randomCard();
}
void initCards(void) {
    for (int p = 0; p < PLAYER_COUNT; p++) {
        for (int i = 0; i < HAND_SIZE; i++) refillCard(p, i);
    }
}
int isPassable(int r, int c) {
    if (!inBoard(r, c)) return 0;
    if (board[r][c].terrain == WALL || board[r][c].terrain == SEALED) return 0;
    if (isOccupied(r, c)) return 0;
    return 1;
}
int isValidMove(int sr, int sc, int er, int ec, int cardType) {
    if (cardType < 0 || !isPassable(er, ec)) return 0;
    int dr = er - sr, dc = ec - sc;
    int ar = absValue(dr), ac = absValue(dc);
    if (cardType == CARD_STEP) return ar + ac == 1;
    if (cardType == CARD_DASH) {
        if (ar + ac != 2 || (ar != 0 && ac != 0)) return 0;
        return isPassable(sr + dr / 2, sc + dc / 2);
    }
    if (cardType == CARD_DIAGONAL) return ar == 1 && ac == 1;
    if (cardType == CARD_LONG_DIAGONAL) {
        if (ar != 2 || ac != 2) return 0;
        return isPassable(sr + dr / 2, sc + dc / 2);
    }
    if (cardType == CARD_JUMP) return ar + ac == 2 && (ar == 0 || ac == 0);
    return 0;
}

// 주변 이벤트 공개 (정찰병: 인접 1칸, 탐지기: 2칸)
static void revealNearby(int p, int radius) {
    int sr = players[p].row, sc = players[p].col;
    for (int r = 1; r < SIZE - 1; r++) {
        for (int c = 1; c < SIZE - 1; c++) {
            if (absValue(sr - r) <= radius && absValue(sc - c) <= radius) {
                board[r][c].revealed = 1;
            }
        }
    }
}

// UI 한 칸: 절대좌표 출력 방식으로 한글/기호 폭 차이에 따른 열 틀어짐 최소화
static void printCell(int r, int c, int selectedR, int selectedC, int type) {
    Cell* tile = &board[r][c];
    const char* symbol = "·";
    int ink = COLOR_DEFAULT;
    if (tile->terrain == WALL) { symbol = "■"; ink = COLOR_WALL; }
    else if (tile->terrain == SEALED) { symbol = "◆"; ink = COLOR_SEALED; }
    else if (tile->terrain == ENTRANCE) { symbol = "⌂"; ink = COLOR_MOVE; }
    else if (tile->terrain == MINE) {
        symbol = tile->resolved ? "◆" : "◇";
        ink = tile->resolved ? COLOR_VISITED : COLOR_GOLD;
    }
    else if (tile->visited) { symbol = "·"; ink = COLOR_VISITED; }
    else if (tile->revealed && tile->event != EVENT_NONE) {
        symbol = tile->event == EVENT_TRAP ? "!" : "✧";
        ink = tile->event == EVENT_TRAP ? COLOR_P1 : COLOR_GOLD;
    }
    else if (!tile->revealed) { symbol = "?"; ink = COLOR_WALL; }

    if (type >= 0 && isValidMove(players[currentTurn].row,
        players[currentTurn].col, r, c, type)) {
        symbol = "✦";
        ink = COLOR_MOVE;
    }
    if (players[0].row == r && players[0].col == c) {
        symbol = "♟"; ink = COLOR_P1;
    }
    if (players[1].row == r && players[1].col == c) {
        symbol = "♟"; ink = COLOR_P2;
    }
    if (r == selectedR && c == selectedC) {
        color((ink & 15) | BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE);
    }
    else color(ink);
    cursorAt((short)(5 + c * 3), (short)(5 + r));
    printf("%s", symbol);
    color(COLOR_DEFAULT);
}

static void printSidePanel(int row, int type) {
    Player* p = &players[currentTurn];
    Player* a = &players[0];
    Player* b = &players[1];
    cursorAt(49, (short)(5 + row));
    switch (row) {
    case 0: color(COLOR_GOLD); printf("======= 탐험 현황 ======="); break;
    case 1: printf("라운드 %d / %d", roundCount, explorationLimit); break;
    case 2: printf("현재 차례: 플레이어 %d", currentTurn + 1); break;
    case 3: printf("대형 금광: %s", majorMineFound ? "발견!" : "미발견"); break;
    case 4: color(COLOR_P1); printf("======= P1 %s =======", getCharacterName(a->character)); break;
    case 5: printf("HP %d  금 %dG  위치(%d,%d)", a->hp, a->gold, a->row, a->col); break;
    case 6: printf("곡괭이%d 가방%d 탐지%d 추가%d 교환%d", a->pickaxe, a->bag, a->scanner, a->extraCards, a->rerolls); break;
    case 7: color(COLOR_P2); printf("======= P2 %s =======", getCharacterName(b->character)); break;
    case 8: printf("HP %d  금 %dG  위치(%d,%d)", b->hp, b->gold, b->row, b->col); break;
    case 9: printf("곡괭이%d 가방%d 탐지%d 추가%d 교환%d", b->pickaxe, b->bag, b->scanner, b->extraCards, b->rerolls); break;
    case 10: color(COLOR_GOLD); printf("======= 현재 이동 카드 ======="); break;
    case 11: printf("[0] 기본  [1] %s  [2] %s", getCardName(p->hand[0]), getCardName(p->hand[1])); break;
    case 12:
        printf("[3] %s", getCardName(p->hand[2]));
        if (hasTemporaryCard) printf("  [4] %s", getCardName(temporaryCard));
        break;
    }
    color(COLOR_DEFAULT);
    (void)type;
}

void printBoard(int cursorRow, int cursorCol, int cardType) {
    system("cls");
    color(COLOR_GOLD);
    cursorAt(2, 1);
    printf("====================== 광산 탈출 : 탐험 단계 ======================");
    color(COLOR_DEFAULT);
    for (int c = 0; c < SIZE; c++) {
        cursorAt((short)(5 + c * 3), 4);
        printf("%2d", c);
    }
    for (int r = 0; r < SIZE; r++) {
        cursorAt(1, (short)(5 + r));
        printf("%2d |", r);
        for (int c = 0; c < SIZE; c++) printCell(r, c, cursorRow, cursorCol, cardType);
        printSidePanel(r, cardType);
    }
    cursorAt(2, 19);
    printf("■ 벽  ◇ 금광 후보  ◆ 봉인/채굴 완료  ⌂ 입구  ✦ 이동 가능  ! 탐지된 함정  ✧ 탐지된 아이템");
    cursorAt(2, 20);
    printf("[0~3] 카드  [4] 추가 카드  [F] 탐지기  [R] 카드 교환  [방향키] 목적지  [ENTER] 이동  [ESC] 뒤로  [Q] 종료");
    cursorAt(2, 21);
    if (cardType >= 0) printf("선택: %s (%s)", getCardName(cardType), cardHelp(cardType));
    else printf("현재 선택된 카드: 없음");
    cursorAt(2, 22);
    printf("알림: %s", statusMessage);
    cursorAt(2, 24);
    fflush(stdout);
}

// 무작위 카드 중 한 장 교체 (이동 횟수 소모 없음)
static void rerollCard(void) {
    Player* p = &players[currentTurn];
    if (p->rerolls <= 0) { message("카드 교환권이 없습니다."); return; }
    message("바꿀 카드를 1~3 중 선택하세요. (ESC 취소)");
    printBoard(-1, -1, -1);
    cursorAt(2, 24);
    printf("교환할 카드 (1~3): ");
    int key = _getch();
    if (key >= '1' && key <= '3') {
        int slot = key - '1';
        int old = p->hand[slot];
        refillCard(currentTurn, slot);
        p->rerolls--;
        message("카드 교환: %s -> %s", getCardName(old), getCardName(p->hand[slot]));
    }
    else message("카드 교환을 취소했습니다.");
}

int selectMoveCard(void) {
    Player* p = &players[currentTurn];
    // ESC로 카드 선택에 돌아와도 이미 뽑은 추가 카드는 유지
    while (1) {
        printBoard(-1, -1, -1);
        cursorAt(2, 24);
        printf("플레이어 %d: 카드 선택 (0~3, 추가카드 T, 탐지 F, 교환 R): ", currentTurn + 1);
        int key = _getch();
        if (key >= '0' && key <= '3') return key - '0';
        if (key == '4' && hasTemporaryCard) return 4;
        if (key == 't' || key == 'T') {
            if (hasTemporaryCard) message("이미 추가 카드가 준비되었습니다. [4]로 선택하세요.");
            else if (p->extraCards <= 0) message("추가 카드 선택권이 없습니다.");
            else {
                p->extraCards--;
                hasTemporaryCard = 1;
                temporaryCard = randomCard();
                message("추가 카드 공개: %s. [4] 선택 또는 다른 카드 선택 가능", getCardName(temporaryCard));
            }
        }
        if (key == 'f' || key == 'F') {
            if (p->scanner <= 0) message("탐지기가 없습니다.");
            else {
                p->scanner--;
                revealNearby(currentTurn, 2);
                message("주변 2칸을 탐지했습니다. ! = 함정, ✧ = 아이템");
            }
        }
        if (key == 'r' || key == 'R') rerollCard();
        if (key == 'q' || key == 'Q') return -1;
    }
}

int selectDestination(int cardType, int* er, int* ec) {
    Player* p = &players[currentTurn];
    int rows[SIZE * SIZE], cols[SIZE * SIZE], count = 0;
    for (int r = 0; r < SIZE; r++) {
        for (int c = 0; c < SIZE; c++) {
            if (isValidMove(p->row, p->col, r, c, cardType)) {
                rows[count] = r; cols[count] = c; count++;
            }
        }
    }
    if (count == 0) {
        message("이 카드로는 이동할 수 없습니다. 다른 카드를 골라주세요.");
        return 0;
    }
    int selected = 0;
    int bestStart = 100000;
    for (int i = 0; i < count; i++) {
        int dist = absValue(rows[i] - p->row) + absValue(cols[i] - p->col);
        if (dist < bestStart) { bestStart = dist; selected = i; }
    }
    while (1) {
        printBoard(rows[selected], cols[selected], cardType);
        cursorAt(2, 24);
        printf("가능한 위치 %d곳 | 현재 (%d,%d) | 방향키 / ENTER / ESC", count, rows[selected], cols[selected]);
        int key = _getch();
        if (key == 0 || key == 224) {
            int arrow = _getch(), dirR = 0, dirC = 0;
            if (arrow == 72) dirR = -1;
            else if (arrow == 80) dirR = 1;
            else if (arrow == 75) dirC = -1;
            else if (arrow == 77) dirC = 1;
            else continue;
            int next = -1, best = 1000000;
            for (int i = 0; i < count; i++) {
                if (i == selected) continue;
                int dr = rows[i] - rows[selected];
                int dc = cols[i] - cols[selected];
                int forward = dr * dirR + dc * dirC;
                if (forward <= 0) continue;
                int sideways = dirR ? absValue(dc) : absValue(dr);
                int score = sideways * 100 + forward * 10;
                if (score < best) { best = score; next = i; }
            }
            if (next >= 0) selected = next;
            continue;
        }
        if (key == 13) { *er = rows[selected]; *ec = cols[selected]; return 1; }
        if (key == 27) return 0;
        if (key == 'q' || key == 'Q') return -1;
    }
}

static void resolveMine(Cell* tile) {
    Player* p = &players[currentTurn];
    int base = 0;
    const char* name = "";
    switch (tile->mineType) {
    case MINE_MAJOR: base = 100; name = "대형 금광"; majorMineFound = 1; break;
    case MINE_MEDIUM: base = 60; name = "중형 금광"; break;
    case MINE_SMALL: base = 40; name = "소형 금광"; break;
    case MINE_FAKE: base = 0; name = "가짜 금광"; break;
    default: break;
    }
    if (base == 0) {
        // 가짜 금광은 보상 없고 체력 1 감소 (생존가는 보호 적용)
        if (p->trapGuards > 0) {
            p->trapGuards--;
            message("가짜 금광! 보물이 없습니다. 생존가 능력으로 피해 방어!");
        }
        else {
            p->hp--;
            message("가짜 금광! 함정으로 체력 -1. 남은 HP: %d", p->hp);
        }
    }
    else {
        int rate = 40 + 20 * p->pickaxe + 20 * p->bag;
        if (p->character == 1 || p->character == 3) rate += 20;
        if (rate > 100) rate = 100;
        int collected = base * rate / 100;
        p->gold += collected;
        message("%s 발견! 원석 %dG, 채굴률 %d%%, 실제 획득 %dG", name, base, rate, collected);
    }
    tile->resolved = 1;
}

static void resolveEvent(Cell* tile) {
    Player* p = &players[currentTurn];
    switch (tile->event) {
    case EVENT_TRAP:
        if (p->trapGuards > 0) {
            p->trapGuards--;
            message("함정 발견! 생존가 능력으로 피해 방어 (남은 보호 %d회)", p->trapGuards);
        }
        else {
            p->hp--;
            message("함정에 걸렸습니다! HP -1 (현재 %d)", p->hp);
        }
        break;
    case EVENT_PICKAXE: p->pickaxe = 1; message("곡괭이 획득! 금광 채굴률 +20%%"); break;
    case EVENT_BAG: p->bag = 1; message("보물 가방 획득! 금광 채굴률 +20%%"); break;
    case EVENT_SCANNER: p->scanner++; message("탐지기 획득! 카드 선택 화면에서 [F] 사용"); break;
    case EVENT_HOURGLASS:
        if (explorationLimit < MAX_ROUNDS) {
            explorationLimit += 3;
            if (explorationLimit > MAX_ROUNDS) explorationLimit = MAX_ROUNDS;
            message("모래시계 획득! 탐험 제한이 %d라운드로 증가!", explorationLimit);
        }
        else message("모래시계 발견! 탐험 제한은 최대 %d라운드", MAX_ROUNDS);
        break;
    case EVENT_EXTRA_CARD: p->extraCards++; message("추가 카드 선택권 획득! 다음 차례에 [T] 사용"); break;
    case EVENT_REROLL: p->rerolls++; message("카드 교환권 획득! 카드 선택 화면에서 [R] 사용"); break;
    case EVENT_GEM: {
        int gold = p->character == 3 ? 30 : 20;
        p->gold += gold;
        message("작은 보석 발견! %dG 획득 (보유 %dG)", gold, p->gold);
        break;
    }
    case EVENT_HEAL:
        if (p->hp < 3) p->hp++;
        message("응급약 발견! 현재 HP %d/3", p->hp);
        break;
    default: message("빈 칸입니다."); break;
    }
    tile->resolved = 1;
}

void movePlayer(int er, int ec, int cardSlot) {
    Player* p = &players[currentTurn];
    p->row = er;
    p->col = ec;
    Cell* tile = &board[er][ec];
    tile->visited = 1;
    tile->revealed = 1;
    if (cardSlot >= 1 && cardSlot <= HAND_SIZE) refillCard(currentTurn, cardSlot - 1);
    if (p->character == 2) revealNearby(currentTurn, 1);
    if (tile->terrain == MINE && !tile->resolved) resolveMine(tile);
    else if (tile->event != EVENT_NONE && !tile->resolved) resolveEvent(tile);
    else message("플레이어 %d 이동 완료: (%d,%d)", currentTurn + 1, er, ec);
}

void initGame(void) {
    currentTurn = 0;
    roundCount = 1;
    explorationLimit = INITIAL_ROUNDS;
    majorMineFound = 0;
    temporaryCard = 0;
    hasTemporaryCard = 0;
    initBoard();
    for (int i = 0; i < PLAYER_COUNT; i++) {
        Player* p = &players[i];
        p->character = 0;
        p->hp = 3;
        p->gold = 0;
        p->pickaxe = 0;
        p->bag = 0;
        p->scanner = 0;
        p->extraCards = 0;
        p->rerolls = 0;
        p->trapGuards = 0;
    }
    players[0].row = 11; players[0].col = 5;
    players[1].row = 11; players[1].col = 7;
    board[11][5].visited = 1;
    board[11][7].visited = 1;
    selectCharacters();
    initCards();
    if (players[0].character == 2) revealNearby(0, 1);
    if (players[1].character == 2) revealNearby(1, 1);
    message("탐험 시작! 금광 4곳과 숨겨진 장비를 찾아보세요.");
}

void playGame(void) {
    int quitGame = 0;
    int eliminated = 0;
    while (roundCount <= explorationLimit) {
        int chosen = selectMoveCard();
        if (chosen < 0) { quitGame = 1; break; }
        int card = chosen == 0 ? CARD_STEP :
            chosen == 4 ? temporaryCard : players[currentTurn].hand[chosen - 1];
        int r = 0, c = 0;
        int result = selectDestination(card, &r, &c);
        if (result < 0) { quitGame = 1; break; }
        if (result == 0) continue;
        movePlayer(r, c, chosen);
        // 행동이 확정되었을 때만 임시 카드 초기화
        hasTemporaryCard = 0;
        temporaryCard = 0;
        if (players[currentTurn].hp <= 0) {
            eliminated = 1;
            break;
        }
        currentTurn = 1 - currentTurn;
        if (currentTurn == 0) roundCount++;
    }
    printBoard(-1, -1, -1);
    cursorAt(2, 24);
    if (quitGame) printf("사용자가 게임을 종료했습니다.");
    else if (eliminated) printf("플레이어 %d의 HP가 0이 되어 탐험이 종료되었습니다.", currentTurn + 1);
    else printf("탐험 제한 도달! 붕괴 및 탈출은 3단계에서 구현합니다.");
    cursorAt(2, 25);
    printf("최종 보유 금: P1 %dG / P2 %dG", players[0].gold, players[1].gold);
    cursorAt(2, 27);
    printf("아무 키나 누르면 종료합니다.");
    _getch();
}
