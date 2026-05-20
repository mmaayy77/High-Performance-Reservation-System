// 多线程压力测试工具，用于验证系统在高并发下的稳定性

#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <atomic>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <jsoncpp/json/json.h>

using namespace std;

// 全局计数器（线程安全）
atomic<int> total_success(0);
atomic<int> total_failed(0);

// 压测参数配置
const string SERVER_IP = "127.0.0.1";
const int SERVER_PORT = 6000;
const int CONCURRENCY = 20;    // 并发线程数（模拟20个用户同时在线）
const int REQUESTS_PER_THREAD = 500; // 每个用户发送多少次请求

void worker() {
    int local_success = 0;
    int local_failed = 0;

    for (int i = 0; i < REQUESTS_PER_THREAD; ++i) {
        int sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd < 0) { local_failed++; continue; }

        struct sockaddr_in saddr;
        memset(&saddr, 0, sizeof(saddr));
        saddr.sin_family = AF_INET;
        saddr.sin_port = htons(SERVER_PORT);
        saddr.sin_addr.s_addr = inet_addr(SERVER_IP.c_str());

        if (connect(sockfd, (struct sockaddr *)&saddr, sizeof(saddr)) < 0) {
            local_failed++;
            close(sockfd);
            continue;
        }

        // 构造“纯查询”请求 JSON
        Json::Value val;
        val["type"] = 3; // CKYY 对应枚举值 3

        string send_data = val.toStyledString();
        send(sockfd, send_data.c_str(), send_data.size(), 0);

        // 接收响应（压测时不打印，只接收以清空缓冲区）
        char buff[4096];
        int n = recv(sockfd, buff, sizeof(buff), 0);
        if (n > 0) {
            local_success++;
        } else {
            local_failed++;
        }

        close(sockfd); // 模拟短连接频繁请求
    }

    total_success += local_success;
    total_failed += local_failed;
}

int main() {
    cout << "--- 压力测试开始 ---" << endl;
    cout << "并发数: " << CONCURRENCY << " | 每个线程请求数: " << REQUESTS_PER_THREAD << endl;
    cout << "总请求预设: " << CONCURRENCY * REQUESTS_PER_THREAD << endl;

    auto start_time = chrono::high_resolution_clock::now();

    // 启动线程池
    vector<thread> threads;
    for (int i = 0; i < CONCURRENCY; ++i) {
        threads.push_back(thread(worker));
    }

    // 等待所有线程结束
    for (auto &t : threads) {
        t.join();
    }

    auto end_time = chrono::high_resolution_clock::now();
    chrono::duration<double> diff = end_time - start_time;

    // 计算结果
    double seconds = diff.count();
    double qps = total_success / seconds;

    cout << "\n--- 测试结果 ---" << endl;
    cout << "成功总数: " << total_success << endl;
    cout << "失败总数: " << total_failed << endl;
    cout << "总耗时: " << seconds << " 秒" << endl;
    cout << "平均响应时间: " << (seconds / total_success) * 1000 << " 毫秒" << endl;
    cout << "\033[32;1mQPS (每秒处理请求数): " << qps << "\033[0m" << endl;

    return 0;
}
