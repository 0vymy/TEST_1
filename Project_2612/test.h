#ifndef TEST_H
#define TEST_H

#define SIZE 6

//체스판
void initBoard();// 체스판, 말 초기화
void printBoard(int sr, int sc, int turn);// 체스판 출력

//체스 말 관련
const char* getSymbol(char piece);
int isMyPiece(char piece, int turn); //현재 플레이어 말인지 확인


//이동 규칙
int inBoard(int row, int col);
int absValue(int n);
int isPathClear(int sr, int sc, int er, int ec);
int isValidMove(int sr, int sc, int er, int ec, int turn); // 말별 이동 가능 여부 판단

// 입력 및 이동
int selectPiece(int turn, int* sr, int* sc); // 이동할 말 선택
int movePiece(int sr, int sc, int er, int ec, int turn); // 말 이동 및 공격
int inputPosition(int* row, int* col); // 좌표 입력

#endif