# ThreadsafeQueueLib
# high-performance, header-only C++ library providing specialized concurrent queue implementations tailored for low-latency producer–consumer workloads.

## 🚀 Overview

This library provides a safe, scalable alternative to `std::queue` by utilizing lock-free programming and hardware-aware optimizations. It is designed specifically for systems where synchronization overhead is a critical bottleneck.

### Supported Models
* **SPSC / MPSC / MPMC** architectures.
* **Bounded** (ring-buffer) and **Unbounded** (linked) variants.

## 🛠 Technical Core

* **Lock-Free Progress:** Implementation of non-blocking algorithms using C++ atomics and fine-grained memory ordering (`acquire/release` semantics).
* **Linearizability:** Guarantees that all operations appear atomic and consistent across threads.
* **Cache Efficiency:** Minimizes **false sharing** via explicit memory alignment and padding (`alignas`).
* **Zero-Overhead Abstractions:** Extensive use of **template metaprogramming** and C++20 concepts to ensure compile-time optimization and eliminate virtual dispatch.

---

## 📈 Status: Ongoing

This project is currently in the **initial implementation and research phase**. Core primitives and the SPSC bounded ring buffer are under active development.

