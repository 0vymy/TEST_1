#include <stdio.h>
#include <stdlib.h>
#include "game.h"

Mine mines[MINE_COUNT];
static const int mineRows[MINE_COUNT] = { 2,4,4,7,7,8,8,3,3,9,9 };
static const int mineCols[MINE_COUNT] = { 5,3,7,3,7,2,8,1,9,4,6 };

void initMines(void) {
    for (int i = 0; i < MINE_COUNT; i++) {
        mines[i].row = mineRows[i]; mines[i].col = mineCols[i];
        mines[i].carrierPlayer = mines[i].carrierUnit = mines[i].deliveredBy = -1;
        board[mineRows[i]][mineCols[i]].terrain = MINE;
    }
    board[5][5].terrain = GOAL;
}

void onUnitArrived(int p, int u) {
    Unit* unit = &players[p].units[u];
    if (unit->lives <= 0 || unit->carryMine >= 0) return;
    for (int i = 0; i < MINE_COUNT; i++) {
        if (mines[i].row != unit->row || mines[i].col != unit->col) continue;
        if (mines[i].deliveredBy >= 0 || mines[i].carrierPlayer >= 0) return;
        mines[i].carrierPlayer = p; mines[i].carrierUnit = u;
        unit->carryMine = i;
        snprintf(gameMessage, sizeof(gameMessage), "P%d %d번 말: 자원 %c 획득!", p + 1, u + 1, 'A' + i);
        return;
    }
}

// 광산에서 X까지 자기 팀의 영토를 BFS로 탐색한다.
// 기존 확정 영토도 연결 경로에 포함한다.
static int secureConnected(int p, int m) {
    int seen[SIZE][SIZE] = { 0 };
    int qr[SIZE * SIZE], qc[SIZE * SIZE], head = 0, tail = 0;
    int r = mines[m].row, c = mines[m].col, goalConnected = 0;
    const int dr[4] = { -1,1,0,0 }, dc[4] = { 0,0,-1,1 };
    if (board[r][c].owner != p) return 0;
    seen[r][c] = 1; qr[tail] = r; qc[tail++] = c;
    while (head < tail) {
        r = qr[head]; c = qc[head++];
        // 중앙 X에 인접한 칸은 대각선 연결도 인정
        if (abs(r - 5) <= 1 && abs(c - 5) <= 1) {
            goalConnected = 1;
        }
        for (int d = 0; d < 4; d++) {
            int nr = r + dr[d], nc = c + dc[d];
            if (!isPlayable(nr, nc) || seen[nr][nc]) continue;
            if (board[nr][nc].terrain == GOAL) { goalConnected = 1; continue; }
            if (board[nr][nc].owner != p) continue;
            seen[nr][nc] = 1; qr[tail] = nr; qc[tail++] = nc;
        }
    }
    if (!goalConnected) return 0;
    for (int rr = 0; rr < SIZE; rr++) for (int cc = 0; cc < SIZE; cc++)
        if (seen[rr][cc]) board[rr][cc].protectedCell = 1;
    return 1;
}

void checkDeliveries(void) {
    if (winner >= 0) return;
    for (int p = 0; p < PLAYER_COUNT; p++) for (int u = 0; u < UNIT_COUNT; u++) {
        Unit* unit = &players[p].units[u]; int m = unit->carryMine;
        if (unit->lives <= 0 || m < 0 || unit->row != 5 || unit->col != 5) continue;
        if (!secureConnected(p, m)) continue;
        mines[m].deliveredBy = p;
        mines[m].carrierPlayer = mines[m].carrierUnit = -1;
        unit->carryMine = -1;
        snprintf(gameMessage, sizeof(gameMessage), "P%d 자원 %c 전달 성공! 연결 영토 확정!", p + 1, 'A' + m);

    }
}
int deliveredCount(int p) {
    int count = 0;
    for (int i = 0; i < MINE_COUNT; i++) if (mines[i].deliveredBy == p) count++;
    return count;
}

