/********************************************************************
 * 2017 -
 * open source under Apache License Version 2.0
 ********************************************************************/
/**
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "platform.h"

#include "QingStorCommon.h"
#include "Exception.h"
#include "ExceptionInternal.h"
#include "Logger.h"

#include "lib/encode.h"

#include <sstream>

namespace QingStor {
namespace Internal {

void
Signature(HeaderContent *h, const char *path_with_query,
			const QSCredential *cred,
			QSRequestType qsrt,
			MemoryData *md)
{
	char timebuf[65];
	char tmpbuf[33];		/* SHA_DIGEST_LENGTH is 20 */
	struct tm *tm_info;
	time_t t;
	std::stringstream sstr;

	/* CONTENT_LENGTH is not a port of StringToSign */
	if (QSRT_LIST_OBJECT == qsrt || QSRT_INIT_MP_UPLOAD == qsrt ||
			QSRT_GET_DATA == qsrt || QSRT_LIST_BUCKET == qsrt ||
			QSRT_HEAD_OBJECT == qsrt || QSRT_DELETE_OBJECT == qsrt ||
			QSRT_ABORT_MP_UPLOAD == qsrt)
	{
		HeaderContent_Add(h, CONTENTLENGTH, "0");
	}
	else if (QSRT_UPLOAD_MP == qsrt || QSRT_COMP_MP_UPLOAD == qsrt)
	{
		std::stringstream tsstr;
		tsstr<<md->sizeleft;
		HeaderContent_Add(h, CONTENTLENGTH, tsstr.str().c_str());
	}

	/* DATE header */
	t = time(NULL);
	tm_info = gmtime(&t);
	strftime(timebuf, 64, "%a, %d %b %Y %H:%M:%S GMT", tm_info);
	HeaderContent_Add(h, DATE, timebuf);

	if (QSRT_LIST_OBJECT == qsrt || QSRT_GET_DATA == qsrt)
	{
		sstr<<"GET\n\n\n"<<timebuf<<"\n"<<path_with_query;
	}
	else if (QSRT_HEAD_OBJECT == qsrt)
	{
		sstr<<"HEAD\n\n\n"<<timebuf<<"\n"<<path_with_query;
	}
	else if (QSRT_DELETE_OBJECT == qsrt || QSRT_DELETE_BUCKET == qsrt || QSRT_ABORT_MP_UPLOAD == qsrt)
	{
		sstr<<"DELETE\n\n\n"<<timebuf<<"\n"<<path_with_query;
	}
	else if (QSRT_INIT_MP_UPLOAD == qsrt)
	{
		HeaderContent_Add(h, CONTENTTYPE, "plain/text");
		sstr<<"POST\n\n"<<"plain/text"<<"\n"<<timebuf<<"\n"<<path_with_query;
	}
	else if (QSRT_CREATE_BUCKET == qsrt)
	{
		sstr<<"PUT\n\n\n"<<timebuf<<"\n"<<path_with_query;
	}
	else if (QSRT_UPLOAD_MP == qsrt)
	{
		HeaderContent_Add(h, CONTENTTYPE, "plain/text");
		sstr<<"PUT\n\n"<<"plain/text"<<"\n"<<timebuf<<"\n"<<path_with_query;
	}
	else
	{
		if (QSRT_COMP_MP_UPLOAD != qsrt)
		{
			THROW(InvalidParameter, "unknown QSRequestType %d", qsrt);
		}
		HeaderContent_Add(h, CONTENTTYPE, "application/json");
		sstr<<"POST\n\n"<<"application/json"<<"\n"<<timebuf<<"\n"<<path_with_query;
	}

	QingStor::Internal::sha256hmac(sstr.str().c_str(), tmpbuf, cred->secret.c_str());
	std::string signature = QingStor::Internal::Base64Encode(tmpbuf, 32);		// SHA256_DIGEST_LENGHT is 32

	sstr.str("");
	sstr.clear();
	sstr<<"QS-HMAC-SHA256 "<<cred->keyid<<":"<<signature.c_str();

	if ((!cred->keyid.empty()) && (!cred->secret.empty()))
	{
		HeaderContent_Add(h, AUTHORIZATION, sstr.str().c_str());
	}
}

