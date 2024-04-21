/****************************************************************/
/*                 作業系統  模擬行程排程器     　        　    */
/* 50915112 廖子科 Zi-Ke,Liao -2024/04/20  ©All RIGHTS RESERVED.*/
/*                                                              */
/*  Notice:                                                     */
/*  若是DEV C++ 無法執行，需要額外設定(Visual Studio不用).      */
/*  Tool > Compiler Option > General                            */
/*  第一格方塊輸入 -std=c++11,  將上方Add the following....打勾 */
/*  完成後OK 就可以執行了                                       */
/*                                                              */
/****************************************************************/

#include <iostream>
#include <iomanip>
#include <string>
#include <cmath> 
#include <list>
#include <map>
#include <ctime>
#include <Windows.h>

using namespace std;
void mypause() {
    printf("輸入任何按鍵繼續！！！");
    getchar();
    return;
}

/********* User Setting **********/
double Delay_speed = 1000;  //每一週期會等到1秒，若speed調為0.1 ，則每一周期會等0.1秒
int ready_list_size = 3; //就緒list空間限制為3
int blocked_list_size = 3; //懸置list空間限制為3
int running_time_limit = 10;  //每個程序最多跑10秒 超過等待下一次先給別的程序跑
int ceated_time_range = 2; //建立等待時間1~2
int running_need_time_range = 10; //其他載入等待時間1~10
int other_need_time_range = 3; //其他載入等待時間1~3
int waitIO_Probability = 30; //等待IO機率
int runmode = 3; //執行模式 1:自動跑 , 2:自動跑(每一周期停1*Delay_speed秒) , 3:每周期都會暫停
/*********************************/

struct proces_item;
void FSM_EventHandle(int event, int now_list_pos, bool flashOtherRD, bool flashRunningRD, bool flashWeight); //事件呼叫處理
void FSM_StateTransfer(list<proces_item>* now_list, int nowmovePos, list<proces_item>* next_list, int en_state, bool flashOtherRD, bool flashRunningRD, bool flashWeight); //事件狀態轉換
void clear_list_hasmove();
void Delay_S(int second); //等待1秒為單位*second
void showList(string title, list<proces_item>* listptr); //顯示單一個串列
void showAllList(); //顯示所有的串列

void created_to_ready();
void created_to_swappedReady();
void ready_to_running();
void ready_to_swappedReady();
void blocked_to_ready();
void blocked_to_swappedBlocked();
void running_to_ready();
void running_to_blocked();
void running_to_exit();
void swappedReady_to_ready();
void swappedBlocked_to_blocked();
void swappedBlocked_to_swappedReady();

string OPlog = ""; //移動紀錄
map<int, string> process_route; //路徑紀錄

struct proces_item { //程序資訊
    int id;
    string name;   //程序名稱
    string content;//內容
    int other_need_time;   //紀錄其他需等時間
    int running_need_time; //紀錄程序還需要執行多久
    int weight_rev;    //調整權重 (-50~50)
    double weight_value; //權重:越低越好
    bool hasmove; //避免同一個時間(1s)被移動兩個狀態
    //計算比例:進來順序*1.5 + 執行預估所需時間*0.5 + weight_rev
};

//建立list: 程序建立 ,就緒 , 懸置 , 置換/懸置 , 置換/就緒
list<proces_item> create_list, ready_list, blocked_list, swappedReady_list, swappedBlocked_list, exit_list, running_list;
proces_item* nextToRunning = nullptr; //抓去執行的指標

