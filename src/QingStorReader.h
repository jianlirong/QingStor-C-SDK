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

#ifndef __QINGSTOR_LIBQINGSTOR_QINGSTORREADER_H_
#define __QINGSTOR_LIBQINGSTOR_QINGSTORREADER_H_

#include "QingStorRWBase.h"
#include "DownloadPipeline.h"
#include "Memory.h"

#include <list>

namespace QingStor {
namespace Internal {

class ObjectInfo;

class QingStorReader : public QingStorRWBase {
public:
	QingStorReader(shared_ptr<Configuration> configuration, std::string bucket, ObjectInfo object);

	int transferData(char *buff, int buffsize);

	void close();

private:
	shared_ptr<DownloadPipeline> mPipeline;

	/*
	 * Form a download pipeline that will fetch all objects listed in the given
	 * objects.
	 */
	void setupPipeline(std::list<shared_ptr<ObjectInfo> > objects);

};

}
}



#endif /* __QINGSTOR_LIBQINGSTOR_QINGSTORREADER_H_ */