// 포획된 말의 미확정 영토만 중립으로 되돌린다.
static void clearUnitTerritory(int p, int u) {
    for (int r = 0; r < SIZE; r++) for (int c = 0; c < SIZE; c++) {
        Cell* cell = &board[r][c];
        if (cell->owner == p && cell->ownerUnit == u && !cell->protectedCell) {
            cell->owner = cell->ownerUnit = -1;
        }
    }
}

// 중앙에서 떨어진 빈칸을 우선하고, 없으면 다른 안전한 빈칸을 사용한다.
static int respawnUnit(int p, int u, int ar, int ac) {
    int rows[SIZE * SIZE], cols[SIZE * SIZE];
    for (int pass = 0; pass < 4; pass++) {
        int n = 0;
        for (int r = 0; r < SIZE; r++) for (int c = 0; c < SIZE; c++) {
            int dr = abs(r - 5), dc = abs(c - 5), dist = dr > dc ? dr : dc;
            if (board[r][c].terrain != EMPTY || board[r][c].owner != -1) continue;
            if ((r == ar && c == ac) || findUnitAt(r, c, NULL, NULL)) continue;
            if (pass == 0 && (dist < 3 || dist>4)) continue;
            if (pass == 1 && dist < 2) continue;
            if (pass == 2 && dist < 2) continue;
            int near = 0;
            for (int j = 0; j < UNIT_COUNT; j++) {
                Unit* e = &players[1 - p].units[j];
                if (e->lives > 0 && e->row >= 0 && abs(e->row - r) + abs(e->col - c) <= 1) near = 1;
            }
            if (abs(ar - r) + abs(ac - c) <= 1) near = 1;
            if (pass < 2 && near) continue;
            rows[n] = r; cols[n++] = c;
        }
        if (n) {
            int k = rand() % n; Unit* unit = &players[p].units[u];
            unit->row = unit->baseRow = rows[k]; unit->col = unit->baseCol = cols[k];
            unit->inBase = 1; board[rows[k]][cols[k]].terrain = BASE;
            return 1;
        }
    }
    return 0;
}

void captureUnit(int p, int u, int attacker, int attackRow, int attackCol) {
    Unit* unit = &players[p].units[u];
    if (unit->lives <= 0) return;
    if (unit->carryMine >= 0) {
        int m = unit->carryMine;
        mines[m].carrierPlayer = mines[m].carrierUnit = -1;
        unit->carryMine = -1;
    }
    clearUnitTerritory(p, u);
    if (inBoard(unit->baseRow, unit->baseCol) && board[unit->baseRow][unit->baseCol].terrain == BASE)
        board[unit->baseRow][unit->baseCol].terrain = EMPTY;
    unit->row = unit->col = unit->baseRow = unit->baseCol = -1;
    unit->inBase = 0;
    unit->lives--;
    if (unit->lives > 0 && !respawnUnit(p, u, attackRow, attackCol)) {
        // 빈칸이 전혀 없을 경우 리스폰 불가로 사망 처리
        unit->lives = 0;
    }
    snprintf(gameMessage, sizeof(gameMessage), "P%d %d번 말 포획! %s (수명 %d/2)", p + 1, u + 1, unit->lives ? "새 거점에서 부활" : "영구 사망", unit->lives);
    int alive = 0;
    for (int j = 0; j < UNIT_COUNT; j++) if (players[p].units[j].lives > 0) alive++;
    if (!alive) winner = attacker;
}

// 중심지까지의 맨해튼 거리에 따른 자원 점수
int resourceValue(int r, int c) {
    int d = abs(r - SIZE / 2) + abs(c - SIZE / 2);
    return d <= 3 ? 2 : d <= 5 ? 3 : 5;
}

// 전달을 완료한 자원만 점수에 반영
int playerScore(int p) {
    int score = 0;
    for (int i = 0; i < MINE_COUNT; i++)
        if (mines[i].deliveredBy == p)
            score += resourceValue(mines[i].row, mines[i].col);
    return score;
}
