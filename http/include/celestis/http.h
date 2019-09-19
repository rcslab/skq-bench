
#ifndef __CELESTIS_HTTP_H__
#define __CELESTIS_HTTP_H__

#include <unistd.h>

#include <string>
#include <unordered_map>
#include <deque>
#include <thread>

#include <celestis/event.h>
#include <celestis/eventloop.h>
#include <celestis/eventsock.h>
#include <celestis/eventbuffer.h>
#include <celestis/eventserversock.h>

namespace Celestis {

class HTTPSession;
class HTTPRequest;
class HTTPResponse;
class HTTPServer;

enum class HTTPEvent {
    ParsingError,
    Error,
    Accept,
    Output,	 /* Confusing not using this for now */
    Input,   /* Another side write something into socket */
    Sent,
    Disconnect
};

typedef std::function<void(HTTPServer&, HTTPSession&, HTTPEvent)> HTTPCb;

inline bool isHexChar(char c) {
    return ((c>=48 && c<=57) || (c>=65 && c<=70) || (c>=97 && c<=102)); 
}

enum class HTTPStatusCode {
    SuccessfulOK = 200,
    SuccessfulCreated = 201,
    SuccessfulAccepted = 202,

    ClientBadRequest = 400,
    ClientUnauthorized = 401,
    ClientForbidden = 403,
    ClientNotFound = 404,
    ClientMethodNotAllowed = 405,

    ServerInternalServerError = 500,
    ServerNotImplemented = 501,
    ServerBadGateway = 502,
    ServerServiceUnavailable = 503,
    ServerGatewayTimeout = 504,
    ServerHTTPVersionNotSupported = 505
};

enum class HTTPMethod {
    INVALID,
    GET,
    HEAD,
    POST,
    PUT,
    DELETE,
    CONNECT,
    OPTIONS,
    TRACE,
    PATCH
};

class HTTPRequest
{
public:
    std::unordered_map<std::string, std::string> headerFields;
    std::unordered_map<std::string, std::string> requestQuery;
    HTTPRequest() {};
    int getMajorVersion() { return majorVersion; }
    int getMinorVersion() { return minorVersion; }
    HTTPMethod getMethod() { return method; }
    std::string& getRequestTarget() { return requestTarget; }
    std::string& getRequestTargetPath() { return requestTargetPath; }
    int getRequestPort() { return requestPort; }
private:
    HTTPMethod method;
    std::string requestTarget, requestTargetPath, requestTargetRaw, messageBody;
    int requestPort;
    int majorVersion, minorVersion;
    friend class HTTPParser;
    friend class HTTPSession;
};

class HTTPParser
{
public:
    static const int CR = '\r';
    static const int LF = '\n';
    static const int SP = ' ';
    static const int COLON = ':';

    static const int default_http_port = 80;
    static const int default_https_port = 443;
    static HTTPStatusCode parseStartLine(std::string& content, HTTPRequest& req, bool CRLF = false);	
    static HTTPStatusCode parseHeaderField(std::string& content, HTTPRequest& req, bool CRLF = false);

    HTTPParser() {};
    static HTTPStatusCode parseURI(std::string& content, HTTPRequest& req);
private:
    /* last 3 refs are output */
    static HTTPStatusCode formatURIAndPort(std::string& content, int sp, std::string& out, int& port, int& pathBegin);
    static bool escapeURIToPrevDir(std::string& content);

    enum class StartLineParsingStage {
	Method,
	RequestTarget,
	HTTPVersion,
	HTTPVersionMajor,
	HTTPVersionMinor,
	Done
    };
    enum class HeaderFieldParsingStage {
	FieldName,
	FieldValueOWS,
	FieldValue,
	Done
    };
    enum class URIParsingType {
	Unknown,
	OriginForm,
	AbsoluteForm,
	AuthorityForm,
	AsteriskForm
    };
};

class HTTPResponse
{
public:
    HTTPResponse() {};
    std::string getResponse();
    std::unordered_map<std::string, std::string>& getHeaderFields();	
private:
    std::unordered_map<std::string, std::string> headerFields;
    std::string httpVersionString;
    friend class HTTPSession;
};

class HTTPSession 
{
friend class HTTPServer;

public:
    HTTPSession(HTTPServer& s, EventLoop& elp, int fd, HTTPCb cb);
    ~HTTPSession();
    int getFd();
    HTTPRequest& getRequest() { return request; }
    HTTPResponse& getResponse() { return response; }
    bool isKeepAlive() { return keepAlive; }
    int sendResponse(std::string& content, int status, std::string& reason);
    int sendResponseFd(int fd, int offset, int sz, int status, std::string& reason);
    void close();
private:
    int sendResponse(std::string& content, int status, std::string& reason, bool sendfile);
    void eventBufferInputCB(EventBuffer &ev);
    void eventBufferOutputCB(EventOutputBuffer &ev);
    void eventBufferErrorCB(EventContext &ctx);
    int fd;
    HTTPCb cb;
    bool keepAlive = true;
    EventLoop& el;
    EventSock sock;
    EventBuffer buffer;
    EventContext *ctx;
    HTTPServer& server;
    HTTPRequest request;
    HTTPResponse response;	
};

class HTTPServer
{
public:
    const char* const default_server_identifier = "Celestis/0.0";
    HTTPServer(std::string server_addr, uint16_t server_port, int worker_num, bool lb, bool mkq, int mkq_mode, HTTPCb httpcb);
    ~HTTPServer();
    void setCallback(HTTPCb cb);
    int getSessionCount();
    void startServer();
    void stopServer();
    void eventAddToDeferList(HTTPSession &s);
private:
    void startWorker();
    int setSocket(std::string address, uint16_t port);
    void eventCallback(Event &ev, int fd, HTTPEvent evType);
    std::unordered_map<int, HTTPSession> sessions;
    HTTPCb cb;	
    bool s_lb;
    bool m_kq;
    int m_kq_mode;
    EventLoop el;
    std::string address;
    int fd, port;
    int numWorker;
    std::vector<std::thread> workers;
};

};

#endif /* __CELETIS_HTTP_H__ */

