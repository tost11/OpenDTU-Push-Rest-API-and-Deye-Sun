// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <HTTPClient.h>
#include <TaskSchedulerDeclarations.h>
#include <ThreadSafeQueue.h>
#include <map>
#include <vector>
#include <thread>
#include <memory>
#include <utils/LightFuture.h>
#include <WString.h>

struct RestResponse {
    bool success;
    int httpCode;
    String body;
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
                                                     uint8_t maxRetries = 2, uint32_t timeout = 0);

    void setMaxParallelRequests(uint8_t max);
    void setDefaultTimeout(uint32_t timeoutMs);
    uint8_t getQueueSize() const;
    uint8_t getActiveRequestCount() const;

private:
    void loop();
    void processQueue();
    void processActiveRequests();
    bool startRequestThread(const RestRequest& request);
    void executeRequest(RestRequest request);
    uint32_t calculateBackoff(uint8_t retryCount);

    Task _loopTask;
    ThreadSafeQueue<RestRequest> _requestQueue;
    static const uint32_t MAX_REQUEST_QUEUE_SIZE = 20;       // 30 seconds

    struct ActiveRequest {
        uint32_t id;
        RestRequest request;
        std::thread thread;
        uint32_t startTime;
        uint8_t retryCount;
        bool completed;
    };

    std::vector<ActiveRequest> _activeRequests;
    uint8_t _maxParallelRequests;
    uint32_t _defaultTimeout;
    uint32_t _nextRequestId;
    std::mutex _activeRequestsMutex;
};

extern RestRequestHandlerClass RestRequestHandler;
