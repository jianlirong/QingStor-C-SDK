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

#ifndef __QINGSTOR_LIBQINGSTOR_CONTEXT_H_
#define __QINGSTOR_LIBQINGSTOR_CONTEXT_H_

#include "Memory.h"
#include "Configuration.h"

#include <json/json.h>

#include <string>
#include <vector>

namespace QingStor {
namespace Internal {

class ObjectInfo {
public:
	std::string key;
	int64_t size;
};

class ListObjectResult {
public:
	std::string name;
	std::string prefix;
	uint32_t limit;
	std::vector<ObjectInfo> objects;
};

class BucketInfo {
public:
	std::string name;
	std::string location;
	std::string url;
	std::string created;
};

class ListBucketResult {
public:
	std::vector<BucketInfo> buckets;
};

class HeadObjectResult {
public:
	std::string content_type;
	int64_t content_length;
	std::string last_modified;
	std::string etag;
};

class Context {
public:
	Context(std::string location, std::string access_key_id, std::string secret_access_key, int64_t chunk_size);

	Context(std::string config_file);

	shared_ptr<ListObjectResult> listObjects(std::string bucket, std::string prefix);

	shared_ptr<ListBucketResult> listBuckets(std::string location);

	shared_ptr<HeadObjectResult> headObject(std::string bucket, std::string key);

	void deleteObject(std::string bucket, std::string key);

	void createBucket(std::string location, std::string bucket);

	void deleteBucket(std::string location, std::string bucket);

	shared_ptr<Configuration> configuration() {
		return mConfiguration;
	}

private:
	shared_ptr<Configuration> mConfiguration;

	bool extractListObjectContent(shared_ptr<ListObjectResult> result, struct json_object *resp_body,
								shared_ptr<std::string> current_marker, bool *eof);

	bool extractListBucketContent(shared_ptr<ListBucketResult> result, struct json_object *resp_body);

	bool extractHeadObjectContent(shared_ptr<HeadObjectResult> result, struct json_object *resp_body);
};

}
}

#endif /* __QINGSTOR_LIBQINGSTOR_CONTEXT_H_ */
