#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

//座標の表現
#define X 0
#define Y 1

//先手、後手の表現（先手が下になっているのであとで考慮が必要）
#define FIRST 1  //**
#define SECOND -1//**

//自分の手番
int myTURN; //コマンドライン引数で実行時にセット

//駒の表現（後手の駒は+11する）
#define OUT_OF_BOARD 100
#define EMPTY -1
#define OU 0
#define HI 1
#define KA 2
#define KI 3
#define GI 4
#define FU 5
#define RY 6 //ここから成り駒
#define UM 7
#define NG 9
#define TO 10
#define ENEMY 11


//盤面
typedef struct {
    int Board[7][7]; //盤面
    int Reserve[2][FU + 1]; //持ち駒。0が先手、1が後手、インデックスは駒名をそのまま入れる
} Kyokumen;

//盤面上の座標
typedef struct {
    int x, y;
} Point;

//指し手
typedef struct {
    Point from, to;
    unsigned int koma;
    bool promote; //成るかどうか
} Move;

int min(int a, int b)
{
    return a < b ? a : b;
}

int max(int a, int b)
{
    return a < b ? b : a;
}

//Kyokumenが等価か比較（変更前のReserveを使っています！！）
bool cmpKyokumen(Kyokumen K1, Kyokumen K2) {
    for (int i = 0; i < 7; i++) {
        for (int j = 0; j < 7; j++) {
            if (K1.Board[i][j] != K2.Board[i][j]) return false;
        }
    }

    for (int i = 0; i < 2; i++) {
        for (int j = 0; j < FU + 1; j++) {
            if (K1.Reserve[i][j] != K2.Reserve[i][j]) return false;
        }
    }

    return true;
}

//反則処理
void commitFoul(int turn) {
    if (turn == myTURN) printf("You Lose.\n");
    else printf("You Win.\n");
    exit(0);
}

//それは自分の駒か？
bool isSelf(int koma, int turn) {
    if (turn == FIRST){
        return (OU <= koma && koma <= TO); //**
    }else{
        return (OU + ENEMY <= koma && koma <= TO + ENEMY); //**
    }
}

//それは相手の駒か？
bool isEnemy(int koma, int turn) {
    int rev = (-1) * turn; //自分と相手の手番をひっくり返す //**
    return isSelf(koma, rev);
}

//その手は盤内に入ってるか？
bool isInBoard(Move t) {
    return !(t.from.x > 5 || t.from.y > 5 || t.to.x > 5 || t.to.y > 5 || t.from.x < 0 || t.from.y < 0 || t.to.x < 1 || t.to.y < 1); //(t.from.x, t.from.y) = (0,0)でも許される（持ち駒のfromを0として扱うことにする）
}

