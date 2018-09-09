#include "stdafx.h"
#include "RpcClient.h"

size_t curlCallbackResponse(void *contents, size_t size, size_t nmemb, std::string *s);
size_t curlCallbackHeaders(char* b, size_t size, size_t nitems, std::map<std::string, std::string> *userdata);

RpcClient::RpcClient(const std::string &language, const std::string &apikey) {
	this->curl = curl_easy_init();
	if (curl == nullptr) 
		throw std::runtime_error("Unable to initalize libcurl!");

	this->requestHeaders = nullptr;
	this->requestHeaders = curl_slist_append(requestHeaders, "Content-Type: application/json");
	this->requestHeaders = curl_slist_append(requestHeaders,(std::string("Accept-Language: ") + language).c_str());
	if (!apikey.empty())
		this->requestHeaders = curl_slist_append(requestHeaders, (std::string("Authorization: Bearer ") + apikey).c_str());
	
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, requestHeaders);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlCallbackResponse);
	curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, curlCallbackHeaders);
	curl_easy_setopt(curl, CURLOPT_HEADER, 0);
}

RpcClient::~RpcClient() {
	curl_easy_cleanup(this->curl);
}

RpcClient::Response RpcClient::fetch(const std::string & url) {
	RpcClient::Response response;

	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response.responseBody);
	curl_easy_setopt(curl, CURLOPT_HEADERDATA, &response.responseHeader);
	response.code = curl_easy_perform(curl);

	return response;
}

size_t curlCallbackResponse(void *contents, size_t size, size_t nmemb, std::string *s) {
	size_t newLength = size * nmemb;
	size_t oldLength = s->size();
	try {
		s->resize(oldLength + newLength);
	} catch (std::bad_alloc &e) {
		//TODO: Handle memory problem
		return 0;
	}

	std::copy((char*)contents, (char*)contents + newLength, s->begin() + oldLength);
	return size * nmemb;
}

size_t curlCallbackHeaders(char* b, size_t size, size_t nitems, std::map<std::string, std::string> *userdata) {
	const size_t numbytes = size * nitems;
	const std::string line(b);

	std::vector<std::string> strings;
	std::istringstream iLine(line);
	std::string tmp;

	while (std::getline(iLine, tmp, ':')) {
		tmp.erase(tmp.find_last_not_of(" \n\r\t") + 1);
		strings.push_back(tmp);
	}

	if (strings.size() >= 2)
		userdata->operator[](strings[0]) = strings[1];

	return numbytes;
}