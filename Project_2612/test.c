#include <stdio.h>
#include <Windows.h>
#include <ctype.h>
#include "test.h"

char board[SIZE][SIZE];
// 체스 말 기호 변환
const char* getSymbol(char piece) {
	switch (piece) {
	case 'K': return "♔";
	case 'Q': return "♕";
	case 'R': return "♖";
	case 'N': return "♘";
	case 'P': return "♙";
	
	case 'k': return "♚";
	case 'q': return "♛";
	case 'r': return "♜";
	case 'n': return "♞";
	case 'p': return "♟";
		
	default: return " ";
	}

}
// 1. 체스판 초기화
void initBoard(){

	// 체스 말 (비숏 제외)
	char black[] = { 'r', 'n', 'q', 'k', 'n', 'r'};
	char white[] = { 'R', 'N', 'Q', 'K', 'N', 'R' };

	//체스 칸
	for (int i = 0; i < SIZE; i++) {
		for (int j = 0; j < SIZE; j++) {
			board[i][j] = ' ';
		}
	}

	//체스 배치
	for (int i = 0; i < SIZE; i++) {
		board[0][i] = black[i];
		board[1][i] = 'p';
		board[4][i] = 'P';
		board[5][i] = white[i];
	}
}

// 2. 현재 플레이어의 말인지 확인
int isMyPiece(char piece, int turn) {
	if (turn == 0) {
		return piece >= 'A' && piece <= 'Z'; //흰색이 시작
	}
	else {
		return piece >= 'a' && piece <= 'z';
	}
}

// 체스판 범위 확인
int inBoard(int row, int col) {
	return row >= 0 && row < SIZE && col >= 0 && col < SIZE;
}

// 정수 절댓값 계산
int absValue(int n) {
	return n < 0 ? -n : n;
}

// 룩, 퀸 이동 시 중간에 말이 있는지 검사
int isPathClear(int sr, int sc, int er, int ec) {
	int rowStep = (er > sr) - (er < sr);
	int colStep = (ec > sc) - (ec < sc);

	int r = sr + rowStep;
	int c = sc + colStep;

	while (r != er || c != ec) {
		if (board[r][c] != ' ') {
			return 0;
		}
		r += rowStep;
		c += colStep;
	}
	return 1;
}

// 3. 말별 이동 가능 여부 판단
int isValidMove(int sr, int sc, int er, int ec, int turn) {
	if (!inBoard(sr, sc) || !inBoard(er, ec)) {
		return 0;
	}

	char piece = board[sr][sc];
	char target = board[er][ec];

	// 자신의 말이 아니면 이동 불가
	if (!isMyPiece(piece, turn))
		return 0;

	// 자기 말이 있는 칸으로 이동 불가
	if (isMyPiece(target, turn)) {
		return 0;
	}
	if (sr == er && sc == ec) {
		return 0;
	}

	int dr = er - sr;
	int dc = ec - sc;
	int ar = absValue(dr);
	int ac = absValue(dc);

	switch (piece) {
		//폰: 백팀은 위, 흑팀은 아래쪽
	case 'p':
	case 'P': {
		int direction = (turn == 0) ? -1 : 1;

		if (dr == direction && dc == 0 && target == ' ') {
			return 1;
		}

		if (dr == direction && ac == 1 && target != ' ' && !isMyPiece(target, turn)) {
			return 1;
		}
		return 0;
	}
			//룩 : 가로, 세로 이동 가능
	case 'R':
	case 'r':
		if (dr == 0 || dc == 0) {
			return isPathClear(sr, sc, er, ec);
		}
		return 0;
		//나이트: L자 이동
	case 'N':
	case 'n':
		return (ar == 2 && ac == 1) || (ar == 1 && ac == 2);

		//퀸: 가로, 세로, 대각선
	case 'Q':
	case 'q':
		if (dr == 0 || dc == 0 || ar == ac) {
			return isPathClear(sr, sc, er, ec);
			return 0;
		}
		//킹: 모든 방향으로 한 칸
	case 'K':
	case 'k':
		return ar <= 1 && ac <= 1;

	default:
		return 0;
	}
}

