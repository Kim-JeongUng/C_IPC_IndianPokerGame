#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <string.h>
#include <signal.h>

struct card {
	int value; // 카드 숫자 (1~13)
	char suit; // 카드 문양
};
struct gameInfo{ // manager와 player간 주고 받는 데이터 형식 
    struct card cards[52]; // 상대의 카드 정보
    int num_cards; // player가 현재 소유한 카드 개수 == 라운드 수 로 볼 수 있음
    int manager_pid; // 데이터를 보내는 프로세스의 pid
    int player_pid; // 데이터를 받는 프로세스의 pid
    int coins; //플레이어의 코인 수 
    int bettingCoins; //해당 턴에 배팅한 코인 확인 (player -> manager로만 전달)
    int callCoins; // 콜 코인 수 계산 (player1코인 - player2코인 manager->player로만 전달)
    int roundSumCoins; //라운드에서 사용된 총 코인수 계산
};

// 카드 정보
struct card cards[52];
struct card open_card;
int top = 0;

// 메세지 큐 key IDsSS
int msqid_p1_down; // manager -> player1
int msqid_p1_up; // player 1 -> manager
int msqid_p2_down;// manager -> player2
int msqid_p2_up; // player 1 -> manager2

// 플레이어들의 pid;
int p1_pid;
int p2_pid;


void make_cards(){
    // make cards
    printf("Make cards");
	for (int i=0;i<13;i++) {
		cards[i].value=i%13+1;
		cards[i].suit='c';
	}
	for (int i=0;i<13;i++) {
		cards[i+13].value=i%13+1;
		cards[i+13].suit='d';
	}
	for (int i=0;i<13;i++) {
		cards[i+26].value=i%13+1;
		cards[i+26].suit='h';
	}
	for (int i=0;i<13;i++) {
		cards[i+39].value=i%13+1;
		cards[i+39].suit='s';
	}
    for (int i=0;i<52;i++) {
		printf("(%c,%d) ", cards[i].suit,cards[i].value);
	}
    printf("\n\n");
}
void shuffle(struct card *cards, int num){
    srand(time(NULL));
    struct card temp;
    int randInt;
    printf("Shuffling the cards\n");
    for (int i=0; i<num-1; i++){
        randInt = rand() % (num-i) + i;
        temp = cards[i];
        cards[i] = cards[randInt];
        cards[randInt] = temp;
    }
    for (int i=0;i<52;i++) {
		printf("(%c,%d) ", cards[i].suit,cards[i].value);
	}
    printf("\n");
}

