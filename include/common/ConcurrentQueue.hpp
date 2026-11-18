/**
  ******************************************************************************
  * @file    ConcurrentQueue.hpp
  * @author  工科女的日常:The Daily Life of An Engineering Girl - RedBook - NewCoder - CSDN_ WeChat Official Account
  * @details link——RedBook: http://xhslink.com/a/ipmoHKmrFZWfb     NewCoder:https://www.nowcoder.com/share/jump/7241315122413047270
  * @version V1.0.0
  * @date    8-April-2026
  * @brief   Header File
  ******************************************************************************
  * @attention
  * Copyright (c) 2026-2036 The Daily Life of An Engineering Girl(工科女的日常).
  * All rights reserved.
  *
  * *----------------------------------------------------------------------------*/

/**
 * ============================================================================
 * @brief   线程安全并发队列 — 模板类头文件
 * ----------------------------------------------------------------------------
 * @details 本文件定义了一个基于互斥锁（std::mutex）与条件变量
 *          （std::condition_variable）实现的线程安全先进先出队列模板类
 *          ConcurrentQueue<T>，位于命名空间 MultiProject 内。
 *
 *          本类在本项目中承担多线程流水线各阶段之间的数据传递职责：
 *            采集线程 → [ConcurrentQueue] → 预处理线程
 *            预处理线程 → [ConcurrentQueue] → 推理线程
 *            推理线程  → [ConcurrentQueue] → 报警/日志线程
 *          以上各阶段线程通过本队列解耦，生产者和消费者无需彼此感知，
 *          可在不同速率下异步运行。
 *
 *          本类对外提供以下 4 个公共接口：
 *
 *          1. push(item)  — 生产者接口：将元素入队并通知等待的消费者
 *          2. pop(item)   — 消费者接口：阻塞等待直至队列非空或收到停止信号，
 *                           取出队首元素；停止信号触发后队列排空则返回 false
 *          3. stop()      — 停止信号接口：置位 stop_ 标志并唤醒所有等待线程，
 *                           使所有阻塞在 pop() 中的消费者线程优雅退出
 *          4. size()      — 查询接口：线程安全地返回当前队列长度
 *
 * @note    本类采用 header-only 设计，模板实现全部位于头文件中，
 *          无需额外的 .cpp 编译单元。
 *          包含本头文件的编译单元须链接 pthread 库（Linux 平台）
 *          以支持 std::mutex 和 std::condition_variable 的底层实现。
 *
 * @par     线程安全性说明
 *          push() / stop() / size() 使用 std::lock_guard 实现互斥保护；
 *          pop() 使用 std::unique_lock 配合条件变量等待，
 *          支持多生产者多消费者（MPMC）并发访问模式。
 *          stop_ 成员虽在 stop() 中通过互斥锁写入，
 *          但在 pop() 的条件变量谓词 lambda 中读取时互斥锁已持有，
 *          线程安全性由互斥锁保证，无需额外的 std::atomic 修饰。
 * ============================================================================
 */

#ifndef CONCURRENT_QUEUE_HPP
// 头文件保护宏：若本头文件已被包含（宏已定义），则跳过全部内容，防止重复包含
#define CONCURRENT_QUEUE_HPP
// 定义头文件保护宏，与上方 #ifndef 配对使用

#include <queue>
// 引入 std::queue，作为 ConcurrentQueue 内部存储数据的底层容器；
// std::queue 默认以 std::deque 为底层实现，提供 push()/front()/pop() 等 FIFO 接口

#include <mutex>
// 引入 std::mutex 和 std::lock_guard、std::unique_lock；
// std::mutex 提供基本的排他性互斥锁原语，
// std::lock_guard 用于 RAII 方式自动加锁/解锁（不可手动解锁），
// std::unique_lock 用于配合条件变量等待（可在等待期间自动释放锁）

#include <condition_variable>
// 引入 std::condition_variable；
// 条件变量允许消费者线程在队列为空时进入高效休眠状态（释放 CPU），
// 并在生产者 push() 后被精确唤醒，避免忙等待（busy-waiting）轮询

#include <atomic>
// 引入 std::atomic；
// 本文件中 stop_ 成员当前声明为普通 bool，由互斥锁保护；
// 引入本头文件为后续可能的无锁优化（如将 stop_ 改为 std::atomic<bool>）预留扩展空间

