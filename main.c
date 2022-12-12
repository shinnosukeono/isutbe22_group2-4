#include<stdio.h>

//はじめに
//先手、後手を数値で表した際は断らない限り、先手->1,後手->-1で表す。
//盤面を数値で表した際は断らない限り、以下の対応を言う。
//5| 20 21 22 23 24
//4| 15 16 17 18 19
//3| 10 11 12 13 14
//2| 5  6  7  8  9
//1| 0  1  2  3  4
// |________________
//   A  B  C  D  E
//先手の持ち駒->27,後手の持ち駒->25 (26+(先手or後手)で計算できる)

//補助的な数学的関数
int abs(int x){
    return x>0?x:-x;
}

int max(int a,int b){
    return a<b?b:a;
}

int sign(int x){
    if(x>0) return 1;
    else if(x==0) return 0;
    else return -1;
}

//駒と盤面

//盤面、初期化済み
//0->そのマスに何もない。それ以外の数字->駒のcode(後述)
int board[25]={
    11,5,3,7,9,
    1,0,0,0,0,
    0,0,0,0,0,
    0,0,0,0,2,
    10,8,4,6,12
};

//駒を表す構造体
//type->駒の種類,cor->駒の位置,own->駒の所有者,prom->成ってるかどうか
//code->各駒を識別する用のコード、設定後変更しない。
//typeの対応は下記の通り
//歩->0,銀->1,金->2,角->3,飛->4,王->5
//cor,ownは盤面、先手後手に対応
//promは成っていれば1,そうでなければ0
//codeは歩には1,2,銀には3,4,金には5,6,角には7,8,飛車には9,10,王には11,12を割り当てた。
typedef struct piece{
    int type;
    int cor;
    int own;
    int prom;
    int code;
}piece;

//駒をインスタンス化
piece fu1,fu2,gi1,gi2,ki1,ki2,kk1,kk2,hi1,hi2,ou1,ou2;

//pieceの初期化用
void init(piece *piece,int type,int cor,int own,int prom,int code){
    piece->type=type;
    piece->cor=cor;
    piece->own=own;
    piece->prom=prom;
    piece->code=code;
}

//インスタンスの初期化
void clear(){
    init(&fu1,0,5,1,0,1);
    init(&fu2,0,19,-1,0,2);
    init(&gi1,1,2,1,0,3);
    init(&gi2,1,22,-1,0,4);
    init(&ki1,2,1,1,0,5);
    init(&ki2,2,23,-1,0,6);
    init(&kk1,3,3,1,0,7);
    init(&kk2,3,21,-1,0,8);
    init(&hi1,4,4,1,0,9);
    init(&hi2,4,20,-1,0,10);
    init(&ou1,5,0,1,0,11);
    init(&ou2,5,24,-1,0,12);
}

//先手の駒の動ける位置を相対位置として列挙。後手は-1をかければよい。
int p1move[6][25]={
    {5},
    {6,5,4,-4,-6},
    {6,5,4,1,-1,-5},
    {0},
    {0},
    {-6,-5,-4,-1,1,4,5,6}
};

//codeを受け取って、対応するインスタンスを返す。
piece *koma(int code){
    switch (code){
        case 1: return &fu1;
        case 2: return &fu2;
        case 3: return &gi1;
        case 4: return &gi2;
        case 5: return &ki1;
        case 6: return &ki2;
        case 7: return &kk1;
        case 8: return &kk2;
        case 9: return &hi1;
        case 10: return &hi2;
        case 11: return &ou1;
        case 12: return &ou2;
    }
}

//駒を打つときの関数と変数
//以下度々用いるが、turn->先手or後手,type->手の種類
//typeの対応は以下の通り
//0: 不正な手, 1:盤上にあった駒の移動, 2:盤上にあった駒が移動して成り駒になる,3:持ち駒から手を打つ

//from->駒のもといた位置or持ち駒の場合は駒のcode
//to->駒の動きたい位置
int from,to;

//角の手が妥当かどうか。
int valid_kaku(){
    int fromh=from/5,fromc=from%5;
    int toh=to/5,toc=to%5;
    if(abs(fromh-toh)!=abs(fromc-toc)) return 1;
    for(int i=1;i<abs(toh-fromh);i++){
        if(board[(fromh+sign(toh-fromh)*i)*5+(fromc+sign(toc-fromc)*i)]!=0) return 1;
    }
    return 0;
}

//飛車の手が妥当かどうか。
int valid_hisya(){
    int fromh=from/5,fromc=from%5;
    int toh=to/5,toc=to%5;
    if(toh!=fromh && toc!=fromc) return 1;
    for(int i=1;i<max(abs(toh-fromh),abs(toc-fromc));i++){
        if(board[(fromh+sign(toh-fromh)*i)*5+fromc+sign(toc-fromc)*i]!=0) return 1;
    }
    return 0;
}

