// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#if DEYE_SUN || TOST

#include <HTTPClient.h>
#include <ThreadSafeQueue.h>
#include <map>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <memory>
#include <utils/LightFuture.h>
#include <WString.h>

// Forward declare Scheduler to keep API compatible
class Scheduler;

struct RestResponse {
    bool success;
    int httpCode;
    String body;
    String location;  // Populated on 3xx responses (from Location header)
};

struct RestRequest {
    uint32_t id;
    String url;
    String method;
    String body;
    String contentType;
    std::map<String, String> headers;
    uint8_t maxRetries;
    uint32_t timeout;
    uint32_t nextRetryTime;  // Internal: for retry backoff
    bool forceNewConnection;  // If true, close existing connection before sending
    uint32_t maxBodyBytes;    // Max response body bytes to read (default 512)
    std::shared_ptr<LightPromise<RestResponse>> promise;  // For future result
};

class RestRequestHandlerClass {
public:
    RestRequestHandlerClass();
    void init(Scheduler& scheduler);

    // Queue a request and return a future for the result
    LightFuture<RestResponse> queueRequest(String url, String method = "GET",
                                          String body = "", String contentType = "",
                                          uint8_t maxRetries = 2, uint32_t timeout = 0);

    // Advanced: Queue with custom headers
    LightFuture<RestResponse> queueRequestWithHeaders(String url, String method,
                                                     String body, String contentType,
                                                     std::map<String, String> headers,
                                                     uint8_t maxRetries = 2, uint32_t timeout = 0,
                                                     bool forceNewConnection = false,
                                                     uint32_t maxBodyBytes = 512);

    void setDefaultTimeout(uint32_t timeoutMs);
    uint8_t getQueueSize() const;
    uint8_t getActiveRequestCount() const;
    void printStats();

private:
    void workerLoop();
    void executeRequest(RestRequest request);
    void ensureWorkerRunning();

    ThreadSafeQueue<RestRequest> _requestQueue;
    static const uint32_t MAX_REQUEST_QUEUE_SIZE = 20;

    // Persistent worker thread
    std::thread _workerThread;
    std::atomic<bool> _workerRunning;
    std::atomic<bool> _workerAlive;

    // Worker signalling
    std::mutex _workerMutex;
    std::condition_variable _workerCv;

    // Persistent HTTP client (owned exclusively by worker thread)
    HTTPClient _httpClient;
    String _lastConnectedHost;

    // In-flight request tracking
    std::atomic<uint32_t> _currentRequestId;
    std::shared_ptr<LightPromise<RestResponse>> _currentPromise;

    // Debug counters (atomic, never reset — printed every 60s at DEBUG level)
    std::atomic<uint32_t> _statRequestsSent;
    std::atomic<uint32_t> _statExceptionCount;
    std::atomic<uint32_t> _statRestartCount;
    std::atomic<uint32_t> _statConnectionReused;

    uint32_t _defaultTimeout;
    uint32_t _nextRequestId;
};

extern RestRequestHandlerClass RestRequestHandler;

#endif // DEYE_SUN || TOST
