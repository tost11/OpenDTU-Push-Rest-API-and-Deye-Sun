// SPDX-License-Identifier: GPL-2.0-or-later
#include "RestRequestHandler.h"
#include <esp_pthread.h>
#include <esp_log.h>

#undef TAG
static const char* TAG = "REST";

RestRequestHandlerClass RestRequestHandler;

RestRequestHandlerClass::RestRequestHandlerClass()
    : _workerRunning(false)
    , _workerAlive(false)
    , _currentRequestId(0)
    , _statRequestsSent(0)
    , _statExceptionCount(0)
    , _statRestartCount(0)
    , _statConnectionReused(0)
    , _defaultTimeout(10000)
    , _nextRequestId(1)
{
}

void RestRequestHandlerClass::init(Scheduler& scheduler)
{
    ESP_LOGI(TAG, "Initializing RestRequestHandler (persistent worker)");

    _workerRunning = true;

    // Configure ESP32 pthread for the worker thread
    auto cfg = esp_pthread_get_default_config();
    cfg.thread_name = "rest_worker";
    cfg.stack_size = 16384;  // 16KB stack - required for HTTPS/TLS on ESP32
    cfg.prio = 5;
    esp_pthread_set_cfg(&cfg);

    _workerThread = std::thread(&RestRequestHandlerClass::workerLoop, this);
}

void RestRequestHandlerClass::workerLoop()
{
    _workerAlive = true;
    ESP_LOGI(TAG, "Worker thread started");

    while (_workerRunning) {
        try {
            // Wait for work or shutdown signal
            {
                std::unique_lock<std::mutex> lock(_workerMutex);
                _workerCv.wait(lock, [this] {
                    return !_workerRunning || _requestQueue.size() > 0;
                });
            }

            if (!_workerRunning) {
                break;
            }

            // Pop next request
            auto optRequest = _requestQueue.pop();
            if (!optRequest.has_value()) {
                continue;
            }

            executeRequest(std::move(optRequest.value()));

        } catch (const std::exception& e) {
            ESP_LOGE(TAG, "Worker exception: %s — resetting connection", e.what());
            _statExceptionCount++;
            _httpClient.end();
            _lastConnectedHost = "";

            // Resolve any in-flight promise so TostHandle isn't stuck waiting
            if (_currentPromise) {
                RestResponse err;
                err.success = false;
                err.httpCode = -1;
                err.body = String("Worker exception: ") + e.what();
                _currentPromise->set_value(err);
                _currentPromise = nullptr;
            }
            _currentRequestId = 0;
            // Continue loop — do not exit the thread

        } catch (...) {
            ESP_LOGE(TAG, "Worker unknown exception — resetting connection");
            _statExceptionCount++;
            _httpClient.end();
            _lastConnectedHost = "";

            if (_currentPromise) {
                RestResponse err;
                err.success = false;
                err.httpCode = -1;
                err.body = "Unknown worker exception";
                _currentPromise->set_value(err);
                _currentPromise = nullptr;
            }
            _currentRequestId = 0;
            // Continue loop
        }
    }

    _httpClient.end();
    _workerAlive = false;
    ESP_LOGI(TAG, "Worker thread exited");
}

