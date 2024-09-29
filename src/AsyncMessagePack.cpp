#include "AsyncMessagePack.h"

AsyncMessagePackResponse::AsyncMessagePackResponse(bool isArray) : _isValid{false} {
  _code = 200;
  _contentType = asyncsrv::T_application_msgpack;
  if (isArray)
    _root = _jsonBuffer.add<JsonArray>();
  else
    _root = _jsonBuffer.add<JsonObject>();
}

size_t AsyncMessagePackResponse::setLength() {
  _contentLength = measureMsgPack(_root);
  if (_contentLength) {
    _isValid = true;
  }
  return _contentLength;
}

size_t AsyncMessagePackResponse::_fillBuffer(uint8_t* data, size_t len) {
  ChunkPrint dest(data, _sentLength, len);
  serializeMsgPack(_root, dest);
  return len;
}

AsyncCallbackMessagePackWebHandler::AsyncCallbackMessagePackWebHandler(const String& uri, ArJsonRequestHandlerFunction onRequest)
    : _uri(uri), _method(HTTP_GET | HTTP_POST | HTTP_PUT | HTTP_PATCH), _onRequest(onRequest), _maxContentLength(16384) {}

bool AsyncCallbackMessagePackWebHandler::canHandle(AsyncWebServerRequest* request) {
  if (!_onRequest)
    return false;

  WebRequestMethodComposite request_method = request->method();
  if (!(_method & request_method))
    return false;

  if (_uri.length() && (_uri != request->url() && !request->url().startsWith(_uri + "/")))
    return false;

  if (request_method != HTTP_GET && !request->contentType().equalsIgnoreCase(asyncsrv::T_application_msgpack))
    return false;

  return true;
}

void AsyncCallbackMessagePackWebHandler::handleRequest(AsyncWebServerRequest* request) {
  if (_onRequest) {
    if (request->method() == HTTP_GET) {
      JsonVariant json;
      _onRequest(request, json);
      return;

    } else if (request->_tempObject != NULL) {
      JsonDocument jsonBuffer;
      DeserializationError error = deserializeMsgPack(jsonBuffer, (uint8_t*)(request->_tempObject));

      if (!error) {
        JsonVariant json = jsonBuffer.as<JsonVariant>();
        _onRequest(request, json);
        return;
      }
    }
    request->send(_contentLength > _maxContentLength ? 413 : 400);
  } else {
    request->send(500);
  }
}

void AsyncCallbackMessagePackWebHandler::handleBody(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
  if (_onRequest) {
    _contentLength = total;
    if (total > 0 && request->_tempObject == NULL && total < _maxContentLength) {
      request->_tempObject = malloc(total);
    }
    if (request->_tempObject != NULL) {
      memcpy((uint8_t*)(request->_tempObject) + index, data, len);
    }
  }
}