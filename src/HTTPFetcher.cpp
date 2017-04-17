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

#include "HTTPFetcher.h"
#include "Exception.h"
#include "ExceptionInternal.h"
#include "QingStorCommon.h"
#include "Logger.h"

#include <stdio.h>
#include <string.h>

#include <sstream>

namespace QingStor {
namespace Internal {

size_t
HTTPFetcher::WriterCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
	size_t realsize = size * nmemb;
	HTTPFetcher *fetcher = (HTTPFetcher *)userp;

	if (fetcher->mNused == fetcher->mReadOff)
	{
		fetcher->mNused = fetcher->mReadOff = 0;
	}

	if (realsize > fetcher->mBuffSize - fetcher->mNused)
	{
		if (fetcher->mReadOff > 0)
		{
			/*
			 * Shift the valid data in the buffer to make room for more
			 */
			memmove(fetcher->mReadBuff,
					fetcher->mReadBuff + fetcher->mReadOff,
					fetcher->mNused - fetcher->mReadOff);
			fetcher->mNused -= fetcher->mReadOff;
			fetcher->mReadOff = 0;
		}

		if (realsize > fetcher->mBuffSize - fetcher->mNused)
		{
			fetcher->mPaused = true;
			LOG(DEBUG2, "paused %s (%d bytes in buffer, size %d)",
					fetcher->mUrl, (int) fetcher->mNused, (int) fetcher->mBuffSize);
			return CURL_WRITEFUNC_PAUSE;
		}
	}

	memcpy(fetcher->mReadBuff + fetcher->mNused, contents, realsize);
	fetcher->mNused += realsize;
	return realsize;
}

HTTPFetcher::HTTPFetcher(const char *url, const char *host, const char *bucket,
						QSCredential *cred, int buffsize, int64_t offset, int len)
						: mState(FETCHER_INIT),
						  mUrl(strdup(url)),
						  mBucket(strdup(bucket)),
						  mHost(strdup(host)),
						  mCred(*cred),
						  mConfBuffSize(buffsize),
						  mOffset(offset),
						  mLen(len)
{
	mPaused = false;
	mHttpHeaders = NULL;
	mParent = NULL;
	mBytesDone = 0;
	mReadBuff = 0;
	mBuffSize = 0;
	mNused = 0;
	mReadOff = 0;
	mEof = false;
	mNFailures = 0;
}

void HTTPFetcher::start(CURLM *curl_mhandle)
{
	CURL *curl;
	char *path;
	char *query;
	std::stringstream sstr;

	if (!mReadBuff)
	{
		mBuffSize = CURL_MAX_WRITE_SIZE > mConfBuffSize ? CURL_MAX_WRITE_SIZE : mConfBuffSize;
		mReadBuff = (char *) malloc(mBuffSize);
		if (mReadBuff == NULL)
		{
			THROW(OutOfMemoryException, "could not allocate enough memory for HTTPFetcher read buff");
		}
	}

	if (mState != FETCHER_INIT && mState != FETCHER_FAILED)
	{
		THROW(QingStorException, "invalid fetcher state");
	}

	curl = curl_easy_init();
	if (!curl)
	{
		THROW(QingStorNetworkException, "could not create curl handle");
	}
	mCurl = curl;

	curl_multi_add_handle(curl_mhandle, mCurl);
	mParent = curl_mhandle;

	curl_easy_setopt(curl, CURLOPT_VERBOSE, 0L);
	curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, HTTPFetcher::WriterCallback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)this);
	curl_easy_setopt(curl, CURLOPT_FORBID_REUSE, 1L);
	curl_easy_setopt(curl, CURLOPT_URL, mUrl);

	HeaderContent_Add(&mHeaders, HOST, mHost);

	if ((mOffset + mBytesDone > 0) || (mLen >= 0))
	{
		int64_t offset = mOffset + mBytesDone;
		int64_t len = mLen;
		char rangebuf[128];

		if (len >= 0)
		{
			/*
			 * Read 'len' bytes, starting from offset.
			 */
			snprintf(rangebuf, sizeof(rangebuf),
					"bytes=%ld-%ld", offset, offset + len - 1);
		}
		else
		{
			/*
			 * Read from offset to the end.
			 */
			snprintf(rangebuf, sizeof(rangebuf),
					"bytes=%ld-", offset);
		}
		HeaderContent_Add(&mHeaders, RANGE, rangebuf);
	}

	qs_parse_url(mUrl,
				NULL, /* schema */
				NULL, /* host */
				&path,
				&query,
				NULL /* fullurl */);
	if (query)
	{
		sstr<<"/"<<mBucket<<path<<"?"<<query;
	}
	else
	{
		sstr<<"/"<<mBucket<<path;
	}
	Signature(&mHeaders, sstr.str().c_str(), &mCred, QSRT_GET_DATA, NULL);
	if (path)
	{
		free(path);
	}
	if (query)
	{
		free(query);
	}

	mHttpHeaders = HeaderContent_GetList(&mHeaders);
	curl_easy_setopt(mCurl, CURLOPT_HTTPHEADER, mHttpHeaders);

	mState = FETCHER_RUNNING;

	LOG(DEBUG1, "starting download from %s (off %ld, len %d)",
			mUrl, mOffset, mLen);
}

