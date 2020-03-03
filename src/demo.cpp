#include <algorithm>
#include <ctime>
#include <fstream>
#include <iostream>
#include <map>
#include <random>
#include <set>
#include <sstream>
#include <vector>

#include <cqcppsdk/cqcppsdk.h>
#include <dolores/dolores.hpp>

using namespace cq;
using namespace std;
using dolores::matchers::contains;
using namespace dolores::matchers;
using Message = cq::message::Message;
using MessageSegment = cq::message::MessageSegment;

typedef int64_t i64;
const string PATH = "data/app/xyz.byha.loveletter/";
const map<i64, string> idName = {{0, "无"},
                                 {1, "侍卫"},
                                 {2, "牧师"},
                                 {3, "男爵"},
                                 {4, "侍女"},
                                 {5, "王子"},
                                 {6, "国王"},
                                 {7, "伯爵夫人"},
                                 {8, "公主"}};

const int cardNumber[20] = {0, 5, 2, 2, 2, 2, 1, 1, 1};
bool isGameOpen;
vector<i64> lib;
set<i64> playerList;
struct Player {
    i64 id, qq, Card, secondCard;
    bool isAlive, isProtected;
    inline bool operator<(const Player b) const {
        return id < b.id;
    }
    Player(int i) : id(i), qq(0), Card(0), secondCard(0), isAlive(0), isProtected(0){};
    Player() : id(0), qq(0), Card(0), secondCard(0), isAlive(0), isProtected(0){};
    ~Player() {
    }
};
map<i64, Player> player;
i64 alivePlayerNumber, playerTurn;

string AT(i64 Id) {
    return "[CQ:at,qq=" + to_string(Id) + "]";
}
string getPlayerCount() {
    return "共" + to_string(playerList.size()) + "人.";
}
string cardStr(i64 cardId) {
    return to_string(cardId) + ":" + idName.at(cardId);
}