//typeが1or2のときの打った手がルールに則ているかどうか判定。
int valid_type12(int turn,int type){
    if(to==from) return 1;
    int code=board[from];
    if(code==0) return 1;
    piece *mover=koma(code);
    if(mover->own != turn) return 1;
    if(board[to]!=0){
        piece *p=koma(board[to]);
        if(p->own==mover->own) return 1;
    }
    if(type==2){
        if(to/5!=2+2*turn && from/5!=2+2*turn) return 1;
    }
    int flag=1;
    if((0<=mover->type && mover->type<3)||mover->type==6){
        if(mover->prom==1){
            for(int i=0;p1move[2][i]!=0;i++){
                if(to-from==(mover->own)*p1move[2][i]) flag=0;
            }
        }
        else{
            for(int i=0;p1move[mover->type][i]!=0;i++){
                if(to-from==(mover->own)*p1move[mover->type][i]) flag=0;
            }
        }
        return flag;
    }
    if(mover->type==3){
        if(mover->prom==1){
            for(int i=0;p1move[5][i]!=0;i++){
                if(to-from==(mover->own)*p1move[5][i]) flag=0;
            }           
        }
        if(flag==0) return 0;
        else return valid_kaku();
    }
    if(mover->type==4){
        if(mover->prom==1){
            for(int i=0;p1move[2][i]!=0;i++){
                if(to-from==(mover->own)*p1move[2][i]) flag=0;
            }
        }
        if(flag==0) return 0;
        return valid_hisya();
    }
}

//typeが3のときの手の妥当性を判定
int valid_type3(int turn){
    piece *piece=koma(from);
    if(piece->cor!=26+turn) return 1;
    if(board[to]!=0) return 1;
    if(piece->type==0 && to/5==2+2*turn) return 1;
    return 0;
}

//打った手が妥当かどうか
int valid(int turn,int type){
    if(type==0) return 0;
    if(type==3) return valid_type3(turn);
    else return valid_type12(turn,type);
}

//駒をとる関数
void take(int code){
    piece *killed=koma(code);
    board[killed->cor]=0;
    killed->own*=-1;
    killed->prom=0;
    killed->cor=26+killed->own;
}

//手を打つ関数
void move(piece *piece,int type){
    if(type==1 || type==2){
        board[from]=0;
        if(board[to]!=0) take(board[to]);
        board[to]=piece->code;
        piece->cor=to;
        if(type==2) piece->prom=1;
    }
    if(type==3){
        board[to]=piece->code;
        piece->cor=to;
    }
}

int nifu(){
    if(fu1.own==fu2.own && fu1.cor/5==fu2.cor/5) return 1;
    else return 0;
}
//入出力

//入力関数。入力の形式の妥当性も判断し、from,toに値を格納
int input(char *s,int turn){
    if(s[2]-'0'>=1 && s[2]-'0'<=5){
        from=(s[0]-'1')*5+(s[1]-'A');
        to=(s[2]-'1')*5+(s[3]-'A');
        if(from<0 || from>24 || to<0 || to>24) return 0;
        if(s[4]=='N') return 2;
        else return 1;
    }
    else{
        to=(s[0]-'1')*5+(s[1]-'A');
        from=(s[2]-'A')*26+(s[3]-'A');
        switch(from){
            case 150: from=1;
            case 164: from=3;
            case 268: from=5;
            case 270: from=7;
            case 190: from=9;
            default: return 0;
        }
        if(from%2 || from>10) return 0;
        piece *piece=koma(from);
        if(piece->cor==26+turn){
            if(piece->own!=turn) return 0;
            return 3;
        }
        else{
            from++;
            piece=koma(from);
            if(piece->cor!=26+turn) return 0;
            if(piece->own!=turn) return 0;
            return 3;
        }
    }
}

int main(){
    clear();
    int turn=1,type;
    while(1){
        char s[10];
        scanf("%s",s);
        type=input(s,turn);
        printf("%d\n",type);
        printf("%d %d\n",from,to);
        printf("%d %d\n",board[from],valid(turn,type));
        if(valid(turn,type)==1) return 1;

        piece *mover=koma(board[from]);
        move(mover,type);
        turn*=-1;
        if(nifu()){
            if(turn==1){
                puts("Player2 Win");
                return 0;
            }
            else if(turn==-1){
                puts("Player1 Win");
                return 0;
            }
        }
        if(ou1.own==-1){
            puts("Player1 Win");
            return 0;
        }
        else if(ou2.own==1){
            puts("Player2 Win");
            return 0;
        }
    }
}