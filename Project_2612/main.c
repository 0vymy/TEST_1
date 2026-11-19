
#include <stdio.h>     // 표준 입출력 함수 사용
#include <stdlib.h>    // 난수 생성 관련 함수 사용
#include <time.h>      // 현재 시간을 이용한 난수 초기화
#include <Windows.h>   // Windows 콘솔 관련 기능 사용
#include "test.h"      // 게임 관련 함수 및 구조체 선언

int main(void) {

    // 콘솔 입출력 인코딩을 UTF-8로 설정 (한글 및 특수기호 출력)
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    // 현재 시간을 기준으로 난수 초기화 (게임 실행마다 다른 카드 및 이벤트 생성)
    srand((unsigned int)time(NULL));

    // 게임 초기화 (게임판, 플레이어, 이동 카드 등 설정)
    initGame();

    // 게임 시작 및 진행
    playGame();

    return 0;
}
