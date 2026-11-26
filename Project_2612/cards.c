#include <stdio.h>
#include <stdlib.h>
#include <conio.h>
#include <Windows.h>
#include "game.h"

int publicCard = -1;
int uiSelectedUnit = -1;
static int uiCardSlot = -1;

typedef struct {
    int count;
    int moves[MAX_MOVES][2];  // 카드 중심에서 행·열 변화
} MoveCard;

// 카드 이름 없이 16종 이동 패턴만 저장
static const MoveCard cards[CARD_COUNT] = {
    {2, {{-2,0},{1,0}}},
    {4, {{-1,-2},{-1,2},{1,-1},{1,1}}},
    {3, {{0,-2},{-1,-1},{1,1}}},
    {3, {{0,2},{-1,1},{1,-1}}},
    {3, {{0,-2},{-1,0},{0,2}}},
    {4, {{0,-1},{0,1},{-1,-1},{-1,1}}},
    {4, {{0,-1},{-1,-1},{0,1},{1,1}}},
    {4, {{0,1},{-1,1},{0,-1},{1,-1}}},
    {4, {{-1,-1},{-1,1},{1,-1},{1,1}}},
    {3, {{-1,-1},{-1,1},{1,0}}},
    {3, {{-1,0},{0,-1},{1,0}}},
    {3, {{-1,0},{0,1},{1,0}}},
    {3, {{-1,0},{1,-1},{1,1}}},
    {3, {{-1,0},{0,-1},{0,1}}},
    {3, {{-1,-1},{1,-1},{0,1}}},
    {3, {{-1,1},{1,1},{0,-1}}}
};

// 방향키는 72, 80, 75, 77 그대로 사용
static int readKey(void) {
    int key = _getch();
    return key == 0 || key == 224 ? _getch() : key;
}

static void goTo(short x, short y) {
    fflush(stdout);
    if (rgbEnabled) { printf("\x1b[%d;%dH", y + 1, x + 1); return; }
    COORD pos = { x,y };
    SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), pos);
}

// 손패나 공개 카드에 있는 번호인지 확인
static int held(int card) {
    if (publicCard == card) return 1;
    for (int p = 0; p < PLAYER_COUNT; p++)
        for (int s = 0; s < HAND_SIZE; s++)
            if (players[p].hand[s] == card) return 1;
    return 0;
}

// 카드 16종 중 아직 쓰지 않은 카드 추첨
static int drawCard(void) {
    int list[CARD_COUNT], count = 0;
    for (int i = 0; i < CARD_COUNT; i++)
        if (!held(i)) list[count++] = i;
    return list[rand() % count];
}

void initCards(void) {
    publicCard = -1;
    for (int p = 0; p < PLAYER_COUNT; p++)
        for (int s = 0; s < HAND_SIZE; s++) players[p].hand[s] = -1;
    for (int p = 0; p < PLAYER_COUNT; p++)
        for (int s = 0; s < HAND_SIZE; s++) players[p].hand[s] = drawCard();
    publicCard = drawCard();
}

// 사용 카드와 공개 카드 1장 교환
void refillCard(int p, int slot) {
    int old = players[p].hand[slot];
    players[p].hand[slot] = publicCard;
    publicCard = old;
}

// 5×5 패턴을 화면 고정 위치에 표시
static void drawCardPattern(int card, short x, short y, int p) {
    for (int r = 0; r < 5; r++) {
        for (int c = 0; c < 5; c++) {
            goTo((short)(x + c * 2), (short)(y + r));
            int status = r == 2 && c == 2 ? 2 : 0;
            for (int i = 0; i < cards[card].count; i++)
                if (r == 2 + cards[card].moves[i][0] && c == 2 + cards[card].moves[i][1]) status = 1;
            if (status == 2) {
                if (p == 0) setBG(101, 64, 78); else if (p == 1) setBG(53, 72, 102); else setBG(77, 65, 91);
                setFG(255, 225, 235); printf("♝");
            }
            else if (status == 1) { setBG(100, 139, 85); setFG(225, 245, 220); printf("+"); }
            else { setBG(38, 35, 44); printf(" "); }
            printf(" "); resetColor();
        }
    }
}
void drawHandCards(void) {
    const short panelX[2] = { 1,84 };
    for (int p = 0; p < PLAYER_COUNT; p++) {
        goTo(panelX[p], 18);
        if (p == 0) setFG(255, 155, 175); else setFG(147, 183, 255);
        printf("====== %s CARDS ======", p == 0 ? "RED" : "BLUE"); resetColor();
        for (int s = 0; s < HAND_SIZE; s++) {
            short x = (short)(panelX[p] + s * 11);
            goTo(x, 19);
            if (p == currentTurn && s == uiCardSlot) setBG(100, 139, 85);
            printf("   [%d]    ", s + 1); resetColor();
            drawCardPattern(players[p].hand[s], x, 20, p);
        }
    }
    goTo(50, 27); setFG(220, 194, 245); printf("EXCHANGE CARD"); resetColor();
    drawCardPattern(publicCard, 52, 28, -1);
}

