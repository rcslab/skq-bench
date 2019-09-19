#include <sstream>

#include <celestis/http.h>
#include <celestis/debug.h>

namespace Celestis {

HTTPSession::HTTPSession(HTTPServer& s, EventLoop& elp, int fd, HTTPCb cb)
	: fd(fd), cb(cb), el(elp), sock(elp, nullptr, fd),
		buffer(sock, nullptr, nullptr), server(s)
{
	buffer.setOutputCB([&](EventContext &ctx, EventOutputBuffer &buf) {
				this->ctx = &ctx;
				this->eventBufferOutputCB(buf);
				this->ctx = nullptr;
			});
	buffer.setInputCB([&](EventContext &ctx, EventInputBuffer &buf) {
				this->ctx = &ctx;
				this->eventBufferInputCB(buffer);
				this->ctx = nullptr;
			});
	buffer.setErrorCB([&](EventContext &ctx, EventBuffer &buf) {
				LOG("Connection closed.");
				this->ctx = &ctx;
				this->eventBufferErrorCB(ctx);
				buf.cancel();
				::close(fd);
				this->ctx = nullptr;
			});
	buffer.setMode(EventBufferMode::EOL_CRLF);
	el.addEvent(&sock);
}

HTTPSession::~HTTPSession()
{
}

void 
HTTPSession::eventBufferInputCB(EventBuffer &ev)
{
	int pos, mbLength = 0;
	HTTPStatusCode status;
	std::string tmp;
	std::stringstream ss;
	
	if (sock.readShutdown() || sock.writeShutdown()) {
		// also do clean up here.
		LOG("Connection closed.");
		ev.cancel();
		return;
	}

	tmp = ev.readln();
	status = HTTPParser::parseStartLine(tmp, request);
	if (status != HTTPStatusCode::SuccessfulOK) {
		/* ignore the packet */
		LOG("Invalid request start-line.");
		cb(server, *this, HTTPEvent::ParsingError);
		return;
	}
	ss<<"HTTP/"<<request.getMajorVersion()<<"."<<request.getMinorVersion();
	response.httpVersionString = ss.str();
	request.messageBody = "";
	
	while (true) {
		tmp = ev.readln();
		if (tmp == "") {
			break;
		} else {
			if (mbLength == 0) {
				pos = tmp.find("Content-Length: ");
				if (pos != std::string::npos) {
					mbLength = std::stoi(tmp.substr(16, tmp.length()-16));
					continue;
				}
			}
			status = HTTPParser::parseHeaderField(tmp, request);
			if (status != HTTPStatusCode::SuccessfulOK) {
				LOG("Invalid request header-fields.");
				cb(server, *this, HTTPEvent::ParsingError);
				return;
			}
		}
	}

	/* parsing message body  */
	pos = mbLength;
	while (pos > 0) {
		tmp = ev.readln();
		request.messageBody.append(tmp);
		pos -= tmp.length();
	}

	response.headerFields.clear();
	/* some essential headers */
	response.headerFields["Server"] = server.default_server_identifier;
	response.headerFields["Content-Type"] = "text/html";
	
	std::unordered_map<std::string, std::string>::const_iterator it = request.headerFields.find("Connection");
	if (it == request.headerFields.end()) {
		response.headerFields["Connection"] = "keep-alive";
		keepAlive = true;
	} else {
		response.headerFields["Connection"] = it->second;
		keepAlive = (it->second == "keep-alive");
	}
	
	cb(server, *this, HTTPEvent::Input);
}

void 
HTTPSession::eventBufferOutputCB(EventOutputBuffer &ev)
{
	cb(server, *this, HTTPEvent::Output);
}

void
HTTPSession::eventBufferErrorCB(EventContext &ctx)
{
	cb(server, *this, HTTPEvent::Error);
}

void
HTTPSession::close()
{
	this->buffer.cancel();
	/*ctx->appendDefer([this](){
			    this->sock.cancel();
			});*/
}

int
HTTPSession::sendResponseFd(int fd, int offset, int sz, int status, std::string& reason)
{
	int rt;
	std::string content_length = "Content-Length: " + std::to_string(sz - offset);

	rt = sendResponse(content_length, status, reason, true);
	buffer.sendfile(fd, offset, sz);
	buffer.flush();
	return rt;
}

int 
HTTPSession::sendResponse(std::string& content, int status, std::string& reason)
{
	return sendResponse(content, status, reason, false);
}

int 
HTTPSession::sendResponse(std::string& content, int status, std::string& reason, bool sendfile) 
{
	std::string tmp;

	buffer.write(response.httpVersionString.c_str(), response.httpVersionString.length());
	buffer.write(" ", 1);
	buffer.write(std::to_string(status).c_str(), 3);
	buffer.write(" ", 1);
	buffer.write(reason.c_str(), reason.length());
	buffer.write("\r\n", 2);
	
	if (content.length() > 0) {
		if (!sendfile) {
			tmp = std::to_string(content.length());
			buffer.write("Content-Length: ", 16);
			buffer.write(tmp.c_str(), tmp.length());
		} else {
			buffer.write(content.c_str(), content.length());
		}
		buffer.write("\r\n", 2);
	}
	
	for (auto &i: response.headerFields) {
		buffer.write(i.first.c_str(), i.first.length());
		buffer.write(": ", 2);
		buffer.write(i.second.c_str(), i.second.length());
		buffer.write("\r\n", 2);
	}

	buffer.write("\r\n", 2);
	if (!sendfile) {
		buffer.write(content.c_str(), content.length());
		buffer.flush();
	}

	return 0;
}

};
