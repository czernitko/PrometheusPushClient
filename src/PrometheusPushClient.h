#ifndef PROMETHEUS_PUSH_CLIENT_H
#define PROMETHEUS_PUSH_CLIENT_H

#include <Arduino.h>
#include <Client.h>
#include <array>

enum class MetricType
{
    Counter,
    Gauge
};

struct MetricLabel
{
    const char *key;   // PROGMEM pointer
    const char *value; // PROGMEM pointer
};

// Callback type definition to update metric values
typedef float (*MetricUpdateCallback)(const char *metricName, MetricLabel *labels, size_t labelCount);

// Callback type definition to update common label values
typedef const char *(*CommonLabelValueUpdateCallback)(const char *labelKey);

template <size_t MAX_METRICS, size_t MAX_METRIC_LABELS = 0, size_t MAX_COMMON_LABELS = 0>
class PrometheusPushClient
{
private:
    Client *_client;
    std::array<MetricLabel, MAX_COMMON_LABELS> _commonLabels;

    struct InternalMetric
    {
        const char *name;
        const char *help;
        MetricType type;
        float value;
        std::array<MetricLabel, MAX_METRIC_LABELS> labels;
    };

    std::array<InternalMetric, MAX_METRICS> _metrics;
    size_t _count = 0;
    MetricUpdateCallback _onUpdate = nullptr;
    CommonLabelValueUpdateCallback _onCommonLabelValueUpdate = nullptr;

    // Helper: URL-safe printing
    void printUrlSafe(Print *stream, const char *str)
    {
        while (*str)
        {
            if (isalnum(*str) || *str == '-' || *str == '_' || *str == '.')
            {
                stream->print(*str);
            }
            else if (*str == ' ')
            {
                stream->print('_'); // Simplest: replace space with underscore
            }
            else
            {
                // Optional: Full percent encoding for other special chars
                stream->print('%');
                stream->print(static_cast<char>(("0123456789ABCDEF")[(*str >> 4) & 0xf]));
                stream->print(static_cast<char>(("0123456789ABCDEF")[*str & 0xf]));
            }
            str++;
        }
    }

    void processHeaders(Print *stream, const char *host, const char *jobName, const char *instanceName, size_t totalSize)
    {
        stream->print(F("POST /metrics/job/"));
        printUrlSafe(stream, jobName);
        if (instanceName)
        {
            stream->print(F("/instance/"));
            printUrlSafe(stream, instanceName);
        }
        stream->println(F(" HTTP/1.1"));
        stream->print(F("Host: "));
        stream->println(host);
        stream->println(F("Content-Type: text/plain; version=0.0.4"));
        stream->print(F("Content-Length: "));
        stream->println(totalSize);
        stream->println(F("Connection: close"));
        stream->println();
    }

    // Helper: Counts bytes or writes to stream
    size_t processMetric(Print *stream, InternalMetric &m)
    {
        size_t bytes = 0;

        const char *typeStr = (m.type == MetricType::Counter) ? "counter" : "gauge";

        // 1. HELP & TYPE
        bytes += (stream ? stream->printf_P(PSTR("# HELP %s %s\n# TYPE %s %s\n"), m.name, m.help, m.name, typeStr)
                         : snprintf_P(nullptr, 0, PSTR("# HELP %s %s\n# TYPE %s %s\n"), m.name, m.help, m.name, typeStr));

        // 2. NAME
        if (stream)
            stream->print(FPSTR(m.name));
        bytes += strlen_P(m.name);

        // 3. COMBINED LABELS (Common + Local)
        bool hasCommon = (MAX_COMMON_LABELS > 0 && _commonLabels[0].key != nullptr);
        bool hasLocal = (MAX_METRIC_LABELS > 0 && m.labels[0].key != nullptr);

        if (hasCommon || hasLocal)
        {
            if (stream)
                stream->print('{');
            bytes++;
            bool first = true;

            // First, print Common Labels
            for (size_t i = 0; i < MAX_COMMON_LABELS; i++)
            {
                if (_commonLabels[i].key == nullptr)
                    break;
                if (!first)
                {
                    if (stream)
                        stream->print(',');
                    bytes++;
                }
                bytes += (stream ? stream->printf_P(PSTR("%s=\"%s\""), _commonLabels[i].key, _commonLabels[i].value)
                                 : strlen_P(_commonLabels[i].key) + strlen_P(_commonLabels[i].value) + 3);
                first = false;
            }

            // Then, print Local Labels
            for (size_t i = 0; i < MAX_METRIC_LABELS; i++)
            {
                if (m.labels[i].key == nullptr)
                    break;
                if (!first)
                {
                    if (stream)
                        stream->print(',');
                    bytes++;
                }
                bytes += (stream ? stream->printf_P(PSTR("%s=\"%s\""), m.labels[i].key, m.labels[i].value)
                                 : strlen_P(m.labels[i].key) + strlen_P(m.labels[i].value) + 3);
                first = false;
            }
            if (stream)
                stream->print('}');
            bytes++;
        }

        // 4. VALUE
        bytes += (stream ? stream->printf_P(PSTR(" %.2f\n"), m.value)
                         : snprintf_P(nullptr, 0, PSTR(" %.2f\n"), m.value));
        return bytes;
    }

public:
    PrometheusPushClient(Client *client, std::array<MetricLabel, MAX_COMMON_LABELS> commonLabels = {}) : _client(client), _commonLabels(commonLabels) {}