void RestRequestHandlerClass::executeRequest(RestRequest request)
{
    ESP_LOGD(TAG, "Executing request %d: %s %s", request.id, request.method.c_str(), request.url.c_str());

    _currentRequestId = request.id;
    _currentPromise = request.promise;

    RestResponse response;

    // Track connection reuse — compare URL base (host portion)
    // HTTPClient internally decides reuse based on host:port match
    // We use the full URL for comparison (conservative: if URL changes, count as new)
    if (!_lastConnectedHost.isEmpty() && request.url.startsWith(_lastConnectedHost)) {
        _statConnectionReused++;
        ESP_LOGD(TAG, "Reusing connection to %s", _lastConnectedHost.c_str());
    } else {
        if (!_lastConnectedHost.isEmpty()) {
            ESP_LOGD(TAG, "New connection (host changed from %s)", _lastConnectedHost.c_str());
        }
        // Extract base URL (scheme + host + port) for future comparison
        // Find the 3rd '/' in "https://host:port/path" to get base
        int slashCount = 0;
        int baseEnd = -1;
        for (int i = 0; i < (int)request.url.length(); i++) {
            if (request.url[i] == '/') {
                slashCount++;
                if (slashCount == 3) {
                    baseEnd = i;
                    break;
                }
            }
        }
        _lastConnectedHost = (baseEnd > 0) ? request.url.substring(0, baseEnd) : request.url;
    }

    // Configure persistent client with connection reuse
    _httpClient.setReuse(true);
    _httpClient.setConnectTimeout(5000);    // 5s connect timeout — separate from read timeout
    _httpClient.begin(request.url);
    _httpClient.setTimeout(request.timeout);

    // Set headers
    if (!request.contentType.isEmpty()) {
        _httpClient.addHeader("Content-Type", request.contentType);
    }
    for (const auto& header : request.headers) {
        _httpClient.addHeader(header.first.c_str(), header.second.c_str());
    }

    // Send request (blocking — but we're in the dedicated worker thread)
    int httpCode = -1;
    if (request.method == "GET") {
        httpCode = _httpClient.GET();
    } else if (request.method == "POST") {
        httpCode = _httpClient.POST(request.body);
    } else if (request.method == "PUT") {
        httpCode = _httpClient.PUT(request.body);
    } else if (request.method == "DELETE") {
        httpCode = _httpClient.sendRequest("DELETE", request.body);
    }

    // Build response
    response.httpCode = httpCode;
    response.success = (httpCode >= 200 && httpCode < 300);
    if (httpCode > 0) {
        // Cap response body to 512 bytes to minimize RAM usage on ESP32 without PSRAM
        int contentLength = _httpClient.getSize();
        int readSize = (contentLength > 0 && contentLength < 512) ? contentLength : 512;
        WiFiClient* stream = _httpClient.getStreamPtr();
        if (stream && stream->available()) {
            char buf[513];
            int bytesRead = stream->readBytes(buf, readSize);
            buf[bytesRead] = '\0';
            response.body = String(buf);
        } else {
            response.body = "";
        }
    } else {
        response.body = _httpClient.errorToString(httpCode);
    }

    // Do NOT call _httpClient.end() — keeps TCP/TLS connection alive for reuse

    _statRequestsSent++;

    // Resolve the promise (triggers the LightFuture for TostHandle)
    if (request.promise) {
        request.promise->set_value(response);
    }

    _currentPromise = nullptr;
    _currentRequestId = 0;

    ESP_LOGD(TAG, "Request %d completed (code=%d)", request.id, httpCode);
}

void RestRequestHandlerClass::ensureWorkerRunning()
{
    if (!_workerAlive && _workerRunning) {
        ESP_LOGW(TAG, "Worker thread died — restarting (restart #%u)", _statRestartCount.load() + 1);
        _statRestartCount++;

        if (_workerThread.joinable()) {
            _workerThread.detach();  // non-blocking — let the old thread finish in background
        }

        // Reconfigure pthread before spawning
        auto cfg = esp_pthread_get_default_config();
        cfg.thread_name = "rest_worker";
        cfg.stack_size = 16384;
        cfg.prio = 5;
        esp_pthread_set_cfg(&cfg);

        _workerThread = std::thread(&RestRequestHandlerClass::workerLoop, this);
    }
}

void RestRequestHandlerClass::printStats()
{
    ESP_LOGD(TAG, "Stats since boot — sent: %u | exceptions: %u | restarts: %u | conn_reused: %u",
        _statRequestsSent.load(), _statExceptionCount.load(),
        _statRestartCount.load(), _statConnectionReused.load());
}

LightFuture<RestResponse> RestRequestHandlerClass::queueRequest(String url, String method,
                                                                String body, String contentType,
                                                                uint8_t maxRetries, uint32_t timeout)
{
    return queueRequestWithHeaders(url, method, body, contentType, {}, maxRetries, timeout);
}

LightFuture<RestResponse> RestRequestHandlerClass::queueRequestWithHeaders(
    String url, String method, String body, String contentType,
    std::map<String, String> headers, uint8_t maxRetries, uint32_t timeout)
{
    // Check if worker is alive, restart if needed
    ensureWorkerRunning();

    if (_requestQueue.size() >= MAX_REQUEST_QUEUE_SIZE) {
        ESP_LOGW(TAG, "Request queue is full (%d) do not handle new one", MAX_REQUEST_QUEUE_SIZE);

        // Return already resolved future with error
        auto errorPromise = std::make_shared<LightPromise<RestResponse>>();
        auto errorFuture = errorPromise->get_future();

        RestResponse errorResponse;
        errorResponse.success = false;
        errorResponse.httpCode = -1;
        errorResponse.body = "Request queue is full";

        errorPromise->set_value(errorResponse);
        return errorFuture;
    }

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
    request.promise = std::make_shared<LightPromise<RestResponse>>();
    auto future = request.promise->get_future();

    // Queue the request and wake up worker
    _requestQueue.push(request);
    _workerCv.notify_one();

    ESP_LOGD(TAG, "Queued request %d: %s %s", request.id, method.c_str(), url.c_str());

    return future;
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
    return (_currentRequestId.load() != 0) ? 1 : 0;
}