// 점령 가능한 칸만 소유권 변경, 보호된 상대 영토는 유지
static void claimCell(int p, int u, int r, int c) {
    if (!isPlayable(r, c)) return;
    if (board[r][c].terrain == GOAL) return; // 중앙 X는 누구나 사용 가능
    if (board[r][c].protectedCell && board[r][c].owner != p) return;
    int owner;
    if (findUnitAt(r, c, &owner, NULL) && owner != p) return;
    if (board[r][c].protectedCell) return;
    if (board[r][c].owner == p) return; // 최초 점령한 말의 소유 기록 유지
    board[r][c].owner = p;
    board[r][c].ownerUnit = u;
}

// 경로가 다른 말이나 기지 칸을 통과하는지 검사한다.
// 카드 이동 자체는 길찾기가 아니라, 가로 우선/세로 우선 두 경로 중 하나를 선택한다.
static int routeOpen(int p, int sr, int sc, int er, int ec, int horizontalFirst) {
    int r = sr, c = sc;
    while (r != er || c != ec) {
        if (horizontalFirst && c != ec) c += ec > c ? 1 : -1;
        else if (!horizontalFirst && r != er) r += er > r ? 1 : -1;
        else if (r != er) r += er > r ? 1 : -1;
        else c += ec > c ? 1 : -1;
        if (r == er && c == ec) break; // 도착 칸 검사는 별도로 수행
        if (!isPlayable(r, c) || findUnitAt(r, c, NULL, NULL)) return 0;
        if (board[r][c].protectedCell && board[r][c].owner != p) return 0;
    }
    return 1;
}

// 두 후보 경로 중 보호 영토/기지/상대 말에 덜 막히는 경로를 선택
// 한 칸 대각선, ㄱ자 이동도 상하좌우로 끊어지지 않게 칠한다.
static int pathCost(int p, int sr, int sc, int er, int ec, int horizontalFirst) {
    int r = sr, c = sc, cost = 0;
    for (int step = 0; step < abs(er - sr) + abs(ec - sc); step++) {
        if (horizontalFirst && c != ec) c += ec > c ? 1 : -1;
        else if (!horizontalFirst && r != er) r += er > r ? 1 : -1;
        else if (r != er) r += er > r ? 1 : -1;
        else c += ec > c ? 1 : -1;
        if (!isPlayable(r, c) || board[r][c].terrain == GOAL) {
            cost += 100;
            continue;
        }
        if (board[r][c].protectedCell && board[r][c].owner != p)
            cost += 100;
        int owner;
        if (findUnitAt(r, c, &owner, NULL))
            cost += 10000;
        if (board[r][c].owner == p) cost--;
    }
    return cost;
}

// 실제 지나간 길을 상하좌우로 이어진 연속 칸으로 점령
void claimPath(int p, int u, int sr, int sc, int er, int ec) {
    int horizontalFirst = pathCost(p, sr, sc, er, ec, 1) <= pathCost(p, sr, sc, er, ec, 0);
    if (!routeOpen(p, sr, sc, er, ec, horizontalFirst)) horizontalFirst = !horizontalFirst;
    int r = sr, c = sc;
    claimCell(p, u, r, c);
    while (r != er || c != ec) {
        if (horizontalFirst && c != ec) c += ec > c ? 1 : -1;
        else if (!horizontalFirst && r != er) r += er > r ? 1 : -1;
        else if (r != er) r += er > r ? 1 : -1;
        else c += ec > c ? 1 : -1;
        claimCell(p, u, r, c);
    }
}

