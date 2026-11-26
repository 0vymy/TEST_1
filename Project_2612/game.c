#include <stdio.h>
#include <stdlib.h>
#include <conio.h>
#include <Windows.h>
#include "game.h"

Cell board[SIZE][SIZE];
Player players[PLAYER_COUNT];
int currentTurn = 0, roundCount = 1, winner = -1;
int uiMoveTargets[SIZE][SIZE];
int uiCursorRow = -1, uiCursorCol = -1;
char gameMessage[120] = "";

// 지정된 콘솔 위치에 출력
static void goTo(short x, short y) {
    fflush(stdout);
    if (rgbEnabled) { printf("\x1b[%d;%dH", y + 1, x + 1); return; }
    COORD pos = { x, y };
    SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), pos);
}

int inBoard(int r, int c) {
    return r >= 0 && r < SIZE && c >= 0 && c < SIZE;
}

int isPlayable(int r, int c) {
    if (!inBoard(r, c)) return 0;
    Terrain t = board[r][c].terrain;
    return t == EMPTY || t == MINE || t == GOAL;
}

int findUnitAt(int r, int c, int* playerIndex, int* unitIndex) {
    for (int p = 0; p < PLAYER_COUNT; p++) {
        for (int u = 0; u < UNIT_COUNT; u++) {
            Unit* unit = &players[p].units[u];
            if (unit->lives <= 0 || unit->row != r || unit->col != c) continue;
            if (playerIndex) *playerIndex = p;
            if (unitIndex) *unitIndex = u;
            return 1;
        }
    }
    return 0;
}

void clearVisualMarks(void) {
    for (int r = 0; r < SIZE; r++)
        for (int c = 0; c < SIZE; c++) uiMoveTargets[r][c] = 0;
    uiCursorRow = uiCursorCol = -1;
}

void markMoveTarget(int r, int c) {
    if (inBoard(r, c) && board[r][c].terrain != VOID_CELL) uiMoveTargets[r][c] = 1;
}

// 둥근 육각형 게임판 + 6개 기지 구성
static void initBoard(void) {
    const int widths[SIZE] = { 5,7,9,11,11,11,11,11,9,7,5 };
    for (int r = 0; r < SIZE; r++) {
        for (int c = 0; c < SIZE; c++) {
            board[r][c].terrain = VOID_CELL;
            board[r][c].owner = -1;
            board[r][c].ownerUnit = -1;
            board[r][c].protectedCell = 0;
        }
        int start = (SIZE - widths[r]) / 2;
        for (int c = start; c < start + widths[r]; c++)
            board[r][c].terrain = EMPTY;
    }
    // 기지는 시작 배치 단계에서 플레이어가 직접 지정한다.
}

// 게임 시작 상태 초기화
void initGame(void) {
    currentTurn = 0;
    roundCount = 1;
    winner = -1;
    initBoard();
    gameMessage[0] = '\0';
    for (int p = 0; p < PLAYER_COUNT; p++) {
        for (int u = 0; u < UNIT_COUNT; u++) {
            Unit* unit = &players[p].units[u];
            unit->baseRow = unit->row = -1;
            unit->baseCol = unit->col = -1;
            unit->inBase = 1;
            unit->carryMine = -1;
            unit->lives = 2;
        }
    }
    initMines();
    initCards();
    clearVisualMarks();
}

// RGB 출력: 매 화면마다 VT 모드를 확인하고, 실패하면 16색 사용
int rgbEnabled = 0;
static WORD fallbackFG = 7, fallbackBG = 0;