// 4. 체스판 출력
void printBoard(int sr, int sc, int turn) {
	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

	//열
	printf("\n      A   B   C   D   E   F\n");
	printf("    -------------------------\n");

	for (int i = 0; i < SIZE; i++) {
		printf(" %d |", SIZE - i);

		for (int j = 0; j < SIZE; j++) {
			char piece = board[i][j];

			// 선택한 말의 이동 가능한 칸
			int possible = 0;

			if (inBoard(sr, sc) && isValidMove(sr, sc, i, j, turn)) {
				possible = 1;
			}

			//말이 없는 이동 가능한 칸
			if (possible && piece == ' ') {
				SetConsoleTextAttribute(hConsole, 10);
				printf(" + ");
			}
			//말이 있는 칸
			else {
				if (i == sr && j == sc) {
					SetConsoleTextAttribute(hConsole, 14);
				}
				else if (possible) {
					//공격 가능한 상대 말
					SetConsoleTextAttribute(hConsole, 13);
				}
				else if (piece >= 'A' && piece <= 'Z') {
					SetConsoleTextAttribute(hConsole, 15);
				}
				else if (piece >= 'a' && piece <= 'z') {
					SetConsoleTextAttribute(hConsole, 12);
				}
				else {
					SetConsoleTextAttribute(hConsole, 7);
				}

				printf(" %s ", getSymbol(piece));
			}

			SetConsoleTextAttribute(hConsole, 7);
			printf("|");
		}
		printf("\n    -------------------------\n");
	}

	printf("\n");
}

// 5. 좌표 입력
int inputPosition(int* row, int* col) {
	char buffer[64];
	char column, extra;
	int number;

	if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
		return -1;
	}
	//x 입력 시 종료
	if (buffer[0] == 'X' || buffer[0] == 'x') {
		return -1;
	}
	// 예: A2, C4
	if (sscanf_s(buffer, "%c%d %c", &column, 1u, &number, &extra, 1u) != 2) {
		printf("좌표를 올바르게 입력하시오. 예: A2\n");
		return 0;
	}

	column = (char)toupper((unsigned char)column);

	if (column < 'A' || column > 'F' || number < 1 || number > 6) {
		printf("A~F, 1~6 범위에서 입력하세요.\n");
		return 0;
	}
	*row = SIZE - number;
	*col = column - 'A';

	return 1;
}

// 6. 이동할 말 선택
int selectPiece(int turn, int* sr, int* sc) {
	while (1) {
		printf("이동할 말의 좌표 입력 (예: B1, 종료: X): ");

		int result = inputPosition(sr, sc);

		if (result == -1) {
			return 0;
		}
		if (result == 0) {
			continue;
		}
		if (!isMyPiece(board[*sr][*sc], turn)) {
			printf("자신의 말을 선택하시오.\n");
			continue;
		}
		return 1;
	}
}

// 7. 말 이동 및 상대 말 공격
// 반환값: 0 = 실패, 1 = 성공, 2 = 승리
int movePiece(int sr, int sc, int er, int ec, int turn) {
	if (!inBoard(sr, sc) || !inBoard(er, ec)) {
		printf("체스판 범위를 벗어났습니다.\n");
		return 0;
	}
	if (!isMyPiece(board[sr][sc], turn)) {
		printf("자신의 말을 선택하시오.\n");
		return 0;
	}
	if (isMyPiece(board[er][ec], turn)) {
		printf("자신의 말이 있는 칸으로 이동할 수 없습니다.\n");
		return 0;
	}
	if (!isValidMove(sr, sc, er, ec, turn)) {
		printf("이동할 수 없는 위치입니다.\n");
		return 0;
	}

	//공격 대상 저장
	char target = board[er][ec];

	//체스판 상태 갱신
	board[er][ec] = board[sr][sc];
	board[sr][sc] = ' ';

	// 상대 킹을 잡으면 승리
	if (target == 'K' || target == 'k') {
		return 2;
	}
	return 1;
}