#include <stdio.h>
#include <Windows.h> //콘솔 관련 기능 사용
#include "test.h"
#include <stdlib.h> // system() 함수

int main() {  

	SetConsoleOutputCP(CP_UTF8); // 콘솔 출력 인코딩을 UTF-8로 설정

	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

	// 백팀: 흰색
	SetConsoleTextAttribute(hConsole, 15);
	printf("♔ ♕ ♖ ♘ ♙ vs ");

	// 흑팀: 빨강
	SetConsoleTextAttribute(hConsole, 12);
	printf("♚ ♛ ♜ ♞ ♟\n");

	// 기본 색상 복구
	SetConsoleTextAttribute(hConsole, 7);

	int turn = 0; // 0: 백팀, 1: 흑팀
	int sr, sc, er, ec;
	int result;

	initBoard();

	while (1) {
		system("cls"); //콘솔 화면 초기화

		//현재 체스판 출력
		printBoard(-1, -1, turn);

		if (turn == 0) {
			printf("\n[ WHITE TUNR ]\n");
		}
		else {
			printf("\n[ BLACK TURN ]\n");
		}

		// 말 선택
		if (!selectPiece(turn, &sr, &sc)) {
			break;
		}

		//선택한 말의 이동 가능 범위 표시
		system("cls");
		printBoard(sr, sc, turn);

		//이동할 위치 입력
		while (1) {
			printf("이동할 위치 입력(종료: x): ");

			int input = inputPosition(&er, &ec);

			if (input == -1) {
				goto endGame;
			}
			if (input == 0) {
				continue;
			}
			result = movePiece(sr, sc, er, ec, turn);
			
			if (result != 0) {
				break;
			}
		}
		

		//턴 교대
		turn = 1 - turn;
	}

endGame:
	SetConsoleTextAttribute(hConsole, 7);
	printf("\n게임을 종료합니다.\n");

	return 0;
}

//체크, 체크 메이트, 캐슬링, 앙파상, 폰 승격 고려 필요

// 말 선택 방식 수정-> 키보드 방향키
// 만약 이동 가능 경로 1개이면 바로 버튼 누르고 이동 가능하게 수정
// 폰 공격 대상 생겼을 때 기존 앞 경로 삭제 필요

//백->흑 공격 범위일 시 색 바뀌어야 함

// 전체적으로 게임 종료 조건 수정-> 게임 끝 전까지는 종료되지 않도록 수정 필요

//게임 왜 끝 안남?

