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

#ifndef __QINGSTOR_LIBQINGSTOR_QINGSTORWRITER_H_
#define __QINGSTOR_LIBQINGSTOR_QINGSTORWRITER_H_

#include "QingStorRWBase.h"
#include "QingStorCommon.h"

namespace QingStor {
namespace Internal {

class QingStorWriter : public QingStorRWBase {
public:
	QingStorWriter(shared_ptr<Configuration> configuration, std::string bucket, ObjectInfo object);

	~QingStorWriter() {
		if (mBuffer)
		{
			delete mBuffer;
		}
	}
	void transferData(const char *buffer, int32_t length);

	void close();

private:
	std::string mUploadId;
	std::string mKey;
	QSCredential mCred;
	char *mBuffer;
	int64_t mBuffSize;
	int64_t mWritePos;
	int32_t mPartNum;

	void flush();

	void initMultipartUpload(std::string host, std::string url, std::string bucket, QSCredential *cred);

	bool uploadMultipart(std::string host, std::string url, std::string bucket, QSCredential * cred,
				const char *data, int32_t length);

	bool completeMultipartUpload(std::string host, std::string url, std::string bucket, QSCredential *cred);

	bool extractUploadIDContent(struct json_object *resp_body);

	void doSend(const char *data, int32_t length);
};

}
}

#endif /* __QINGSTOR_LIBQINGSTOR_QINGSTORWRITER_H_ */
