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
    struct card cards[52]; // player가 현재 소유한 카드 정보
    int num_cards; // player가 현재 소유한 카드 개수
    int manager_pid; // 데이터를 보내는 프로세스의 pid
    int player_pid; // 데이터를 받는 프로세스의 pid
    int coins;
    int bettingCoins;
    int callCoins;
    int roundSumCoins;
};

void my_turn(int sig){
    printf("Its your turn\n");
}
void win_sig(int sig){
    printf("You are the winner!\n");
    exit(0);
}
void lose_sig(int sig){
    printf("You are the loser...\n");
    exit(0);
}
void tie_sig(int sig){
    printf("Its tie....\n");
    exit(0);
}
int main(void) {
    // message queue setting
    struct gameInfo my_info;
    struct gameInfo received_info;
    int msqid_down;
    int msqid_up;
    int temp_coin;
    key_t key_down = 20001;
    key_t key_up = 20002;
    if((msqid_down=msgget(key_down,IPC_CREAT|0666))==-1){return -1;}  

    if((msqid_up=msgget(key_up,IPC_CREAT|0666))==-1){return -1;}
    // 초기 정보 수신
    if(msgrcv(msqid_down, &my_info, sizeof(struct gameInfo), 0, 0)==-1){return 1;}
               
    my_info.player_pid = getpid();  
    // 나의 프로세스 id 전달
    if(msgsnd(msqid_up, &my_info, sizeof(struct gameInfo), 0)==-1){return 0;}
    ////////////////////////////////////////////////////////////////////////////////////////////////////
    ////////play game//////////////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////////////////////////////
    printf("----------Indian Poker Game Starts-----------\n");
    printf(" 룰 설명 : 인디언포커게임은 나의 패는 공개하지 않은 채 상대의 패를 보고 배팅하는 게임입니다. 콜을 받아 최종 패를 오픈했을 때 내 패가 상대 패보다 높을경우 해당 라운드를 이깁니다.\n");
    printf(" 0코인을 배팅하여 다이할 수 있고, 라운드별 내가 낸 금액의 총합과 상대가 낸 총액의 차이인 콜 금액을 내서 카드를 오픈하고 다음 카드를 받을 수 있습니다.\n");
    printf(" 카드가 주어질 때 기본배팅 코인은 1 코인이고, 승자가 기본배팅코인과 패자의 총 배팅금액까지 가져갑니다.");
    printf("카드가 다 떨어지거나, 한 플레이어의 보유 코인이 0일 경우 게임이 끝납니다.\n\n");
    signal(SIGINT, win_sig); // 승리 시그널
    signal(SIGQUIT, lose_sig); //패배 시그널
    signal(SIGILL, tie_sig); //패배 시그널
    
    my_info.num_cards = 0;
    while(1){
        // 시그널 올 때까지 대기
        signal(SIGUSR1, my_turn);
        pause();
        // 현재 정보 수신
        if(msgrcv(msqid_down, &received_info, sizeof(struct gameInfo), 0, 0)==-1){return 1;}
        if(my_info.num_cards != received_info.num_cards || received_info.callCoins== 0){
            my_info = received_info;
            printf("\n-------------새로운 카드를 받았습니다.------------\n");
            //배팅전 코인 저장
            temp_coin = my_info.coins;

            received_info.coins -= 1;
            printf("기본배팅 완료(1코인)\n");
        }
        my_info = received_info;
        
        printf("상대방의 카드 : (%c,%d)", my_info.cards[my_info.num_cards-1].suit, my_info.cards[my_info.num_cards-1].value);    
        printf("\n나의 보유 코인 :  %d",my_info.coins);
        printf("\n콜 코인 :  %d\n",my_info.callCoins);
        while(1){
            printf("Betting : ");
            scanf("%d", &my_info.bettingCoins);
            if(my_info.bettingCoins >= my_info.callCoins && my_info.bettingCoins <= my_info.coins){
                break; // raise
            }
            else if(my_info.bettingCoins == 0){//DIE
                break;
            }
            else if(my_info.bettingCoins < my_info.callCoins){
                printf("배팅금액은 콜 코인과 같거나 커야하고 또는 0을 입력해 다이 할 수 있습니다.\n");
            }
            else
                printf("%d코인을 초과할 수 없습니다.\n",my_info.coins);
        }
        my_info.roundSumCoins += my_info.bettingCoins;
        my_info.coins -= my_info.bettingCoins;

        if(my_info.bettingCoins == 0){
            printf("--------------DIE---You lost %d Coins--------------\n",my_info.roundSumCoins+1);
        }
        else if(my_info.bettingCoins == my_info.callCoins){
            printf("------------------------Call----------------------\n");
            
        }
        else if(my_info.coins == 0){
            printf("----------------------All In----------------------\n");
        }
        else{
            printf("----------------------Raise-----------------------\n");
        }

        if(msgsnd(msqid_up, &my_info, sizeof(struct gameInfo), 0)==-1){return 0;}


        if(my_info.bettingCoins == my_info.callCoins &&  my_info.bettingCoins!=0){
            if(msgrcv(msqid_down, &received_info, sizeof(struct gameInfo), 0, 0)==-1){return 1;}
            printf("나의 카드 : (%c,%d)\n",received_info.cards[received_info.num_cards-1].suit, received_info.cards[received_info.num_cards-1].value);
            if(my_info.cards[my_info.num_cards-1].value < received_info.cards[received_info.num_cards-1].value){
                printf("이겼습니다. ");
                printf("%d코인을 얻었습니다. \n", my_info.roundSumCoins + received_info.roundSumCoins+2);
            }
            else if(my_info.cards[my_info.num_cards-1].value > received_info.cards[received_info.num_cards-1].value){
                printf("졌습니다. ");
                printf("%d코인을 잃었습니다. \n", my_info.roundSumCoins+1);
            }
            if(my_info.cards[my_info.num_cards-1].value == received_info.cards[received_info.num_cards-1].value){
                printf("비겼습니다. ");
                printf("%d코인을 돌려받았습니다. \n", my_info.roundSumCoins+1);
            }
        }
        
        printf("Your turn is over. Waiting for the next turn.\n");
        printf("--------------------------------------------------\n");
    }
    return 0;
}