HTTPFetcher::~HTTPFetcher()
{
	cleanup();

	if (mReadBuff)
	{
		free(mReadBuff);
		mReadBuff = NULL;
	}
}

void HTTPFetcher::cleanup()
{
	if (mCurl)
	{
		if (mParent)
		{
			curl_multi_remove_handle(mParent, mCurl);
			mParent = NULL;
		}
		curl_easy_cleanup(mCurl);
		mCurl = NULL;
	}

	if (mHttpHeaders)
	{
		curl_slist_free_all(mHttpHeaders);
		mHttpHeaders = NULL;
	}
}

void HTTPFetcher::fail()
{
	cleanup();
	mState = FETCHER_FAILED;
	mNFailures++;

	LOG(DEBUG1, "download from %s (off %ld, len %d) failed",
			mUrl, mOffset, mLen);
}

void HTTPFetcher::done()
{
	cleanup();
	mState = FETCHER_DONE;

	LOG(DEBUG1, "download from %s (off %ld, len %d) is completed",
			mUrl, mOffset, mLen);
}

int HTTPFetcher::get(char *buff, int bufflen, bool *eof)
{
	int avail;

retry:
	avail = mNused - mReadOff;
	if (avail > 0)
	{
		if (avail > bufflen)
		{
			avail = bufflen;
		}
		memcpy(buff, mReadBuff + mReadOff, avail);
		mReadOff += avail;
		mBytesDone += avail;
		*eof = false;
		return avail;
	}

	if (mPaused)
	{
		LOG(DEBUG2, "unpaused %s", mUrl);
		mPaused = false;
		curl_easy_pause(mCurl, CURLPAUSE_CONT);
		/*
		 * curl_easy_pause() will typically call the write callback immediately,
		 * so loop back to see if we have more data now.
		 */
		goto retry;
	}

	if (mState == FETCHER_DONE)
	{
		LOG(DEBUG2, "EOF from %s (off %ld, len %d)",
				mUrl, mOffset, mLen);
		*eof = true;
		return 0;
	}

	/* need more data */
	*eof = false;
	return 0;
}

void HTTPFetcher::handleResult(CURLcode res)
{
	if (!mCurl)
	{
		THROW(QingStorException, "invalid state: curl is not initialized");
	}
	if (CURLE_OK == res)
	{
		long respcode;
		CURLcode eres;

		eres = curl_easy_getinfo(mCurl, CURLINFO_RESPONSE_CODE, &respcode);
		if (eres != CURLE_OK)
		{
			LOG(LOG_ERROR, "could not get response code from handle %p: %s",
					mCurl, curl_easy_strerror(eres));
		}

		if (respcode == 0)
		{
			THROW(QingStorNetworkException, "got response code as 0");
		}

		if ((respcode != 200) && (respcode != 206))
		{
			LOG(WARNING, "received HTTP code %ld", respcode);
			fail();
		}
		else
		{
			/*
			 * Success! Note: We might still have unread data in the fetcher buffer.
			 */
			done();
		}
	}
	else if (CURLE_OPERATION_TIMEDOUT == res)
	{
		LOG(WARNING, "net speed is too slow");
		fail();
	}
	else
	{
		LOG(WARNING, "curl_multi_perform() failed: %s",
				curl_easy_strerror(res));
		fail();
	}
}

}
}



