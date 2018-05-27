#pragma once

#include <curl/curl.h>

#include "stdafx.h"

class RpcClient {

private:
	CURL * curl;
	struct curl_slist *requestHeaders;

public:

	struct Response {
		CURLcode code;
		std::map<std::string, std::string> responseHeader;
		std::string responseBody;
	};

	RpcClient(const std::string &language, const std::string &apikey);
	~RpcClient();

	Response fetch(const std::string &url);
};

