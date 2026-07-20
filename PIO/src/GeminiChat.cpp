#include "GeminiChat.h"
#include "WiFiManagerHandler.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Arduino.h>
#include <lvgl.h>

GeminiChat::GeminiChat() : _apiKey("") {}

void GeminiChat::setApiKey(const String &apiKey) {
    _apiKey = apiKey;
}

void GeminiChat::initialize() {
    if (_apiKey.isEmpty()) {
        _apiKey = getStoredApiKey();
    }
}


// FreeRTOS task
void GeminiChat::sendMessageTask(void *param) {
    auto *taskParams = static_cast<std::pair<GeminiChat*, String>*>(param);

    GeminiChat *instance = taskParams->first;
    String message = taskParams->second;

    instance->sendMessage(message);

    delete taskParams;
    vTaskDelete(NULL);
}


void GeminiChat::sendMessageAsync(const String &message) {

    auto *taskParams = new std::pair<GeminiChat*, String>(this, message);

    xTaskCreatePinnedToCore(
        sendMessageTask,
        "GeminiChatTask",
        4096,
        taskParams,
        1,
        NULL,
        1
    );
}


// Clean response
String cleanResponse(String responseText) {

    responseText.replace("* ", "");
    responseText.replace("*", " ");
    responseText.replace("**", "");
    responseText.replace("#", "");
    responseText.replace("  ", "");

    return responseText;
}



String GeminiChat::sendMessage(const String &message) {

    if (_apiKey.isEmpty()) {
        return "API key not set.";
    }


    WiFiClientSecure client;
    client.setInsecure();


    HTTPClient https;

    https.setTimeout(15000);


    // OpenRouter endpoint
    String url = "https://openrouter.ai/api/v1/chat/completions";


    if (!https.begin(client, url)) {

        Serial.println("Failed to connect to OpenRouter");

        return "Failed to connect to OpenRouter.";
    }


    // Headers
    https.addHeader(
        "Content-Type",
        "application/json"
    );

    https.addHeader(
        "Authorization",
        "Bearer " + _apiKey
    );


    // JSON request

    DynamicJsonDocument jsonRequest(2048);


    jsonRequest["model"] =
        "google/gemma-3-1b-it:free";


    JsonArray messages =
        jsonRequest.createNestedArray("messages");


    JsonObject user =
        messages.createNestedObject();


    user["role"] = "user";
    user["content"] = message;


    jsonRequest["max_tokens"] = 100;


    String requestBody;

    serializeJson(
        jsonRequest,
        requestBody
    );


    int httpResponseCode =
        https.POST(requestBody);



    if (httpResponseCode != HTTP_CODE_OK) {

        String error =
            "HTTP error: " +
            String(httpResponseCode);

        https.end();

        return error;
    }



    String response =
        https.getString();


    https.end();



    DynamicJsonDocument jsonResponse(4096);


    DeserializationError error =
        deserializeJson(
            jsonResponse,
            response
        );


    if (error) {

        return "JSON parsing failed";
    }



    // Read OpenRouter response

    if (jsonResponse.containsKey("choices")) {


        JsonArray choices =
            jsonResponse["choices"];


        if (choices.size() > 0) {


            String textResponse =
                choices[0]
                ["message"]
                ["content"]
                .as<String>();



            textResponse =
                cleanResponse(textResponse);



            const int maxResponseLength = 500;


            if (textResponse.length() > maxResponseLength) {

                textResponse =
                    textResponse.substring(
                        0,
                        maxResponseLength
                    )
                    + "...";
            }



            #ifdef ui_TextArea_AI_response

            lv_textarea_set_text(
                ui_TextArea_AI_response,
                textResponse.c_str()
            );

            #endif



            return textResponse;
        }
    }



    return "No response received.";
}
