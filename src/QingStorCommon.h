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

#ifndef _QINGSTOR_LIBQINGSTOR_QSCOMMON_H_
#define _QINGSTOR_LIBQINGSTOR_QSCOMMON_H_

#include <curl/curl.h>
#include <json/json.h>

#include "lib/http_parser.h"

#include <list>
#include <string>

namespace QingStor {
namespace Internal {

typedef struct {
	std::string keyid;
	std::string secret;
} QSCredential;

typedef enum {
	HOST,
	LOCATION,
	RANGE,
	DATE,
	CONTENTLENGTH,
	CONTENTMD5,
	CONTENTTYPE,
	EXPECT,
	AUTHORIZATION,
	ETAG,
} HeaderField;

typedef struct {
	std::list<std::string> fields;
} HeaderContent;

typedef struct {
	char *data;
	size_t size;
	size_t position;
} BufferInfo;

typedef enum {
	QSRT_NONE,
	QSRT_LIST_BUCKET,
	QSRT_CREATE_BUCKET,
	QSRT_DELETE_BUCKET,
	QSRT_LIST_OBJECT,
	QSRT_HEAD_OBJECT,
	QSRT_DELETE_OBJECT,
	QSRT_GET_DATA,
	QSRT_INIT_MP_UPLOAD,
	QSRT_UPLOAD_MP,
	QSRT_COMP_MP_UPLOAD
} QSRequestType;

typedef struct {
	const char *advance;
	size_t sizeleft;
} MemoryData;

typedef size_t (*MemReadCallback)(void *ptr, size_t size, size_t nmemb, void *userp);

extern bool HeaderContent_Add(HeaderContent *h, HeaderField f, const char *value);

extern struct curl_slist *HeaderContent_GetList(HeaderContent *h);

extern void Signature(HeaderContent *h, const char *path_with_query,
					const QSCredential *cred,
					QSRequestType qsrt,
					MemoryData *md);

extern json_object* DoGetJSON(const char *host, const char *url, const char *bucket,
							const char *location,
							const QSCredential *cred,
							QSRequestType qsrt,
							MemoryData *md);

extern std::string GetFieldString(HeaderField f);

extern size_t ParserCallback(void *contents, size_t size, size_t nmemb, void *userp);

extern void qs_parse_url(const char *url, char **schema, char **host, char **path,
						char **query, char **fullurl);

extern json_object* ParseHttpHeader(const char *content);

}
}

#endif /* _QINGSTOR_LIBQINGSTOR_QSCOMMON_H_ */
