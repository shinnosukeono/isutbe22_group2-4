#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

//座標の表現
#define X 0
#define Y 1

//先手、後手の表現（先手が下になっているのであとで考慮が必要）
#define FIRST 1  //**
#define SECOND -1//**

//手番
int myTURN; //コマンドライン引数で実行時にセット
int aiTURN;

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

//AI用
#define DEPTH 5        // 探索深さ
#define INFTY (1 << 8) // 極端に大きな数
int p[TO + 1] = {50, 11, 10, 9, 8, 1, 21, 20, 0, 9, 9};
int p_hand[FU + 1] = {50, 12, 11, 10, 9, 5};

//盤面
typedef struct {
    int Board[7][7]; //盤面
    int reserve_sente; //先手の持ち駒、2*(駒の番号),2*(駒の番号)+1 bit目に各駒の枚数を記録
    int reserve_gote; //後手の持ち駒、同上
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

Kyokumen possible_K[DEPTH + 1][1500];
Kyokumen trace_K[DEPTH + 1]; // 最適局面の保存
Move trace_M[DEPTH + 1];     // 最適手の保存
Move t[DEPTH + 1][1500];     // 可能手の保存

int min(int a, int b)
{
    return a < b ? a : b;
}

int max(int a, int b)
{
    return a < b ? b : a;
}

//Kyokumenが等価か比較
bool cmpKyokumen(Kyokumen *K1, Kyokumen *K2) {
    for (int i = 0; i < 7; i++) {
        for (int j = 0; j < 7; j++) {
            if (K1->Board[i][j] != K2->Board[i][j]) return false;
        }
    }

    bool res=true;
    res=res && (K1->reserve_sente==K2->reserve_sente);
    res=res && (K1->reserve_gote==K2->reserve_gote);

    return res;
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
bool canJump(Move t, Kyokumen *K, int turn) {
    int type = K->Board[t.from.x][t.from.y] % ENEMY; //駒の種類
    bool self = isSelf(K->Board[t.to.x][t.to.y], turn);
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
                if (K->Board[t.from.x][min(t.from.y, t.to.y) + i] != EMPTY) { //間に駒があれば
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
                if (K->Board[min(t.from.x, t.to.x) + i][t.from.y] != EMPTY) { // 間に駒があれば
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
                    if (K->Board[min(t.from.x, t.to.x) + i][min(t.from.y, t.to.y) + i] != EMPTY) {//間に駒があれば
                        judge = false; //移動不可
                        break;
                    }
                }
                return judge;
            }

            if (difx == -dify) {
                for (int i = 1; i < abs(dify); i++) {
                    if (K->Board[min(t.from.x, t.to.x) + i][max(t.from.y, t.to.y) - i] != EMPTY) { //間に駒があれば
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
            if (((dify == 1 && (difx == 1 || difx == 0 || difx == -1)) ||
                (dify == 0 && (difx == 1 || difx == -1)) ||
                (dify == -1 && difx == 0)) && !self)
                return true;
        } else { //後手
            if (((dify == -1 && (difx == 1 || difx == 0 || difx == -1)) ||
                (dify == 0 && (difx == 1 || difx == -1)) ||
                (dify == 1 && difx == 0)) && !self)
                return true;
        }
        break;
    case 4: //銀
        if (turn == FIRST) { //先手
            if (((dify == 1 && (difx == 1 || difx == 0 || difx == -1)) ||
                (dify == -1 && (difx == 1 || difx == -1))) && !self)
                return true;
        } else { //後手
            if (((dify == -1 && (difx == 1 || difx == 0 || difx == -1)) ||
                (dify == 1 && (difx == 1 || difx == -1))) && !self)
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
bool canPromote(Move t, Kyokumen *K, int turn) {
    int type = K->Board[t.from.x][t.from.y] % ENEMY; //駒の種類
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
bool isWaiting(Kyokumen *K, int turn) {
    Move t;
    bool judge = false;
    int cnt = 0;

    //自分の王の場所を探す
    for (int i = 1; i <= 5; i++) {
        for (int j = 1; j <= 5; j++) {
            if (turn == FIRST && K->Board[i][j] == OU ) { //**
                t.to.x = i;
                t.to.y = j;
            } else if (turn == SECOND && K->Board[i][j] == OU + ENEMY){
                t.to.x = i;
                t.to.y = j;
            }
            //**
        }
    }

    for (int i = 1; i <= 5; i++) {
        for (int j = 1; j <= 5; j++) {
            if (isEnemy(K->Board[i][j], turn)) { //相手の駒があるなら判定に入る
                t.from.x = i;
                t.from.y = j;
                t.promote = 1; //canJumpが歩の成る成らないの判定を含んでしまっているので、promote=1として回避（他の手には関係ない）
                if (canJump(t, K, (-1) * turn)) { //相手の駒が相手の手で自分の王に飛べるなら
                    judge =  true; //それは王手
                }
            }
        }
    }

    return judge;
}

//行きどころのない駒の有無の判定
void freezingFU_Judge(Kyokumen *K, int turn){
    if (turn == FIRST){
        for (int i=1; i<=5; i++){
            if (K->Board[i][5] == FU){
                commitFoul(turn);
            }
        }
    }else{
        for (int i=1; i<=5; i++){
            if (K->Board[i][1] == FU + ENEMY){
                commitFoul(turn);
            }
        }
    }
    return;
}

//千日手判定
void repJudge(Kyokumen *K, int i, int turn) {
    int cnt = 1;
    while (cmpKyokumen(K+i, K+i-4)) {
        if (i < 4) break;
        cnt++;
        i -= 4;
        if (cnt == 4) { //千日手成立
            if (isWaiting(K+i, turn * (-1))) //連続王手のケース（自分が王手をかけ続けた）
                commitFoul(turn); //自分の反則
            if (isWaiting(K+i+1, turn)) //連続王手のケース（自分が王手から逃げ続けた）
                commitFoul(turn * (-1)); //相手の反則
            else commitFoul(FIRST); //通常の千日手は先手の反則
        }
    }
}

//全探索（実装は560行目以降、windJudgeの内部で使うので先に宣言）
void fullSearch(Kyokumen *K, int turn, int *cnt, int depth);

//詰み（自分の勝ち）判定
void winJudge(Kyokumen *K, int turn) {
    bool judge = true;
    int cnt = 0;
    fullSearch(K, turn * (-1), &cnt, DEPTH);
    for (int i = 0; i < cnt; i++) {
        if (!isWaiting(&(possible_K[DEPTH][i]), turn * (-1))) { //相手が王手を回避できる局面が一つでもあるなら
            judge = false; //相手はまだ負けていない（自分はまだ勝っていない）
        }
    }

    if (judge) commitFoul(turn * (-1));
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
Kyokumen makeMove(Move *t, Kyokumen K, int turn) {
    if (!isInBoard(*t)) //盤の外を使っている
        commitFoul(turn);

    if (t->from.x == 0) { //駒を打つとき
        if (t->koma == EMPTY) //存在しない駒（反則）
            commitFoul(turn);
        if (turn == FIRST && ((K.reserve_sente)>>(2*(t->koma)))%4 == 0) { //その駒は持っていない（反則）//**
            commitFoul(turn);
        } else if (turn == SECOND && ((K.reserve_gote)>>(2*(t->koma)))%4 == 0) { //**
            commitFoul(turn); //**
        }
        if (K.Board[t->to.x][t->to.y] != EMPTY) { //すでに駒がある（反則）
            commitFoul(turn);
        } else {
            if (t->koma == FU) { // 二歩の判定
                for (int i = 0; i < 5; i++) {
                    if (turn == FIRST &K.Board[t->to.x][i] == FU) // 二歩 //**
                        commitFoul(turn);
                    if (turn == SECOND &K.Board[t->to.x][i] == FU + ENEMY){ //**
                        commitFoul(turn); //**
                    }
                }
            }
            //**
            if (turn == FIRST){
                K.reserve_sente-=(1<<(2*(t->koma)));                           // 駒を使用
                K.Board[t->to.x][t->to.y] = t->koma;               // 駒を置く
            }else{
                K.reserve_gote-=(1<<(2*(t->koma)));                           
                K.Board[t->to.x][t->to.y] = t->koma + ENEMY;
            }
            //**
        }
    } else { //駒を動かすとき
        if (!isSelf(K.Board[t->from.x][t->from.y], turn)) //そこに自分の駒はない（反則）
            commitFoul(turn);
        else if (!canJump(*t, &K, turn)) { //その場所には動けない（反則）
            commitFoul(turn);
        } else {
            if (K.Board[t->to.x][t->to.y] != EMPTY) { //行き先に相手の駒があれば（自分の駒がないことはcanJumpで保証）
                //**
                if (turn == FIRST){
                    K.reserve_sente+=(1<<(((K.Board[t->to.x][t->to.y] % ENEMY) - FU > 0) ? 2*((K.Board[t->to.x][t->to.y] % ENEMY)- FU) : 2*(K.Board[t->to.x][t->to.y] % ENEMY))); //持ち駒に追加（成り駒だったらもとに戻す、相手の駒だったら自分の駒にする）
                } else {
                    K.reserve_gote+=(1<<(((K.Board[t->to.x][t->to.y] % ENEMY) - FU > 0) ? 2*((K.Board[t->to.x][t->to.y] % ENEMY)- FU) : 2*(K.Board[t->to.x][t->to.y] % ENEMY)));
                }
                //**
            }
            if (t->promote == 1) { //成る
                if (canPromote(*t, &K, turn)) {//合法
                    K.Board[t->to.x][t->to.y] = K.Board[t->from.x][t->from.y] + 5;
                } else {
                    commitFoul(turn);
                }  
            } else { //成らない
                K.Board[t->to.x][t->to.y] = K.Board[t->from.x][t->from.y]; // 駒を置く
            }
        }
        K.Board[t->from.x][t->from.y] = EMPTY;
    }

    if (isWaiting(&K, turn)) commitFoul(turn); //指してもまだ王手なら反則（王手無視）

    return K;
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
    K.reserve_sente=0;
    K.reserve_gote=0;

    return K;
}

//盤面の表示（テストのときなどに）
void printBoard(Kyokumen *K, int turn, bool w) {
    char *komaCh[2][12] = {{" 王", " 飛", " 角", " 金", " 銀", " 歩", " 龍", " 馬", "", " 全", " と", "   "},
                        {"e王", "e飛", "e角", "e金", "e銀", "e歩", "e龍", "e馬", "", "e全", "eと", "   "} };
    char *turnCh[2] = {"先手", "後手"};

    //盤面の表示
    puts("   A   B   C   D   E  ");
    puts(" +---+---+---+---+---+");
    for (int i = 5; i >= 1; i--) {
        printf("%d|", i);
        for (int j = 1; j <= 5; j++) {
            printf("%s|", komaCh[((K->Board[j][i] >= ENEMY) ? 1 : 0)][(K->Board[j][i] == EMPTY) ? 11 : (K->Board[j][i] % ENEMY)]);
        }
        putchar('\n');
        puts(" +---+---+---+---+---+");
    }

    //持ち駒の表示
    puts("持ち駒");
    printf("%s:", turnCh[0]);
    for(int j=0; j<= FU; j++){
        int cnt=(K->reserve_sente>>2*j)%4;
        while(cnt>0){
            printf("%s",komaCh[0][j]);
            cnt--;
        }
    }
    printf("\n");
    printf("%s:", turnCh[1]);
    for(int j=0; j<= FU; j++){
        int cnt=(K->reserve_gote>>2*j)%4;
        while(cnt>0){
            printf("%s",komaCh[0][j]);
            cnt--;
        }
    }
    printf("\n");

    //手番の表示（王手をかけられているならそれも表示）
    //**
    if (turn == FIRST){
        printf("NEXT: %s %s\n", turnCh[0], w ? "王手" : "");
    }else{
        printf("NEXT: %s %s\n", turnCh[1], w ? "王手" : "");
    }
    //**
}

/* Evaluation Functionに関するメモ（小野）
・二駒関係あるいは三駒関係の表を作成
・探索コストは、現在の盤面から表を参照する時間
・駒は12枚あるので、関係は78通り
・探索中は差分計算（駒の移動のときは移動した駒についてのみ更新して24回、駒を取った場合は48回）で参照回数を減らせる
・駒の種類は所有者と成る・成らないをあわせて20種類、駒の位置は盤上と駒台の27種類で、重みを1バイトで表現すると表のサイズは(20 * 27) ^ 2 = 300KB 程度
・KPPTだと王の場所が一つにつき25通り。駒の種類は19種類、駒の位置は27種類、手番が2種類で、それが自分の王と相手の王で2つあるので表のサイズは 2 * (2 * (25 * (19 * 27) ^ 2)) = 26MB 程度
・探索はMin-Max + Alpha-Beta Method
・alpha-betaのcutが発生しやすくなるように手を並べられるか？→強そうな手を最初におけばいい？
*/

// 候補盤面の作成（反則の判定後に敗北処理をしないかわりにflagに情報を残す）
Kyokumen makeCand(Move *t, Kyokumen K, int turn, bool *flag)
{
    if (!isInBoard(*t)) // 盤の外を使っている
        *flag = false;

    if (t->from.x == 0)
    {                         // 駒を打つとき
        if (t->koma == EMPTY) // 存在しない駒（反則）
            *flag = false;
        if (turn == FIRST && ((K.reserve_sente) >> (2 * (t->koma))) % 4 == 0)
        { // その駒は持っていない（反則）//**
            *flag = false;
        }
        else if (turn == SECOND && ((K.reserve_gote) >> (2 * (t->koma))) % 4 == 0)
        {                     //**
            *flag = false; //**
        }
        if (K.Board[t->to.x][t->to.y] != EMPTY)
        { // すでに駒がある（反則）
            *flag = false;
        }
        else
        {
            if (t->koma == FU)
            { // 二歩の判定
                for (int i = 0; i < 5; i++)
                {
                    if (turn == FIRST & K.Board[t->to.x][i] == FU) // 二歩 //**
                        *flag = false;
                    if (turn == SECOND & K.Board[t->to.x][i] == FU + ENEMY)
                    {                     //**
                        *flag = false; //**
                    }
                }
            }
            //**
            if (turn == FIRST)
            {
                K.reserve_sente -= (1 << (2 * (t->koma))); // 駒を使用
                K.Board[t->to.x][t->to.y] = t->koma;       // 駒を置く
            }
            else
            {
                K.reserve_gote -= (1 << (2 * (t->koma)));
                K.Board[t->to.x][t->to.y] = t->koma + ENEMY;
            }
            //**
        }
    }
    else
    {                                                     // 駒を動かすとき
        if (!isSelf(K.Board[t->from.x][t->from.y], turn)) // そこに自分の駒はない（反則）
            *flag = false;
        else if (!canJump(*t, &K, turn))
        { // その場所には動けない（反則）
            *flag = false;
        }
        else
        {
            if (K.Board[t->to.x][t->to.y] != EMPTY)
            { // 行き先に相手の駒があれば（自分の駒がないことはcanJumpで保証）
                //**
                if (turn == FIRST)
                {
                    K.reserve_sente += (1 << (((K.Board[t->to.x][t->to.y] % ENEMY) - FU > 0) ? 2 * ((K.Board[t->to.x][t->to.y] % ENEMY) - FU) : 2 * (K.Board[t->to.x][t->to.y] % ENEMY))); // 持ち駒に追加（成り駒だったらもとに戻す、相手の駒だったら自分の駒にする）
                }
                else
                {
                    K.reserve_gote += (1 << (((K.Board[t->to.x][t->to.y] % ENEMY) - FU > 0) ? 2 * ((K.Board[t->to.x][t->to.y] % ENEMY) - FU) : 2 * (K.Board[t->to.x][t->to.y] % ENEMY)));
                }
                //**
            }
            if (t->promote == 1)
            { // 成る
                if (canPromote(*t, &K, turn))
                { // 合法
                    K.Board[t->to.x][t->to.y] = K.Board[t->from.x][t->from.y] + 5;
                }
                else
                {
                    *flag = false;
                }
            }
            else
            {                                                              // 成らない
                K.Board[t->to.x][t->to.y] = K.Board[t->from.x][t->from.y]; // 駒を置く
            }
        }

        if (*flag) {
            K.Board[t->from.x][t->from.y] = EMPTY;
        }
    }

    if (isWaiting(&K, turn))
        *flag = false; // 指してもまだ王手なら反則（王手無視）

    return K;
}

// 評価関数
int evaluationFunction(Kyokumen K) {
    int point_FIRST = 0, point_SECOND = 0;

    for (int i = 0; i <= 5; i++) {
        for (int j = 0; j <= 5; j++) {
            if (isSelf(K.Board[i][j], aiTURN)) { //自分の駒だったら
                if (aiTURN == FIRST) {
                    point_FIRST += p[K.Board[i][j]];
                } else {
                    point_SECOND += p[K.Board[i][j] % ENEMY];
                }
            } else if (isEnemy(K.Board[i][j], aiTURN)) { //相手の駒だったら
                if (aiTURN == FIRST) {
                    point_SECOND += p[K.Board[i][j] % ENEMY];
                } else {
                    point_FIRST += p[K.Board[i][j]];
                }
            }
        }
    }

    //ここから持ち駒

    for (int i = OU; i <= FU; i++) {
        while ((K.reserve_sente >> (2 * i)) % 4 != 0) {
            point_FIRST += p_hand[i];
            K.reserve_sente -= 1 << (2 * i);
        }

        while((K.reserve_gote >> (2 * i)) % 4 != 0) {
            point_SECOND += p_hand[i];
            K.reserve_gote -= 1 << (2 * i);
        }
    }

    return (aiTURN == FIRST) ? (point_FIRST - point_SECOND) : (point_SECOND - point_FIRST);
}

// 可能な手（1手先）をすべて生成して配列にしまう（alpha-betaのcutが発生しやすくなるように手を並べられるか？）
void fullSearch(Kyokumen *K, int turn, int *cnt, int depth)
{
    // 盤の中での移動
    for (int i = 1; i <= 5; i++)
    {
        for (int j = 1; j <= 5; j++)
        {
            if (isSelf(K->Board[i][j], turn))
            { // そこに自分の駒があれば
                for (int k = 1; k <= 5; k++)
                {
                    for (int l = 1; l <= 5; l++)
                    {
                        for (int m = 0; m <= 1; m++)
                        {
                            Move t_cand;
                            t_cand.from.x = i;
                            t_cand.from.y = j;
                            t_cand.to.x = k;
                            t_cand.to.y = l;
                            t_cand.promote = m;
                            if (canJump(t_cand, K, turn))
                            {                                                     // 移動可能であれば
                                bool flag = true;
                                possible_K[depth][*cnt] = makeCand(&t_cand, *K, turn, &flag); // 次の盤面を保存
                                t[depth][*cnt] = t_cand;                          // 手を保存
                                if (flag) (*cnt)++; //合法手でなければカウンタが増えないので、あとから上書きされる（合法手の数はpossible_Kの要素数ではなく*cntの値が示すことに注意）
                            }
                        }
                    }
                }
            }
        }
    }

    // 駒を打つとき
    for (int i = OU; i <= FU; i++)
    {
        switch (turn)
        {
        case FIRST:
            if (((K->reserve_sente) >> (2 * i)) % 4 != 0)
            { // 駒を持っている
                for (int j = 1; j <= 5; j++)
                {
                    for (int k = 1; k <= 5; k++)
                    {
                        if (K->Board[j][k] == EMPTY)
                        { // マスが空白ならば
                            Move t_cand;
                            t_cand.from.x = 0;
                            t_cand.from.y = 0;
                            t_cand.to.x = j;
                            t_cand.to.y = k;
                            t_cand.koma = i;
                            if (canJump(t_cand, K, turn))
                            {
                                bool flag = true;
                                possible_K[depth][*cnt] = makeCand(&t_cand, *K, turn, &flag);
                                t[depth][*cnt] = t_cand;
                                if (flag) (*cnt)++;
                            }
                        }
                    }
                }
            }
            break;

        case SECOND:
            if (((K->reserve_gote) >> (2 * i)) % 4 != 0)
            { // 駒を持っている
                for (int j = 1; j <= 5; j++)
                {
                    for (int k = 1; k <= 5; k++)
                    {
                        if (K->Board[j][k] == EMPTY)
                        { // マスが空白ならば
                            Move t_cand;
                            t_cand.from.x = 0;
                            t_cand.from.y = 0;
                            t_cand.to.x = j;
                            t_cand.to.y = k;
                            t_cand.koma = i;
                            if (canJump(t_cand, K, turn))
                            {
                                bool flag = true;
                                possible_K[depth][*cnt] = makeCand(&t_cand, *K, turn, &flag);
                                t[depth][*cnt] = t_cand;
                                if (flag) (*cnt)++;
                            }
                        }
                    }
                }
            }
            break;
        }
    }
}

int maxLevel(Kyokumen *K, int turn, int alpha, int beta, int depth);
int minLevel(Kyokumen *K, int turn, int alpha, int beta, int depth);

// 自分の手における最適評価値を返す
int maxLevel(Kyokumen *K, int turn, int alpha, int beta, int depth)
{
    if (depth == DEPTH)
        return evaluationFunction(*K); // 深さ制限に到達したら現在の局面の評価値を返す

    int cnt = 0;
    
    fullSearch(K, turn, &cnt, depth); // 次の手をすべて生成

    int score_max = -(INFTY); // 初期化
    for (int i = 0; i < cnt; i++)
    {
        int score;
        score = minLevel(&(possible_K[depth][i]), turn * (-1), alpha, beta, depth + 1); // 相手の手の探索
        if (score >= beta)
            return score;
        if (score > score_max)
        {
            score_max = score;
            trace_K[depth] = possible_K[depth][i];      // 最大スコアを与えた局面を覚えておく
            trace_M[depth] = t[depth][i]; // その手も覚えておく
            alpha = max(alpha, score_max);
        }
    }
    return score_max;
}

// 相手の手における最適評価値を返す
int minLevel(Kyokumen *K, int turn, int alpha, int beta, int depth)
{
    if (depth == DEPTH)
        return evaluationFunction(*K);

    int cnt = 0;

    fullSearch(K, turn, &cnt, depth);

    int score_min = INFTY;
    for (int i = 0; i < cnt; i++)
    {
        int score;
        score = maxLevel(&(possible_K[depth][i]), turn * (-1), alpha, beta, depth + 1);
        if (score <= alpha)
            return score;
        if (score < score_min)
        {
            score_min = score;
            trace_K[depth] = possible_K[depth][i]; // 最大スコアを与えた局面を覚えておく
            trace_M[depth] = t[depth][i];
            beta = min(beta, score_min);
        }
    }
    return score_min;
}

// 最適手の表示（人間の入力形式に合わせて出力）
void printAIMove(Kyokumen *K)
{
    char s[10] = "";
    if (trace_M[0].from.x == 0)
    { // 駒を打つ
        s[0] = '1' + trace_M[0].to.y - 1;
        s[1] = 'A' + trace_M[0].to.x - 1;
        switch (trace_M[0].koma)
        {
        case HI:
            s[2] = 'H';
            s[3] = 'I';
            break;
        case KA:
            s[2] = 'K';
            s[3] = 'K';
            break;
        case KI:
            s[2] = 'K';
            s[3] = 'I';
            break;
        case GI:
            s[2] = 'G';
            s[3] = 'I';
            break;
        case FU:
            s[2] = 'F';
            s[3] = 'U';
            break;
        }
    }
    else
    { //駒の移動
        s[0] = '1' + trace_M[0].from.y - 1;
        s[1] = 'A' + trace_M[0].from.x - 1;
        s[2] = '1' + trace_M[0].to.y - 1;
        s[3] = 'A' + trace_M[0].to.x - 1;
        if (trace_M[0].promote == 1)
            s[4] = 'N';
    }

    printf("%s\n", s);
}

// AIが手を指す
Kyokumen makeAIMove(Kyokumen *K)
{
    maxLevel(K, aiTURN, -(INFTY), INFTY, 0);
    printAIMove(K);
    return makeMove(&trace_M[0], *K, aiTURN);
}

int main(int argc, char *argv[]) {
    myTURN = (atoi(argv[1]) == 0 ? 1 : -1); //実行時引数が0なら1（先手）、それ以外なら-1（後手）に変換
    aiTURN = (-1) * myTURN;
    Kyokumen K[200]; //盤面
    Move t[200]; //150手分あればよいが、少し多めに用意
    int turn = FIRST; //手番を先手に設定
    int i = 0;
    bool w[2] = {false, false}; //w[0]には先手が王手をかけられているか？を示すbool値が入る

    K[0] = initBoard(); //初期盤面を作成
    printBoard(K, turn, w[turn]); //初期盤面の表示

    while (i < 150) {
        if (turn == myTURN) { //自分の番
            char s[10];
            scanf("%s", s); // 指し手の入力
            makeInstruction(s, &t[i]);
            K[i + 1] = makeMove(&t[i], K[i], turn); // 指し手を実行して新しい盤面に書き込み
            i++;
            // freezingFU_Judge(K+i, turn); //行きどころのない駒の有無を判定
            repJudge(K, i, turn);             // 千日手判定
            winJudge(K + i, turn);            // 詰み判定
            turn = (-1) * turn;                          // ゲーム続行、手番を更新
            w[turn] = isWaiting(K + i, turn); // 王手（王手をかけた）判定
            printBoard(K + i, turn, w[turn]);
        } else { //AIの番
            K[i + 1] = makeAIMove(&K[i]); //AIが手を指す
            i++;
            repJudge(K, i, turn);
            winJudge(K + i, turn);
            turn = (-1) * turn;
            w[turn] = isWaiting(K + i, turn);
            printBoard(K + i, turn, w[turn]);
        }
    }

    return 0;
}