void initConsoleColors(void) {
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    fflush(stdout);
    rgbEnabled = GetConsoleMode(h, &mode) &&
        SetConsoleMode(h, mode | ENABLE_PROCESSED_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
}

// RGB 미지원 환경에서도 팀과 자원의 종류가 구분되도록 직접 매핑
static WORD fallbackColor(int r, int g, int b, int bg) {
    if (bg) {
        if (r == 48 && g == 57 && b == 46) return 2;
        if (r == 60 && g == 56 && b == 44) return 6;
        if (r == 56 && g == 46 && b == 64) return 5;
        if (r == 101 && g == 64 && b == 78) return 4;
        if (r == 168 && g == 85 && b == 104) return 12;
        if (r == 53 && g == 72 && b == 102) return 1;
        if (r == 82 && g == 108 && b == 170) return 9;
        if (r == 75 && g == 111 && b == 66) return 2;
        if (r == 100 && g == 139 && b == 85) return 10;
        if (r == 232 && g == 207 && b == 134) return 14;
        return 8;
    }
    if (g > r + 25 && g > b + 25) return 10;
    if (b > r + 25 && r > g + 15) return 13;
    if (b > r + 15 && b > g + 15) return 11;
    if (r > g + 25 && r > b + 15) return 12;
    if (r > b + 35 && g > b + 25) return 14;
    if (r < 130 && g < 130 && b < 130) return 8;
    return 15;
}
void setFG(int r, int g, int b) {
    if (rgbEnabled) printf("\x1b[38;2;%d;%d;%dm", r, g, b);
    else {
        fflush(stdout);
        fallbackFG = fallbackColor(r, g, b, 0);
        SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), fallbackFG | fallbackBG);
    }
}
void setBG(int r, int g, int b) {
    if (rgbEnabled) printf("\x1b[48;2;%d;%d;%dm", r, g, b);
    else {
        fflush(stdout);
        fallbackBG = fallbackColor(r, g, b, 1) << 4;
        SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), fallbackFG | fallbackBG);
    }
}
void resetColor(void) {
    if (rgbEnabled) printf("\x1b[0m");
    else {
        fflush(stdout);
        fallbackFG = 7; fallbackBG = 0;
        SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 7);
    }
}
static void teamColor(int p) {
    if (winner >= 0 && p != winner) setFG(112, 112, 120);
    else if (p == 0) setFG(255, 155, 175);
    else setFG(147, 183, 255);
}
static void resourceColor(int r, int c) {
    int v = resourceValue(r, c);
    if (v == 2) setFG(151, 229, 166);
    else if (v == 3) setFG(255, 218, 114);
    else setFG(208, 160, 255);
}
static int resourceAt(int r, int c) {
    for (int i = 0; i < MINE_COUNT; i++)
        if (mines[i].row == r && mines[i].col == c &&
            mines[i].carrierPlayer < 0 && mines[i].deliveredBy < 0) return i;
    return -1;
}
static int baseOwner(int r, int c) {
    for (int p = 0; p < PLAYER_COUNT; p++) for (int u = 0; u < UNIT_COUNT; u++)
        if (players[p].units[u].lives > 0 && players[p].units[u].baseRow == r &&
            players[p].units[u].baseCol == c) return p;
    return -1;
}
static const char* tileSymbol(int r, int c) {
    if (findUnitAt(r, c, NULL, NULL)) return "♝";
    if (board[r][c].terrain == BASE) return "⊹";
    if (board[r][c].terrain == GOAL) return "✵";
    if (resourceAt(r, c) >= 0) return "✦";
    return " ";
}


#define TILE_W 4
#define TILE_H 2
#define BOARD_X 35
#define BOARD_Y 3

// 타일 배경색 결정

