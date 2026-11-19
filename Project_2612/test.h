
#ifndef TEST_H
#define TEST_H

// 게임 기본 설정
#define SIZE 13              // 게임판 크기 (13×13)
#define PLAYER_COUNT 2       // 플레이어 수 (2인용)
#define HAND_SIZE 3          // 플레이어가 기본적으로 보유하는 이동 카드 수
#define CARD_COUNT 5         // 이동 카드 종류 (기본 이동 포함)

// 게임판 지형 종류
#define EMPTY 0              // 일반 이동 가능 칸
#define WALL 1               // 이동할 수 없는 장애물
#define MINE 2               // 금광 후보 지역
#define SEALED 3             // 이동할 수 없는 중앙 봉인 구역
#define ENTRANCE 4           // 광산 입구 (추후 탈출구로 활용)

// 이동 카드 종류
#define CARD_STEP 0          // 기본 이동: 상하좌우 1칸
#define CARD_DASH 1          // 질주: 상하좌우 2칸
#define CARD_DIAGONAL 2      // 대각선 이동: 대각선 1칸
#define CARD_LONG_DIAGONAL 3 // 긴 대각선 이동: 대각선 2칸
#define CARD_JUMP 4          // 도약: 중간 장애물을 뛰어넘어 직선 2칸

// 숨겨진 이벤트 종류 (해당 위치를 탐색하기 전에는 공개되지 않음)
#define EVENT_NONE 0         // 이벤트 없음
#define EVENT_TRAP 1         // 함정: 플레이어에게 피해 발생
#define EVENT_PICKAXE 2      // 곡괭이: 금광 채굴 효율 증가
#define EVENT_BAG 3          // 가방: 금광에서 획득하는 보물량 증가
#define EVENT_SCANNER 4      // 탐지기: 주변에 숨겨진 이벤트 확인
#define EVENT_HOURGLASS 5    // 모래시계: 탐험 가능 라운드 증가
#define EVENT_EXTRA_CARD 6   // 추가 카드: 이동 카드 선택권 증가
#define EVENT_REROLL 7       // 카드 교환권: 보유 이동 카드 재추첨
#define EVENT_GEM 8          // 보석: 추가 골드 획득
#define EVENT_HEAL 9         // 회복: 플레이어 체력 회복

// 금광 종류
#define MINE_UNSET 0         // 금광 종류가 지정되지 않은 상태
#define MINE_MAJOR 1         // 대형 금광
#define MINE_MEDIUM 2        // 중형 금광
#define MINE_SMALL 3         // 소형 금광
#define MINE_FAKE 4          // 가짜 금광

// 게임판 각 칸의 정보를 저장하는 구조체
typedef struct {
    int terrain;             // 지형 종류 (일반 칸, 장애물, 금광 등)
    int visited;             // 플레이어가 해당 칸을 방문했는지 여부
    int revealed;            // 숨겨진 이벤트가 공개되었는지 여부
    int event;               // 해당 칸에 배치된 이벤트 종류
    int resolved;            // 이벤트가 이미 처리되었는지 여부
    int mineType;            // 금광 종류 (대형, 중형, 소형, 가짜)
} Cell;

// 플레이어의 상태 및 보유 아이템을 저장하는 구조체
typedef struct {
    int row;                 // 플레이어의 현재 행 위치
    int col;                 // 플레이어의 현재 열 위치
    int character;           // 선택한 캐릭터 종류
    int hand[HAND_SIZE];     // 현재 보유 중인 이동 카드

    // 플레이어 기본 상태
    int hp;                  // 현재 체력
    int gold;                // 획득한 골드

    // 채굴 관련 아이템
    int pickaxe;             // 곡괭이 보유 상태
    int bag;                 // 가방 보유 상태

    // 탐험 및 이동 관련 아이템
    int scanner;             // 보유 중인 탐지기 수
    int extraCards;          // 추가 카드 선택권 수
    int rerolls;             // 카드 재추첨 가능 횟수

    // 캐릭터 특수 능력
    int trapGuards;          // 함정 피해를 방어할 수 있는 횟수
} Player;

// 전역 변수
extern Cell board[SIZE][SIZE];       // 13×13 광산 게임판
extern Player players[PLAYER_COUNT]; // 두 플레이어의 정보
extern int currentTurn;              // 현재 플레이어 (0: P1, 1: P2)
extern int roundCount;               // 현재 진행 중인 라운드

// 게임판 관련 함수
void initBoard(void);                // 광산 게임판 및 지형 초기화
void printBoard(int cursorRow, int cursorCol, int cardType);
// 게임판과 플레이어 상태 출력, 선택 위치 및 이동 가능 위치 표시

// 캐릭터 관련 함수
void selectCharacters(void);         // 플레이어별 캐릭터 선택
const char* getCharacterName(int type); // 캐릭터 종류에 따른 이름 반환

// 이동 카드 관련 함수
const char* getCardName(int type);    // 이동 카드 종류에 따른 이름 반환
void initCards(void);                // 두 플레이어에게 이동 카드 초기 지급
void refillCard(int playerIndex, int slot);
// 사용한 이동 카드를 무작위로 다시 지급

// 이동 규칙 관련 함수
int inBoard(int row, int col);        // 지정한 위치가 게임판 범위 안인지 확인
int isPassable(int row, int col);     // 장애물 및 다른 플레이어의 위치 확인
int isValidMove(int sr, int sc, int er, int ec, int cardType);
// 선택한 이동 카드의 규칙에 따라 이동 가능 여부 판단

// 플레이어 입력 및 이동 관련 함수
int selectMoveCard(void);            // 사용할 이동 카드 선택
int selectDestination(int cardType, int* er, int* ec);
// 방향키를 이용해 이동 가능한 목적지 선택
void movePlayer(int er, int ec, int cardSlot);
// 플레이어 위치 변경 및 사용한 카드 보충

// 게임 진행 관련 함수
void initGame(void);                 // 게임판, 플레이어, 카드 등 게임 초기화
void playGame(void);                 // 턴 진행, 이동 처리 및 게임 종료 관리

#endif
