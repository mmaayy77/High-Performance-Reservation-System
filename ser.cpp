#include "ser.h"
#include <openssl/sha.h>
#include <iomanip>
#include <sstream>
#include <random>

// 生成 16 位随机盐值
string generate_salt(int len = 16) {
    const char char_set[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    string res = "";
    random_device rd;
    mt19937 gen(rd());
    uniform_int_distribution<> dis(0, sizeof(char_set) - 2);
    for (int i = 0; i < len; ++i) {
        res += char_set[dis(gen)];
    }
    return res;
}

// SHA-256 计算
string sha256_hash(const string str) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, str.c_str(), str.size());
    SHA256_Final(hash, &sha256);
    stringstream ss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        ss << hex << setw(2) << setfill('0') << (int)hash[i];
    }
    return ss.str();
}

// mysqlclient
bool mysql_client::mysql_ConnectServer()
{
    MYSQL* mysql = mysql_init(&mysql_con);
    if (mysql == NULL)
    {
        return false;
    }

    mysql = mysql_real_connect(mysql, db_ips.c_str(), db_username.c_str(), db_passwd.c_str(), db_dbname.c_str(), 3306, NULL, 0);
    if (mysql == NULL)
    {
        cout << "connect db server err" << endl;
        return false;
    }

    return true;
}

bool mysql_client::mysql_Register(const string& tel, const string& passwd, const string& name)
{
    // 1. 依然先生成盐和哈希
    string salt = generate_salt();
    string hashed_pwd = sha256_hash(passwd + salt);

    // 2. 初始化预处理语句句柄
    MYSQL_STMT* stmt = mysql_stmt_init(&mysql_con);
    if (!stmt) return false;

    // 3. 准备 SQL 模板，使用 ? 占位符
    const char* sql = "INSERT INTO user_info(tel_id, name, passwd, salt, status) VALUES(?, ?, ?, ?, 1)";
    if (mysql_stmt_prepare(stmt, sql, strlen(sql)) != 0) {
        mysql_stmt_close(stmt);
        return false;
    }

    // 4. 定义绑定参数数组
    MYSQL_BIND bind[4];
    memset(bind, 0, sizeof(bind));

    // 绑定 tel_id (第一个 ?)
    bind[0].buffer_type = MYSQL_TYPE_STRING;
    bind[0].buffer = (char*)tel.c_str();
    bind[0].buffer_length = tel.length();

    // 绑定 name (第二个 ?)
    bind[1].buffer_type = MYSQL_TYPE_STRING;
    bind[1].buffer = (char*)name.c_str();
    bind[1].buffer_length = name.length();

    // 绑定 passwd (第三个 ?)
    bind[2].buffer_type = MYSQL_TYPE_STRING;
    bind[2].buffer = (char*)hashed_pwd.c_str();
    bind[2].buffer_length = hashed_pwd.length();

    // 绑定 salt (第四个 ?)
    bind[3].buffer_type = MYSQL_TYPE_STRING;
    bind[3].buffer = (char*)salt.c_str();
    bind[3].buffer_length = salt.length();

    // 5. 将参数绑定到预处理语句
    if (mysql_stmt_bind_param(stmt, bind) != 0) {
        mysql_stmt_close(stmt);
        return false;
    }

    // 6. 执行！
    if (mysql_stmt_execute(stmt) != 0) {
        cout << "预处理执行失败: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return false;
    }

    // 7. 清理
    mysql_stmt_close(stmt);
    return true;
}

bool mysql_client::mysql_Login(const string& tel, const string& passwd, string& name)
{
    // 1. 使用预处理语句查询
    MYSQL_STMT* stmt = mysql_stmt_init(&mysql_con);
    const char* sql = "SELECT name, passwd, salt FROM user_info WHERE tel_id = ?";
    mysql_stmt_prepare(stmt, sql, strlen(sql));

    // 2. 绑定输入参数 (tel)
    MYSQL_BIND bind_in[1];
    memset(bind_in, 0, sizeof(bind_in));
    bind_in[0].buffer_type = MYSQL_TYPE_STRING;
    bind_in[0].buffer = (char*)tel.c_str();
    bind_in[0].buffer_length = tel.length();
    mysql_stmt_bind_param(stmt, bind_in);

    // 3. 执行并绑定输出参数
    mysql_stmt_execute(stmt);

    char db_name[50], db_pass[128], db_salt[32];
    unsigned long len[3];
    MYSQL_BIND bind_out[3];
    memset(bind_out, 0, sizeof(bind_out));

    bind_out[0].buffer_type = MYSQL_TYPE_STRING; bind_out[0].buffer = db_name; bind_out[0].buffer_length = 50; bind_out[0].length = &len[0];
    bind_out[1].buffer_type = MYSQL_TYPE_STRING; bind_out[1].buffer = db_pass; bind_out[1].buffer_length = 128; bind_out[1].length = &len[1];
    bind_out[2].buffer_type = MYSQL_TYPE_STRING; bind_out[2].buffer = db_salt; bind_out[2].buffer_length = 32; bind_out[2].length = &len[2];

    mysql_stmt_bind_result(stmt, bind_out);

    if (mysql_stmt_fetch(stmt) != 0) {
        mysql_stmt_close(stmt);
        return false; // 用户不存在
    }

    // 4. 校验哈希
    string input_hash = sha256_hash(passwd + string(db_salt, len[2]));
    if (input_hash != string(db_pass, len[1])) {
        mysql_stmt_close(stmt);
        return false; // 密码错误
    }

    name = string(db_name, len[0]);
    mysql_stmt_close(stmt);
    return true;
}

