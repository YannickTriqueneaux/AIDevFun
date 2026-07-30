#include "Engine/AI/OpenAIClient.h"

#include <stdexcept>
#include <string>
#include <utility>

#include <nlohmann/json.hpp>

#if defined(_WIN32)
    #define WIN32_LEAN_AND_MEAN
    #include <Windows.h>
    #include <winhttp.h>
#endif

namespace
{
#if defined(_WIN32)
    class WinHttpHandle
    {
    public:
        explicit WinHttpHandle(HINTERNET handle = nullptr)
            : handle_(handle)
        {
        }

        ~WinHttpHandle()
        {
            if (handle_ != nullptr)
            {
                WinHttpCloseHandle(handle_);
            }
        }

        WinHttpHandle(const WinHttpHandle&) = delete;
        WinHttpHandle& operator=(const WinHttpHandle&) = delete;

        [[nodiscard]] HINTERNET Get() const
        {
            return handle_;
        }

    private:
        HINTERNET handle_ = nullptr;
    };

    std::wstring ToWide(std::string_view value)
    {
        if (value.empty())
        {
            return {};
        }

        const int size = MultiByteToWideChar(
            CP_UTF8,
            0,
            value.data(),
            static_cast<int>(value.size()),
            nullptr,
            0);
        if (size <= 0)
        {
            throw std::runtime_error("Failed to encode an HTTP header.");
        }

        std::wstring result(static_cast<std::size_t>(size), L'\0');
        MultiByteToWideChar(
            CP_UTF8,
            0,
            value.data(),
            static_cast<int>(value.size()),
            result.data(),
            size);
        return result;
    }

    std::string PostResponses(
        const std::string& apiKey,
        const std::string& requestBody)
    {
        WinHttpHandle session(WinHttpOpen(
            L"ProceduralRaylibEngine/1.0",
            WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
            WINHTTP_NO_PROXY_NAME,
            WINHTTP_NO_PROXY_BYPASS,
            0));
        if (session.Get() == nullptr)
        {
            throw std::runtime_error("Unable to initialize the HTTP client.");
        }

        WinHttpSetTimeouts(
            session.Get(),
            10'000,
            10'000,
            30'000,
            300'000);

        WinHttpHandle connection(WinHttpConnect(
            session.Get(),
            L"api.openai.com",
            INTERNET_DEFAULT_HTTPS_PORT,
            0));
        if (connection.Get() == nullptr)
        {
            throw std::runtime_error("Unable to connect to api.openai.com.");
        }

        WinHttpHandle request(WinHttpOpenRequest(
            connection.Get(),
            L"POST",
            L"/v1/responses",
            nullptr,
            WINHTTP_NO_REFERER,
            WINHTTP_DEFAULT_ACCEPT_TYPES,
            WINHTTP_FLAG_SECURE));
        if (request.Get() == nullptr)
        {
            throw std::runtime_error("Unable to create the OpenAI request.");
        }

        const std::wstring headers = ToWide(
            "Content-Type: application/json\r\nAuthorization: Bearer " +
            apiKey);
        const BOOL sent = WinHttpSendRequest(
            request.Get(),
            headers.c_str(),
            static_cast<DWORD>(headers.size()),
            const_cast<char*>(requestBody.data()),
            static_cast<DWORD>(requestBody.size()),
            static_cast<DWORD>(requestBody.size()),
            0);
        if (!sent || !WinHttpReceiveResponse(request.Get(), nullptr))
        {
            throw std::runtime_error("The OpenAI HTTP request failed.");
        }

        DWORD statusCode = 0;
        DWORD statusCodeSize = sizeof(statusCode);
        WinHttpQueryHeaders(
            request.Get(),
            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX,
            &statusCode,
            &statusCodeSize,
            WINHTTP_NO_HEADER_INDEX);

        std::string responseBody;
        while (true)
        {
            DWORD available = 0;
            if (!WinHttpQueryDataAvailable(request.Get(), &available))
            {
                throw std::runtime_error("Failed while reading the OpenAI response.");
            }
            if (available == 0)
            {
                break;
            }

            const std::size_t offset = responseBody.size();
            responseBody.resize(offset + available);
            DWORD bytesRead = 0;
            if (!WinHttpReadData(
                    request.Get(),
                    responseBody.data() + offset,
                    available,
                    &bytesRead))
            {
                throw std::runtime_error("Failed while reading the OpenAI response.");
            }
            responseBody.resize(offset + bytesRead);
        }

        if (statusCode < 200 || statusCode >= 300)
        {
            std::string message = "OpenAI API error " + std::to_string(statusCode);
            try
            {
                const nlohmann::json error = nlohmann::json::parse(responseBody);
                message += ": " + error.at("error").value("message", responseBody);
            }
            catch (const nlohmann::json::exception&)
            {
                if (!responseBody.empty())
                {
                    message += ": " + responseBody;
                }
            }
            throw std::runtime_error(message);
        }

        return responseBody;
    }
#endif
}

namespace Engine
{
    OpenAIClient::OpenAIClient(OpenAISettings settings)
        : settings_(std::move(settings))
    {
    }

    bool OpenAIClient::IsConfigured() const
    {
        return settings_.IsConfigured();
    }

    const std::string& OpenAIClient::GetModel() const
    {
        return settings_.model;
    }

    OpenAIResponse OpenAIClient::CreateResponse(
        std::string_view instructions,
        std::string_view prompt,
        std::string_view previousResponseId) const
    {
        if (!IsConfigured())
        {
            throw std::runtime_error(
                "OpenAI is not configured. Add apiKey and model to settings.json.");
        }

        nlohmann::json request{
            {"model", settings_.model},
            {"instructions", instructions},
            {"input", prompt},
            {"reasoning", {{"effort", "medium"}}},
            {"store", true}
        };
        if (!previousResponseId.empty())
        {
            request["previous_response_id"] = previousResponseId;
        }

#if defined(_WIN32)
        const std::string responseBody =
            PostResponses(settings_.apiKey, request.dump());
#else
        throw std::runtime_error(
            "The OpenAI HTTP transport is currently implemented for Windows only.");
#endif

        const nlohmann::json response = nlohmann::json::parse(responseBody);
        OpenAIResponse result;
        result.id = response.value("id", "");

        for (const nlohmann::json& output : response.value(
                 "output",
                 nlohmann::json::array()))
        {
            if (output.value("type", "") != "message")
            {
                continue;
            }

            for (const nlohmann::json& content : output.value(
                     "content",
                     nlohmann::json::array()))
            {
                if (content.value("type", "") == "output_text")
                {
                    if (!result.text.empty())
                    {
                        result.text += '\n';
                    }
                    result.text += content.value("text", "");
                }
            }
        }

        if (result.text.empty())
        {
            throw std::runtime_error(
                "The OpenAI response did not contain output text.");
        }
        return result;
    }
}