enum state_value {
    state_created = 1,       //建立
    state_ready,             //就緒
    state_blocked,           //懸置
    state_running,           //執行
    state_exit,              //終止
    state_swappedReady,      //置換/就緒
    state_swappedBlocked,    //置換/懸置
};
enum enum_event {
    EVENT1 = 1, EVENT2, EVENT3, EVENT4, EVENT5, EVENT6, EVENT7, EVENT8, EVENT9, EVENT10, EVENT11, EVENT12
};
struct State_info { //狀態表資訊
    int event;
    int NowState;             //當前狀態
    list<proces_item>* NowStateList;
    void (*NextEventDoFun)(); //將要要執行的函數指標
    int NextState;            //下一個狀態
    list<proces_item>* NextStateList;
}State_Table[] = {
    {EVENT1, state_created,&create_list,  created_to_ready,  state_ready, &ready_list},
    {EVENT2, state_created,&create_list,  created_to_swappedReady,  state_swappedReady ,&swappedReady_list},
    {EVENT3, state_ready,  &ready_list,   ready_to_running,  state_running ,&running_list},
    {EVENT4, state_ready,  &ready_list,   ready_to_swappedReady,  state_swappedReady, &swappedReady_list},
    {EVENT5, state_blocked,&blocked_list, blocked_to_ready,  state_ready, &ready_list},
    {EVENT6, state_blocked,&blocked_list, blocked_to_swappedBlocked,  state_swappedBlocked, &swappedBlocked_list},
    {EVENT7, state_running,&running_list, running_to_ready,  state_ready, &ready_list},
    {EVENT8, state_running,&running_list, running_to_blocked, state_blocked, &blocked_list},
    {EVENT9, state_running,&running_list, running_to_exit,  state_exit, &exit_list},
    {EVENT10, state_swappedReady,   &swappedReady_list,  swappedReady_to_ready, state_ready, &ready_list },
    {EVENT11, state_swappedBlocked, &swappedBlocked_list, swappedBlocked_to_blocked, state_blocked, &blocked_list},
    {EVENT12, state_swappedBlocked, &swappedBlocked_list,swappedBlocked_to_swappedReady, state_swappedReady, &swappedReady_list}
};

string tempstr = ""; //輔助變數:存就緒移動到置換就緒的程序名稱
int maxID = 0; //輔助變數:存放程序ID(不重複)