//移動できるか？
bool canJump(Move t, Kyokumen K, int turn) {
    int type = K.Board[t.from.x][t.from.y] % ENEMY; //駒の種類
    bool self = isSelf(K.Board[t.to.x][t.to.y], turn);
    int difx = t.to.x - t.from.x;
    int dify = t.to.y - t.from.y;

    switch (type)
    {
    case 0: //王
        if (abs(difx) <= 1 && abs(dify) <= 1 && !(difx == 0 && dify == 0) && !self) //移動範囲内、行き先に自分の駒がない
            return true;
        break;
    case 6: //龍
        //龍 = 王 + 飛車と考えて、まず王の処理
        if (abs(difx) <= 1 && abs(dify) <= 1 && !(difx == 0 && dify == 0) && !self) // 移動範囲内、行き先に自分の駒がない
            return true; //これに当てはまらなかったら飛車の処理へ
    case 1: //飛車
        if (difx == 0 && dify != 0) { //移動範囲内
            bool judge = true;

            if (self) judge = false; //行き先に自分の駒があれば移動不可

            for (int i = 1; i < abs(dify); i++) {
                if (K.Board[t.from.x][min(t.from.y, t.to.y) + i] != EMPTY) { //間に駒があれば
                    judge = false; //移動不可
                    break;
                }
            }
            return judge;
        }

        if (dify == 0 && difx != 0)
        { // 移動範囲内
            bool judge = true;

            if (self) judge = false; // 行き先に自分の駒があれば移動不可

            for (int i = 1; i < abs(difx); i++) {
                if (K.Board[min(t.from.x, t.to.x) + i][t.from.y] != EMPTY) { // 間に駒があれば
                    judge = false; // 移動不可
                    break;
                }
            }
            return judge;
        }
        break;
    case 7: //馬
        // 馬 = 王 + 角と考えて、まず王の処理
        if (abs(difx) <= 1 && abs(dify) <= 1 && !(difx == 0 && dify == 0) && !self) // 移動範囲内、行き先に自分の駒がない
            return true;                                                            // これに当てはまらなかったら角の処理へ
    case 2: //角
        if (abs(difx) == abs(dify) && difx != 0) { //移動範囲内
            bool judge = true;

            if (self) judge = false; //行き先に自分の駒があれば移動不可

            if (difx == dify) {
                for (int i = 1; i < abs(dify); i++) {
                    if (K.Board[min(t.from.x, t.to.x) + i][min(t.from.y, t.to.y) + i] != EMPTY) {//間に駒があれば
                        judge = false; //移動不可
                        break;
                    }
                }
                return judge;
            }

            if (difx == -dify) {
                for (int i = 0; i < abs(dify); i++) {
                    if (K.Board[min(t.from.x, t.to.x) + i][max(t.from.y, t.to.y) - i] != EMPTY) { //間に駒があれば
                        judge = false; //移動不可
                        break;
                    }
                }
                return judge;
            }
        }
        break;
    case 9: //成銀
    case 10: //と金
    case 3: //金
        if (turn == FIRST) { //先手
            if ((dify == 1 && (difx == 1 || difx == 0 || difx == -1)) ||
                (dify == 0 && (difx == 1 || difx == -1)) ||
                (dify == -1 && difx == 0) && !self)
                return true;
        } else { //後手
            if ((dify == -1 && (difx == 1 || difx == 0 || difx == -1)) ||
                (dify == 0 && (difx == 1 || difx == -1)) ||
                (dify == 1 && difx == 0) && !self)
                return true;
        }
        break;
    case 4: //銀
        if (turn == FIRST) { //先手
            if ((dify == 1 && (difx == 1 || difx == 0 || difx == -1)) ||
                (dify == -1 && (difx == 1 || difx == -1)) && !self)
                return true;
        } else { //後手
            if ((dify == -1 && (difx == 1 || difx == 0 || difx == -1)) ||
                (dify == 1 && (difx == 1 || difx == -1)) && !self)
                return true;
        }
        break;
    case 5: //歩
        if (turn == FIRST) { //先手
            if (t.to.y == 5 && t.promote != 1) //成らないのは反則
                break;
            if (dify == 1 && difx == 0 && !self)
                return true;
        } else { //後手
            if (t.to.y == 1 && t.promote != 1) //成らないのは反則
                break;
            if (dify == -1 && difx == 0 && !self)
                return true;
        }
        break;
    default:
        break;
    }

    return false;
}

//成れるか？
bool canPromote(Move t, Kyokumen K, int turn) {
    int type = K.Board[t.from.x][t.from.y] % ENEMY; //駒の種類
    bool cond = ((turn == FIRST && (t.from.y == 5 || t.to.y == 5)) || (turn == SECOND && (t.from.y == 1 || t.to.y == 1))); //盤面の制約 //**

    switch (type)
    {
    case 0:
        break;
    case 1:
        return cond;
    case 2:
        return cond;
    case 3:
        break;
    case 4:
        return cond;
    case 5:
        return cond;
    }

    return 0;
}

//自分が王手をかけられているか？
bool isWaiting(Kyokumen K, int turn, Move *res) {
    Move t;
    bool judge = false;
    int cnt = 0;

    //自分の王の場所を探す
    for (int i = 1; i <= 5; i++) {
        for (int j = 1; j <= 5; j++) {
            if (turn == FIRST && K.Board[i][j] == OU ) { //**
                t.to.x = i;
                t.to.y = j;
            }
            //**
            if (turn == SECOND && K.Board[i][j] == OU + ENEMY){
                t.to.x = i;
                t.to.y = j;
            }
            //**
        }
    }

    for (int i = 1; i <= 5; i++) {
        for (int j = 1; j <= 5; j++) {
            if (isEnemy(K.Board[i][j], turn)) { //相手の駒があるなら判定に入る
                t.from.x = i;
                t.from.y = j;
                t.promote = 1; //canJumpが歩の成る成らないの判定を含んでしまっているので、promote=1として回避（他の手には関係ない）
                if (canJump(t, K, (-1) * turn)) { //相手の駒が相手の手で自分の王に飛べるなら
                    res[cnt++] = t; //その手を記憶
                    judge =  true; //それは王手
                }
            }
        }
    }

    return judge;
}

//千日手判定
void repJudge(Kyokumen *K, int i, int turn, Move *res) {
    int cnt = 1;
    while (cmpKyokumen(K[i], K[i - 4])) {
        if (i < 4) break;
        cnt++;
        i -= 4;
        if (cnt == 4) { //千日手成立
            if (isWaiting(K[i], turn * (-1), res)) //連続王手のケース（自分が王手をかけ続けた）
                commitFoul(turn); //自分の反則
            if (isWaiting(K[i + 1], turn, res)) //連続王手のケース（自分が王手から逃げ続けた）
                commitFoul(turn * (-1)); //相手の反則
            else commitFoul(FIRST); //通常の千日手は先手の反則
        }
    }
}