static void tileBackground(int r, int c) {
    Cell* cell = &board[r][c];

    int red, green, blue;

    // 위쪽: 안개 라벤더
    // 중앙: 회보라
    // 아래쪽: 은은한 청록
    // 행에 따라 자연스럽게 색상이 변화

    if (r <= 5) {
        red = 92 - r * 3;
        green = 77 + r;
        blue = 112 - r * 3;
    }
    else {
        int t = r - 5;

        red = 77 - t * 2;
        green = 82 + t * 2;
        blue = 97 - t * 2;
    }

    // 칸 경계를 구분할 정도의 아주 약한 명도 차이
    if ((r + c) % 2 == 0) {
        red += 3;
        green += 3;
        blue += 3;
    }

    // 빨강 영토
    if (cell->owner == 0) {
        red = 150;
        green = 84;
        blue = 113;

        if (cell->protectedCell) {
            red = 201;
            green = 124;
            blue = 151;
        }
    }
    // 파랑 영토
    else if (cell->owner == 1) {
        red = 78;
        green = 108;
        blue = 154;

        if (cell->protectedCell) {
            red = 135;
            green = 166;
            blue = 211;
        }
    }

    // 패배 팀 영토
    if (winner >= 0 &&
        cell->owner >= 0 &&
        cell->owner != winner) {
        red = 48;
        green = 45;
        blue = 58;
    }

    // 이동 가능 / 배치 가능 영역
    if (uiMoveTargets[r][c]) {
        red = 77;
        green = 107;
        blue = 101;
    }

    // 현재 선택한 칸
    if (uiCursorRow == r &&
        uiCursorCol == c) {
        red = 216;
        green = 191;
        blue = 141;
    }

    setBG(red, green, blue);
}



static void printTile(int r, int c, short x, short y) {
    Cell* cell = &board[r][c];

    if (cell->terrain == VOID_CELL) {
        resetColor();
        return;
    }

    int p = -1, u = -1;
    int hasUnit = findUnitAt(r, c, &p, &u);

    int selected =
        (uiCursorRow == r && uiCursorCol == c);

    // 타일 배경: 가로 4칸, 세로 2줄
    goTo(x, y);
    tileBackground(r, c);
    printf("    ");

    goTo(x, (short)(y + 1));
    tileBackground(r, c);
    printf("    ");
    resetColor();

    // 타일 중앙에 내용 출력
    goTo((short)(x + 1), (short)(y + 1));
    tileBackground(r, c);

    if (selected) {
        setFG(45, 36, 26);
    }
    else if (hasUnit) {
        if (p == 0)
            setFG(255, 220, 231);
        else
            setFG(218, 232, 255);
    }
    else if (cell->terrain == GOAL) {
        setFG(255, 237, 153);
    }
    else if (resourceAt(r, c) >= 0) {
        resourceColor(r, c);
    }
    else if (cell->terrain == BASE) {
        p = baseOwner(r, c);
        if (p >= 0) teamColor(p);
    }
    else {
        setFG(195, 195, 205);
    }

    // 말 표시

    if (hasUnit) {
        // 윗줄: 마법 장식
        goTo((short)(x + 1), y);
        tileBackground(r, c);

        if (players[p].units[u].carryMine >= 0) {
            setFG(255, 225, 140);
            printf("✦");  // 자원 보유
        }
        else {
            setFG(225, 205, 245);
            printf("✧");  // 일반 상태
        }

        // 아랫줄: 장식된 말
        // 아랫줄: 장식된 말
        // 아랫줄: 장식된 말
        int mr, mg, mb;

        if (selected) {
            mr = 45; mg = 36; mb = 26;
        }
        else if (p == 0) {
            mr = 255; mg = 110; mb = 145;
        }
        else {
            mr = 145; mg = 200; mb = 255;
        }

        // 왼쪽 날개
        goTo(x, (short)(y + 1));
        tileBackground(r, c);
        setFG(mr, mg, mb);
        printf("༺");

        // 중앙 말
        goTo((short)(x + 1), (short)(y + 1));
        tileBackground(r, c);
        setFG(mr, mg, mb);
        printf("♝");

        // 오른쪽 날개
        goTo((short)(x + 2), (short)(y + 1));
        tileBackground(r, c);
        setFG(mr, mg, mb);
        printf("༻");
    }
    else {
        const char* symbol = tileSymbol(r, c);

        if (selected && symbol[0] == ' ')
            printf("+");
        else if (symbol[0] != ' ')
            printf("%s", symbol);
    }


    resetColor();
}