namespace MultiProject {
// 开启命名空间 MultiProject，将 ConcurrentQueue 类包裹其中，
// 避免与标准库或其他第三方库中的同名队列类产生符号冲突；
// 本项目中轴承监测（BearingVibrationProject）
// 均在此命名空间下共享本队列模板

template<typename T>
// 声明模板类型参数 T，表示队列中存储的元素类型；
// T 可以是任意可复制构造（CopyConstructible）的类型，
// 例如：ConcurrentQueue<cv::Mat>（图像帧）、
//       ConcurrentQueue<VibrationData>（振动数据包）、
//       ConcurrentQueue<std::string>（日志消息）
class ConcurrentQueue {
// ConcurrentQueue 类定义开始；
// 本类封装了一个线程安全的 FIFO 队列，适用于生产者/消费者多线程模型
public:
// 公共接口区域：以下方法可被外部调用方（生产者线程和消费者线程）直接访问

    void push(const T& item) {
        // push()：生产者接口，将一个元素追加到队尾
        // 参数 item：待入队的元素，以 const 引用传入避免不必要的拷贝（实际仍执行一次拷贝入队）
        std::lock_guard<std::mutex> lock(mutex_);
        // 构造 lock_guard，在当前作用域内对 mutex_ 加锁；
        // lock_guard 在构造时自动调用 mutex_.lock()，在析构时自动调用 mutex_.unlock()，
        // 保证即使函数内抛出异常也能正确释放锁（RAII 语义）
        queue_.push(item);
        // 将 item 复制到 std::queue 的队尾；
        // 此操作在 mutex_ 的保护下执行，保证对 queue_ 的修改对其他线程不可见，
        // 直至本次 lock_guard 析构（mutex_ 解锁）后才对其他线程可见
        cond_.notify_one();
        // 唤醒一个正在 cond_.wait() 中休眠的消费者线程（若有）；
        // notify_one() 仅唤醒一个等待线程，避免惊群效应（Thundering Herd）；
        // 被唤醒的消费者线程将重新竞争 mutex_，并在 pop() 的谓词中验证队列非空后取出数据
    }
    bool pop(T& item) {
        // pop()：消费者接口，从队首取出一个元素
        // 若队列为空且未收到停止信号，本函数将阻塞当前线程直至满足唤醒条件；
        // 参数 item：输出参数，通过引用传出取出的队首元素（队列非空时有效）；
        // 返回值：成功取出元素返回 true；收到停止信号且队列已空返回 false（消费者应据此退出循环）
        std::unique_lock<std::mutex> lock(mutex_);
        // 构造 unique_lock，对 mutex_ 加锁；
        // 与 lock_guard 不同，unique_lock 允许在 wait() 期间临时释放锁，
        // 这是配合 std::condition_variable::wait() 的必要条件：
        // wait() 调用时会原子地释放 mutex_ 并将线程挂起，
        // 被唤醒时再原子地重新获取 mutex_，防止唤醒信号在 wait() 执行前丢失（避免虚假唤醒丢通知）
        cond_.wait(lock, [this]{ return !queue_.empty() || stop_; });
        // 条件变量等待，带谓词的重载版本；
        // 等价于：while (!(!queue_.empty() || stop_)) { cond_.wait(lock); }
        // 即：仅当"队列非空"或"收到停止信号"两个条件之一满足时才退出等待；
        // 谓词以 lambda 形式传入，捕获 this 指针以访问 queue_ 和 stop_ 成员；
        // 带谓词的 wait() 可自动处理虚假唤醒（Spurious Wakeup）：
        //   若线程被虚假唤醒但谓词仍为 false，wait() 将重新挂起线程，无需调用方额外处理
        if (stop_ && queue_.empty()) return false;
        // 二次检查：退出等待后，若 stop_ 已置位且队列已排空，
        // 表明系统正在关闭且无剩余数据可处理，返回 false 通知消费者线程退出工作循环；
        // 若 stop_=true 但队列仍有数据（stop_ && !queue_.empty()），
        // 则继续取出剩余数据（drain 语义），确保在停止前处理完所有已入队的元素
        item = queue_.front();
        // 将队首元素赋值给输出参数 item；
        // queue_.front() 返回队首元素的引用，此处触发一次 T 类型的拷贝赋值操作；
        // 在 mutex_ 保护下执行，保证读取操作的原子性
        queue_.pop();
        // 从 std::queue 中移除队首元素（释放其存储空间）；
        // front() 与 pop() 必须分两步执行：std::queue 的 pop() 无返回值，
        // 不支持 "取出并删除" 的原子操作，因此须先 front() 取值再 pop() 删除
        return true;
        // 返回 true 表示成功取出一个有效元素，item 已被正确赋值
    }
    void stop() {
        // stop()：停止信号接口，通知所有阻塞在 pop() 中的消费者线程退出
        // 通常在系统关闭流程中由主线程或管理线程调用一次；
        // 调用后所有消费者线程在取完队列剩余数据后将陆续从 pop() 返回 false 并退出循环
        std::lock_guard<std::mutex> lock(mutex_);
        // 对 mutex_ 加锁，保证 stop_ 的写操作对所有线程可见；
        // 在互斥锁保护下写入 stop_ 可防止与 pop() 中的读取操作产生数据竞争（Data Race）
        stop_ = true;
        // 置位停止标志，将 stop_ 从 false 改为 true；
        // pop() 的条件变量谓词 [this]{ return !queue_.empty() || stop_; }
        // 在下一次评估时将因 stop_=true 而返回 true，解除所有等待线程的阻塞
        cond_.notify_all();
        // 唤醒所有正在 cond_.wait() 中休眠的消费者线程；
        // 使用 notify_all() 而非 notify_one() 是因为需要通知全部消费者线程退出，
        // 而不仅仅是其中一个；每个被唤醒的线程在重新获取 mutex_ 后
        // 将执行 pop() 中的 if (stop_ && queue_.empty()) 检查并决定是否退出
    }
    size_t size() {
        // size()：查询接口，以线程安全方式返回队列当前的元素数量
        // 返回值类型 size_t 为无符号整数，与 std::queue::size() 的返回类型一致；
        // 注意：由于多线程环境下队列长度随时可能变化，
        //       返回值仅反映调用瞬间的快照（snapshot），调用方不应依赖此值做出严格决策
        std::lock_guard<std::mutex> lock(mutex_);
        // 对 mutex_ 加锁，防止在读取 queue_.size() 期间其他线程修改队列，
        // 保证返回值的一致性（避免读取到正在 push/pop 修改中的中间状态）
        return queue_.size();
        // 返回底层 std::queue 的当前元素数量；
        // lock_guard 在本语句执行后随即析构，自动释放 mutex_
    }
private:
// 私有成员区域：以下数据成员仅供类内部方法访问，外部不可直接读写