//勝敗判定（王手をかけられたとき、それを回避する手を打たなかったら負け）
void winJudge(Kyokumen K, int turn, Move *res) {
    if (isWaiting(K, turn, res)) commitFoul(turn);
}

// 指し手の入力をMove型に変換（反則の判定は別の関数でやる）
void makeInstruction(const char *s, Move *t)
{
    if (!(s[2] >= '0' && s[2] <= '9'))
    {
        t->from.x = 0; // 駒を打つときの表示
        t->from.y = 0;
        t->to.y = (int)(s[0] - '0');     // 1オリジン
        t->to.x = (int)(s[1] - 'A' + 1); // 1オリジン
        t->koma = EMPTY;
        switch (s[2])
        {
        case 'H':
            t->koma = HI;
            break;
        case 'K':
            if (s[3] == 'K')
                t->koma = KA;
            else if (s[3] == 'I')
                t->koma = KI;
            break;
        case 'G':
            t->koma = GI;
            break;
        case 'F':
            t->koma = FU;
            break;
        default: // 反則判定に使う？
            break;
        }
    }
    else
    { // 移動するとき
        t->from.y = (int)(s[0] - '0');
        t->from.x = (int)(s[1] - 'A' + 1);
        t->to.y = (int)(s[2] - '0');
        t->to.x = (int)(s[3] - 'A' + 1);
        if (s[4] == 'N')
            t->promote = 1;
        else
            t->promote = 0;
    }
}

//駒の移動、反則判定
Kyokumen makeMove(const char *s, Move *t, Kyokumen K, int turn) {
    Kyokumen n_K = K;
    makeInstruction(s, t);

    if (!isInBoard(*t)) //盤の外を使っている
        commitFoul(turn);

    if (t->from.x == 0) { //駒を打つとき
        if (t->koma == EMPTY) //存在しない駒（反則）
            commitFoul(turn);
        if (turn == FIRST && K.Reserve[0][t->koma] == 0) { //その駒は持っていない（反則）//**
            commitFoul(turn);
        } else if (turn == SECOND && K.Reserve[1][t->koma] == 0) { //**
            commitFoul(turn); //**
        } else if (K.Board[t->to.x][t->to.y] != EMPTY) { //すでに駒がある（反則）
            commitFoul(turn);
        } else {
            if (t->koma == FU) { // 二歩の判定
                for (int i = 0; i < 5; i++) {
                    if (turn == FIRST && K.Board[t->to.x][i] == FU) // 二歩 //**
                        commitFoul(turn);
                    if (turn == SECOND && K.Board[t->to.x][i] == FU + ENEMY){ //**
                        commitFoul(turn); //**
                    }
                }
            }
            //**
            if (turn == FIRST){
                n_K.Reserve[0][t->koma]--;                           // 駒を使用
                n_K.Board[t->to.x][t->to.y] = t->koma;               // 駒を置く
            }else{
                n_K.Reserve[1][t->koma]--;                           
                n_K.Board[t->to.x][t->to.y] = t->koma + ENEMY;
            }
            //**
        }
    } else { //駒を動かすとき
        if (!isSelf(K.Board[t->from.x][t->from.y], turn)) //そこに自分の駒はない（反則）
            commitFoul(turn);
        else if (!canJump(*t, K, turn)) { //その場所には動けない（反則）
            commitFoul(turn);
        } else {
            n_K.Board[t->from.x][t->from.y] = EMPTY;
            if (K.Board[t->to.x][t->to.y] != EMPTY) { //行き先に相手の駒があれば（自分の駒がないことはcanJumpで保証）
                //**
                if (turn == FIRST){
                    n_K.Reserve[0][((K.Board[t->to.x][t->to.y] % ENEMY) - FU > 0) ? ((K.Board[t->to.x][t->to.y] % ENEMY)- FU) : K.Board[t->to.x][t->to.y] % ENEMY]++; //持ち駒に追加（成り駒だったらもとに戻す、相手の駒だったら自分の駒にする）
                } else {
                    n_K.Reserve[1][((K.Board[t->to.x][t->to.y] % ENEMY) - FU > 0) ? ((K.Board[t->to.x][t->to.y] % ENEMY)- FU) : K.Board[t->to.x][t->to.y] % ENEMY]++;
                }
                //**
            }
            if (t->promote == 1) { //成る
                if (canPromote(*t, K, turn)) {//合法
                    n_K.Board[t->to.x][t->to.y] = K.Board[t->from.x][t->from.y] + 5;
                } else {
                    commitFoul(turn);
                }  
            } else { //成らない
                n_K.Board[t->to.x][t->to.y] = K.Board[t->from.x][t->from.y]; // 駒を置く
            }
        }
    }

    return n_K;
}