static void drawBoardOutline(void) {
    const short bx = BOARD_X;
    const short by = BOARD_Y;

    resetColor();

    for (int r = 0; r < SIZE; r++) {
        for (int c = 0; c < SIZE; c++) {

            if (board[r][c].terrain == VOID_CELL)
                continue;

            short x = (short)(bx + c * TILE_W);
            short y = (short)(by + r * TILE_H);

            int top =
                (r == 0 ||
                    board[r - 1][c].terrain == VOID_CELL);

            int bottom =
                (r == SIZE - 1 ||
                    board[r + 1][c].terrain == VOID_CELL);

            int left =
                (c == 0 ||
                    board[r][c - 1].terrain == VOID_CELL);

            int right =
                (c == SIZE - 1 ||
                    board[r][c + 1].terrain == VOID_CELL);

            // 은은한 라벤더 외곽선
            setFG(194, 151, 223);

            // 왼쪽 외곽
            if (left) {
                goTo((short)(x - 1), y);
                printf("│");

                goTo((short)(x - 1), (short)(y + 1));
                printf("│");
            }

            // 오른쪽 외곽
            if (right) {
                goTo((short)(x + 4), y);
                printf("│");

                goTo((short)(x + 4), (short)(y + 1));
                printf("│");
            }

            // 위쪽 외곽
            if (top) {
                short lineY = r == 0
                    ? y
                    : (short)(y - 1);

                goTo(x, lineY);
                printf("────");
            }

            // 아래쪽 외곽
            if (bottom) {
                short lineY = r == SIZE - 1
                    ? (short)(y + 1)
                    : (short)(y + 2);

                goTo(x, lineY);
                printf("────");
            }
        }
    }

    resetColor();
}


static void playerPanel(int p, short x) {
    goTo(x, 2); teamColor(p); printf("+------------------------+");
    goTo(x, 3); printf("  ♝  %s TEAM", p == 0 ? "RED" : "BLUE");
    goTo(x, 4); printf("+------------------------+");
    goTo(x, 5); printf("  SCORE : %02d PT", playerScore(p)); resetColor();
    int tiles = 0, secured = 0;
    for (int r = 0; r < SIZE; r++) for (int c = 0; c < SIZE; c++)
        if (board[r][c].owner == p) { tiles++; if (board[r][c].protectedCell) secured++; }
    goTo(x, 6); printf("확정 자원 : %d", deliveredCount(p));
    goTo(x, 7); printf("영토 : %d / 확정 %d", tiles, secured);
    goTo(x, 8); teamColor(p); printf("----- 말 상태 -----"); resetColor();
    for (int u = 0; u < UNIT_COUNT; u++) {
        Unit* unit = &players[p].units[u];
        const char* state = unit->lives <= 0 ? "사망" : unit->row < 0 ? "미배치" : unit->inBase ? "대기" : "이동";
        goTo(x, (short)(9 + u));
        if (p == currentTurn && u == uiSelectedUnit) setBG(75, 111, 66);
        teamColor(p); printf("%c%d ", p == currentTurn && u == uiSelectedUnit ? '>' : ' ', u + 1);
        printf("%s %s ", state, unit->lives == 2 ? "♥♥" : unit->lives == 1 ? "♥♡" : "♡♡");
        if (unit->carryMine >= 0) {
            Mine* m = &mines[unit->carryMine]; resourceColor(m->row, m->col);
            printf("✦%c", 'A' + unit->carryMine);
        }
        else printf("-");
        resetColor();
    }
    for (int kind = 0; kind < 2; kind++) {
        goTo(x, (short)(13 + kind * 2)); printf(kind == 0 ? "보유 자원 :" : "확정 자원 :");
        int n = 0;
        for (int i = 0; i < MINE_COUNT; i++) {
            if ((kind == 0 ? mines[i].carrierPlayer : mines[i].deliveredBy) != p) continue;
            // 최대 11개도 패널 폭 안에서 출력
            goTo((short)(x + (n % 6) * 4), (short)(14 + kind * 2 + n / 6));
            resourceColor(mines[i].row, mines[i].col); printf("✦%c", 'A' + i); resetColor(); n++;
        }
        if (!n) { goTo(x, (short)(14 + kind * 2)); printf("없음"); }
    }
}

