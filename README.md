# 高性能分布式在线预约系统 (High-Performance Reservation System)

## 项目简介
本项目是一款基于 C/S 架构的企业级资源预约平台，通过 Libevent 异步库与 MySQL 事务机制，实现从用户鉴权、资源调度到审计的闭环流程。旨在解决高并发场景下的名额抢占、数据一致性及用户信息安全问题。

## 技术栈
*   **语言**: C++ (C++11)
*   **通信底层**: Libevent (Reactor 模型)
*   **数据库**: MySQL (事务支持)
*   **数据交换**: Jsoncpp
*   **安全加密**: OpenSSL (SHA-256)
*   **设计模式**: DAO 层封装、RBAC 权限模型

## 核心特性

### 1. 并发冲突控制 (Concurrency Control)
*   应用 **MySQL 事务** 与 **行级锁 (Select for Update)** 实现“查询-扣减-记录”原子化操作。
*   严防“超卖/重报”现象，确保高并发场景下的数据强一致性。

### 2. 安全加固体系 (Security)
*   **加盐哈希**: 采用 **SHA-256 + 动态随机盐 (Salt)** 实现密码持久化，杜绝明文风险，防范彩虹表攻击。
*   **预处理语句**: 全面应用 **Prepared Statements**，从根源杜绝 SQL 注入攻击。
*   **权限管理**: 基于 **RBAC 模型** 构建权限校验层，有效拦截非法越权操作。

### 3. 数据库封装优化 (Database Optimization)
*   自主封装 **DAO 层**，解耦业务逻辑与底层存储。
*   针对高频预约查询字段（`tel_id`, `tk_id`）建立**复合索引**，大幅提升检索效率。

## 快速开始

### 依赖安装
```bash
sudo apt-get install libmysqlclient-dev libevent-dev libjsoncpp-dev libssl-dev

#编译
## 编译服务端
g++ ser.cpp -o server -lmysqlclient -levent -ljsoncpp -lpthread -lcrypto

## 编译客户端
g++ client.cpp -o client -ljsoncpp


#数据库初始化
CREATE DATABASE Project_DB;
-- (此处省略建表语句)