    /**
     * @brief Add a metric.
     * @param labels Defaults to empty array.
     * @param type Defaults to Counter.
     */
    bool addMetric(const char *name, const char *help = "",
                   std::array<MetricLabel, MAX_METRIC_LABELS> labels = {},
                   MetricType type = MetricType::Gauge)
    {
        if (_count >= MAX_METRICS)
            return false;
        _metrics[_count++] = {name, help, type, 0.0f, labels};
        return true;
    }

    // Overload for when user wants to specify Type but no Labels
    bool addMetric(const char *name, const char *help, MetricType type)
    {
        return addMetric(name, help, {}, type);
    }

    void setUpdateCallback(MetricUpdateCallback cb)
    {
        _onUpdate = cb;
    }

    void setCommonLabelValueUpdateCallback(CommonLabelValueUpdateCallback cb)
    {
        _onCommonLabelValueUpdate = cb;
    }

    int push(const char *host, uint16_t port, const char *jobName, const char *instanceName = nullptr)
    {
#ifdef DEBUG_PROMETHEUS_PUSH_CLIENT_PORT
        DEBUG_PROMETHEUS_PUSH_CLIENT_PORT.println("Pushing metrics to Prometheus push gateway...");
#endif
        // 1. Update Values using Callback
        if (_onUpdate)
        {
            for (size_t i = 0; i < _count; i++)
            {
                // Pass the label array raw pointer and the template size
                _metrics[i].value = _onUpdate(_metrics[i].name, _metrics[i].labels.data(), MAX_METRIC_LABELS);
            }
        }

        if (_onCommonLabelValueUpdate)
        {
            // Print all common labels before and after the update
            for (size_t i = 0; i < MAX_COMMON_LABELS; i++)
            {
                if (_commonLabels[i].key == nullptr)
                    break;
                _commonLabels[i].value = _onCommonLabelValueUpdate(_commonLabels[i].key);
            }
        }

        // 2. Dry Run
        size_t totalSize = 0;
        for (size_t i = 0; i < _count; i++)
        {
            totalSize += processMetric(nullptr, _metrics[i]);
        }

        // 3. Connect
        if (!_client->connect(host, port))
        {
#ifdef DEBUG_PROMETHEUS_PUSH_CLIENT_PORT
            DEBUG_PROMETHEUS_PUSH_CLIENT_PORT.println(F("TCP connection to Prometheus push gateway failed."));
#endif
            return -1;
        }

        // 4. Headers
#ifdef DEBUG_PROMETHEUS_PUSH_CLIENT_PORT
        processHeaders(&DEBUG_PROMETHEUS_PUSH_CLIENT_PORT, host, jobName, instanceName, totalSize);
#endif
        processHeaders(_client, host, jobName, instanceName, totalSize);

        // 5. Body
        for (size_t i = 0; i < _count; i++)
        {
#ifdef DEBUG_PROMETHEUS_PUSH_CLIENT_PORT
            processMetric(&DEBUG_PROMETHEUS_PUSH_CLIENT_PORT, _metrics[i]);
#endif
            processMetric(_client, _metrics[i]);
        }

        // 6. Response
        unsigned long timeout = millis();
        while (_client->available() == 0)
        {
            if (millis() - timeout > 5000)
            {
                _client->stop();
#ifdef DEBUG_PROMETHEUS_PUSH_CLIENT_PORT
                DEBUG_PROMETHEUS_PUSH_CLIENT_PORT.println(F("Timeout while waiting for response from Prometheus push gateway."));
#endif
                return -2;
            }
        }

        int statusCode = -1;
        if (_client->available())
        {
            String statusLine = _client->readStringUntil('\n');
            int firstSpace = statusLine.indexOf(' ');
            int secondSpace = statusLine.indexOf(' ', firstSpace + 1);
            if (firstSpace > 0 && secondSpace > firstSpace)
            {
                statusCode = statusLine.substring(firstSpace + 1, secondSpace).toInt();
            }
            if (statusCode == 200)
            {
#ifdef DEBUG_PROMETHEUS_PUSH_CLIENT_PORT
                DEBUG_PROMETHEUS_PUSH_CLIENT_PORT.print(F("Successfully pushed metrics to Prometheus push gateway at "));
                DEBUG_PROMETHEUS_PUSH_CLIENT_PORT.println(host);
#endif
            }
            else
            {
#ifdef DEBUG_PROMETHEUS_PUSH_CLIENT_PORT
                DEBUG_PROMETHEUS_PUSH_CLIENT_PORT.println(F("=== BEGIN Pushgateway response ==="));
                DEBUG_PROMETHEUS_PUSH_CLIENT_PORT.println(statusLine);
                while (_client->available())
                {
                    DEBUG_PROMETHEUS_PUSH_CLIENT_PORT.write(_client->read());
                }
                DEBUG_PROMETHEUS_PUSH_CLIENT_PORT.println(F("=== END Pushgateway response ==="));
#endif
            }
        }
        _client->stop();
        return statusCode;
    }
};

#endif // PrometheusPushClient.h