// 카드의 이동 가능한 목적지를 수집 (기지에서도 카드로 출격)
static int destinations(int p, int u, int slot, int rows[], int cols[]) {
    Unit* unit = &players[p].units[u];
    int card = players[p].hand[slot], count = 0;
    if (unit->lives <= 0 || unit->row < 0) return 0;
    for (int i = 0; i < cards[card].count; i++) {
        int r = unit->row + cards[card].moves[i][0];
        int c = unit->col + cards[card].moves[i][1];
        int baseP;
        if (!isPlayable(r, c) && !(inBoard(r, c) && board[r][c].terrain == BASE && findUnitAt(r, c, &baseP, NULL) && baseP != p)) continue;
        // 아군 말은 막고, 상대 말은 포획 가능
        if (board[r][c].protectedCell && board[r][c].owner != p) continue;
        int targetP;
        if (findUnitAt(r, c, &targetP, NULL)) {
            if (targetP == p || roundCount == 1) continue;
        }
        // 중간 경로도 다른 말이 막고 있으면 통과할 수 없다.
        if (!routeOpen(p, unit->row, unit->col, r, c, 1) &&
            !routeOpen(p, unit->row, unit->col, r, c, 0)) continue;
        rows[count] = r; cols[count] = c; count++;
    }
    return count;
}

static int canMove(int p, int u) {
    for (int s = 0; s < HAND_SIZE; s++) {
        int rows[MAX_MOVES], cols[MAX_MOVES];
        if (destinations(p, u, s, rows, cols) > 0) return 1;
    }
    return 0;
}


// 각 선택 화면의 조작법을 왼쪽 안내영역에 표시한다.
// printBoard()가 먼저 화면을 새로 그리므로 별도 지우기는 필요하지 않다.
static void drawTurnGuide(int mode, int p, int error) {
    goTo(1, 31); setFG(220, 194, 245);
    if (mode == 0) printf("P%d - 말 선택", p + 1);
    else if (mode == 1) printf("P%d - 카드 선택", p + 1);
    else printf("이동 위치 선택");

    setFG(220, 220, 225);
    if (mode == 0) {
        goTo(1, 32); printf("위/아래 또는 1~3 : 선택");
        goTo(1, 33); printf("Enter : 말 선택");
        goTo(1, 34); printf("S : 턴 종료");
        goTo(1, 35); printf("Q : 게임 종료");
    }
    else if (mode == 1) {
        goTo(1, 32); printf("좌/우 방향키 : 카드 선택");
        goTo(1, 33); printf("Enter : 카드 확정");
        goTo(1, 34); printf("ESC : 이전 단계");
    }
    else {
        goTo(1, 32); printf("방향키 : 목적지 선택");
        goTo(1, 33); printf("Enter : 이동 확정");
        goTo(1, 34); printf("ESC : 이전 단계");
    }
    if (error) {
        goTo(1, 36);
        setFG(255, 150, 160);
        printf("이미 이동했거나 이동 불가");
    }
    resetColor();
    fflush(stdout);
}

// 방향키로 선택한 말 강조. 숫자 1~3은 바로 그 말을 선택한다.
static int chooseUnit(int p, int used[]) {
    int selected = 0;
    int invalidSelection = 0;
    while (used[selected] || !canMove(p, selected))
        selected = (selected + 1) % UNIT_COUNT;
    while (1) {
        uiSelectedUnit = selected;
        clearVisualMarks();
        uiCursorRow = players[p].units[selected].row;
        uiCursorCol = players[p].units[selected].col;
        printBoard();
        drawTurnGuide(0, p, invalidSelection);
        int key = readKey();
        if (key == 's' || key == 'S') return -1;
        if (key == 'q' || key == 'Q') return -2;
        if (key == 72 || key == 80) {
            int delta = key == 72 ? UNIT_COUNT - 1 : 1;
            for (int i = 0; i < UNIT_COUNT; i++) {
                selected = (selected + delta) % UNIT_COUNT;
                if (!used[selected] && canMove(p, selected)) break;
            }
            invalidSelection = 0;
            continue;
        }
        if (key >= '1' && key <= '3') selected = key - '1';
        else if (key != 13) continue;
        if (used[selected] || !canMove(p, selected)) {
            invalidSelection = 1;
            continue;
        }
        return selected;
    }
}

// 카드 1~3 중 하나 선택: 왼쪽·오른쪽 키
static int chooseCard(int p) {
    int slot = 0;
    while (1) {
        uiCardSlot = slot;
        printBoard();
        drawTurnGuide(1, p, 0);
        int key = readKey();
        if (key == 75) slot = (slot + HAND_SIZE - 1) % HAND_SIZE;
        else if (key == 77) slot = (slot + 1) % HAND_SIZE;
        else if (key == 13) return slot;
        else if (key == 27) { uiCardSlot = -1; return -1; }
    }
}