int main() {
    srand(time(0));
    //srand(50915112);
    //ios::sync_with_stdio(0), cin.tie(0);
    system("clear");
    int RM_nextToRunning = -1; //刪除就緒->執行的位置
    int run_time_temp = 0; //紀錄到幾
    int skip_to_blocked = 0; //解決執行->置換問題
    //***************************************** //建立新的動作 建立預設等待1~2秒
    // 1:ID值 2:標題 3:內容 4:紀錄其他(除了執行)需等時間 5:紀錄程序還需要執行多久 6:調整權重(-50~50) 
    // 7:最後加權完權重:越低越好(不可調整) 8:避免同一個時間(1s)被移動兩個狀態，代表有被移動過
    create_list.push_back({ ++maxID, "程序" + to_string(maxID),  "Line.exe> content",  rand() % ceated_time_range + 1,-1,  0,0,0 });
    create_list.push_back({ ++maxID, "程序" + to_string(maxID),  "GTA6.exe> content",  rand() % ceated_time_range + 1,-1,  0,0,0 });
    create_list.push_back({ ++maxID, "程序" + to_string(maxID),  "The Crew3> content", rand() % ceated_time_range + 1,-1,  0,0,0 });
    create_list.push_back({ ++maxID, "程序" + to_string(maxID),  "XAMPP.exe> content", rand() % ceated_time_range + 1,-1,  0,0,0 });
    create_list.push_back({ ++maxID, "程序" + to_string(maxID),  "VSCode.exe> content",rand() % ceated_time_range + 1,-1,  0,0,0 });
    process_route[1] = "1(" + to_string(next(create_list.begin(), 0)->other_need_time) + "),";
    process_route[2] = "1(" + to_string(next(create_list.begin(), 1)->other_need_time) + "),";
    process_route[3] = "1(" + to_string(next(create_list.begin(), 2)->other_need_time) + "),";
    process_route[4] = "1(" + to_string(next(create_list.begin(), 3)->other_need_time) + "),";
    process_route[5] = "1(" + to_string(next(create_list.begin(), 4)->other_need_time) + "),";
    showAllList(); //顯示所有串列內容
    if (runmode == 1) Delay_S(0);
    else if (runmode == 2) Delay_S(1);
    else mypause();

    while (create_list.size() > 0 || ready_list.size() > 0 || blocked_list.size() > 0
        || swappedReady_list.size() > 0 || swappedBlocked_list.size() > 0 || running_list.size() > 0) {
        clear_list_hasmove();
        /***** create_list *****/
        if (create_list.size() && create_list.front().hasmove == 0 && (--create_list.front().other_need_time) <= 0) { //建立狀態等待時間到了
            /***** 建立 = > 就緒(就緒鏈結串列有位置) *****/
            if (ready_list.size() < ready_list_size - 1) {
                FSM_EventHandle(EVENT1, 0, 1, 1, 1);
            }
            else { /***** 建立 = > 置換就緒(就緒鏈結串列滿了) *****/
                FSM_EventHandle(EVENT2, 0, 1, 1, 1);
            }
        }
        /***** ready_list *****/
        //取出就緒list內權重最低的
        if (ready_list.size() && nextToRunning == nullptr && !running_list.size()) {
            double minweight = ready_list.front().weight_value;
            int temp = 0;
            RM_nextToRunning = 0;
            //nextToRunning = nullptr;
            for (auto& listItem : ready_list) {
                if (minweight >= listItem.weight_value && listItem.hasmove == 0) {
                    nextToRunning = &listItem; //更新最小值的stru執行 => 懸置ct位址
                    RM_nextToRunning = temp;  //更新最小值 list的位置
                }
                temp++;
            }
            if (nextToRunning != nullptr) OPlog = OPlog + nextToRunning->name + " 行程將進入執行\n";
        }
        /***** 就緒 => 執行 *****/
        //確保下一個指標有東西 & 目前沒有執行的程序 & 就緒指標狀態等待時間到了
        if (nextToRunning != nullptr && (--(nextToRunning->other_need_time)) <= 0) {
            FSM_EventHandle(EVENT3, RM_nextToRunning, 0, 0, 0);
            run_time_temp = max(0, running_list.front().running_need_time - running_time_limit); //刷新時間限制
            nextToRunning = nullptr; //清空
            RM_nextToRunning = -1;   //清空
        }
        /***** running_list *****/
        if (running_list.size() && running_list.front().hasmove == 0) {
            if (skip_to_blocked == 1) { //判斷懸置list沒滿再移動過去
                if (blocked_list.size() < blocked_list_size) {
                    /***** 執行 => 懸置 *****/
                    nextToRunning = nullptr; //清空
                    RM_nextToRunning = -1;   //清空
                    skip_to_blocked = 0;
                    FSM_EventHandle(EVENT8, 0, 1, 0, 1);
                }
            }
            else if ((--(running_list.front().running_need_time)) <= run_time_temp) { //時間到
                if (running_list.front().running_need_time <= 0) {
                    /***** 執行 => 結束 *****/
                    running_list.front().weight_rev = 0;   //清空
                    running_list.front().weight_value = 0; //清空
                    FSM_EventHandle(EVENT9, 0, 0, 0, 0);
                }
                else {
                    /***** 就緒 => 置換就緒 (前面執行跑超過使用時間限制 而且 就緒快滿了) 抓一個權重最低(值最高的)過去 *****/
                    int ready_to_swappedready_pos = -1;
                    if (ready_list.size() && ready_list.size() < ready_list_size - 1) { //確保就緒有資料而且快滿了
                        double maxweight = ready_list.front().weight_value;
                        int temp = 0;
                        //nextToRunning = nullptr;
                        for (auto& listItem : ready_list) {
                            if (maxweight <= listItem.weight_value && listItem.hasmove == 0) {
                                if (nextToRunning != nullptr && nextToRunning->id == listItem.id) {
                                    continue;
                                }
                                tempstr = listItem.name;
                                ready_to_swappedready_pos = temp;  //更新最小值 list的位置
                            }
                            temp++;
                        }
                    }
                    if (ready_to_swappedready_pos != -1) {
                        FSM_EventHandle(EVENT4, ready_to_swappedready_pos, 1, 0, 0);
                    }
                    /***** 執行 => 就緒(跑超過使用時間限制) *****/
                    FSM_EventHandle(EVENT7, 0, 1, 0, 1);
                }
            }
            else {
                /***** 執行 => 懸置(等IO) 用亂數有可能會執行 *****/
                if (rand() % waitIO_Probability == 1) {
                    if (blocked_list.size() < blocked_list_size) { //判斷懸置list沒滿再移動過去
                        nextToRunning = nullptr; //清空
                        RM_nextToRunning = -1;   //清空
                        skip_to_blocked = 0;
                        FSM_EventHandle(EVENT8, 0, 1, 0, 1);
                    }
                    else skip_to_blocked = 1;
                }
            }
        }
        /***** blocked_list *****/
        if (blocked_list.size() && blocked_list.front().hasmove == 0 && (--blocked_list.front().other_need_time) <= 0) { //懸置狀態等待時間到了
            if (ready_list.size() < ready_list_size - 1) {
                /***** 懸置 = > 就緒(就緒鏈結串列有位置) *****/
                FSM_EventHandle(EVENT5, 0, 1, 0, 1);
            }
            else if (blocked_list.size() < blocked_list_size - 1) {
                /***** 懸置 = > 置換/懸置(就緒鏈結串列滿了 而且置換滿了) *****/
                FSM_EventHandle(EVENT6, 0, 1, 0, 0);
            }
        }


        /***** swappedBlocked_list *****/
        if (swappedBlocked_list.size() && swappedBlocked_list.front().hasmove == 0 && (--swappedBlocked_list.front().other_need_time) <= 0) { //懸置狀態等待時間到了
            if (blocked_list.size() < blocked_list_size - 1 && swappedBlocked_list.front().weight_value < 0) {
                /***** 置換/懸置 = > 懸置(懸置list有位置且該程序優先度<0) *****/
                FSM_EventHandle(EVENT11, 0, 1, 0, 0);
            }
            else {
                /***** 置換/懸置 = > 置換/就緒 *****/
                FSM_EventHandle(EVENT12, 0, 1, 0, 0);
            }
        }


        /***** swappedReady_list *****/
        /***** 置換就緒 => 就緒 (就緒list有位置) *****/
        if (swappedReady_list.size() && swappedReady_list.front().hasmove == 0 && ready_list.size() < ready_list_size - 1) {
            FSM_EventHandle(EVENT10, 0, 1, 1, 1);
        }

        OPlog += " ----------------------------------------\n";
        system("cls");
        cout << "路徑log:\n";
        for (auto& item : process_route) cout << "程序ID" << item.first << ":  " << item.second << "\n\n";
        cout << "\n\n移動操作log:\n";
        cout << OPlog << "\n\n";
        cout << "\n\nlist資訊:\n";
        showAllList();
        if (runmode == 1) Delay_S(0);
        else if (runmode == 2) Delay_S(1);
        else mypause();
    }
    cout << "\n-----------------------" << '\n' << OPlog << '\n' << "-----------------------";
    return 0;
}


