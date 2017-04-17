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

#include "QingStorWriter.h"
#include "QingStorCommon.h"
#include "Exception.h"
#include "ExceptionInternal.h"

#include <sstream>

namespace QingStor {
namespace Internal {

QingStorWriter::QingStorWriter(shared_ptr<Configuration> configuration, std::string bucket, ObjectInfo object)
							: QingStorRWBase(configuration, bucket, object)
{
	std::stringstream sstr;
	sstr<<bucket<<"."<<configuration->mLocation<<"."<<configuration->mHost;
	std::string host = sstr.str();
	sstr.str("");
	sstr.clear();

	sstr<<configuration->mProtocol<<"://"<<host<<"/"<<object.key<<"?uploads";
	std::string url = sstr.str();
	sstr.str("");
	sstr.clear();

	mCred = {configuration->mAccessKeyId, configuration->mSecretAccessKey};
	initMultipartUpload(host, url, bucket, &mCred);
	mPartNum = 0;
	mBuffSize = configuration->mChunkSize;
	mWritePos = 0;
	mBuffer = new char[mBuffSize];
}

void QingStorWriter::transferData(const char *buffer, int32_t buffsize)
{
	int32_t buffPos = 0;
	while (buffPos != buffsize)
	{
		while (mWritePos != mBuffSize)
		{
			mBuffer[mWritePos++] = buffer[buffPos++];
			if (buffPos == buffsize)
			{
				break;
			}
		}
		if (mWritePos == mBuffSize)
		{
			doSend(mBuffer, mBuffSize);
			mWritePos = 0;
		}
	}

	return;
}

void QingStorWriter::initMultipartUpload(std::string host, std::string url, std::string bucket,
						QSCredential *cred)
{
	struct json_object* resp_body = NULL;

	try {
		resp_body = DoGetJSON(host.c_str(), url.c_str(), bucket.c_str(), NULL,
							cred, QSRT_INIT_MP_UPLOAD, NULL);
		if (!resp_body)
		{
			THROW(QingStorNetworkException, "could not init multipart upload");
		}
		if (!extractUploadIDContent(resp_body))
		{
			struct json_object *code = NULL;
			const char *err_code = NULL;
			if (!json_object_object_get_ex(resp_body, "code", &code))
			{
				THROW(QingStorNetworkException, "could not init multipart with message \"%s\"",
										json_object_to_json_string(resp_body));
			}
			err_code = json_object_get_string(code);

			if (strcmp(err_code, "permission_denied") == 0)
			{
				THROW(QingStorNetworkException, "you don't have enough permission to access"
						" bucket %s", bucket.c_str());
			}
			else
			{
				THROW(QingStorNetworkException, "could not init multipart with message \"%s\"",
										json_object_to_json_string(resp_body));
			}
			json_object_put(resp_body);
		}
	} catch (QingStorException & e)
	{
		/* always cleanup */
		if (resp_body)
		{
			json_object_put(resp_body);
		}

		throw e;
	}
	return;
}

bool QingStorWriter::extractUploadIDContent(struct json_object *resp_body)
{
	if (!resp_body)
	{
		return false;
	}

	/*
	 * get upload id
	 */
	struct json_object *value;
	if (!json_object_object_get_ex(resp_body, "upload_id", &value))
	{
		return false;
	}
	mUploadId = std::string(json_object_get_string(value));

	/*
	 * get key
	 */
	if (!json_object_object_get_ex(resp_body, "key", &value))
	{
		return false;
	}
	mKey = std::string(json_object_get_string(value));

	/*
	 * TODO: verify the bucket
	 */
	return true;
}

bool QingStorWriter::uploadMultipart(std::string host, std::string url, std::string bucket, QSCredential *cred,
							const char *data, int32_t length)
{
	struct json_object *resp_body = NULL;
	MemoryData md;
	md.advance = data;
	md.sizeleft = length;

	try {
		resp_body = DoGetJSON(host.c_str(), url.c_str(), bucket.c_str(), NULL, cred, QSRT_UPLOAD_MP, &md);
		if (resp_body)
		{
			json_object_put(resp_body);
		}
	} catch (QingStorException & e)
	{
		/*
		 * always cleanup
		 */
		if (resp_body)
		{
			json_object_put(resp_body);
		}

		throw e;
	}

	return (resp_body == NULL);
}

void QingStorWriter::doSend(const char *data, int32_t length)
{
	std::stringstream sstr;
	sstr<<mBucket<<"."<<mConfiguration->mLocation<<"."<<mConfiguration->mHost;
	std::string host = sstr.str();
	sstr.str("");
	sstr.clear();

	sstr<<mConfiguration->mProtocol<<"://"<<host<<"/"<<mKey;
	sstr<<"?part_number="<<mPartNum<<"&upload_id="<<mUploadId;
	std::string url = sstr.str();
	sstr.str("");
	sstr.clear();

	if (!uploadMultipart(host.c_str(), url.c_str(), mBucket.c_str(), &mCred, data, length))
	{
		THROW(QingStorIOException, "writer transfer data");
	}
	else
	{
		mPartNum++;
	}

	return;
}

void QingStorWriter::flush()
{
	if (mWritePos != 0)
	{
		doSend(mBuffer, mWritePos);
		mWritePos = 0;
	}

	return;
}

void QingStorWriter::close()
{
	flush();

	std::stringstream sstr;
	sstr<<mBucket<<"."<<mConfiguration->mLocation<<"."<<mConfiguration->mHost;
	std::string host = sstr.str();
	sstr.str("");
	sstr.clear();

	sstr<<mConfiguration->mProtocol<<"://"<<host<<"/"<<mKey;
	sstr<<"?upload_id="<<mUploadId;
	std::string url = sstr.str();
	sstr.str("");
	sstr.clear();

	completeMultipartUpload(host.c_str(), url.c_str(), mBucket, &mCred);

	if (mBuffer)
	{
		delete [] mBuffer;
		mBuffer = NULL;
	}
	return;
}

bool QingStorWriter::completeMultipartUpload(std::string host, std::string url, std::string bucket, QSCredential *cred)
{
	struct json_object *resp_body = NULL;
	struct json_object *req_body = NULL;
	char *body;
	MemoryData md;

	try {
		/* generate the multiparts request body */
		do {
			int i;
			struct json_object *value = NULL;

			req_body = json_object_new_object();
			if (!req_body)
			{
				THROW(OutOfMemoryException, "could not create new json object");
			}
			value = json_object_new_array();
			if (!value)
			{
				json_object_put(req_body);
				req_body = NULL;
				THROW(OutOfMemoryException, "could not create new json object");
			}
			json_object_object_add(req_body, "object_parts", value);
			for(i = 0; i < mPartNum; i++)
			{
				struct json_object *part_num_obj;
				struct json_object *element;

				part_num_obj = json_object_new_int(i);
				if (!part_num_obj)
				{
					THROW(OutOfMemoryException, "could not create new json object");
				}
				element = json_object_new_object();
				if (!element)
				{
					json_object_put(part_num_obj);
					THROW(OutOfMemoryException, "could not create new json object");
				}
				json_object_object_add(element, "part_number", part_num_obj);
				if (json_object_array_add(value, element) != 0)
				{
					json_object_put(element);
					THROW(OutOfMemoryException, "could not add new object into a json array");
				}
			}
		} while(0);

		body = (char *)json_object_to_json_string(req_body);
		md.advance = body;
		md.sizeleft = strlen(body);

		resp_body = DoGetJSON(host.c_str(), url.c_str(), bucket.c_str(), NULL,
								cred, QSRT_COMP_MP_UPLOAD, &md);
		if (resp_body)
		{
			json_object_put(resp_body);
		}
		if (req_body)
		{
			json_object_put(req_body);
		}
	} catch (QingStorException & e)
	{
		if (resp_body)
		{
			json_object_put(resp_body);
		}
		if (req_body)
		{
			json_object_put(req_body);
		}
		throw e;
	}

	return (resp_body == NULL);
}

}
}