    std::queue<T> queue_;
    // 底层数据容器，存储待处理的元素；
    // std::queue<T> 默认以 std::deque<T> 为底层实现，
    // 支持 O(1) 摊销时间的队尾插入（push）和队首删除（pop）；
    // 所有对 queue_ 的读写均须在 mutex_ 加锁的前提下进行

    std::mutex mutex_;
    // 互斥锁，用于保护对 queue_ 和 stop_ 的并发访问；
    // std::mutex 不可复制、不可移动，因此 ConcurrentQueue 对象本身也不可复制或移动；
    // 若需要在容器中存储 ConcurrentQueue，应使用指针（如 std::unique_ptr）

    std::condition_variable cond_;
    // 条件变量，与 mutex_ 配合实现"等待队列非空"的高效阻塞机制；
    // 消费者线程通过 cond_.wait() 在队列为空时挂起（释放 CPU），
    // 生产者线程在 push() 中通过 cond_.notify_one() 精确唤醒一个等待的消费者；
    // std::condition_variable 必须与 std::unique_lock<std::mutex> 配合使用

    bool stop_ = false;
    // 停止标志，默认 false（正常运行状态）；
    // 调用 stop() 后置为 true，通知所有消费者线程在队列排空后退出工作循环；
    // 本成员在 mutex_ 的保护下读写，无需声明为 std::atomic<bool>；
    // 命名后缀下划线（stop_）遵循 Google C++ 编码规范中私有成员变量的命名约定
};
// 结束 ConcurrentQueue<T> 模板类定义

} // namespace MultiProject
// 关闭命名空间 MultiProject

#endif // CONCURRENT_QUEUE_HPP
// 结束头文件保护宏条件编译块，与文件开头的 #ifndef CONCURRENT_QUEUE_HPP 配对；
// 行尾注释 // CONCURRENT_QUEUE_HPP 标明所关闭的宏名称，便于快速定位配对关系