void printBoard(void) {
    initConsoleColors();
    resetColor();
    if (rgbEnabled) { printf("\x1b[2J\x1b[H"); fflush(stdout); }
    else { system("cls"); initConsoleColors(); }
    const short bx = BOARD_X, by = BOARD_Y;
    goTo(34, 0); setFG(233, 216, 172); printf("======= A R C A N E  W A R S ======="); resetColor();
    goTo(34, 1); printf("ROUND %02d / %02d   ", roundCount > MAX_ROUNDS ? MAX_ROUNDS : roundCount, MAX_ROUNDS);
    teamColor(currentTurn); printf("%s TURN", currentTurn == 0 ? "RED" : "BLUE"); resetColor();
    playerPanel(0, 1); playerPanel(1, 84);
    for (int r = 0; r < SIZE; r++) {
        for (int c = 0; c < SIZE; c++) {
            printTile(r, c,
                (short)(bx + c * TILE_W),
                (short)(by + r * TILE_H));
        }
    }
    drawBoardOutline();

    // RED TEAM 카드 아래: 게임 규칙 및 범례
    goTo(1, 26);
    setFG(220, 194, 245);
    printf("===== GAME GUIDE =====");
    resetColor();

    goTo(1, 27);
    setFG(151, 229, 166);
    printf("✦ 2점   ");
    setFG(255, 218, 114);
    printf("✦ 3점   ");
    setFG(208, 160, 255);
    printf("✦ 5점");
    resetColor();

    goTo(1, 28);
    printf("♝ 말   ⊹ 소환진   ✵ 중심지");

    goTo(1, 29);
    printf("황금색: 현재 선택");

    goTo(1, 30);
    printf("밝은 팀 색: 확정 영토");

    goTo(1, 31);
    printf("%d라운드 종료 후 점수 비교", MAX_ROUNDS);

    goTo(1, 32);
    printf("상대 전멸 시 즉시 승리");
    drawHandCards();
    if (gameMessage[0] != '\0') {
        goTo(1, 36);
        setFG(245, 200, 152);
        printf("%s", gameMessage);
        resetColor();
    }
    if (winner >= 0 || roundCount > MAX_ROUNDS) {
        goTo(42, 11); setBG(39, 35, 46); setFG(233, 216, 172); printf("============================");
        goTo(42, 12); if (winner >= 0) { teamColor(winner); printf("       %s TEAM WIN!       ", winner == 0 ? "RED" : "BLUE"); }
        else printf("          무승부            ");
        goTo(42, 13); printf("       %02d PT : %02d PT        ", playerScore(0), playerScore(1));
        resetColor();
    }
    fflush(stdout);
}

// 키보드: 방향키 입력을 72/80/75/77로 반환
static int readPlacementKey(void) {
    int k = _getch();
    return (k == 0 || k == 224) ? _getch() : k;
}

// 양쪽이 중앙 5열을 두고 경쟁한다. 먼저 둔 자기 말에서 3칸 이내에 배치.
// 광산/중앙 X/기지에는 직접 배치하지 않고, X 바로 옆도 금지.
static int canPlaceAt(int p, int r, int c) {
    if (!inBoard(r, c) || board[r][c].terrain != EMPTY) return 0;
    if (r < 2 || r > 8) return 0;
    if (c < 3 || c > 7) return 0;
    if (abs(r - 5) + abs(c - 5) <= 1) return 0;
    if (findUnitAt(r, c, NULL, NULL)) return 0;

    (void)p;
    return 1;
}

