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

#ifndef __QINGSTOR_LIBQINGSTOR_DOWNLOADPIPELINE_H_
#define __QINGSTOR_LIBQINGSTOR_DOWNLOADPIPELINE_H_

#include "HTTPFetcher.h"
#include "Memory.h"

#include <list>

namespace QingStor {
namespace Internal {

class DownloadPipeline
{
public:
	DownloadPipeline(int nconnections);

	~DownloadPipeline();

	/*
	 * Try to get at most bufflen bytes from pipeline. Blocks.
	 */
	int read(char *buff, int bufflen, bool *eof);

	void add(shared_ptr<HTTPFetcher> fetcher);

private:
	/*
	 * Fetchers that have been started. The first one in the list is
	 * the one we use to return data to the caller in DownloadPipeline::read().
	 * The rest are doing pre-fetching.
	 */
	std::list<shared_ptr<HTTPFetcher> > mActiveFetchers;
	std::list<shared_ptr<HTTPFetcher> > mPendingFetchers;	/* fetchers not started yet */

	/* number of active connections to use */
	int mNConnections;

	/* Multi-handle that contains the currently active fetcher's CURL handle */
	CURLM *mCurlMHandle;

	/*
	 * If there are less than the requested number of downloads active currently,
	 * launch more from the pending list.
	 */
	void launch();
};

}
}



#endif /* __QINGSTOR_LIBQINGSTOR_DOWNLOADPIPELINE_H_ */