std::string GetFieldString(HeaderField f)
{
	switch(f) {
	case HOST:
		return std::string("Host");
	case LOCATION:
		return std::string("Location");
	case RANGE:
		return std::string("Range");
	case DATE:
		return std::string("Date");
	case CONTENTLENGTH:
		return std::string("Content-Length");
	case CONTENTMD5:
		return std::string("Content-MD5");
	case CONTENTTYPE:
		return std::string("Content-Type");
	case EXPECT:
		return std::string("Expect");
	case AUTHORIZATION:
		return std::string("Authorization");
	case ETAG:
		return std::string("ETag");
	default:
		return std::string("unknown");
	}
}

bool HeaderContent_Add(HeaderContent *h, HeaderField f, const char *v)
{
	if (strcmp(v, "") != 0)
	{
		std::stringstream sstr;
		sstr<<GetFieldString(f)<<": "<<v;
		h->fields.push_back(sstr.str());
		return true;
	}
	else
	{
		return false;
	}
}

struct curl_slist*
HeaderContent_GetList(HeaderContent *h)
{
	struct curl_slist *chunk = NULL;
	std::list<std::string>::iterator itr = h->fields.begin();
	while (itr != h->fields.end())
	{
		chunk = curl_slist_append(chunk, (char *)itr->c_str());
		itr++;
	}
	return chunk;
}

size_t
ParserCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
	size_t realsize = size * nmemb;
	BufferInfo *bufferInfo = (BufferInfo *)userp;
	if (bufferInfo->data == NULL)
	{
		bufferInfo->data = new char[1024];
		bufferInfo->size = 1024;
		bufferInfo->position = 0;
	}
	while (bufferInfo->position + realsize >= bufferInfo->size)
	{
		char* p = new char[(bufferInfo->size << 1)];
		memcpy(p, bufferInfo->data, bufferInfo->position);
		delete bufferInfo->data;
		bufferInfo->data = p;
		bufferInfo->size <<= 1;
	}
	memcpy(bufferInfo->data + bufferInfo->position, contents, realsize);
	bufferInfo->position += realsize;

	return realsize;
}

static size_t
mem_read_callback(void *ptr, size_t size, size_t nmemb, void *userp)
{
	MemoryData *puppet = (MemoryData *)userp;

	size_t realsize = size * nmemb;
	size_t n2read = realsize < puppet->sizeleft ? realsize : puppet->sizeleft;
	if (!n2read)
	{
		return 0;
	}

	memcpy(ptr, puppet->advance, n2read);
	puppet->advance += n2read;
	puppet->sizeleft -= n2read;

	return n2read;
}

json_object*
DoGetJSON(const char *host, const char *url, const char *bucket,
			const char *location,
			const QSCredential *cred,
			QSRequestType qsrt,
			MemoryData *md, int retries)
{
	struct json_object *result = NULL;
	int failing = 0;

retry:
	try {
		result = DoGetJSON_Internal(host, url, bucket, location, cred, qsrt, md);
	} catch (...) {
		if(++failing < retries) {
			LOG(WARNING, "qingstor request type %d is failed, retrying", qsrt);
 			goto retry;
		} else {
			LOG(LOG_ERROR, "qingstor request type %d is failed after retried %d times", qsrt, retries);
			throw;
		}
	}
	return result;
}

