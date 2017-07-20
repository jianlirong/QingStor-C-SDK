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

#include "Context.h"

#include "QingStorCommon.h"
#include "Exception.h"
#include "ExceptionInternal.h"

#include <stdlib.h>

#include <sstream>

namespace QingStor {
namespace Internal {

static int
ObjectContentComp(const void *a_, const void *b_)
{
	const ObjectInfo *a = (const ObjectInfo *) a_;
	const ObjectInfo *b = (const ObjectInfo *) b_;

	return strcmp(a->key.c_str(), b->key.c_str());
}

Context::Context(std::string location, std::string access_key_id, std::string secret_access_key, int64_t chunk_size)
{
	mConfiguration = shared_ptr<Configuration> (new Configuration(location, access_key_id, secret_access_key, chunk_size));
}

Context::Context(std::string config_file)
{
	mConfiguration = shared_ptr<Configuration> (new Configuration(config_file));
}

shared_ptr<ListBucketResult> Context::listBuckets(std::string location)
{
	std::stringstream sstr;
	struct json_object *resp_body = NULL;
	shared_ptr<ListBucketResult> result = shared_ptr<ListBucketResult> (new ListBucketResult());

	do {
		sstr<<mConfiguration->mProtocol<<"://"<<mConfiguration->mHost<<"/";
		QSCredential cred = {mConfiguration->mAccessKeyId, mConfiguration->mSecretAccessKey};

		try {
			if (location.empty())
			{
				resp_body = DoGetJSON(mConfiguration->mHost.c_str(), sstr.str().c_str(), NULL,
									NULL, &cred, QSRT_LIST_BUCKET, NULL, mConfiguration->mConnectionRetries);
			}
			else
			{
				resp_body = DoGetJSON(mConfiguration->mHost.c_str(), sstr.str().c_str(), NULL,
									location.c_str(), &cred, QSRT_LIST_BUCKET, NULL, mConfiguration->mConnectionRetries);
			}
			if (!resp_body)
			{
				THROW(QingStorNetworkException, "could not list bucket \"%s\"", sstr.str().c_str());
			}
			if (!extractListBucketContent(result, resp_body))
			{
				THROW(QingStorNetworkException, "could not parse list bucket result \"%s\"",
						json_object_to_json_string(resp_body));
			}
			json_object_put(resp_body);
		} catch (QingStorException & e)
		{
			/* always cleanup */
			if (resp_body)
			{
				json_object_put(resp_body);
			}

			throw e;
		}
	} while (false);

	return result;
}

shared_ptr<ListObjectResult> Context::listObjects(std::string bucket, std::string prefix)
{
	std::stringstream sstr;
	struct json_object *resp_body = NULL;
	shared_ptr<ListObjectResult> result = shared_ptr<ListObjectResult> (new ListObjectResult());

	/* to record the last file name */
	shared_ptr<std::string> current_marker = shared_ptr<std::string> (new std::string());

	/* to check if there is no left files */
	bool eof = false;

	sstr<<bucket<<"."<<mConfiguration->mLocation<<"."<<mConfiguration->mHost;
	std::string host = sstr.str();
	sstr.str("");
	sstr.clear();

	do {
		if (!prefix.empty())
		{
			if (current_marker->empty())
			{
				sstr<<mConfiguration->mProtocol<<"://"<<host;
				sstr<<"/?prefix="<<prefix;
			}
			else
			{
				sstr<<mConfiguration->mProtocol<<"://"<<host;
				sstr<<"/?marker="<<current_marker->c_str()<<"&prefix="<<prefix;
			}
		}
		else
		{
			if (current_marker->empty())
			{
				sstr<<mConfiguration->mProtocol<<"://"<<host;
				sstr<<"/";
			}
			else
			{
				sstr<<mConfiguration->mProtocol<<"://"<<host;
				sstr<<"/?marker="<<current_marker->c_str();
			}
		}

		QSCredential cred = {mConfiguration->mAccessKeyId, mConfiguration->mSecretAccessKey};

		try {
			resp_body = DoGetJSON(host.c_str(), sstr.str().c_str(),
								bucket.c_str(), NULL, &cred, QSRT_LIST_OBJECT, NULL, mConfiguration->mConnectionRetries);
			if (!resp_body)
			{
				THROW(QingStorNetworkException, "could not list bucket \"%s\"", sstr.str().c_str());
			}
			if (!extractListObjectContent(result, resp_body, current_marker, &eof))
			{
				struct json_object *code = NULL;
				const char  *err_code = NULL;
				if (!json_object_object_get_ex(resp_body, "code", &code))
				{
					THROW(QingStorNetworkException, "could not list bucket with response "
							"\"%s\"", json_object_to_json_string(resp_body));
				}
				err_code = json_object_get_string(code);

				/*
				 * bucket doesn't exist
				 */
				if (strcmp(err_code, "bucket_not_exists") == 0)
				{
					THROW(QingStorNetworkException, "the bucket %s you are accessing doesn't exist", bucket.c_str());
				}
				else if (strcmp(err_code, "permission_denied") == 0)
				{
					THROW(AccessControlException, "you don't have enough permission to access "
							"bucket %s", bucket.c_str());
				}
				else if (strcmp(err_code, "invalid_access_key_id") == 0)
				{
					THROW(InvalidParameter, "the access key id \"%s\" you provided does not exist",
										cred.keyid.c_str());
				}
				else
				{
					THROW(QingStorNetworkException, "could not list bucket with "
							"response \"%s\"", json_object_to_json_string(resp_body));
				}
			}
			json_object_put(resp_body);
			sstr.str("");
			sstr.clear();
		} catch (QingStorException & e)
		{
			/* always cleanup */
			if (resp_body)
			{
				json_object_put(resp_body);
			}
			sstr.str("");
			sstr.clear();

			throw e;
		}
	} while (!eof);

	return result;
}

bool Context::extractListObjectContent(shared_ptr<ListObjectResult> result, struct json_object *resp_body,
									shared_ptr<std::string> current_marker, bool *eof)
{
	if ((!result) || (!resp_body))
	{
		return false;
	}

	/*
	 * get bucket name
	 */
	struct json_object *value = NULL;
	if (!json_object_object_get_ex(resp_body, "name", &value))
	{
		return false;
	}
	result->name = std::string(json_object_get_string(value));

	/*
	 * get bucket prefix
	 */
	if (!json_object_object_get_ex(resp_body, "prefix", &value))
	{
		return false;
	}
	result->prefix = std::string(json_object_get_string(value));

	/*
	 * get bucket maxSize
	 */
	if (!json_object_object_get_ex(resp_body, "limit", &value))
	{
		return false;
	}
	result->limit = json_object_get_int(value);

	/*
	 * get key list
	 */
	if (!json_object_object_get_ex(resp_body, "keys", &value))
	{
		return false;
	}

	/*
	 * if there is no elements in "keys", set eof to true
	 */
	if ((json_object_array_length(value) == 0) ||
			(json_object_array_length(value) < static_cast<int>(result->limit)))
	{
		*eof = true;
	}

	for (int i = 0; i < json_object_array_length(value); i++)
	{
		struct json_object *element;
		struct json_object *tmpvalue;
		ObjectInfo item;

		element = json_object_array_get_idx(value, i);
		/*
		 * get key value
		 */
		if (!json_object_object_get_ex(element, "key", &tmpvalue))
		{
			return false;
		}
		item.key = std::string(json_object_get_string(tmpvalue));
		/*
		 * get size value
		 */
		if (!json_object_object_get_ex(element, "size", &tmpvalue))
		{
			return false;
		}
		item.size = json_object_get_int64(tmpvalue);
		result->objects.push_back(item);
		/*
		 * set current_marker to the last file
		 */
		if (i == json_object_array_length(value) - 1)
		{
			*current_marker = item.key;
		}
	}
	qsort(&(result->objects[0]), result->objects.size(), sizeof(ObjectInfo), ObjectContentComp);
	return true;
}

bool Context::extractListBucketContent(shared_ptr<ListBucketResult> result, struct json_object *resp_body)
{
	int count = 0;
	struct json_object *value;

	if ((!result) || (!resp_body))
	{
		return false;
	}

	/*
	 * get count value
	 */
	if (!json_object_object_get_ex(resp_body, "count", &value))
	{
		return false;
	}
	count = json_object_get_int(value);

	/*
	 * get bucket list
	 */
	if (!json_object_object_get_ex(resp_body, "buckets", &value))
	{
		return false;
	}

	if ((json_object_array_length(value) != count))
	{
		return false;
	}

	for (int i = 0; i < count; i++)
	{
		struct json_object *element;
		struct json_object *tmpvalue;
		BucketInfo item;

		element = json_object_array_get_idx(value, i);

		/*
		 * get name value
		 */
		if (!json_object_object_get_ex(element, "name", &tmpvalue))
		{
			return false;
		}
		item.name = std::string(json_object_get_string(tmpvalue));

		/*
		 * get location value
		 */
		if (!json_object_object_get_ex(element, "location", &tmpvalue))
		{
			return false;
		}
		item.location = std::string(json_object_get_string(tmpvalue));

		/*
		 * get url value
		 */
		if (!json_object_object_get_ex(element, "url", &tmpvalue))
		{
			return false;
		}
		item.url = std::string(json_object_get_string(tmpvalue));

		/*
		 * get created value
		 */
		if (!json_object_object_get_ex(element, "created", &tmpvalue))
		{
			return false;
		}
		item.created = std::string(json_object_get_string(tmpvalue));
		result->buckets.push_back(item);
	}

	return true;
}

shared_ptr<HeadObjectResult> Context::headObject(std::string bucket, std::string key)
{
	std::stringstream sstr;
	std::string host;
	struct json_object *resp_body = NULL;
	shared_ptr<HeadObjectResult> result = shared_ptr<HeadObjectResult> (new HeadObjectResult());
	sstr<<bucket<<"."<<mConfiguration->mLocation<<"."<<mConfiguration->mHost;
	host = sstr.str();
	sstr.str("");
	sstr.clear();
	do {
		sstr<<mConfiguration->mProtocol<<"://"<<bucket<<"."<<mConfiguration->mLocation<<"."<<mConfiguration->mHost;
		sstr<<"/"<<key;

		QSCredential cred = {mConfiguration->mAccessKeyId, mConfiguration->mSecretAccessKey};

		try {
			resp_body = DoGetJSON(host.c_str(), sstr.str().c_str(), bucket.c_str(), NULL, &cred, QSRT_HEAD_OBJECT, NULL, mConfiguration->mConnectionRetries);
			if (!resp_body)
			{
				THROW(QingStorNetworkException, "could not head object \"%s\"", sstr.str().c_str());
			}
			if (!extractHeadObjectContent(result, resp_body))
			{
				THROW(QingStorNetworkException, "could not parse head object result \"%s\"",
												json_object_to_json_string(resp_body));
			}
			json_object_put(resp_body);
		} catch (QingStorException &e)
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

	} while (false);

	return result;
}

void Context::createBucket(std::string location, std::string bucket)
{
	std::stringstream sstr;
	std::string host, url;
	struct json_object *resp_body = NULL;

	/* construct the host */
	if (location.empty())
	{
		sstr<<bucket<<"."<<mConfiguration->mLocation<<"."<<mConfiguration->mHost;
	}
	else
	{
		sstr<<bucket<<"."<<location<<"."<<mConfiguration->mHost;
	}
	host = sstr.str();
	sstr.str("");
	sstr.clear();

	/* construct the url */
	sstr<<mConfiguration->mProtocol<<"://"<<host<<"/";
	url = sstr.str();
	sstr.str("");
	sstr.clear();

	QSCredential cred = {mConfiguration->mAccessKeyId, mConfiguration->mSecretAccessKey};
	do {
		try {
			resp_body = DoGetJSON(host.c_str(), url.c_str(), bucket.c_str(), NULL, &cred,
											QSRT_CREATE_BUCKET, NULL, mConfiguration->mConnectionRetries);
		} catch (QingStorException & e)
		{
			/* always clean up */
			if (resp_body)
			{
				json_object_put(resp_body);
			}
			throw e;
		}
	} while (false);

	return ;
}

void Context::deleteBucket(std::string location, std::string bucket)
{
	std::stringstream sstr;
	std::string host, url;
	struct json_object *resp_body = NULL;

	/* construct the host */
	if (location.empty())
	{
		sstr<<bucket<<"."<<mConfiguration->mLocation<<"."<<mConfiguration->mHost;
	}
	else
	{
		sstr<<bucket<<"."<<location<<"."<<mConfiguration->mHost;
	}
	host = sstr.str();
	sstr.str("");
	sstr.clear();

	/* construct the url */
	sstr<<mConfiguration->mProtocol<<"://"<<host<<"/";
	url = sstr.str();
	sstr.str("");
	sstr.clear();

	QSCredential cred = {mConfiguration->mAccessKeyId, mConfiguration->mSecretAccessKey};
	do {
		try {
			resp_body = DoGetJSON(host.c_str(), url.c_str(), bucket.c_str(), NULL, &cred,
											QSRT_DELETE_BUCKET, NULL, mConfiguration->mConnectionRetries);
		} catch (QingStorException & e)
		{
			/* always clean up */
			if (resp_body)
			{
				json_object_put(resp_body);
			}
			throw e;
		}
	} while (false);

	return ;
}


void Context::deleteObject(std::string bucket, std::string key)
{
	std::stringstream sstr;
	std::string host, url;
	struct json_object *resp_body = NULL;

	/* construct the host */
	sstr<<bucket<<"."<<mConfiguration->mLocation<<"."<<mConfiguration->mHost;
	host = sstr.str();
	sstr.str("");
	sstr.clear();

	/* construct the url */
	sstr<<mConfiguration->mProtocol<<"://"<<host<<"/"<<key;
	url = sstr.str();
	sstr.str("");
	sstr.clear();

	QSCredential cred = {mConfiguration->mAccessKeyId, mConfiguration->mSecretAccessKey};
	do {
		try {
			resp_body = DoGetJSON(host.c_str(), url.c_str(), bucket.c_str(), NULL, &cred,
											QSRT_DELETE_OBJECT, NULL, mConfiguration->mConnectionRetries);
		} catch (QingStorException & e)
		{
			/* always clean up */
			if (resp_body)
			{
				json_object_put(resp_body);
			}
			throw e;
		}
	} while (false);

	return ;
}

bool Context::extractHeadObjectContent(shared_ptr<HeadObjectResult> result, struct json_object *resp_body)
{
	struct json_object *value;

	if ((!result) || (!resp_body))
	{
		return false;
	}

	/*
	 * get content type
	 */
	if (!json_object_object_get_ex(resp_body, "Content-Type", &value))
	{
		return false;
	}
	result->content_type = std::string(json_object_get_string(value));

	/*
	 * get object size
	 */
	if (!json_object_object_get_ex(resp_body, "Content-Length", &value))
	{
		return false;
	}
	result->content_length = json_object_get_int64(value);

	/*
	 * get last modified date
	 */
	if (!json_object_object_get_ex(resp_body, "Last-Modified", &value))
	{
		return false;
	}
	result->last_modified = std::string(json_object_get_string(value));

	return true;
}

}
}