bool mysql_client::mysql_Show_Ticket(Json::Value& resval)
{
    string sql = "select tk_id,addr,max,num ,use_date from ticket_info";
    if (mysql_query(&mysql_con, sql.c_str()) != 0)
    {
        cout << "show ticket err" << endl;
        return false;
    }

    MYSQL_RES* r = mysql_store_result(&mysql_con);
    if (r == NULL)
    {
        return false;
    }

    int n = mysql_num_rows(r);
    if (n == 0)
    {
        resval["status"] = "OK";
        resval["num"] = 0;
        return true;
    }

    resval["status"] = "OK";
    resval["num"] = n;

    for (int i = 0; i < n; i++)
    {
        MYSQL_ROW row = mysql_fetch_row(r);
        Json::Value tmp;
        tmp["tk_id"] = row[0];
        tmp["add"] = row[1];
        tmp["max"] = row[2];
        tmp["num"] = row[3];
        tmp["use_date"] = row[4];
        resval["arr"].append(tmp);
    }

    mysql_free_result(r); // 【修复】释放结果集，防止内存泄漏
    return true;
}

bool mysql_client::mysql_user_begin()
{
    if (mysql_query(&mysql_con, "begin") != 0)
    {
        return false;
    }
    return true;
}

bool mysql_client::mysql_user_commit()
{
    if (mysql_query(&mysql_con, "commit") != 0)
    {
        return false;
    }
    return true;
}

bool mysql_client::mysql_user_rollback()
{
    if (mysql_query(&mysql_con, "rollback") != 0)
    {
        return false;
    }
    return true;
}

bool mysql_client::mysql_Subscribe_Ticket(int tk_id, string tel)
{
    mysql_user_begin(); // 启动事务

    // 加上 FOR UPDATE 开启行级锁
    // 这样当一个事务在查询时，其他事务必须等待，直到当前事务提交
    string s1 = "SELECT max, num FROM ticket_info WHERE tk_id =" + to_string(tk_id) + " FOR UPDATE";

    if (mysql_query(&mysql_con, s1.c_str()) != 0)
    {
        cout << "查询失败: " << mysql_error(&mysql_con) << endl;
        mysql_user_rollback();
        return false;
    }

    MYSQL_RES* r = mysql_store_result(&mysql_con);
    if (r == NULL || mysql_num_rows(r) != 1)
    {
        cout << "获取结果集失败或记录不存在" << endl;
        if (r) mysql_free_result(r);
        mysql_user_rollback();
        return false;
    }

    int Num = mysql_num_rows(r);
    if (Num != 1)
    {
        cout << "记录行不为一" << endl;
        mysql_user_rollback();
        return false;
    }

    MYSQL_ROW row = mysql_fetch_row(r);
    string str_max = row[0]; // 总票数
    string str_num = row[1]; // 当前已定票数
    int tk_max = atoi(str_max.c_str());
    int tk_num = atoi(str_num.c_str());
    mysql_free_result(r);
    if (tk_max <= tk_num)
    {
        cout << "没有可用的票" << endl;
        mysql_user_rollback();
        return false;
    }

    tk_num++;
    // update ticket_info set num=3 where tk_id=1;
    string s2 = string("update ticket_info set num=") + to_string(tk_num) + string(" where tk_id=") + to_string(tk_id);
    if (mysql_query(&mysql_con, s2.c_str()) != 0)
    {
        cout << "修改预订票数失败" << endl;
        mysql_user_rollback();
        return false;
    }

    // sub_ticket
    // insert into sub_ticket values(0,1,'1350000000',now());
    string s3 = string("insert into sub_ticket values(0,") + to_string(tk_id) + string(",'") + tel + string("',now())");
    if (mysql_query(&mysql_con, s3.c_str()) != 0)
    {
        cout << "存入预订信息失败" << endl;
        mysql_user_rollback();
        return false;
    }

    mysql_user_commit();
    return true;
}