json_object*
DoGetJSON_Internal(const char *host, const char *url, const char *bucket,
			const char *location,
			const QSCredential *cred,
			QSRequestType qsrt,
			MemoryData *md)
{
	CURL *curl;
	char *path;
	char *query;
	volatile BufferInfo jsonInfo;
	volatile BufferInfo yamlInfo;
	struct json_object *result = NULL;
	jsonInfo.data = NULL;
	yamlInfo.data = NULL;

	try {
		curl = curl_easy_init();
		if (!curl)
		{
			THROW(OutOfMemoryException, "cound not create curl instance");
		}
		curl_easy_setopt(curl, CURLOPT_URL, url);
		curl_easy_setopt(curl, CURLOPT_FORBID_REUSE, 1L);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&jsonInfo);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, ParserCallback);
		if (QSRT_HEAD_OBJECT == qsrt)
		{
			curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
			curl_easy_setopt(curl, CURLOPT_HEADERDATA, (void *)&yamlInfo);
			curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, ParserCallback);
		}
		if (QSRT_DELETE_OBJECT == qsrt || QSRT_DELETE_BUCKET == qsrt || QSRT_ABORT_MP_UPLOAD == qsrt)
		{
			curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
		}
		if (QSRT_CREATE_BUCKET == qsrt)
		{
			curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
		}
		if (QSRT_INIT_MP_UPLOAD == qsrt)
		{
			curl_easy_setopt(curl, CURLOPT_POST, 1L);
			curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "uploads");
			curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)strlen("uploads"));
		}
		if (QSRT_UPLOAD_MP == qsrt || QSRT_COMP_MP_UPLOAD == qsrt)
		{
			/*
			 * now specify which file/data to upload
			 */
			if (md == NULL)
			{
				THROW(InvalidParameter, "qsrt type is %d while MemoryData is empty", qsrt);
			}
			curl_easy_setopt(curl, CURLOPT_READDATA, (void *)md);

			/*
			 * we want to use our own read function
			 */
			curl_easy_setopt(curl, CURLOPT_READFUNCTION, mem_read_callback);

			/*
			 * provide the size of the upload
			 */
			curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, (curl_off_t)md->sizeleft);

			if (QSRT_UPLOAD_MP == qsrt)
			{
				/*
				 * enable uploading
				 */
				curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);

				/*
				 * HTTP PUT please
				 */
				curl_easy_setopt(curl, CURLOPT_PUT, 1L);
			}
			else
			{
				/*
				 * HTTP POST please
				 */
				curl_easy_setopt(curl, CURLOPT_POST, 1L);
			}
		}
		HeaderContent *header = new HeaderContent;
		HeaderContent_Add(header, HOST, host);
		if (location != NULL)
		{
			HeaderContent_Add(header, LOCATION, location);
		}

		qs_parse_url(url,
					NULL /* schema */,
					NULL /* host */,
					&path,
					&query,
					NULL /* fullurl */);

		std::stringstream sstr;
		if (query)
		{
			sstr<<"/"<<bucket<<path<<"?"<<query;
		}
		else
		{
			sstr<<"/"<<bucket<<path;
		}
		Signature(header, sstr.str().c_str(), cred, qsrt, md);
		if (path)
		{
			delete path;
		}
		if (query)
		{
			delete query;
		}

		struct curl_slist *chunk = HeaderContent_GetList(header);
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);

		CURLcode res = curl_easy_perform(curl);

		if (CURLE_OK != res)
		{
			THROW(QingStorNetworkConnectException, "curl_easy_perform() failed: %s",
					curl_easy_strerror(res));
		}
		else
		{
			if (QSRT_LIST_OBJECT == qsrt || QSRT_INIT_MP_UPLOAD == qsrt || QSRT_LIST_BUCKET == qsrt)
			{
				if (!jsonInfo.data)
				{
					THROW(QingStorNetworkException, "HTTP response is empty");
				}
				else
				{
					jsonInfo.data[jsonInfo.position] = '\0';
					result = json_tokener_parse(jsonInfo.data);
					if (!result)
					{
						THROW(QingStorNetworkException, "HTTP response could not parse into JSON: %s with qsrt as %d",
								jsonInfo.data, qsrt);
					}
					delete jsonInfo.data;
					jsonInfo.data = NULL;
				}
			}
			else
			{
				if (jsonInfo.data)
				{
					jsonInfo.data[jsonInfo.position] = '\0';
					THROW(QingStorNetworkException, "HTTP response is non-empty: %s with qsrt as %d",
							jsonInfo.data, qsrt);
					delete jsonInfo.data;
					jsonInfo.data = NULL;
				}
				if (QSRT_HEAD_OBJECT == qsrt)
				{
					if (!yamlInfo.data)
					{
						THROW(QingStorNetworkException, "HTTP response header is empty");
					}
					yamlInfo.data[yamlInfo.position] = '\0';
					result = ParseHttpHeader(yamlInfo.data);
					if (!result) {
						THROW(QingStorNetworkException, "HTTP Header response could not parse into JSON: %s with qsrt as %d", yamlInfo.data, qsrt);
					}
					delete yamlInfo.data;
					yamlInfo.data = NULL;
				}
			}
		}
		curl_slist_free_all(chunk);
		curl_easy_cleanup(curl);
		delete header;
	} catch(const QingStorException & e)
	{
		if (jsonInfo.data)
		{
			delete jsonInfo.data;
			jsonInfo.data = NULL;
		}
		if (yamlInfo.data)
		{
			delete yamlInfo.data;
			yamlInfo.data = NULL;
		}
		throw e;
	}

	return result;
}

