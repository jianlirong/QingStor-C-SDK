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

#include "QingStorReader.h"
#include "Context.h"
#include "Exception.h"
#include "ExceptionInternal.h"
#include "Logger.h"

#include <sstream>

namespace QingStor {
namespace Internal {

QingStorReader::QingStorReader(shared_ptr<Configuration> configuration, std::string bucket, ObjectInfo object)
							: QingStorRWBase(configuration, bucket, object)
{
	std::list<shared_ptr<ObjectInfo> > objects;
	shared_ptr<ObjectInfo> obj (new ObjectInfo());
	obj->key = object.key;
	obj->size = object.size;
	objects.push_back(obj);

	setupPipeline(objects);
}

int QingStorReader::transferData(char *buff, int buffsize)
{
	bool eof = false;
	int rnum = mPipeline->read(buff, buffsize, &eof);
	if (eof)
	{
		THROW(QingStorEndOfStream,"transferData");
	}
	return rnum;
}

void QingStorReader::setupPipeline(std::list<shared_ptr<ObjectInfo> > objects)
{
	std::stringstream sstr;
	std::string host;
	int64_t chunkSize;
	int buffSize;

	/*
	 * If we're going to use multiple connections, divide the file into
	 * chunks. Otherwise fetch the whole file as one transfer.
	 *
	 * If we're downloading in chunks, use a buffer that's large enough
	 * to hold whole chunk. A buffer larger than 128 MB doesn't seem
	 * reasonable though.
	 */
	if (mConfiguration->mNConnections > 1)
	{
		chunkSize = mConfiguration->mChunkSize;
		if (chunkSize > 128 * 1024 * 1024)
		{
			buffSize = 128 * 1024 * 1024;
		}
		else
		{
			buffSize = chunkSize;
		}
	}
	else
	{
		chunkSize = -1;
		buffSize = -1;
	}

	sstr<<mBucket<<"."<<mConfiguration->mLocation<<"."<<mConfiguration->mHost;
	host = sstr.str();
	sstr.str("");
	sstr.clear();

	/*
	 * Create a pipeline that will download all the contents.
	 */
	mPipeline = shared_ptr<DownloadPipeline> (new DownloadPipeline(mConfiguration->mNConnections));
	std::list<shared_ptr<ObjectInfo> >::iterator itr = objects.begin();
	while (itr != objects.end())
	{
		shared_ptr<ObjectInfo> object = *itr;
		itr++;
		shared_ptr<HTTPFetcher> fetcher;
		int64_t offset;
		int64_t len;

		sstr<<mConfiguration->mProtocol<<"://"<<host<<"/"<<object->key;

		LOG(DEBUG1, "key: %s, size: %ld", sstr.str().c_str(), object->size);

		/*
		 * Divide the file into chunks of requested size.
		 */
		offset = 0;
		do {
			if (chunkSize == -1 || offset + chunkSize > object->size)
			{
				/* last chunk */
				len = -1;
			}
			else
			{
				len = chunkSize;
			}
			QSCredential cred = {mConfiguration->mAccessKeyId, mConfiguration->mSecretAccessKey};
			fetcher = shared_ptr<HTTPFetcher> (new HTTPFetcher(sstr.str().c_str(), host.c_str(), mBucket.c_str(),
										&cred, buffSize, offset, len));
			mPipeline->add(fetcher);
			offset += len;
		} while (len != -1);

		sstr.str("");
		sstr.clear();
	}
}

void QingStorReader::close()
{
	return;
}

}
}
