/**
 *
 * 编译：g++ stress_test.cpp -o stress_test -lpthread
 * 运行：./stress_test [并发数] [每线程请求数]
 * 示例：./stress_test 50 100   → 50并发，每人发100次查询
 *
 * 测试前确保服务器已启动：./server
 */

#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>
#include <numeric>
#include <algorithm>
#include <mutex>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>

using namespace std;
using namespace chrono;

// ========== 配置区 ==========
const char*  SERVER_IP      = "127.0.0.1";
const short  SERVER_PORT    = 6000;
const string TEST_TEL       = "11111111111"; // 改成数据库里真实存在的手机号
const string TEST_PASSWD    = "111111";     // 对应密码
// ============================

atomic<int>      success_count(0);
atomic<int>      fail_count(0);
mutex            latency_mutex;
vector<long long> all_latencies; // 收集每次请求延迟，用于算分位数

// 循环发送，保证发完
bool send_all(int sockfd, const string& data) {
    int sent = 0, total = data.size();
    while (sent < total) {
        int n = send(sockfd, data.c_str() + sent, total - sent, 0);
        if (n <= 0) return false;
        sent += n;
    }
    return true;
}

// 发一次请求，返回是否成功，同时记录延迟
bool do_request(int sockfd, const string& req, long long& out_ms) {
    auto t0 = high_resolution_clock::now();

    if (!send_all(sockfd, req)) return false;

    char buf[4096] = {0};
    int n = recv(sockfd, buf, 4095, 0);

    auto t1 = high_resolution_clock::now();
    out_ms = duration_cast<microseconds>(t1 - t0).count() / 1000.0;

    if (n <= 0) return false;
    return string(buf, n).find("\"OK\"") != string::npos;
}

void worker(int thread_id, int req_count) {
    // ===== 第一步：建立一次 TCP 连接 =====
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) { fail_count += req_count; return; }

    struct sockaddr_in saddr;
    memset(&saddr, 0, sizeof(saddr));
    saddr.sin_family      = AF_INET;
    saddr.sin_port        = htons(SERVER_PORT);
    saddr.sin_addr.s_addr = inet_addr(SERVER_IP);

    if (connect(sockfd, (struct sockaddr*)&saddr, sizeof(saddr)) == -1) {
        close(sockfd); fail_count += req_count; return;
    }

    // ===== 第二步：登录一次 =====
    string login_req =
        "{\"type\":1,\"user_tel\":\"" + TEST_TEL +
        "\",\"user_passwd\":\"" + TEST_PASSWD + "\"}";
    long long dummy;
    if (!do_request(sockfd, login_req, dummy)) {
        close(sockfd); fail_count += req_count; return;
    }

    // ===== 第三步：在同一连接上反复发查询，测真实处理速度 =====
    string query_req = "{\"type\":3}";
    vector<long long> local_latencies;
    local_latencies.reserve(req_count);

    for (int i = 0; i < req_count; i++) {
        long long ms = 0;
        if (do_request(sockfd, query_req, ms)) {
            success_count++;
            local_latencies.push_back(ms);
        } else {
            fail_count++;
        }
    }

    close(sockfd);

    // 汇总到全局延迟数组
    lock_guard<mutex> lock(latency_mutex);
    all_latencies.insert(all_latencies.end(),
                         local_latencies.begin(), local_latencies.end());
}

int main(int argc, char* argv[]) {
    int concurrency    = 50;
    int req_per_thread = 100;

    if (argc >= 2) concurrency    = atoi(argv[1]);
    if (argc >= 3) req_per_thread = atoi(argv[2]);

    int total_requests = concurrency * req_per_thread;

    cout << "======================================" << endl;
    cout << "  预约系统压力测试 v2（长连接版）"       << endl;
    cout << "  服务器:       " << SERVER_IP << ":" << SERVER_PORT << endl;
    cout << "  并发连接数:   " << concurrency         << endl;
    cout << "  每连接请求数: " << req_per_thread      << endl;
    cout << "  总请求数:     " << total_requests      << endl;
    cout << "======================================" << endl;
    cout << "测试中，请稍候..." << endl;

    auto global_start = high_resolution_clock::now();

    vector<thread> threads;
    threads.reserve(concurrency);
    for (int i = 0; i < concurrency; i++)
        threads.emplace_back(worker, i, req_per_thread);
    for (auto& t : threads) t.join();

    auto global_end = high_resolution_clock::now();
    double total_sec = duration_cast<milliseconds>(global_end - global_start).count() / 1000.0;

    int succ = success_count.load();
    int fail = fail_count.load();

    // 计算延迟统计
    double avg_ms = 0, p99_ms = 0, min_ms = 0, max_ms = 0;
    if (!all_latencies.empty()) {
        sort(all_latencies.begin(), all_latencies.end());
        long long sum = 0;
        for (auto v : all_latencies) sum += v;
        avg_ms = (double)sum / all_latencies.size();
        min_ms = all_latencies.front();
        max_ms = all_latencies.back();
        int p99_idx = (int)(all_latencies.size() * 0.99);
        p99_ms = all_latencies[p99_idx];
    }

    double qps          = total_sec > 0 ? succ / total_sec : 0;
    double success_rate = total_requests > 0 ? (double)succ / total_requests * 100.0 : 0;

    cout << endl;
    cout << "============= 测试结果 =============" << endl;
    printf("  总耗时:           %.2f 秒\n",    total_sec);
    printf("  成功请求:          %d / %d\n",   succ, total_requests);
    printf("  成功率:            %.1f%%\n",     success_rate);
    printf("  平均响应时间:      %.1f ms\n",    avg_ms);
    printf("  最小响应时间:      %.1f ms\n",    min_ms);
    printf("  最大响应时间:      %.1f ms\n",    max_ms);
    printf("  P99 响应时间:      %.1f ms\n",    p99_ms);
    printf("  吞吐量 (QPS):      %.0f 请求/秒\n", qps);
    cout << "=====================================" << endl;

    cout << endl;
    cout << "========= 简历可用数据 ==========" << endl;
    printf("  并发 %d 连接，成功率 %.1f%%\n",  concurrency, success_rate);
    printf("  平均响应 %.1f ms，P99 %.1f ms\n", avg_ms, p99_ms);
    printf("  QPS %.0f\n",                      qps);
    cout << "==================================" << endl;

    return 0;
}