bool mysql_client::mysql_Cancel_Reservation(int sub_id)
{
    // 1. 启动事务
    mysql_user_begin();

    // 2. 先查一下这条预约记录对应的 tk_id 是多少（因为后面要把那张票的 num 减1）
    string s1 = "SELECT tk_id FROM sub_ticket WHERE id = " + to_string(sub_id);
    if (mysql_query(&mysql_con, s1.c_str()) != 0) {
        mysql_user_rollback();
        return false;
    }

    MYSQL_RES* r = mysql_store_result(&mysql_con);
    if (r == NULL || mysql_num_rows(r) == 0) {
        if (r) mysql_free_result(r);
        mysql_user_rollback(); // 记录不存在，取消失败
        return false;
    }
    MYSQL_ROW row = mysql_fetch_row(r);
    int tk_id = atoi(row[0]);
    mysql_free_result(r); // 【重要】执行下一个 SQL 前必须释放

    // 3. 删除预约记录
    string s2 = "DELETE FROM sub_ticket WHERE id = " + to_string(sub_id);
    if (mysql_query(&mysql_con, s2.c_str()) != 0) {
        mysql_user_rollback();
        return false;
    }

    // 4. 将对应的 ticket_info 表中的 num 减 1
    string s3 = "UPDATE ticket_info SET num = num - 1 WHERE tk_id = " + to_string(tk_id);
    if (mysql_query(&mysql_con, s3.c_str()) != 0) {
        mysql_user_rollback();
        return false;
    }

    // 5. 提交事务
    mysql_user_commit();
    return true;
}

bool mysql_client::mysql_user_view_reservation(const string& tel, Json::Value& resval)
{
    // 使用 JOIN 关联查询，体现数据库功底
    // s.sub_time 是预约时间，t.addr 是门票地点
    string sql = "SELECT s.id, t.addr, s.sub_time FROM sub_ticket s "
        "JOIN ticket_info t ON s.tk_id = t.tk_id "
        "WHERE s.tel = '" + tel + "'";

    if (mysql_query(&mysql_con, sql.c_str()) != 0) {
        return false;
    }

    MYSQL_RES* r = mysql_store_result(&mysql_con);
    if (r == NULL) return false;

    int n = mysql_num_rows(r);
    resval["status"] = "OK";
    resval["num"] = n;

    for (int i = 0; i < n; i++) {
        MYSQL_ROW row = mysql_fetch_row(r);
        Json::Value tmp;
        tmp["sub_id"] = row[0];   // 预约记录ID
        tmp["addr"] = row[1];     // 门票地点
        tmp["time"] = row[2];     // 预约操作时间
        resval["arr"].append(tmp);
    }
    mysql_free_result(r);
    return true;
}

// socket_listen
bool socket_listen::socket_init()
{
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (-1 == sockfd)
    {
        return false;
    }

    //设置端口复用，防止 bind err
    int opt = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in saddr;
    memset(&saddr, 0, sizeof(saddr));
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(m_port);
    saddr.sin_addr.s_addr = inet_addr(m_ips.c_str());

    int res = bind(sockfd, (struct sockaddr*)&saddr, sizeof(saddr));
    if (-1 == res)
    {
        cout << "bind err" << endl;
        close(sockfd);
        return false;
    }

    res = listen(sockfd, LIS_MAX);
    if (-1 == res)
    {
        return false;
    }

    return true;
}

int socket_listen::accept_client()
{
    int c = accept(sockfd, NULL, NULL);
    return c;
}

//---socket_con

void socket_con::Send_err()
{
    // 【修复】固定字符串，避免两次调用toStyledString()
    string out = "{\"status\":\"ERR\"}\n";
    send(c, out.c_str(), out.size(), 0);
}

void socket_con::Send_ok()
{
    // 【修复】固定字符串，避免两次调用toStyledString()
    string out = "{\"status\":\"OK\"}\n";
    send(c, out.c_str(), out.size(), 0);
}

void socket_con::User_Register()
{
    string tel, passwd, username;
    tel = val["user_tel"].asString();
    passwd = val["user_passwd"].asString();
    username = val["user_name"].asString();

    if (tel.empty() || passwd.empty() || username.empty())
    {
        Send_err();
    }

    // 【改进】直接使用成员变量 cli，不再重复建立连接
    if (!cli.mysql_Register(tel, passwd, username))
    {
        Send_err();
        return;
    }

    Send_ok();
    return;
}

