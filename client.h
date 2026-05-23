#include <iostream>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <string.h>
#include <string>
#include <jsoncpp/json/json.h>

using namespace std;
const int OFFSET = 2;

enum OP_TYPE {DL = 1, ZC,CKYY,YD,CKYD,QXYD,TC};

inline bool send_all(int sockfd, const string& data)
{
    const char* buf = data.c_str();
    int total = (int)data.size();
    int sent  = 0;
    while (sent < total)
    {
        int n = send(sockfd, buf + sent, total - sent, 0);
        if (n <= 0)
        {
            cout << "send error" << endl;
            return false;
        }
        sent += n;
    }
    return true;
}

//ctrl shift  i
class socket_client
{
public:
    socket_client()
    {
        sockfd = -1;
        ips = "127.0.0.1";
        port = 6000;
        dl_flg = false;
        user_op = 0;
        runing = true;
    }

    socket_client(string ips, short port)
    {
        sockfd = -1;
        this->ips = ips;
        this->port = port;
        dl_flg = false;
        user_op = 0;
        runing = true;
    }

    void print_info();

    ~socket_client()
    {
        close(sockfd);
    }

    bool Connect_server();
    void User_Register();
    void User_Login();
    void User_Show_Ticket();
    void User_Subscribe_Ticket();
    void User_Show_Sub_Ticket();
    void User_Cancel_Sub_Ticket();

    void Run();

private:
    string ips;
    short port;
    int sockfd;

    bool dl_flg;

    string username;
    string usertel;

    int  user_op;
    bool runing;

    Json::Value m_val;
};