// 초록색 목적지 후보 사이를 왼쪽·오른쪽으로 순환
static int chooseTarget(int rows[], int cols[], int count, int* er, int* ec) {
    int selected = 0;
    while (1) {
        clearVisualMarks();
        for (int i = 0; i < count; i++) markMoveTarget(rows[i], cols[i]);
        uiCursorRow = rows[selected]; uiCursorCol = cols[selected];
        printBoard();
        drawTurnGuide(2, currentTurn, 0);
        int key = readKey();
        if (key == 75 || key == 77 || key == 72 || key == 80) {
            int best = -1, bestDist = 100000;
            for (int i = 0; i < count; i++) {
                if (i == selected) continue;
                int dr = rows[i] - rows[selected];
                int dc = cols[i] - cols[selected];
                // 원하는 방향 쪽에 있는 후보 중 가장 가까운 칸
                if (key == 75 && dc >= 0) continue;
                if (key == 77 && dc <= 0) continue;
                if (key == 72 && dr >= 0) continue;
                if (key == 80 && dr <= 0) continue;
                int dist = abs(dr) * abs(dr) + abs(dc) * abs(dc);
                if (dist < bestDist) { bestDist = dist; best = i; }
            }
            if (best >= 0) selected = best;
        }
        else if (key == 13) {
            *er = rows[selected]; *ec = cols[selected];
            clearVisualMarks(); return 1;
        }
        else if (key == 27) { clearVisualMarks(); return 0; }
    }
}

// 카드를 골라 목적지 이동 및 광물 상태 갱신
static int moveWithCard(int p, int u) {
    while (1) {
        int slot = chooseCard(p);
        if (slot < 0) return 0;
        int rows[MAX_MOVES], cols[MAX_MOVES];
        int count = destinations(p, u, slot, rows, cols);
        if (count == 0) {
            snprintf(gameMessage, sizeof(gameMessage), "이 카드로 이동할 수 없습니다. 다른 카드를 선택하세요.");
            continue;
        }
        int r, c;
        if (!chooseTarget(rows, cols, count, &r, &c)) continue;
        Unit* unit = &players[p].units[u];
        int oldR = unit->row, oldC = unit->col;
        gameMessage[0] = '\0';
        int enemyP, enemyU;
        int captured = findUnitAt(r, c, &enemyP, &enemyU) && enemyP != p;
        if (captured && roundCount == 1) continue; // 첫 라운드 공격 방지
        if (captured) captureUnit(enemyP, enemyU, p, r, c);
        claimPath(p, u, oldR, oldC, r, c);
        unit->row = r; unit->col = c; unit->inBase = 0;
        refillCard(p, slot);
        uiCardSlot = -1;
        onUnitArrived(p, u);
        if (board[r][c].terrain == GOAL) {
            // X는 영구 점유할 수 없는 공용 목적지: 판정 직후 원래 칸으로 복귀.
            int carrying = unit->carryMine >= 0;
            checkDeliveries();
            if (!carrying) {
                snprintf(gameMessage, sizeof(gameMessage), "P%d: 자원을 가지고 중심지에 도착해야 합니다.", p + 1);
            }
            else if (unit->carryMine >= 0) {
                snprintf(gameMessage, sizeof(gameMessage), "P%d: 자원과 중심지 사이의 영토가 끊겼습니다.", p + 1);
            }
            unit->row = oldR;
            unit->col = oldC;
            unit->inBase = board[oldR][oldC].terrain == BASE;
        }
        return 1;
    }
}

// 플레이어당 서로 다른 말 최대 2개 사용
int playUnitTurn(int p) {
    int used[UNIT_COUNT] = { 0 };
    int moved = 0;
    while (moved < 2 && winner == -1) {
        int available = 0;
        for (int u = 0; u < UNIT_COUNT; u++)
            if (!used[u] && canMove(p, u)) available++;
        if (available == 0) break;
        int u = chooseUnit(p, used);
        if (u == -2) return -1;  // 게임 종료
        if (u == -1) break;      // 차례 넘기기
        if (!moveWithCard(p, u)) continue; // ESC면 다시 말 선택
        int alive = 0;
        for (int i = 0; i < UNIT_COUNT; i++) if (players[p].units[i].lives > 0) alive++;
        used[u] = (alive == UNIT_COUNT);
        moved++;
    }
    uiSelectedUnit = -1;
    uiCardSlot = -1;
    clearVisualMarks();
    if (winner == -1 && moved == 0)
        snprintf(gameMessage, sizeof(gameMessage), "P%d 차례 종료 (이동 없음)", p + 1);
    return moved > 0;
}