void socket_con::User_Login()
{
    string tel = val["user_tel"].asString();
    string passwd = val["user_passwd"].asString();
    string user_name;

    // 【改进】直接使用成员变量 cli
    if (!cli.mysql_Login(tel, passwd, user_name))
    {
        Send_err();
        return;
    }

    Json::Value res_val;
    res_val["status"] = "OK";
    res_val["user_name"] = user_name;
    send(c, res_val.toStyledString().c_str(), strlen(res_val.toStyledString().c_str()), 0);
    return;
}

void socket_con::User_Show_Ticket()
{
    Json::Value resval;
    // 【改进】直接使用成员变量 cli
    if (!cli.mysql_Show_Ticket(resval))
    {
        Send_err();
        return;
    }

    send(c, resval.toStyledString().c_str(), strlen(resval.toStyledString().c_str()), 0);
    return;
}

void socket_con::User_Subscribe_Ticket()
{
    // client  -> tk_id, tel
    int tk_id = val["index"].asInt();
    string tel = val["tel"].asString();

    // 【改进】直接使用成员变量 cli
    if (!cli.mysql_Subscribe_Ticket(tk_id, tel))
    {
        Send_err();
        return;
    }

    Send_ok();
    return;
}

void socket_con::User_Show_Sub_Ticket()
{
    // 1. 获取客户端传来的手机号
    string tel = val["tel"].asString();

    Json::Value resval;
    // 【改进】直接使用成员变量 cli
    if (!cli.mysql_user_view_reservation(tel, resval)) {
        Send_err();
        return;
    }

    // 4. 发送结果给客户端
    string out = resval.toStyledString();
    send(c, out.c_str(), out.size(), 0);
}

void socket_con::User_Cancel_Sub_Ticket()
{
    // 客户端发过来的JSON：{"type":6, "sub_id": 5}
    int sub_id = val["sub_id"].asInt();

    // 【改进】直接使用成员变量 cli
    if (!cli.mysql_Cancel_Reservation(sub_id)) {
        Send_err();
        return;
    }

    // 成功了回复 OK
    Send_ok();
}

void socket_con::Recv_data()
{
    // 【修复】缓冲区扩大到4096，防止注册等长包被截断
    char buff[4096] = { 0 };
    int n = recv(c, buff, 4095, 0);
    if (n <= 0)
    {
        cout << "client close" << endl;
        delete this;
        return;
    }

    // 测试
    cout << "recv:" << buff << endl;

    Json::Reader Read;
    if (!Read.parse(buff, val))
    {
        cout << "Recv_data:解析json失败" << endl;
        Send_err();
        return;
    }

    int ops = val["type"].asInt();
    // DL = 1, ZC,CKYY,YD,CKYD,QXYD,TC
    switch (ops)
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
    case CKYD:
        User_Show_Sub_Ticket();
        break;
    case QXYD:
        User_Cancel_Sub_Ticket();
        break;
    default:
        break;
    }
    // 解析
}

// callback
void SOCK_CON_CALLBACK(int fd, short ev, void* arg)
{
    socket_con* q = (socket_con*)arg;

    if (ev & EV_READ)
    {
        q->Recv_data();
    }
}

void SOCK_LIS_CALLBACK(int sockfd, short ev, void* arg)
{
    socket_listen* p = (socket_listen*)arg;
    if (p == NULL)
    {
        return;
    }

    if (ev & EV_READ) // 处理读事件
    {
        int c = p->accept_client();
        if (c == -1)
        {
            return;
        }

        cout << "accept : c=" << c << endl;

        socket_con* q = new socket_con(c);

        struct event* c_ev = event_new(p->Get_base(), c, EV_READ | EV_PERSIST, SOCK_CON_CALLBACK, q);
        if (c_ev == NULL)
        {
            close(c);
            delete q;
            return;
        }
        q->Set_ev(c_ev);
        // 添加到libevent
        event_add(c_ev, NULL);
    }
}


int main()
{
    // 监听套接字
    socket_listen sock_ser;
    if (!sock_ser.socket_init())
    {
        cout << "socket init err" << endl;
        exit(1);
    }

    // 创建lievent base
    struct event_base* base = event_init();
    if (base == NULL)
    {
        cout << "base null" << endl;
        exit(1);
    }

    // 设置socket_listen中的libevent的base
    sock_ser.Set_base(base);
    // 添加sockfd到libevent
    struct event* sock_ev = event_new(base, sock_ser.Get_sockfd(), EV_READ | EV_PERSIST, SOCK_LIS_CALLBACK, &sock_ser);
    event_add(sock_ev, NULL);
    // 启动事件循环
    event_base_dispatch(base); // select poll epoll

    // 释放资源
    event_free(sock_ev);
    event_base_free(base);
}