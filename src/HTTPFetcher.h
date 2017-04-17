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

#ifndef __QINGSTOR_LIBQINGSTOR_HTTPFETCHER_H_
#define __QINGSTOR_LIBQINGSTOR_HTTPFETCHER_H_

#include "QingStorCommon.h"

#include <curl/curl.h>

namespace QingStor {
namespace Internal {

typedef enum {
	FETCHER_INIT,
	FETCHER_RUNNING,
	FETCHER_FAILED,
	FETCHER_DONE
} FetcherState;

class HTTPFetcher {
public:
	HTTPFetcher(const char *url, const char *host, const char *bucket,
				QSCredential *cred, int buffsize, int64_t offset, int len);

	~HTTPFetcher();

	/*
	 * Starts a new HTTP fetch operation.
	 *
	 * The HTTPFetcher is registered with the given ResourceOwner. Deleting the
	 * ResourceOwner will abort the transfer, mark this HTTPFetcher as failed,
	 * and release the curl handle.
	 */
	void start(CURLM *curl_mhandle);

	/*
	 * Read up to bufflen bytes from the source. Will not block; if no data
	 * is immediately available, return 0.
	 */
	int get(char *buff, int bufflen, bool *eof);

	/*
	 * Called by DownloadPipeline when curl_multi_perform() reports that the
	 * transfer is completed.
	 */
	void handleResult(CURLcode res);

	/*
	 * get state
	 */
	FetcherState state() {
		return mState;
	}

	/*
	 * get curl
	 */
	CURL* curl() {
		return mCurl;
	}

private:
	FetcherState mState;

	CURL *mCurl;
	bool mPaused;

	struct curl_slist *mHttpHeaders;
	CURLM *mParent;

	const char *mUrl;
	const char *mBucket;		/* bucket name, used to create QingStor signature */
	const char *mHost;

	QSCredential mCred;

	int	mConfBuffSize;			/* requested buffer size */
	int64_t mOffset;
	int mLen;

	int64_t mBytesDone;		/* Returned this many bytes to caller (allows retrying from where we left) */

	HeaderContent mHeaders;

	/* TODO: a ring buffer would be more efficient */
	char *mReadBuff;			/* buffer to read into */
	size_t mBuffSize;		/* allocated size (at least CURL_MAX_WRITE_SIZE) */
	size_t mNused;			/* amount of valid data in buffer */
	size_t mReadOff;			/* how much of the valid data has been consumed */

	bool mEof;

	/* TODO: Retry support */
	int mNFailures;			/* how many times have we failed at connecting? */

	void cleanup();

	/*
	 * Release resources, and mark the transfer as failed. It can be retried by
	 * calling HTTPFetcher_start() again.
	 */
	void fail();

	/*
	 * Mark the transfer as completed successfully. This releases the CURL handle and
	 * other resources, except for the buffer. You may continue to read remaining unread
	 * data with HTTPFetcher_get().
	 */
	void done();

	/*
	 * CURL callback that places incoming data in the buffer.
	 */
	static size_t WriterCallback(void *contents, size_t size, size_t nmemb, void *userp);
};

}
}



#endif /* __QINGSTOR_LIBQINGSTOR_HTTPFETCHER_H_ */
