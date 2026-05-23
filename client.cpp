#include "client.h"

bool socket_client::Connect_server()
{
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (-1 == sockfd)
    {
        cout << "create socket err" << endl;
        return false;
    }

    struct sockaddr_in saddr; // 服务器地址
    memset(&saddr, 0, sizeof(saddr));
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(port);
    saddr.sin_addr.s_addr = inet_addr(ips.c_str());
    int res = connect(sockfd, (struct sockaddr*)&saddr, sizeof(saddr));
    if (res == -1)
    {
        cout << "connect ser err" << endl;
        return false;
    }

    cout << "Connect to Server success" << endl;
    return true;
}

void socket_client::print_info()
{
    if (dl_flg)
    {
        cout << "---已登录------用户名：" << username << "-------" << endl;
        cout << "1: 查看预约   2 ： 预订  3：  查看我的预约  4： 取消预约  5 退出" << endl;
        cout << "-----------------------------" << endl;
        cout << "请输入选项编号：" << endl;
        cin >> user_op;
        user_op += OFFSET;
    }
    else
    {
        cout << "---未登录------游客-----------" << endl;
        cout << "1: 登陆   2 ： 注册  3： 退出" << endl;
        cout << "-----------------------------" << endl;
        cout << "请输入选项编号：" << endl;
        cin >> user_op;
        if (user_op == 3)
        {
            user_op = TC;
        }
    }
}

void socket_client::User_Register()
{
    cout << "请输入用户手机号码：" << endl;
    cin >> usertel;
    cout << "请输入用户名" << endl;
    cin >> username;
    string passwd, tmp;
    cout << "请输入密码:" << endl;
    cin >> passwd;
    cout << "请再次输入密码" << endl;
    cin >> tmp;

    if (usertel.empty() || username.empty())
    {
        cout << "手机或用户名不能为空" << endl;
        return;
    }

    if (passwd.compare(tmp) != 0)
    {
        cout << "密码不一致" << endl;
        return;
    }

    Json::Value val;
    val["type"] = ZC;
    val["user_tel"] = usertel;
    val["user_name"] = username;
    val["user_passwd"] = passwd;

    // 先存到变量，再用send_all循环发送
    string json_str = val.toStyledString();
    if (!send_all(sockfd, json_str)) return;

    char buff[256] = { 0 };
    if (recv(sockfd, buff, 255, 0) <= 0)
    {
        cout << "ser close" << endl;
        return;
    }

    val.clear();
    Json::Reader Read;
    if (!Read.parse(buff, val))
    {
        cout << "json 解析失败" << endl;
        return;
    }

    string s = val["status"].asString();
    if (s.compare("OK") != 0)
    {
        cout << "注册失败" << endl;
        return;
    }

    dl_flg = true;
    cout << "注册成功" << endl;
    return;
}

void socket_client::User_Login()
{
    string tel, passwd;
    cout << "请输入手机号码" << endl;
    cin >> tel;
    cout << "请输入密码" << endl;
    cin >> passwd;

    if (tel.empty() || passwd.empty())
    {
        cout << "账号或密码不能为空" << endl;
        return;
    }

    Json::Value val;
    val["type"] = DL;
    val["user_tel"] = tel;
    val["user_passwd"] = passwd;

    //先存到变量，再用send_all循环发送
    string json_str = val.toStyledString();
    if (!send_all(sockfd, json_str)) return;

    char buff[256] = { 0 };
    int n = recv(sockfd, buff, 255, 0);
    if (n <= 0)
    {
        cout << "ser close" << endl;
        return;
    }

    val.clear();
    Json::Reader Read;
    if (!Read.parse(buff, val))
    {
        cout << "解析json失败" << endl;
        return;
    }

    string st = val["status"].asString();
    if (st.compare("OK") != 0)
    {
        cout << "登陆失败" << endl;
        return;
    }

    dl_flg = true;
    username = val["user_name"].asString();
    usertel = tel;

    cout << "登陆成功" << endl;
    return;
}

void socket_client::User_Show_Ticket()
{
    Json::Value val;
    val["type"] = CKYY;
    //先存到变量，再用send_all循环发送
    string json_str = val.toStyledString();
    if (!send_all(sockfd, json_str)) return;
    //
    char buff[4096] = { 0 };
    int n = recv(sockfd, buff, 4095, 0);
    // cout<<buff<<endl;
    if (n <= 0)
    {
        cout << "ser close" << endl;
        return;
    }

    m_val.clear();
    Json::Reader Read;
    if (!Read.parse(buff, m_val))
    {
        cout << "json解析失败" << endl;
        return;
    }

    string st = m_val["status"].asString();
    if (st.compare("OK") != 0)
    {
        cout << "查询预约信息失败" << endl;
        return;
    }

    int num = m_val["num"].asInt();
    if (num == 0)
    {
        cout << "没有可预约的信息" << endl;
        return;
    }

    cout << "编号 地点名称   总票数  已预订  时间" << endl;
    for (int i = 0; i < num; i++)
    {
        cout << "---------------------------------------------------" << endl;
        cout << "| " << m_val["arr"][i]["tk_id"].asString() << "    ";
        cout << m_val["arr"][i]["add"].asString() << "     ";
        cout << m_val["arr"][i]["max"].asString() << "     ";
        cout << m_val["arr"][i]["num"].asString() << "    ";
        cout << m_val["arr"][i]["use_date"].asString() << "|" << endl;
        cout << "---------------------------------------------------" << endl;
    }
    cout << endl;
}