/* 事件狀態轉換 */
//目前list , 要移動的位置(0) , 下一個list , 儲存下一個狀態值enum , 是否刷新其他等待時間 , 是否刷新執行所需時間,是否更新總權重值
void FSM_StateTransfer(list<proces_item>* now_list, int nowmovePos, list<proces_item>* next_list, int en_state,
    bool flashOtherRD, bool flashRunningRD, bool flashWeight) {
    //產生其他等待時間
    if (flashOtherRD) next(now_list->begin(), nowmovePos)->other_need_time = rand() % other_need_time_range + 1;
    //產生執行所需時間
    if (flashRunningRD) now_list->front().running_need_time = rand() % running_need_time_range + 1;

    if (flashWeight) {
        //進來順序*1.5 + 執行預估所需時間*0.5 + weight_rev
        next(now_list->begin(), nowmovePos)->weight_value = next(now_list->begin(), nowmovePos)->id * 1.5 +
            next(now_list->begin(), nowmovePos)->running_need_time * 0.5 + next(now_list->begin(), nowmovePos)->weight_rev;
    }
    //若結束顯示5就好 若是4(執行)需要顯示執行所剩時間  其餘要顯示其他載入所需時間,

    if (en_state == 5) process_route[next(now_list->begin(), nowmovePos)->id] += "5";
    else if (en_state == 4) process_route[next(now_list->begin(), nowmovePos)->id] += to_string(en_state) + "(" + to_string(next(now_list->begin(), nowmovePos)->running_need_time) + "),";
    else process_route[next(now_list->begin(), nowmovePos)->id] += to_string(en_state) + "(" + to_string(next(now_list->begin(), nowmovePos)->other_need_time) + "),";


    next(now_list->begin(), nowmovePos)->hasmove = 1;
    next_list->push_back(*next(now_list->begin(), nowmovePos));
    now_list->erase(next(now_list->begin(), nowmovePos));
}