int main(void) {
    // 카드 생성
    make_cards();
    // 카드 셔플
    shuffle(cards, 52);
    printf("\n<---------cards setting-------------->\n");
    // message queue ID 확인
    key_t key_p1_down = 10001;
    key_t key_p1_up = 10002;
    key_t key_p2_down = 20001;
    key_t key_p2_up = 20002;
    // messge queue create
    if((msqid_p1_down=msgget(key_p1_down,IPC_CREAT|0666))==-1){return -1;}
    if((msqid_p1_up=msgget(key_p1_up,IPC_CREAT|0666))==-1){return -1;}
    if((msqid_p2_down=msgget(key_p2_down,IPC_CREAT|0666))==-1){return -1;}
    if((msqid_p2_up=msgget(key_p2_up,IPC_CREAT|0666))==-1){return -1;}
    // messge queue reset
    msgctl(key_p1_down, IPC_RMID, NULL);
    msgctl(key_p1_up, IPC_RMID, NULL);
    msgctl(key_p1_down, IPC_RMID, NULL);
    msgctl(key_p2_up, IPC_RMID, NULL);
    // messge queue create
    if((msqid_p1_down=msgget(key_p1_down,IPC_CREAT|0666))==-1){return -1;}
    if((msqid_p1_up=msgget(key_p1_up,IPC_CREAT|0666))==-1){return -1;}
    if((msqid_p2_down=msgget(key_p2_down,IPC_CREAT|0666))==-1){return -1;}
    if((msqid_p2_up=msgget(key_p2_up,IPC_CREAT|0666))==-1){return -1;}

    struct gameInfo send_p1; // 플레이어 1에게 전달할 정보
    struct gameInfo send_p2; // 플레이어 2에게 전달할 정보
    struct gameInfo receive_p1; // 플레이어 1에게 전달받을 정보
    struct gameInfo receive_p2; // 플레이어 2에게 전달받을 정보
    // top에 위치한 카드 인덱스 표시
    top = 0;
    // 각 플레이어에게 전달할 초기 게임 정보 생성
    //player 1
    send_p1.num_cards = 0;
    send_p1.manager_pid = getpid();
    send_p1.player_pid = -1;

    send_p1.cards[top]= cards[top];
    send_p1.num_cards ++;
    top ++;

    send_p1.coins = 30;
    send_p1.bettingCoins=0;
    send_p1.callCoins=0;
    send_p1.roundSumCoins=0;

    // player 2
    send_p2.num_cards = 0;
    send_p2.manager_pid = getpid();
    send_p2.player_pid = -1;

    int i = 0;
    send_p2.cards[i]= cards[i+top];
    send_p2.num_cards ++;   
    top ++;

    send_p2.coins = 30;
    send_p2.bettingCoins=0;
    send_p2.callCoins=0;
    send_p2.roundSumCoins=0;

    // 게임 정보 전달
    printf("sending first game info\n");
    
    
    if(msgsnd(msqid_p1_down, &send_p1, sizeof(struct gameInfo), 0)==-1){return 0;};
    if(msgsnd(msqid_p2_down, &send_p2, sizeof(struct gameInfo), 0)==-1){return 0;};
    // 플레이어의 정보 수신 --> 플레이어의 pid 저장
    if(msgrcv(msqid_p1_up, &receive_p1, sizeof(struct gameInfo), 0, 0)==-1){return 1;};
    if(msgrcv(msqid_p2_up, &receive_p2, sizeof(struct gameInfo), 0, 0)==-1){return 1;};
    printf("receive the players pids\n");
    p1_pid = receive_p1.player_pid;
    p2_pid = receive_p2.player_pid;

    int callCoin=0;

    // 게임 시작.
    while(1){
        ////////////////////////////////////////////////////////////////////////
        // Player 1의 차례//////////////////////////////////////////////////////
        //////////////////////////////////////////////////////////////////////


        // 1. 게임 종료 판단
        if (send_p1.coins <= 0)
        {
            kill(p1_pid, SIGQUIT); // 패배
            kill(p2_pid, SIGINT);  // 승리
            printf("Player 2 Win\n");
            return (9);
        }
        if (top >= 51){
            //카드 다씀, 무승부
            kill(p2_pid, SIGILL);
            kill(p1_pid, SIGILL); 
        }

        kill(p1_pid, SIGUSR1);
        printf("Player 1's turn\n");
        // 2. 플레이어 1의 게임 정보 수신
        if(msgsnd(msqid_p1_down, &send_p1, sizeof(struct gameInfo), 0)==-1){return 0;};


        if(msgrcv(msqid_p1_up, &receive_p1, sizeof(struct gameInfo), 0, 0)==-1){return 1;};
        if(receive_p2.roundSumCoins< 0 ||receive_p2.roundSumCoins>100 )
            receive_p2.roundSumCoins = 0;

        send_p1.roundSumCoins = receive_p1.roundSumCoins;
        send_p1.coins = receive_p1.coins;

        if(receive_p1.bettingCoins == 0){           //die
            printf("player1 DIE\n");
            if(receive_p2.roundSumCoins == 0)
                send_p2.coins += 1;
            else 
                send_p2.coins += (send_p1.roundSumCoins + send_p2.roundSumCoins+2);
            send_p1.roundSumCoins = 0;
            send_p2.roundSumCoins = 0;
            
            //카드 업데이트
            send_p1.num_cards = receive_p1.num_cards+1;
            send_p1.cards[send_p1.num_cards-1] = cards[top];
            top ++;
            send_p2.num_cards = receive_p2.num_cards+1;
            send_p2.cards[send_p2.num_cards-1] = cards[top];
            top ++;
        }
        else if (send_p1.callCoins == receive_p1.bettingCoins) //call
        {
            if(send_p1.cards[send_p1.num_cards-1].value < send_p2.cards[send_p2.num_cards-1].value){
                printf("player1 Round Win! get %d coins\n",send_p1.roundSumCoins +1);//player 1 round win
                send_p1.coins += (send_p1.roundSumCoins +  send_p2.roundSumCoins+2);
            }
            else if(send_p1.cards[send_p1.num_cards-1].value > send_p2.cards[send_p2.num_cards-1].value){
                printf("player2 Round Win! get %d coins\n",send_p1.roundSumCoins +1);//player 2 round win
                send_p2.coins += (send_p2.roundSumCoins +  send_p2.roundSumCoins+2);
            }
            else if(send_p1.cards[send_p1.num_cards-1].value == send_p2.cards[send_p2.num_cards-1].value){ 
                printf("Draw Round!\n");
                send_p1.coins += send_p1.roundSumCoins+1;
                send_p2.coins += send_p2.roundSumCoins+1;
            }
            //player2의 정보를 넘겨줌 (카드 확인)
            if(msgsnd(msqid_p1_down, &send_p2, sizeof(struct gameInfo), 0)==-1){return 0;};

            send_p1.roundSumCoins = 0;
            send_p2.roundSumCoins = 0;

            //카드 업데이트
            send_p1.num_cards = receive_p1.num_cards+1;
            send_p1.cards[send_p1.num_cards-1] = cards[top];
            top ++;
            send_p2.num_cards = receive_p2.num_cards+1;
            send_p2.cards[send_p2.num_cards-1] = cards[top];
            top ++;
            
        }
        send_p2.callCoins = abs(send_p1.roundSumCoins - send_p2.roundSumCoins);


        ////////////////////////////////////////////////////////////////////////
        // Player 2의 차례//////////////////////////////////////////////////////
        ///////////////////////////////////////////////////////////////////////

        //1. 게임 종료 판단
        if (send_p2.coins <= 0)
        {
            kill(p2_pid, SIGQUIT); // 패배
            kill(p1_pid, SIGINT);  // 승리
            printf("Player 1 Win\n");
            return (9);
        }
        if (top >= 51){
            //카드 다씀, 무승부
            kill(p2_pid, SIGILL);
            kill(p1_pid, SIGILL); 
        }

        kill(p2_pid, SIGUSR1);
        printf("Player 2's turn\n");
        // 2. 플레이어 1의 게임 정보 수신
        if(msgsnd(msqid_p2_down, &send_p2, sizeof(struct gameInfo), 0)==-1){return 0;};

        if(msgrcv(msqid_p2_up, &receive_p2, sizeof(struct gameInfo), 0, 0)==-1){return 1;}
        if(receive_p1.roundSumCoins< 0 ||receive_p1.roundSumCoins>100 )
            receive_p1.roundSumCoins = 0;

        send_p2.roundSumCoins = receive_p2.roundSumCoins;
        send_p2.coins = receive_p2.coins;


        if(receive_p2.bettingCoins == 0){
            printf("player2 die\n");
            if(receive_p1.roundSumCoins = 0)
                send_p1.coins += 1;
            else
                send_p1.coins += (send_p1.roundSumCoins + send_p2.roundSumCoins+2);
            send_p1.roundSumCoins = 0;
            send_p2.roundSumCoins = 0;

            //카드 업데이트
            send_p1.num_cards = receive_p1.num_cards+1;
            send_p1.cards[send_p1.num_cards-1] = cards[top];
            top ++;
            send_p2.num_cards = receive_p2.num_cards+1;
            send_p2.cards[send_p2.num_cards-1] = cards[top];
            top ++;
        }
        else if (send_p2.callCoins == receive_p2.bettingCoins) //call
        {
            if(send_p1.cards[send_p1.num_cards-1].value < send_p2.cards[send_p2.num_cards-1].value){
                printf("player1 Round Win! get %d coins\n",send_p1.roundSumCoins+1);//player 1 round win
                send_p1.coins += (send_p1.roundSumCoins +  send_p2.roundSumCoins+2);
            }
            else if(send_p1.cards[send_p1.num_cards-1].value > send_p2.cards[send_p2.num_cards-1].value){
                printf("player2 Round Win! get %d coins\n",send_p1.roundSumCoins+1);//player 2 round win
                send_p2.coins += (send_p2.roundSumCoins +  send_p2.roundSumCoins+2);
            }
            else if(send_p1.cards[send_p1.num_cards-1].value == send_p2.cards[send_p2.num_cards-1].value){ 
                printf("Draw Round!\n");
                send_p1.coins += send_p1.roundSumCoins+1;
                send_p2.coins += send_p2.roundSumCoins+1;
            }
            
            //player1의 정보를 넘겨줌 (카드 확인)
            if(msgsnd(msqid_p2_down, &send_p1, sizeof(struct gameInfo), 0)==-1){return 0;};


            send_p1.roundSumCoins = 0;
            send_p2.roundSumCoins = 0;

            //카드 업데이트
            send_p1.num_cards = receive_p1.num_cards+1;
            send_p1.cards[send_p1.num_cards-1] = cards[top];
            top ++;
            send_p2.num_cards = receive_p2.num_cards+1;
            send_p2.cards[send_p2.num_cards-1] = cards[top];
            top ++;
        }
        send_p1.callCoins = abs(send_p1.roundSumCoins - send_p2.roundSumCoins);     

        
    }
    return 9;
}