static char *
extract_field(const char *url, const struct http_parser_url *u,
				enum http_parser_url_fields i)
{
	char *ret = NULL;

	if ((u->field_set & (1 << i)) != 0)
	{
		ret = new char[u->field_data[i].len + 1];
		memcpy(ret, url + u->field_data[i].off, u->field_data[i].len);
		ret[u->field_data[i].len] = 0;
	}
	return ret;
}

/*
 * Extract certain fields from URL.
 */
void
qs_parse_url(const char *url, char **schema, char **host, char **path,
			char **query, char **fullurl)
{
	struct http_parser_url u;
	int len;
	int result;

	if (!url)
	{
		THROW(InvalidParameter, "no URL given"); /* shouldn't happen */
	}

	len = strlen(url);
	if (fullurl)
	{
		*fullurl = strdup(url);
	}

	/* only parse len, no need to memset this->fullurl */
	result = http_parser_parse_url(url, len, false, &u);
	if (result != 0)
	{
		THROW(InvalidParameter, "could not parse URL \"%s\": error code %d\n", url, result);
	}

	if (schema)
	{
		*schema = extract_field(url, &u, UF_SCHEMA);
	}
	if (host)
	{
		*host = extract_field(url, &u, UF_HOST);
	}
	if (path)
	{
		*path = extract_field(url, &u, UF_PATH);
		if ((strlen(*path) == 1) && (*(*path) == '/'))
		{
			*(*path) = '\0';
		}
	}
	if (query)
	{
		char *pos;
		int num = 0;
		char *tmpbuf;
		char *bufpos;

		*query = extract_field(url, &u, UF_QUERY);
		/*
		 * we need to replace all '/'
		 * with '%2F' in the query string.
		 */
		pos = *query;
		if (!pos)
		{
			return;
		}
		while (*pos != '\0')
		{
			if (*pos == '/')
			{
				num++;
			}
			pos++;
		}
		tmpbuf = new char[strlen(*query) + 2 * num + 1];
		tmpbuf[strlen(*query) + 2 * num] = '\0';
		pos = *query;
		bufpos = tmpbuf;
		while (*pos != '\0')
		{
			if (*pos == '/')
			{
				*bufpos = '%';
				*(bufpos + 1) = '2';
				*(bufpos + 2) = 'F';
				bufpos += 3;
			}
			else
			{
				*bufpos = *pos;
				bufpos += 1;
			}
			pos++;
		}
		delete (*query);
		*query = tmpbuf;
	}
}

json_object* ParseHttpHeader(const char *content)
{
	std::stringstream sstr(content);
	std::stringstream jsonstr;
	std::string str;
	jsonstr<<"{\n";
	while(getline(sstr, str))
	{
		std::size_t pos;
		if ((pos = str.find(":")) != std::string::npos)
		{
			std::string key = str.substr(0, pos);
			if (pos + 2 >= str.length())
				return NULL;
			std::string value = str.substr(pos + 2);
			std::size_t epos = value.find("\r");
			if (epos != std::string::npos)
			{
				value.erase(epos);
			}
			if ((key.compare("Content-Length") != 0) && (key.compare("ETag") != 0))
			{
				jsonstr<<"\""<<key<<"\": "<<"\""<<value<<"\",\n";
			}
			else
			{
				jsonstr<<"\""<<key<<"\": "<<value<<",\n";
			}
		}
		str.clear();
	}
	jsonstr<<"}";
	json_object * result = json_tokener_parse(jsonstr.str().c_str());
	return result;
}

}
}