/* 事件呼叫處理 */
void FSM_EventHandle(int event, int now_list_pos, bool flashOtherRD, bool flashRunningRD, bool flashWeight) {
    int flag = 0;
    int en_state = 0;
    list<proces_item>* now_list = nullptr, * NextState = nullptr;
    void (*eventFunction)() = NULL;  //函數指標
    for (int i = 0; i < sizeof(State_Table) / sizeof(State_info); i++) {
        //找到指定的event
        if (event == State_Table[i].event) {
            flag = 1;
            en_state = State_Table[i].NextState; //儲存下一個狀態值
            eventFunction = State_Table[i].NextEventDoFun;
            now_list = State_Table[i].NowStateList;
            NextState = State_Table[i].NextStateList;
            break;
        }
    }
    if (flag) { //代表有找到
        if (eventFunction) {
            eventFunction();
        }
        //目前list,要改的位置,下一個list,是否刷新其他等待時間(random)
        FSM_StateTransfer(now_list, now_list_pos, NextState, en_state, flashOtherRD, flashRunningRD, flashWeight);
    }
    else {
        cout << "錯誤，找不到指定內容!\n";
    }
}

/* 清除所有被移動過的變數 */
void clear_list_hasmove() {
    for (auto& item : create_list) item.hasmove = 0;
    for (auto& item : ready_list) item.hasmove = 0;
    for (auto& item : blocked_list) item.hasmove = 0;
    for (auto& item : swappedReady_list) item.hasmove = 0;
    for (auto& item : swappedBlocked_list) item.hasmove = 0;
    for (auto& item : running_list) item.hasmove = 0;
}

/* 等待1秒為單位*second */
void Delay_S(int second) {
    Sleep(second * 1 * Delay_speed);
}

/* 顯示所有list內容 */
void showAllList() {
    showList("目前執行程序", &running_list);
    showList("建立鏈結串列", &create_list);
    showList("就緒串列", &ready_list);
    showList("懸置串列", &blocked_list);
    showList("置換/就緒串列", &swappedReady_list);
    showList("置換/懸置串列", &swappedBlocked_list);
    showList("已結束串列", &exit_list);
}

/* 顯示單一list內容 */
void showList(string title, list<proces_item>* listptr) {
    ios::fixed;
    cout << "   ";
    cout << "************************************************************   " << title << "   ************************************************************" << '\n';
    cout << "  " << "ID" << "\t" << "標題" << "\t\t" << "內容"
        << "\t\t\t" << "其他需等時間" << "\t" << "程序還需執行時間"
        << "\t" << "調整權重(-50~50)" << "\t" << "最後權重:越低越優先" << '\n';
    for (auto listItem : *listptr) {
        cout << "  " << listItem.id << "\t" << listItem.name << "\t\t" << listItem.content;
        cout << "\t" << (listItem.other_need_time <= 0 ? "NaN" : to_string(listItem.other_need_time));
        cout << "\t\t" << (listItem.running_need_time <= 0 ? "NaN" : to_string(listItem.running_need_time));
        cout << "\t\t\t" << listItem.weight_rev << "\t\t\t" << listItem.weight_value << '\n';
    }
    cout << "\n";
}

void created_to_ready() { OPlog = OPlog + create_list.front().name + " 建立 => 就緒\n"; }
void created_to_swappedReady() { OPlog = OPlog + create_list.front().name + " 建立 => 置換就緒 (就緒鏈結串列已滿)\n"; }
void ready_to_running() { OPlog = OPlog + nextToRunning->name + " 就緒 => 執行 (執行所需時間過長，輪給下一個程序使用)\n"; }
void ready_to_swappedReady() { OPlog = OPlog + tempstr + " 就緒 => 置換就緒(執行超過指定時間，且就緒list快滿了，需要一個移動到置換)\n"; }
void blocked_to_ready() { OPlog = OPlog + blocked_list.front().name + " 懸置 => 就緒(就緒list有位置)\n"; }
void blocked_to_swappedBlocked() { OPlog = OPlog + blocked_list.front().name + " 懸置 => 置換懸置(就緒list沒位置且懸置快滿了)\n"; }
void running_to_ready() { OPlog = OPlog + running_list.front().name + " 執行(超過使用者設定) => 就緒\n"; }
void running_to_blocked() { OPlog = OPlog + running_list.front().name + " 執行 => 懸置(等待IO)\n"; }
void running_to_exit() { OPlog = OPlog + running_list.front().name + " 執行完成 => 結束\n"; }
void swappedReady_to_ready() { OPlog = OPlog + swappedReady_list.front().name + " 置換就緒 => 就緒\n"; }
void swappedBlocked_to_blocked() { OPlog = OPlog + swappedBlocked_list.front().name + " 置換懸置 => 懸置\n"; }
void swappedBlocked_to_swappedReady() { OPlog = OPlog + swappedBlocked_list.front().name + " 置換懸置 => 置換就緒\n"; }
