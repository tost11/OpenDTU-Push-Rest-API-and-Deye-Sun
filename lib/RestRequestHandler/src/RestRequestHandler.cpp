// SPDX-License-Identifier: GPL-2.0-or-later
#include "RestRequestHandler.h"
#include <esp_pthread.h>
#include <esp_log.h>
#include <Arduino.h>

RestRequestHandlerClass RestRequestHandler;

RestRequestHandlerClass::RestRequestHandlerClass()
    : _loopTask(100, TASK_FOREVER, std::bind(&RestRequestHandlerClass::loop, this))
    , _maxParallelRequests(2)
    , _defaultTimeout(10000)
    , _nextRequestId(1)
{
}

void RestRequestHandlerClass::init(Scheduler& scheduler)
{
    ESP_LOGI("REST", "Initializing RestRequestHandler");
    scheduler.addTask(_loopTask);
    _loopTask.enable();
}

void RestRequestHandlerClass::loop()
{
    // Process active requests (check for completion)
    processActiveRequests();

    // Start new requests from queue if slots available
    processQueue();
}

void RestRequestHandlerClass::processQueue()
{
    std::lock_guard<std::mutex> lock(_activeRequestsMutex);

    while (_activeRequests.size() < _maxParallelRequests) {
        auto optRequest = _requestQueue.pop();
        if (!optRequest.has_value()) {
            break;
        }

        RestRequest request = optRequest.value();

        // Check if retry delay has passed
        if (request.nextRetryTime > 0 && millis() < request.nextRetryTime) {
            // Re-queue for later
            _requestQueue.push(request);
            break;
        }

        if (!startRequestThread(request)) {
            // Failed to start thread, set promise with error
            if (request.promise) {
                RestResponse errorResponse;
                errorResponse.success = false;
                errorResponse.httpCode = -1;
                errorResponse.body = "Failed to start thread";
                request.promise->set_value(errorResponse);
            }
        }
    }
}

bool RestRequestHandlerClass::startRequestThread(const RestRequest& request)
{
    ActiveRequest active;
    active.id = request.id;
    active.request = request;
    active.startTime = millis();
    active.retryCount = 0;
    active.completed = false;

    // Configure ESP32 pthread for this thread
    auto cfg = esp_pthread_get_default_config();
    cfg.thread_name = "rest_req";
    cfg.stack_size = 8192;  // 8KB stack per thread
    cfg.prio = 5;  // Medium priority
    esp_pthread_set_cfg(&cfg);

    // Start thread to execute request (pass by value)
    try {
        active.thread = std::thread(&RestRequestHandlerClass::executeRequest, this, request);

        _activeRequests.push_back(std::move(active));
        return true;
    } catch (const std::exception& e) {
        ESP_LOGE("REST", "Failed to create thread: %s", e.what());

        // Set promise with error
        if (request.promise) {
            RestResponse errorResponse;
            errorResponse.success = false;
            errorResponse.httpCode = -1;
            errorResponse.body = String("Thread creation failed: ") + e.what();
            request.promise->set_value(errorResponse);
        }

        return false;
    }
}

void RestRequestHandlerClass::executeRequest(RestRequest request)
{
    ESP_LOGD("REST", "Thread %d: Starting request to %s", request.id, request.url.c_str());

    HTTPClient client;
    RestResponse response;

    // Configure client
    client.begin(request.url);

    // Note: For HTTPS with self-signed certificates, proper cert validation
    // should be configured in production. For now, HTTP is recommended.

    client.setTimeout(request.timeout);

    // Set headers
    if (!request.contentType.isEmpty()) {
        client.addHeader("Content-Type", request.contentType);
    }
    for (const auto& header : request.headers) {
        client.addHeader(header.first.c_str(), header.second.c_str());
    }

    // Send request (synchronous, but in separate thread)
    int httpCode = -1;
    if (request.method == "GET") {
        httpCode = client.GET();
    } else if (request.method == "POST") {
        httpCode = client.POST(request.body);
    } else if (request.method == "PUT") {
        httpCode = client.PUT(request.body);
    } else if (request.method == "DELETE") {
        httpCode = client.sendRequest("DELETE", request.body);
    }

    // Build response
    response.httpCode = httpCode;
    response.success = (httpCode >= 200 && httpCode < 300);
    if (httpCode > 0) {
        response.body = client.getString();
    } else {
        response.body = client.errorToString(httpCode);
    }

    client.end();

    // Set the promise value (triggers the future)
    if (request.promise) {
        request.promise->set_value(response);
    }

    // Mark as completed in active list
    {
        std::lock_guard<std::mutex> lock(_activeRequestsMutex);
        for (auto& active : _activeRequests) {
            if (active.id == request.id) {
                active.completed = true;
                break;
            }
        }
    }

    ESP_LOGD("REST", "Thread %d: Completed (code=%d)", request.id, httpCode);
}