void socket_client::User_Subscribe_Ticket()
{
    User_Show_Ticket();
    cout << "请输入要预订的编号：" << endl;
    int index = 0;
    cin >> index;
    // index 有效性检查

    Json::Value val;
    val["type"] = YD;
    val["tel"] = usertel;
    val["index"] = index;

    // 【修复】先存到变量，再用send_all循环发送
    string json_str = val.toStyledString();
    if (!send_all(sockfd, json_str)) return;

    char buff[256] = { 0 };
    int n = recv(sockfd, buff, 255, 0);
    if (n <= 0)
    {
        cout << "ser close" << endl;
        return;
    }

    val.clear();
    Json::Reader Read;
    if (!Read.parse(buff, val))
    {
        cout << "json 解析失败" << endl;
        return;
    }

    string st = val["status"].asString();
    if (st.compare("OK") != 0)
    {
        cout << "预订失败" << endl;
        return;
    }

    cout << "预订成功" << endl;
    return;
}

void socket_client::User_Show_Sub_Ticket()
{
    // 1. 准备请求数据
    // 我们需要告诉服务器：我是谁（usertel），我想看我的预约（CKYD）
    Json::Value val;
    val["type"] = CKYD;   // 枚举值，对应 5
    val["tel"] = usertel; // 登录时记录下来的手机号

    // 2. 发送给服务端
    string json_str = val.toStyledString();
    if (send(sockfd, json_str.c_str(), json_str.size(), 0) <= 0) {
        cout << "与服务器断开连接..." << endl;
        return;
    }

    // 3. 接收服务端返回的列表
    // 预约列表可能比较长，所以我们准备一个大一点的缓冲区
    char buff[4096] = { 0 };
    int n = recv(sockfd, buff, 4095, 0);
    if (n <= 0) {
        cout << "服务器未响应..." << endl;
        return;
    }

    // 4. 解析返回的 JSON
    Json::Value res;
    Json::Reader read;
    if (!read.parse(buff, res)) {
        cout << "解析预约列表失败！" << endl;
        return;
    }

    if (res["status"].asString() != "OK") {
        cout << "查询预约失败：" << res["reason"].asString() << endl;
        return;
    }

    // 5. 格式化打印（这里是给面试官看的“面子工程”）
    int num = res["num"].asInt();
    if (num == 0) {
        cout << "\n[提示] 您当前没有任何预约记录。" << endl;
        return;
    }

    cout << "\n================ 我的预约列表 ================" << endl;
    printf("%-10s | %-20s | %-20s\n", "记录ID", "地点名称", "预约时间");
    cout << "----------------------------------------------" << endl;

    for (int i = 0; i < num; i++) {
        // 注意：这里的字段名要和服务器给的 resval["arr"] 里的 key 一致
        printf("%-10s | %-20s | %-20s\n",
            res["arr"][i]["sub_id"].asString().c_str(),
            res["arr"][i]["addr"].asString().c_str(),
            res["arr"][i]["time"].asString().c_str());
    }
    cout << "==============================================" << endl;
}

void socket_client::User_Cancel_Sub_Ticket()
{
    // 1. 先调用查看函数，让用户看到自己的预约 ID
    User_Show_Sub_Ticket();

    cout << "请输入要取消的预约记录 ID (输入 0 取消操作): ";
    int sub_id;
    cin >> sub_id;
    if (sub_id == 0) return;

    // 2. 封装 JSON 发给服务端
    Json::Value val;
    val["type"] = QXYD; // 对应枚举中的取消预约
    val["sub_id"] = sub_id;

    // 【修复】先存到变量，再用send_all循环发送
    string json_str = val.toStyledString();
    if (!send_all(sockfd, json_str)) return;

    // 3. 接收结果
    char buff[256] = { 0 };
    if (recv(sockfd, buff, 255, 0) <= 0) return;

    Json::Value res;
    Json::Reader read;
    read.parse(buff, res);

    if (res["status"].asString() == "OK") {
        cout << "取消成功！名额已退还。" << endl;
    }
    else {
        cout << "取消失败，请检查 ID 是否正确。" << endl;
    }
}

void socket_client::Run()
{
    while (runing)
    {
        print_info();

        switch (user_op)
        {
        case DL:
            User_Login();
            break;
        case ZC:
            User_Register();
            break;
        case CKYY:
            User_Show_Ticket();
            break;
        case YD:
            User_Subscribe_Ticket();
            break;
        case QXYD:
            User_Cancel_Sub_Ticket();
            break;
        case CKYD:
            User_Show_Sub_Ticket();
            break;
        case TC:
            runing = false;
            break;
        default:
            cout << "输入无效" << endl;
            break;
        }
    }
}

int main()
{
    socket_client cli;
    if (!cli.Connect_server())
    {
        exit(1);
    }

    cli.Run();

    exit(0);
}