//盤面の初期化
Kyokumen initBoard() {
    Kyokumen K;
    //3重に塗りつぶして盤面を作成（OUT_OF_BOARD->EMPTY->駒）
    for (int i = 0; i < 7; i++) {
        for (int j = 0; j < 7; j++) {
            K.Board[i][j] = OUT_OF_BOARD;
        }
    }

    for (int i = 1; i <= 5; i++) {
        for (int j = 1; j <= 5; j++) {
            K.Board[i][j] = EMPTY;
        }
    }

    K.Board[1][1] = OU;
    K.Board[2][1] = KI;
    K.Board[3][1] = GI;
    K.Board[4][1] = KA;
    K.Board[5][1] = HI;
    K.Board[1][2] = FU;
    K.Board[5][4] = FU + ENEMY;
    K.Board[1][5] = HI + ENEMY;
    K.Board[2][5] = KA + ENEMY;
    K.Board[3][5] = GI + ENEMY;
    K.Board[4][5] = KI + ENEMY;
    K.Board[5][5] = OU + ENEMY;

    //持ち駒はなし
    for (int i = 0; i < 2; i++) {
        for (int j = 0; j <= FU; j++) {
            K.Reserve[i][j] = 0;
        }
    }

    return K;
}

//盤面の表示（テストのときなどに）
void printBoard(Kyokumen K, int turn, bool w) {
    char *komaCh[2][12] = {{" 王", " 飛", " 角", " 金", " 銀", " 歩", " 龍", " 馬", "", " 全", " と", "   "},
                        {"e王", "e飛", "e角", "e金", "e銀", "e歩", "e龍", "e馬", "", "e全", "eと", "   "} };
    char *turnCh[2] = {"先手", "後手"};

    //盤面の表示
    puts("   A   B   C   D   E  ");
    puts(" +---+---+---+---+---+");
    for (int i = 5; i >= 1; i--) {
        printf("%d|", i);
        for (int j = 1; j <= 5; j++) {
            printf("%s|", komaCh[((K.Board[j][i] >= ENEMY) ? 1 : 0)][(K.Board[j][i] == EMPTY) ? 11 : (K.Board[j][i] % ENEMY)]);
        }
        putchar('\n');
        puts(" +---+---+---+---+---+");
    }

    //持ち駒の表示
    puts("持ち駒");
    for (int i = 0; i < 2; i++) {
        printf("%s:", turnCh[i]);
        for (int j = 0; j <= FU; j++) {
            int cnt = K.Reserve[i][j];
            while (cnt > 0) {
                printf("%s", komaCh[0][j]);
                cnt--;
            }
        }
        putchar('\n');
    }

    //手番の表示（王手をかけられているならそれも表示）
    //**
    if (turn == FIRST){
        printf("NEXT: %s %s\n", turnCh[0], w ? "王手" : "");
    }else{
        printf("NEXT: %s %s\n", turnCh[1], w ? "王手" : "");
    }
    //**
}

int main(int argc, char *argv[]) {
    myTURN = (atoi(argv[1]) == 0 ? 1 : -1); //実行時引数が0なら1（先手）、それ以外なら-1（後手）に変換
    Kyokumen K[200]; //盤面
    Move t[200]; //150手分あればよいが、少し多めに用意
    Move res[2][20]; //王手の保存用（勝利判定に使う）。res[0]には先手が王手をかけられている手（つまり後手が指すと後手の勝ちになる手）が入るので注意。
    int turn = FIRST; //手番を先手に設定
    int i = 0;
    bool w[2] = {false, false}; //w[0]には先手が王手をかけられているか？を示すbool値が入る

    K[0] = initBoard(); //初期盤面を作成
    printBoard(K[0], turn, w[turn]); //初期盤面の表示

    //resの初期化
    for (int i = 0; i < 2; i++) {
        for (int j = 0; j < 20; j++) {
            res[i][j].from.x = 0;
            res[i][j].from.y = 0;
            res[i][j].to.x = 0;
            res[i][j].to.y = 0;
            res[i][j].promote = 0;
        }
    }

    while (i < 150) {
        char s[10];
        scanf("%s", s); //指し手の入力
        K[i + 1] = makeMove(s, &t[i], K[i], turn); //指し手を実行して新しい盤面に書き込み
        i++;
        repJudge(K, i, turn, res[turn]); //千日手判定
        winJudge(K[i], turn, res[turn]); //勝敗判定
        turn = (-1) * turn;               //手番を更新
        w[turn] = isWaiting(K[i], turn, res[turn]); //王手の判定
        printBoard(K[i], turn, w[turn]);
    }

    return 0;
}