void RestRequestHandlerClass::processActiveRequests()
{
    std::lock_guard<std::mutex> lock(_activeRequestsMutex);

    for (auto it = _activeRequests.begin(); it != _activeRequests.end(); ) {
        auto& active = *it;

        // Check if thread has completed
        if (!active.completed) {
            // Check timeout
            if (millis() - active.startTime > active.request.timeout) {
                // Force timeout (thread might still be running but we give up)
                RestResponse timeoutResponse;
                timeoutResponse.success = false;
                timeoutResponse.httpCode = -1;
                timeoutResponse.body = "Timeout";

                // Set promise with timeout response
                if (active.request.promise) {
                    try {
                        active.request.promise->set_value(timeoutResponse);
                    } catch (const std::future_error&) {
                        // Promise already set by thread - ignore
                    }
                }

                active.completed = true;
                ESP_LOGW("REST", "Request %d timed out", active.id);
            }
        }

        if (active.completed) {
            // Join thread to clean up
            if (active.thread.joinable()) {
                active.thread.join();
            }

            ESP_LOGD("REST", "Request %d completed and cleaned up", active.id);

            // Remove from active
            it = _activeRequests.erase(it);
        } else {
            ++it;
        }
    }
}

std::future<RestResponse> RestRequestHandlerClass::queueRequest(String url, String method,
                                                                String body, String contentType,
                                                                uint8_t maxRetries, uint32_t timeout)
{
    return queueRequestWithHeaders(url, method, body, contentType, {}, maxRetries, timeout);
}

std::future<RestResponse> RestRequestHandlerClass::queueRequestWithHeaders(
    String url, String method, String body, String contentType,
    std::map<String, String> headers, uint8_t maxRetries, uint32_t timeout)
{
    RestRequest request;
    request.id = _nextRequestId++;
    request.url = url;
    request.method = method;
    request.body = body;
    request.contentType = contentType;
    request.headers = headers;
    request.maxRetries = maxRetries;
    request.timeout = (timeout > 0) ? timeout : _defaultTimeout;
    request.nextRetryTime = 0;

    // Create promise/future pair
    request.promise = std::make_shared<std::promise<RestResponse>>();
    auto future = request.promise->get_future();

    // Queue the request
    _requestQueue.push(request);

    ESP_LOGD("REST", "Queued request %d: %s %s", request.id, method.c_str(), url.c_str());

    return future;
}

uint32_t RestRequestHandlerClass::calculateBackoff(uint8_t retryCount)
{
    // Exponential backoff: 1s, 2s, 4s, 8s, 16s, ...
    return 1000 * (1 << (retryCount - 1));
}

void RestRequestHandlerClass::setMaxParallelRequests(uint8_t max)
{
    _maxParallelRequests = max;
}

void RestRequestHandlerClass::setDefaultTimeout(uint32_t timeoutMs)
{
    _defaultTimeout = timeoutMs;
}

uint8_t RestRequestHandlerClass::getQueueSize() const
{
    return _requestQueue.size();
}

uint8_t RestRequestHandlerClass::getActiveRequestCount() const
{
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(_activeRequestsMutex));
    return _activeRequests.size();
}