void makeGame() {
    int deleteCard;
    if (playerList.size() <= 1) return;
    if (playerList.size() == 2)
        deleteCard = 3;
    else
        deleteCard = 1;
    for (auto x : idName) {
        for (int i = 1; i <= cardNumber[x.first]; ++i) {
            lib.push_back(x.first);
        }
    }
    shuffle(lib.begin(), lib.end(), default_random_engine(time(0)));
    for (int i = 1; i <= deleteCard; ++i) lib.pop_back();
}
void generateId(dolores::Current<cq::MessageEvent> &current) {
    vector<i64> randomPlayerList;
    for (auto x : playerList) {
        randomPlayerList.push_back(x);
    }
    shuffle(randomPlayerList.begin(), randomPlayerList.end(), default_random_engine(time(0)));
    i64 i = 1;
    for (auto x : randomPlayerList) { // generateId
        static Player tmp;
        tmp.qq = x, tmp.id = i, tmp.Card = lib.back(), tmp.secondCard = 0;
        lib.pop_back();
        tmp.isAlive = 1, tmp.isProtected = 0;
        ++i;
        player.insert(make_pair(tmp.id, tmp));
    }
    for (auto x : player) {
        send_private_message(x.second.qq, "LL:你的初始手牌是" + cardStr(x.second.Card));
    }
    playerTurn = 1;
}
void drawCard(i64 playerId, bool isMainCard = false) {
    if (lib.size() <= 0) {
        return;
    }
    if (!isMainCard)
        player.at(playerId).secondCard = lib.back();
    else
        player.at(playerId).Card = lib.back();
    if (!isMainCard)
        send_private_message(player.at(playerId).qq,
                             "LL:你抽到了" + cardStr(lib.back()) + "\n你手上有的牌为"
                                 + cardStr(player.at(playerId).Card) + "," + cardStr(lib.back()));
    else
        send_private_message(player.at(playerId).qq,
                             "LL:你抽到了" + cardStr(lib.back()) + "\n你手上有的牌为" + cardStr(lib.back()));
    lib.pop_back();
}
void checkGame(dolores::Current<cq::MessageEvent> &current, bool showAll = false) {
    if (!isGameOpen) {
        current.send("没有开始呢.", true);
        return;
    }
    string str;
    i64 i = 1;
    for (auto x : player) {
        str += "[" + to_string(i) + '.' + AT(x.second.qq) + "]" + "<" + (x.second.isAlive ? "存活" : "死亡") + ">";
        if (x.second.isAlive)
            if (i == playerTurn) {
                str += "(回合中)";
                if (showAll)
                    send_private_message(
                        x.second.qq,
                        "LL:你手上有的牌为" + cardStr(player.at(i).Card) + "," + cardStr(player.at(i).secondCard));
            } else {
                if (showAll) send_private_message(x.second.qq, "LL:你手上有的牌为" + cardStr(player.at(i).Card));
            }
        str += '\n';
        ++i;
    }
    current.send(str);
}
void startTurn(i64 playerId, dolores::Current<cq::MessageEvent> &current) {
    logging::debug("LL", to_string(__LINE__) + ' ' + to_string(playerId) + " " + to_string(player.size()));
    player.at(playerId).isProtected = false;
    logging::debug("LL", to_string(__LINE__));
    drawCard(playerId);
    logging::debug("LL", to_string(__LINE__));
    checkGame(current);
    current.send("到" + AT(player.at(playerId).qq) + "的回合了.牌库中还剩下" + to_string(lib.size()) + "张牌.");
}
void diePlayer(i64 playerId, dolores::Current<cq::MessageEvent> &current) {
    player.at(playerId).isAlive = false;
    current.send(AT(player.at(playerId).qq) + "死了.😂");
    return;
}
int runTurn(i64 playerId, i64 cardId, i64 targetId, i64 appendId, dolores::Current<cq::MessageEvent> &current) {
    logging::debug("LL", to_string(__LINE__));
    if ((targetId > 0 && targetId <= player.size()) && !player.at(targetId).isAlive) {
        current.send("你想对一具尸体做什么?", true);
        return -1;
    }
    if ((targetId > 0 && targetId <= player.size()) && player.at(targetId).isProtected) {
        current.send("这个人目前被一股神秘力量包裹着,你现在最好不要碰他.", true);
        return -1;
    }
    if (cardId != player.at(playerId).Card && cardId != player.at(playerId).secondCard) {
        current.send("你确定你有这张牌?", true);
        return -1;
    }
    if (cardId == player.at(playerId).Card) {
        swap(player.at(playerId).secondCard, player.at(playerId).Card);
    }
    switch (cardId) {
    case 1:
        if (targetId <= 0 || targetId > player.size()) {
            current.send("目标超出范围.", true);
            return -1;
        }
        if (appendId <= 0 || appendId > 8) {
            current.send("我们好像还没有上架这种牌把?", true);
            return -1;
        }
        if (targetId == playerId) {
            current.send("为什么要寻死呢?", true);
            return -1;
        }
        if (appendId == 1) {
            current.send(AT(player.at(targetId).qq) + "本是一家人,相煎何太急?", true);
            return -1;
        }
        if (player.at(targetId).Card == appendId) {
            current.send("猜中了" + AT(player.at(targetId).qq) + "!😊", true);
            diePlayer(targetId, current);
        } else {
            current.send("猜错了" + AT(player.at(targetId).qq) + ".😥", true);
        }
        break;
    case 2:
        if (targetId == playerId) {
            current.send("您还不知道您自己的牌啊?", true);
            return -1;
        }
        if (targetId <= 0 || targetId > player.size()) {
            current.send("目标超出范围.", true);
            return -1;
        }
        send_private_message(player.at(playerId).qq, "LL:您一不小心看到了TA的牌是" + cardStr(player.at(targetId).Card));
        break;
    case 3:
        if (targetId == playerId) {
            current.send("为什么要寻死呢?", true);
            return -1;
        }
        if (targetId <= 0 || targetId > player.size()) {
            current.send("目标超出范围.", true);
            return -1;
        }
        if (player.at(targetId).Card > player.at(playerId).Card) {
            current.send(AT(player.at(targetId).qq) + "比您巨.😥", true);
            diePlayer(playerId, current);
        } else if (player.at(targetId).Card < player.at(playerId).Card) {
            current.send("您比" + AT(player.at(targetId).qq) + "巨!😊", true);
            diePlayer(targetId, current);
        } else {
            current.send(AT(player.at(targetId).qq) + "您们一样巨,所以都不能死!👍", true);
        }
        break;
    case 4:
        player.at(playerId).isProtected = true;
        current.send("您成功地保护了自己!✋");
        break;
    case 5:
        if (player.at(playerId).secondCard == 7) {
            current.send("你不能这样做.😡", true);
            return -1;
        }
        if (targetId <= 0 || targetId > player.size()) {
            current.send("目标超出范围.", true);
            return -1;
        }
        current.send(AT(player.at(targetId).qq) + "一不小心把自己的牌弄丢了.😨" + AT(playerId) + "好心给了TA一张.");
        if (player.at(targetId).Card == 8) {
            diePlayer(targetId, current);
        }
        drawCard(targetId, true);
        break;
    case 6:
        if (targetId == playerId) {
            current.send("您可千万别人格分裂!", true);
            return -1;
        }
        if (player.at(playerId).secondCard == 7) {
            current.send("你不能这样做.😡", true);
            return -1;
        }
        if (targetId <= 0 || targetId > player.size()) {
            current.send("目标超出范围.", true);
            return -1;
        }
        swap(player.at(targetId).Card, player.at(playerId).Card);
        current.send("和" + AT(targetId) + "不小心把TA们的牌弄混了.", true);
        send_private_message(player.at(playerId).qq, "LL:你发现手上的牌变成了" + cardStr(player.at(playerId).Card));
        send_private_message(player.at(targetId).qq, "LL:你发现手上的牌变成了" + cardStr(player.at(targetId).Card));
        break;
    case 7:
        current.send("打出了一张很神奇的牌" + cardStr(7), true);
        break;
    case 8:
        current.send("宁死不屈.😲", true);
        diePlayer(playerId, current);
        break;
    default:
        current.send("你确定我们有这张牌?如果出现此错误请提交报告,因为这个错误不应该出现.", true);
        return -1;
        break;
    }
    return 0;
}
void changeTurn(dolores::Current<cq::MessageEvent> &current) {
    for (i64 i = playerTurn + 1; i <= player.size(); ++i) {
        if (player.at(i).isAlive) {
            playerTurn = i;
            startTurn(playerTurn, current);
            return;
        }
    }
    for (i64 i = 1; i < playerTurn; ++i) {
        if (player.at(i).isAlive) {
            playerTurn = i;
            startTurn(playerTurn, current);
            return;
        }
    }
    current.send("错误,无法找到下一个玩家,请上报错误.");
    logging::error("LL", to_string(__LINE__) + ":CANNOT FIND NEXT PLAYER!");
}
void joinGame(dolores::Current<cq::MessageEvent> &current) {
    if (playerList.count(current.event.user_id)) {
        current.send("您已经在桌上了," + getPlayerCount(), true);
    } else {
        playerList.insert(current.event.user_id);
        current.send("已经加入," + getPlayerCount(), true);
    }
}
void exitGame(dolores::Current<cq::MessageEvent> &current) {
    if (!playerList.count(current.event.user_id)) {
        current.send("您本来就不在桌上," + getPlayerCount(), true);
    } else {
        playerList.erase(current.event.user_id);
        current.send("已经退出," + getPlayerCount(), true);
    }
}
void endGame(dolores::Current<cq::MessageEvent> &current) {
    i64 maxPoint = -1;
    vector<i64> winner;
    string str;
    for (auto x : player) {
        if (!x.second.isAlive) continue;
        if (x.second.Card > maxPoint) {
            maxPoint = x.second.Card;
            winner.clear();
            winner.push_back(x.second.qq);
        }
        str += AT(x.second.qq) + "手上的牌是" + cardStr(x.second.Card) + '\n';
    }
    current.send(str);
    str.clear();
    for (auto x : winner) str += AT(x);
    str += "赢了.";
    current.send(str);
    isGameOpen = false;
    playerList.clear();
    player.clear();
    lib.clear();
    playerTurn = 0;
}
dolores_on_message("Join LL", command({"加入", "jr", "join", "j"}, {"!", "！"})) {
    if (isGameOpen) {
        current.send("已经开始了呢.", true);
        return;
    }
    if (current.command_argument().length() > 0) return;
    joinGame(current);
}
dolores_on_message("Exit LL", command({"退出", "tc", "t", "exit"}, {"!", "！"})) {
    if (isGameOpen) {
        current.send("已经开始了呢.", true);
        return;
    }
    if (current.command_argument().length() > 0) return;
    exitGame(current);
}
dolores_on_message("Info LL", command({"说明", "sm"}, {"!", "！"})) {
    current.send(
        "加入/退出:!加入/!退出\n开始:!开始.\n出牌:!出 A B "
        "C,其中A是你要出的牌的名字,B是牌作用的目标,C是侍卫猜测的对方牌面数值\n弃牌:!弃 A,A为1时弃掉手上的牌,"
        "A为2时弃掉刚抽到的牌.\n重置:!重置,仅管理员可使用.\n"
        "5张1 效果可以猜除1以外的牌 猜中死.\n"
        "2张2 效果看牌\n"
        "2张3 效果拼点\n"
        "2张4 效果免疫\n"
        "2张5 效果弃1抽1\n"
        "1张6 效果换牌\n"
        "1张7 效果有5/6在手上时必须打出\n"
        "1张8 打出或弃置 死");
}
dolores_on_message("Start LL", command({"开始", "ks"}, {"!", "！"})) {
    if (isGameOpen) {
        current.send("已经开始了呢.", true);
        return;
    }
    if (playerList.size() <= 1) {
        current.send("就" + to_string(playerList.size()) + "个人,打个锤锤.");
        return;
    }
    logging::debug("LL", to_string(__LINE__));
    makeGame();
    logging::debug("LL", to_string(__LINE__));
    generateId(current);
    playerList.clear();
    isGameOpen = true;
    logging::debug("LL", to_string(__LINE__));
    startTurn(playerTurn, current);
}
dolores_on_message("LL Send Card", command({"出", "出牌", "send", "c"}, {"!", "！"})) {
    logging::debug("LL", "Entering Send Card");
    for (auto x : player) {
        logging::debug("LL",
                       to_string(x.second.id) + ' ' + to_string(x.second.qq) + ' ' + to_string(x.second.Card) + ' '
                           + to_string(x.second.secondCard));
    }
    if (!isGameOpen) {
        current.send("还没开始呢.", true);
        return;
    }
    if (current.event.user_id != player.at(playerTurn).qq) {
        current.send("不是你的回合呢.", true);
        return;
    }
    i64 cardId, targetId, appendId;
    i64 argCnt = sscanf(current.command_argument().c_str(), "%lld %lld %lld", &cardId, &targetId, &appendId);
    logging::debug("LL",
                   to_string(argCnt) + " " + to_string(cardId) + " " + to_string(targetId) + " " + to_string(appendId));
    logging::debug("LL", to_string(__LINE__));
    if (argCnt == 0) { // check argCnt
        current.send("参数有误.", true);
        return;
    } else if (argCnt == 1) {
        targetId = 0, appendId = 0;
        if (cardId == 1 || cardId == 2 || cardId == 3 || cardId == 5 || cardId == 6) {
            current.send("参数有误.", true);
            return;
        }
    } else if (argCnt == 2) {
        appendId = 0;
        if (cardId == 1) {
            current.send("参数有误.", true);
            return;
        }
    }
    if (!runTurn(playerTurn, cardId, targetId, appendId, current)) { // sucessfully done turn
        player.at(playerTurn).secondCard = 0; // check if win && start next turn
        i64 cnt = 0;
        string str;
        for (auto x : player)
            if (x.second.isAlive) cnt++;
        if (cnt <= 1 || lib.size() <= 0) {
            endGame(current);
            return;
        } else {
            current.send("还剩" + to_string(cnt) + "人");
            changeTurn(current);
        }
    } else { // mistake happened
        return;
    }
}
dolores_on_message("LL Drop Card", command({"弃", "弃牌", "drop", "q"}, {"!", "！"})) {
    if (!isGameOpen) {
        current.send("还没开始呢.", true);
        return;
    }
    if (current.event.user_id != player.at(playerTurn).qq) {
        current.send("不是你的回合呢.", true);
        return;
    }
    if (player.at(playerTurn).Card == 7 || player.at(playerTurn).secondCard == 7) {
        if (player.at(playerTurn).Card == 5 || player.at(playerTurn).Card == 6 || player.at(playerTurn).secondCard == 5
            || player.at(playerTurn).secondCard == 6) {
            current.send("你不能这样做.", true);
            return;
        }
    }
    if (current.command_argument() == "1") {
        swap(player.at(playerTurn).Card, player.at(playerTurn).secondCard);
    } else if (current.command_argument() != "2") {
        current.send("指令有误.", true);
        return;
    }
    if (player.at(playerTurn).secondCard == 8) {
        diePlayer(playerTurn, current);
    }
    i64 cnt = 0;
    string str;
    for (auto x : player)
        if (x.second.isAlive) {
            cnt++;
            str += AT(x.second.qq);
        }
    if (cnt <= 1) {
        endGame(current);
        return;
    } else {
        current.send("还剩" + to_string(cnt) + "人" + str);
        changeTurn(current);
    }
}
dolores_on_message("ReSend LL", command({"查询", "query", "cx"}, {"!", "！"})) {
    checkGame(current, true);
}
dolores_on_message("LL ", admin(), command({"重置", "关闭", "reset", "close", "cz", "gb"}, {"!"})) {
    playerList.clear();
    player.clear();
    isGameOpen = false;
    lib.clear();
    current.send("亲爱的管理" + AT(current.event.user_id) + "大人,已经重置了.");
}

CQ_INIT {
    dolores::init();
}