// 한 사람이 한 말을 방향키로 배치. 한 번 배치한 칸은 그 말의 귀환 기지.
static void placeOneUnit(int p, int u) {
    int r = 3, c = 5;
    // 앞서 배치한 말이 있으면 주변 빈 칸부터 시작
    if (!canPlaceAt(p, r, c)) {
        int found = 0;
        for (int rr = 2; rr <= 8 && !found; rr++) {
            for (int cc = 3; cc <= 7; cc++) {
                if (canPlaceAt(p, rr, cc)) {
                    r = rr;
                    c = cc;
                    found = 1;
                    break;
                }
            }
        }
    }
    currentTurn = p;
    uiSelectedUnit = u;
    for (;;) {
        clearVisualMarks();
        // 배치 가능한 전체 후보를 표시
        for (int rr = 2; rr <= 8; rr++)
            for (int cc = 3; cc <= 7; cc++)
                if (canPlaceAt(p, rr, cc)) markMoveTarget(rr, cc);
        uiCursorRow = r;
        uiCursorCol = c;
        gameMessage[0] = '\0';
        printBoard();
        goTo(1, 31); setFG(220, 194, 245); printf("P%d - %d번 말 배치", p + 1, u + 1);
        goTo(1, 32); setFG(220, 220, 225); printf("방향키 : 위치 선택");
        goTo(1, 33); printf("Enter  : 배치 확정");
        goTo(1, 34); setFG(151, 229, 166); printf("초록색 : 배치 가능");
        resetColor();
        fflush(stdout);
        int key = readPlacementKey();
        int nr = r, nc = c;
        if (key == 72) nr--;
        else if (key == 80) nr++;
        else if (key == 75) nc--;
        else if (key == 77) nc++;
        else if (key == 13) {
            if (!canPlaceAt(p, r, c)) continue;
            Unit* unit = &players[p].units[u];
            unit->row = unit->baseRow = r;
            unit->col = unit->baseCol = c;
            unit->inBase = 1;
            board[r][c].terrain = BASE;
            clearVisualMarks();
            return;
        }
        // 유효 구역 바깥으로는 커서가 나가지 않음. 광산은 건너뛸 수 있도록 이동은 EMPTY만 요구하지 않음
        if (inBoard(nr, nc) && nr >= 2 && nr <= 8 &&
            nc >= 3 && nc <= 7 &&
            board[nr][nc].terrain != VOID_CELL) {
            r = nr; c = nc;
        }
    }
}

void placeStartingUnits(void) {
    // 스네이크 배치: P1, P2, P2, P1, P1, P2
    const int orderP[6] = { 0,1,1,0,0,1 };
    const int orderU[6] = { 0,0,1,1,2,2 };
    for (int i = 0; i < 6; i++) placeOneUnit(orderP[i], orderU[i]);
    uiSelectedUnit = -1;
    clearVisualMarks();
    gameMessage[0] = '\0';
}

// 두 사람이 각각 차례를 마치면 다음 라운드
void playGame(void) {
    placeStartingUnits();
    while (roundCount <= MAX_ROUNDS && winner == -1) {
        for (int p = 0; p < PLAYER_COUNT&& winner == -1; p++) {
            currentTurn = p;
            printBoard();
            int result = playUnitTurn(p);
            if (result == -1) return;  // Q: 게임 종료
        }
        if (winner == -1) roundCount++;
    }
    uiSelectedUnit = -1;
    clearVisualMarks();
    if (winner == -1) {
        int a = playerScore(0), b = playerScore(1);
        if (a != b) winner = a > b ? 0 : 1;
        snprintf(gameMessage, sizeof(gameMessage), "라운드 종료: 점수 %d 대 %d", a, b);
    }
    printBoard();
    goTo(1, 34); printf("게임 종료 | 아무 키나 누르면 종료합니다.");
    _getch();
}
