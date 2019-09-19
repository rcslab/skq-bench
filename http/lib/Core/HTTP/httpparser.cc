#include <string.h>
#include <stdlib.h>

#include <sstream>

#include <celestis/http.h>
#include <celestis/debug.h>

namespace Celestis {

HTTPStatusCode
HTTPParser::parseStartLine(std::string& content, HTTPRequest& req, bool CRLF)
{
	int p = 0;
	std::string tmp;
	size_t findRt;
	HTTPStatusCode status;

	/* Looking for first WS - Method */
	findRt = content.find(" ");
	if (findRt != std::string::npos) {
		tmp = content.substr(0, findRt);
		if (tmp == "GET") {
			req.method = HTTPMethod::GET;		
		} else if (tmp == "PUT") {
			req.method = HTTPMethod::PUT;
		} else if (tmp == "HEAD") {
			req.method = HTTPMethod::HEAD;
		} else if (tmp == "POST") {
			req.method = HTTPMethod::POST;
		} else if (tmp == "TRACE") {
			req.method = HTTPMethod::TRACE;
		} else if (tmp == "PATCH") {
			req.method = HTTPMethod::PATCH;
		} else if (tmp == "DELETE") {
			req.method = HTTPMethod::DELETE;
		} else if (tmp == "CONNECT") {
			req.method = HTTPMethod::CONNECT;
		} else if (tmp == "OPTIONS") {
			req.method = HTTPMethod::OPTIONS;
		} else {
			return HTTPStatusCode::ClientMethodNotAllowed;
		}
	} else {
		return HTTPStatusCode::ClientBadRequest;
	}
	p = findRt + 1;

	/* Looking for second WS - RequestTarget */
	findRt = content.find(" ", p);
	if (findRt != std::string::npos) {
		req.requestTargetRaw = content.substr(p, findRt - p);
		status = parseURI(req.requestTargetRaw, req);
		switch (status) {
			case HTTPStatusCode::ClientBadRequest:
				return HTTPStatusCode::ClientBadRequest;
			default:
				break;
		}
	} else {
		return HTTPStatusCode::ClientBadRequest;
	}
	p = findRt + 1;
	
	/* Looking for CRLF/EOL - HTTP Version */
	tmp = content.substr(p+5, content.length() - (CRLF? 7: 5) - p);
	p = 0;
	findRt = 0;
	while (p < tmp.length()) {
		if (tmp[p] == '.') {
			if (p > 0) {
				req.majorVersion = stoi(tmp.substr(0, p)); 
				findRt = p + 1;
			} else {
				return HTTPStatusCode::ClientBadRequest;
			}
		} else if (tmp[p] < '0' || tmp[p] > '9') {
			return HTTPStatusCode::ClientBadRequest;
		}
		p++;
	}
	if (p - findRt >= 1) {
		req.minorVersion = stoi(tmp.substr(findRt, p - findRt));
	}

	return HTTPStatusCode::SuccessfulOK;
}

HTTPStatusCode
HTTPParser::parseHeaderField(std::string& content, HTTPRequest& req, bool CRLF)
{
	int p = 0, len = content.length();
	std::string key, val;
	size_t findRt;

	if (len == 0 || content[0] == SP || content[0] == COLON) {
		return HTTPStatusCode::ClientBadRequest;
	}

	findRt = content.find(":");
	if (findRt != std::string::npos) {
		key = content.substr(0, findRt);
	} else {
		return HTTPStatusCode::ClientBadRequest;
	}
	p = findRt + 1;

	findRt = content.find_first_not_of(" ", p);
	if (findRt != std::string::npos) {
		if (CRLF && content.substr(findRt, 2) == "\r\n") {
			return HTTPStatusCode::ClientBadRequest;
		}
		val = content.substr(findRt, len - findRt - (CRLF? 2: 0));	
		
	} else {
		return HTTPStatusCode::ClientBadRequest;
	}

	/* clip OWS at tail */
	for (int i=val.length()-1;i>=0;i--) {
		if (val[i] != SP) {
			val = val.substr(0, i+1);
			break;
		}
	}
	if (val.length() == 0) {
		return HTTPStatusCode::ClientBadRequest;
	}

	req.headerFields.insert({key, val});
	return HTTPStatusCode::SuccessfulOK;
}

bool
HTTPParser::escapeURIToPrevDir(std::string& content) 
{
	int i = content.length(), count = 0;
	std::string tmp;

	if (content[content.length()-1] != '/') {
		count = 1;
	}
	
	while (--i >= 0) {
		if (content[i] == '/') {
			tmp = content.substr(0, i+1);
			transform(tmp.begin(), tmp.end(), tmp.begin(), ::tolower);
			if (tmp == "http://" || tmp == "https://") {
				break;
			}
			count++;
			if (count == 2) {
				content = content.substr(0, i+1);	
			}
		}
	}

	/* only return true for now, but can return false in some case
	 * in the future for some directory access restriction
	 * and return false will trigger 404 error.
	 * */
	return true;
}

HTTPStatusCode
HTTPParser::formatURIAndPort(std::string& content, int sp, std::string& out, int& port, int& pathBegin) 
{
	int p = sp, len = content.length(), i = 0;
	std::size_t pos;
	char prev, curr;
	std::string tmp;
	bool hasHostName = false;

	if (len <= 0) {
		goto BadRequestReturn;
	}

	/* Convert percent encoding */
	pos = content.find("%");
	while (pos != std::string::npos) {
		tmp = content.substr(pos+1, 2);
		if (tmp.length() != 2) {
			goto BadRequestReturn;
		}
		if (!((isHexChar(tmp[0]) & isHexChar(tmp[1])))) {
			goto BadRequestReturn;
		}
		curr = (char)stoi(tmp, nullptr, 16);
		content = content.substr(0, pos) + curr + content.substr(pos+3, len-pos-3);	

		pos = content.find("%", pos+1);
	}

	len = content.length();
	if (p > len) {
		goto BadRequestReturn;
	}

	out = content.substr(0, sp);
	port = default_http_port;
	if (sp >= 1 && out.length() == sp) {
		prev = out[sp-1];
		/* check if the address is https */
		if (sp >= 5 && out[4] == 's') {	
			port = default_https_port;
		}
		hasHostName = true;
	}

	while (p < len) {
		curr = content[p];
		switch (curr) {
			case '/':
				/* eliminate multiple '/' chars */
				if (prev != '/') {
					out += '/';
				}
				if (hasHostName) {
					hasHostName = false;
					pathBegin = p;
				}
				break;	
			case ':':
				/* port number */
				tmp = "";
				for (i=1;i<len-p-1;i++) {
					if (content[p+i] == '/' || p+i+1 == len) {
						if (i > 1) {
							port = atoi(tmp.c_str());
						}
						break;
					} else if (content[p+i] >= '0' && content[p+i] <= '9') {
						tmp += content[p+i];
					} else {
						/* invalid char in port number section */
						goto BadRequestReturn;
					}	
				}	
				p+=i;
				curr = '/';
				out += '/';
				break;
			case '.':
				if (prev == '/') {
					int dotCount = 1;
					for (i=1;i<len-p;i++) {
						if (content[p+i] == '.') {
							dotCount++;
							if (dotCount > 2) {
								goto PageNotFoundReturn;
							}
							if (p+i+1 < len) {
								continue;
							}
						}

						if (content[p+i] == '/' || p+i+1 == len) {
							if (dotCount == 2) {
								/* Escape to prev dir level */
								if (!escapeURIToPrevDir(out)) {
									goto PageNotFoundReturn;
								}
							}
							p+=i;
							curr = '/';
							break;
						} else {
							dotCount = 0;
							break;
						}
					}
					if (dotCount > 0) {
						break;
					}
				}
			case 'a' ... 'z':
			case 'A' ... 'Z':
			case '0' ... '9':
			case '_':
			case '-':
			case '~':
			case '!':
			case '$':
			case '&':
			case '\'':
			case '(':
			case ')':
			case '*':
			case '+':
			case ',':
			case ';':
			case '=':
			case '?':
				out += content[p];	
				break;
			default:
				//found invalid char
				goto PageNotFoundReturn;
				break;
		}
		prev = curr;
		p++;
	}
	return HTTPStatusCode::SuccessfulOK;	

BadRequestReturn:
	return HTTPStatusCode::ClientBadRequest;
PageNotFoundReturn:
	return HTTPStatusCode::ClientNotFound;
}

HTTPStatusCode
HTTPParser::parseURI(std::string& content, HTTPRequest& req)
{
	int p = 0, i, len = content.length(), localState = 0, pathBeginPos = 0;
	std::string tmp, key, val;
	HTTPStatusCode status;
	URIParsingType upType = URIParsingType::Unknown;

	req.requestQuery.clear();

	if (len <= 0) {
		goto BadRequestReturn;
	}

	switch (content[0]) {
		case '*':
			if (req.method == HTTPMethod::OPTIONS) {
				/* Asterisk-Form */
				req.requestTarget = "*";
				return HTTPStatusCode::SuccessfulOK;
			} else {
				goto BadRequestReturn;
			}
		/* alpha */
		case 'A' ... 'Z':
		case 'a' ... 'z':
		case '0' ... '9':
			// compare to http:// or https://
			tmp = content.substr(0, 7);
			transform(tmp.begin(), tmp.end(), tmp.begin(), ::tolower);
			if (tmp == "http://") {
				/* skip first 7 chars in the following parsing stage */
				p = 7;
				req.requestTarget = "http://";
				upType = URIParsingType::AbsoluteForm;
				break;
			}

			tmp = content.substr(0, 8);	
			transform(tmp.begin(), tmp.end(), tmp.begin(), ::tolower);
			if (tmp == "https://") {
				/* skip first 7 chars in the following parsing stage */
				p = 8;
				req.requestTarget = "https://";
				upType = URIParsingType::AbsoluteForm;
				break;
			}
		
			upType = URIParsingType::AuthorityForm;
			break;
		case '/':
			upType = URIParsingType::OriginForm;
			break;
		default:
			goto BadRequestReturn;
	}

	/* 
	 * remove any dir related op (e.g. /../  ,  /./)
   	 * remove any percent encoding
	 * remove any duplicated slash 
	 */
	status = formatURIAndPort(content, p, tmp, req.requestPort, pathBeginPos);
	if (status != HTTPStatusCode::SuccessfulOK) {
		switch (status) {
			case HTTPStatusCode::ClientBadRequest:
				goto BadRequestReturn;
				break;
			case HTTPStatusCode::ClientNotFound:
			default:
				goto PageNotFoundReturn;
				break;
		}
	}
	content = tmp;
	len = content.length();
	req.requestTarget = tmp;
	req.requestTargetPath = tmp.substr(pathBeginPos, tmp.length() - pathBeginPos);

	while (p < len) {
		switch (upType) {
			case URIParsingType::OriginForm:
				/*
				 * Local states:
				 * 0: looking through path(we have formated that before)
				 * 1, 2: query parsing Part (that is what we are interested in)
				 * Figure:   A=B&C=D
				 * A, C: state 1
				 * B, D: state 2
				 */
				if (localState >= 1) {
					for (i=0;i<len-p+1;i++) {
						if (localState == 1) {
							if (content[p+i] == '=') {
								key = content.substr(p, i);
								p+=i;
								localState = 2;
								/* special case: if = is the last char */
								if (p+i == len) {
									if (key.length() > 0) {
										req.requestQuery.insert({key, val});
									}
								}
								break;
							}
						} else if (localState == 2) {
							if (content[p+i] == '&' || p+i == len-1) {
								val = content.substr(p, i+(p+i== len-1 ? 1:0));
								p+=i;
								localState = 1;
								if (key.length() > 0) {
									req.requestQuery.insert({key, val});
								}
								key = "";
								val = "";
								break;
							}
						}
					}
				}else {
					if (content[p] == '?') {
						localState = 1;
						/* clip the content after ?  */
						req.requestTarget = content.substr(0, p);
						req.requestTargetPath = req.requestTarget;
					}
				}
				break;
			case URIParsingType::AuthorityForm:
				/*TODO*******************************TODO 
				 *										*
				 *    | /  \ |          //  \\          * 
				 *   \_\\  //_/        _\\()//_         *
				 *    .'/()\'.        / //  \\ \        *
				 *     \\  //          | \__/ |         *
				 *				this is crab			*
				 *TODO*******************************TODO
				 * */
				break;
			/* Nothing to do with those forms below */
			case URIParsingType::AsteriskForm:
			case URIParsingType::AbsoluteForm:
			default:
				break;
		}
		p++;
	}
	
	return HTTPStatusCode::SuccessfulOK;

BadRequestReturn:
	req.requestTarget = "";
	return HTTPStatusCode::ClientBadRequest;
PageNotFoundReturn:
	req.requestTarget = "";
	return HTTPStatusCode::ClientNotFound;
}


};
