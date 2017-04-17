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

#include "DownloadPipeline.h"
#include "Exception.h"
#include "ExceptionInternal.h"
#include "Logger.h"

namespace QingStor {
namespace Internal {

DownloadPipeline::DownloadPipeline(int nconnections)
{
	mNConnections = nconnections;

	mCurlMHandle = curl_multi_init();
	if (NULL == mCurlMHandle)
	{
		THROW(QingStorException, "could not create CURL multi-handle");
	}
}

void DownloadPipeline::launch()
{
	while (static_cast<int>(mActiveFetchers.size()) < mNConnections)
	{
		shared_ptr<HTTPFetcher> fetcher;

		/*
		 * Get next fetcher from pending list
		 */
		if (mPendingFetchers.empty())
		{
			break;
		}
		fetcher = mPendingFetchers.front();
		mPendingFetchers.pop_front();

		/* Move it to the active list, and start it */
		mActiveFetchers.push_back(fetcher);

		fetcher->start(mCurlMHandle);
	}
}

int DownloadPipeline::read(char *buff, int bufflen, bool *eof_p)
{
	shared_ptr<HTTPFetcher> current_fetcher;
	int r;
	bool eof;

	for (;;)
	{
		/* launch more connections if needed */
		launch();

		if (mActiveFetchers.empty())
		{
			/*
			 * launch() should've launched at least one fetcher,
			 * if there were any still pending.
			 */
			LOG(DEBUG1, "all downloads completed");
			*eof_p = true;
			return 0;
		}
		current_fetcher = mActiveFetchers.front();

		/*
		 * Try to read from the current fetcher
		 */
		r = current_fetcher->get(buff, bufflen, &eof);
		if (eof)
		{
			mActiveFetchers.pop_front();
			continue;
		}

		if (r > 0)
		{
			/* got some data */
			*eof_p = false;
			return r;
		}

		/*
		 * Consider restarting any failed fetchers.
		 * TODO: right now, we only look at the first in the queue, not the
		 * pre-fetching ones. Hopefully requests don't fail often enough for that
		 * to matter.
		 */
		if (current_fetcher->state() == FETCHER_FAILED)
		{
			LOG(DEBUG1, "retrying now");
			current_fetcher->start(mCurlMHandle);
		}

		{
			int running_handles;
			struct CURLMsg *m;
			CURLMcode mres;

			/*
			 * Got nothing. Wait until something happens.
			 */
			mres = curl_multi_wait(mCurlMHandle, NULL, 0, 1000, NULL);
			if (mres != CURLM_OK)
			{
				THROW(QingStorNetworkException, "curl_multi_perform returned error: %s",
						curl_multi_strerror(mres));
			}
			mres = curl_multi_perform(mCurlMHandle, &running_handles);
			if (mres != CURLM_OK)
			{
				THROW(QingStorNetworkException, "curl_multi_perform returned error: %s",
						curl_multi_strerror(mres));
			}

			do {
				int msgq = 0;
				m = curl_multi_info_read(mCurlMHandle, &msgq);

				if (m && (m->msg == CURLMSG_DONE))
				{
					/*
					 * find the fetcher this belongs to.
					 */
					bool found = false;
					std::list<shared_ptr<HTTPFetcher> >::iterator itr = mActiveFetchers.begin();
					while (itr != mActiveFetchers.end())
					{
						shared_ptr<HTTPFetcher> f = *itr;

						if (f->curl() == m->easy_handle)
						{
							found = true;
							f->handleResult(m->data.result);
							break;
						}
						itr++;
					}
					if (!found)
					{
						THROW(QingStorNetworkException, "got CURL result code for a fetcher that's not active");
					}
				}
			} while (m);

			continue;
		}
	}
}

void DownloadPipeline::add(shared_ptr<HTTPFetcher> fetcher)
{
	mPendingFetchers.push_back(fetcher);
}

DownloadPipeline::~DownloadPipeline()
{
	while (!mActiveFetchers.empty())
	{
		mActiveFetchers.pop_front();
	}
	if (mCurlMHandle)
	{
		curl_multi_cleanup(mCurlMHandle);
	}
}

}
}



