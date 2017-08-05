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

#include "platform.h"

#include "qingstor.h"


#include "Context.h"
#include "Memory.h"
#include "Exception.h"
#include "ExceptionInternal.h"
#include "Logger.h"
#include "QingStorReader.h"
#include "QingStorWriter.h"

#include <assert.h>
#include <string.h>

#include <string>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef QINGSTOR_ERROR_MESSAGE_BUFFER_SIZE
#define QINGSTOR_ERROR_MESSAGE_BUFFER_SIZE (4096)
#endif

static THREAD_LOCAL char ErrorMessage[QINGSTOR_ERROR_MESSAGE_BUFFER_SIZE] = "Success";

static void SetErrorMessage(const char *msg) {
	assert(NULL != msg);
	strncpy(ErrorMessage, msg, sizeof(ErrorMessage) - 1);
	ErrorMessage[sizeof(ErrorMessage) - 1] = 0;
}

static void SetLastException(QingStor::exception_ptr e) {
    std::string buffer;
    const char *p;
    p = QingStor::Internal::GetExceptionMessage(e, buffer);
    SetErrorMessage(p);
}

#define PARAMETER_ASSERT(para, retval, eno) \
    if (!(para)) { \
        SetErrorMessage(QingStor::Internal::GetSystemErrorInfo(eno)); \
        errno = eno; \
        return retval; \
    }

using QingStor::Internal::Context;
using QingStor::Internal::shared_ptr;
using QingStor::Internal::ListObjectResult;
using QingStor::Internal::ListBucketResult;
using QingStor::Internal::HeadObjectResult;
using QingStor::Internal::ObjectInfo;
using QingStor::Internal::RangeInfo;
using QingStor::Internal::QingStorReader;
using QingStor::Internal::QingStorWriter;

struct QingStorObjectInternalWrapper {
public:
	QingStorObjectInternalWrapper() :
		reader(true), rw(NULL) {

	}

	~QingStorObjectInternalWrapper() {
		if (reader) {
			delete static_cast<QingStorReader *> (rw);
		}
		else
		{
			delete static_cast<QingStorWriter *> (rw);
		}
	}

	QingStorReader & getReader() {
		if (!reader) {
			THROW(QingStor::QingStorException,
					"Internal error: object was not opened for read.")
		}

		if (!rw) {
			THROW(QingStor::QingStorIOException, "Object is not opened.")
		}

		return *static_cast<QingStorReader *> (rw);
	}

	QingStorWriter & getWriter() {
		if (reader) {
			THROW(QingStor::QingStorException,
					"Internal error: object was not opened for write.");
		}

		if (!rw) {
			THROW(QingStor::QingStorIOException, "Object is not opened.")
		}

		return *static_cast<QingStorWriter *> (rw);
	}

	bool isReader() const {
		return reader;
	}

	void setReader(bool reader) {
		this->reader = reader;
	}

	void setRW(void *rw) {
		this->rw = rw;
	}

private:
	bool reader;
	void *rw;
};

struct QingStorContextInternalWrapper {
public:
	QingStorContextInternalWrapper(Context *ctx) : context(ctx) {

	}

	~QingStorContextInternalWrapper() {
		delete context;
	}

	Context & getContext() {
		return *context;
	}

private:
	Context *context;
};

static void handleException(QingStor::exception_ptr error) {
    try {
        QingStor::rethrow_exception(error);

#ifndef NDEBUG
        std::string buffer;
        LOG(QingStor::Internal::LOG_ERROR, "Handle Exception: %s",
            QingStor::Internal::GetExceptionDetail(error, buffer));
#endif
    } catch (QingStor::AccessControlException &) {
        errno = EACCES;
    } catch (const QingStor::QingStorCanceled &) {
        errno = EIO;
    } catch (const QingStor::QingStorConfigInvalid &) {
        errno = EINVAL;
    } catch (const QingStor::QingStorConfigNotFound &) {
        errno = EINVAL;
    } catch (const QingStor::QingStorTimeoutException &) {
        errno = EIO;
    } catch (QingStor::IllegalArgumentException &) {
        errno = EINVAL;
    } catch (QingStor::InvalidParameter &) {
        errno = EINVAL;
    } catch (QingStor::QingStorNetworkException &) {
        errno = EIO;
    } catch (QingStor::QingStorIOException &) {
        std::string buffer;
        LOG(QingStor::Internal::LOG_ERROR, "Handle Exception: %s", QingStor::Internal::GetExceptionDetail(error, buffer));
        errno = EIO;
    } catch (QingStor::QingStorException & e) {
        std::string buffer;
        LOG(QingStor::Internal::LOG_ERROR, "Unexpected exception %s: %s", typeid(e).name(),
            QingStor::Internal::GetExceptionDetail(e, buffer));
        errno = EINTERNAL;
    } catch (std::exception & e) {
        LOG(QingStor::Internal::LOG_ERROR, "Unexpected exception %s: %s", typeid(e).name(), e.what());
        errno = EINTERNAL;
    }
}

const char *qingstorGetLastError() {
	return ErrorMessage;
}

qingstorContext qingstorInitContext(const char *location, const char *access_key_id, const char *secret_access_key, int64_t chunk_size) {
	PARAMETER_ASSERT(access_key_id != NULL && strlen(access_key_id) > 0, NULL, EINVAL);
	PARAMETER_ASSERT(secret_access_key != NULL && strlen(secret_access_key) > 0, NULL, EINVAL);

	Context *context;
	try {
		// you can set the flag isCanceled through the caller
		// then these signal code will be unnecessary
		signal(SIGINT, QingStor::Internal::handle_signals);
		signal(SIGQUIT, QingStor::Internal::handle_signals);
		signal(SIGTERM, QingStor::Internal::handle_signals);

		std::string str_access_key_id(access_key_id);
		std::string str_secret_access_key(secret_access_key);
		std::string str_location(location);
		context = new Context(str_location, str_access_key_id, str_secret_access_key, chunk_size);
		return new QingStorContextInternalWrapper(context);
	} catch (const std::bad_alloc & e)
	{
		delete context;
		SetErrorMessage("Out of memory");
		errno = ENOMEM;
	} catch (...) {
		delete context;
		SetLastException(QingStor::current_exception());
		handleException(QingStor::current_exception());
	}

	return NULL;
}

qingstorContext qingstorInitContextFromFile(const char *config_file) {
	PARAMETER_ASSERT(config_file != NULL && strlen(config_file) > 0, NULL, EINVAL);

	Context *context;
	try {
		// you can set the flag isCanceled through the caller
		// then these signal code will be unnecessary
		signal(SIGINT, QingStor::Internal::handle_signals);
		signal(SIGQUIT, QingStor::Internal::handle_signals);
		signal(SIGTERM, QingStor::Internal::handle_signals);

		std::string str_config_file(config_file);
		context = new Context(str_config_file);
		return new QingStorContextInternalWrapper(context);
	} catch(const std::bad_alloc & e)
	{
		delete context;
		SetErrorMessage("Out of memory");
		errno = ENOMEM;
	} catch (...) {
		delete context;
		SetLastException(QingStor::current_exception());
		handleException(QingStor::current_exception());
	}

	return NULL;
}

void qingstorDestroyContext(qingstorContext context)
{
	if (context)
	{
		delete context;
	}
	return ;
}

qingstorBucketInfo* qingstorListBuckets(qingstorContext context, const char *location, int *count) {
	PARAMETER_ASSERT(context, NULL, EINVAL);
	PARAMETER_ASSERT(count, NULL, EINVAL);

	qingstorBucketInfo* result = NULL;
	*count = 0;

	try {
		std::string str_location;
		if (location)
		{
			str_location = location;
		}
		shared_ptr<ListBucketResult> res = context->getContext().listBuckets(str_location);
		if (res->buckets.size() > 0)
		{
			*count = res->buckets.size();
			result = (qingstorBucketInfo*) malloc(sizeof(qingstorBucketInfo) * (*count));

			for (int i = 0; i < *count; i++)
			{
				result[i].name = strdup(res->buckets[i].name.c_str());
				result[i].location = strdup(res->buckets[i].location.c_str());
				result[i].url = strdup(res->buckets[i].url.c_str());
				result[i].created = strdup(res->buckets[i].created.c_str());
			}
		}
	} catch (const std::bad_alloc & e)
	{
		SetErrorMessage("Out of memory");
		errno = ENOMEM;
	} catch (...) {
		SetLastException(QingStor::current_exception());
		handleException(QingStor::current_exception());
	}

	return result;
}

int qingstorCreateBucket(qingstorContext context, const char *location, const char *bucket)
{
	PARAMETER_ASSERT(context, -1, EINVAL);
	PARAMETER_ASSERT(bucket != NULL && strlen(bucket) > 0, -1, EINVAL);

	try {
		std::string str_bucket(bucket);
		std::string str_location;
		if (location)
		{
			str_location = location;
		}
		context->getContext().createBucket(str_location, str_bucket);
		return 0;
	} catch (const std::bad_alloc & e)
	{
		SetErrorMessage("Out of memory");
		errno = ENOMEM;
	} catch (...) {
		SetLastException(QingStor::current_exception());
		handleException(QingStor::current_exception());
	}

	return -1;
}

int qingstorDeleteBucket(qingstorContext context, const char *location, const char *bucket)
{
	PARAMETER_ASSERT(context, -1, EINVAL);
	PARAMETER_ASSERT(bucket != NULL && strlen(bucket) > 0, -1, EINVAL);

	try {
		std::string str_bucket(bucket);
		std::string str_location;
		if (location)
		{
			str_location = location;
		}
		context->getContext().deleteBucket(str_location, str_bucket);
		return 0;
	} catch (const std::bad_alloc & e)
	{
		SetErrorMessage("Out of memory");
		errno = ENOMEM;
	} catch (...) {
		SetLastException(QingStor::current_exception());
		handleException(QingStor::current_exception());
	}

	return -1;
}

qingstorListObjectResult* qingstorListObjects(qingstorContext context, const char *bucket,
											const char *path)
{
	PARAMETER_ASSERT(context, NULL, EINVAL);
	PARAMETER_ASSERT(bucket != NULL && strlen(bucket) > 0, NULL, EINVAL);

	qingstorListObjectResult *result = NULL;
	try {
		std::string str_bucket(bucket);
		std::string str_path;
		if(path)
		{
			str_path = path;
		}
		shared_ptr<ListObjectResult> res = context->getContext().listObjects(str_bucket, str_path);
		result = (qingstorListObjectResult *) malloc(sizeof(qingstorListObjectResult));
		if (result == NULL)
		{
			THROW(QingStor::OutOfMemoryException, "could not malloc for qingstorListObjectResult");
		}
		result->name = strdup(res->name.c_str());
		result->prefix = strdup(res->prefix.c_str());
		result->limit = res->limit;
		result->nObjects = res->objects.size();
		result->objects = (qingstorObjectInfo *) malloc(sizeof(qingstorObjectInfo) * result->nObjects);
		if (result->objects == NULL)
		{
			THROW(QingStor::OutOfMemoryException, "could not malloc for qingstorObjectInfo array");
		}
		for (int i = 0; i < result->nObjects; i++)
		{
			result->objects[i].key = strdup(res->objects[i].key.c_str());
			result->objects[i].size = res->objects[i].size;
		}
	} catch (const std::bad_alloc & e)
	{
		SetErrorMessage("Out of memory");
		errno = ENOMEM;
	} catch (...) {
		SetLastException(QingStor::current_exception());
		handleException(QingStor::current_exception());
	}

	return result;
}

qingstorObject qingstorGetObject(qingstorContext context, const char *bucket,
								const char *key, int64_t range_start, int64_t range_end)
{
	PARAMETER_ASSERT(context, NULL, EINVAL);
	PARAMETER_ASSERT(bucket != NULL && strlen(bucket) > 0, NULL, EINVAL);
	PARAMETER_ASSERT(key != NULL && strlen(key) > 0, NULL, EINVAL);
	PARAMETER_ASSERT(range_end >= range_start, NULL, EINVAL);

	QingStorObjectInternalWrapper *result = NULL;
	try {
			result = new QingStorObjectInternalWrapper();
			std::string str_bucket(bucket);
			std::string str_key(key);

			shared_ptr<HeadObjectResult> res = context->getContext().headObject(str_bucket, str_key);
			range_start = (range_start < 0) ? 0 : range_start;
			range_end = (range_end < 0) ? res->content_length - 1 : range_end;
			RangeInfo range = {range_start, range_end};
			ObjectInfo object = {str_key, res->content_length, range};
			QingStorReader *reader = new QingStorReader(context->getContext().configuration(), str_bucket, object);
			result->setReader(true);
			result->setRW((void *) reader);
			return result;
	} catch (const std::bad_alloc & e)
	{
		SetErrorMessage("Out of memory");
		errno = ENOMEM;
	} catch (...) {
		SetLastException(QingStor::current_exception());
		handleException(QingStor::current_exception());
	}

	return NULL;
}

qingstorObject qingstorPutObject(qingstorContext context, const char *bucket,
								const char *key)
{
	PARAMETER_ASSERT(context, NULL, EINVAL);
	PARAMETER_ASSERT(bucket != NULL && strlen(bucket) > 0, NULL, EINVAL);
	PARAMETER_ASSERT(key != NULL && strlen(key) > 0, NULL, EINVAL);

	QingStorObjectInternalWrapper *result = NULL;
	try {
		result = new QingStorObjectInternalWrapper();
		std::string str_bucket(bucket);
		std::string str_key(key);

		ObjectInfo object = {str_key, 0};
		QingStorWriter *writer = new QingStorWriter(context->getContext().configuration(), str_bucket, object);
		result->setReader(false);
		result->setRW((void *) writer);
		return result;
	} catch (const std::bad_alloc & e)
	{
		SetErrorMessage("Out of memory");
		errno = ENOMEM;
	} catch (...) {
		SetLastException(QingStor::current_exception());
		handleException(QingStor::current_exception());
	}

	return NULL;
}

qingstorHeadObjectResult* qingstorHeadObject(qingstorContext context, const char *bucket,
								const char *key)
{
	PARAMETER_ASSERT(context, NULL, EINVAL);
	PARAMETER_ASSERT(bucket != NULL && strlen(bucket) > 0, NULL, EINVAL);
	PARAMETER_ASSERT(key != NULL && strlen(key) > 0, NULL, EINVAL);

	qingstorHeadObjectResult *result = NULL;
	try {
		std::string str_bucket(bucket);
		std::string str_key(key);

		shared_ptr<HeadObjectResult> res = context->getContext().headObject(str_bucket, str_key);
		result = (qingstorHeadObjectResult *) malloc(sizeof(qingstorHeadObjectResult));

		result->content_type = strdup(res->content_type.c_str());
		result->content_length = res->content_length;
		result->last_modified = strdup(res->last_modified.c_str());
		result->etag = strdup(res->etag.c_str());
	} catch (const std::bad_alloc & e)
	{
		SetErrorMessage("Out of memory");
		errno = ENOMEM;
	} catch (...) {
		SetLastException(QingStor::current_exception());
		handleException(QingStor::current_exception());
	}

	return result;
}

int qingstorDeleteObject(qingstorContext context, const char *bucket,
								const char *key)
{
	PARAMETER_ASSERT(context, -1, EINVAL);
	PARAMETER_ASSERT(bucket != NULL && strlen(bucket) > 0, -1, EINVAL);
	PARAMETER_ASSERT(key != NULL && strlen(key) > 0, -1, EINVAL);

	try {
		std::string str_bucket(bucket);
		std::string str_key(key);

		context->getContext().deleteObject(str_bucket, str_key);
		return 0;
	} catch (const std::bad_alloc & e)
	{
		SetErrorMessage("Out of memory");
		errno = ENOMEM;
	} catch (...) {
		SetLastException(QingStor::current_exception());
		handleException(QingStor::current_exception());
	}

	return -1;
}

int qingstorCancelObject(qingstorContext context, qingstorObject object)
{
	PARAMETER_ASSERT(context, -1, EINVAL);
	PARAMETER_ASSERT(object, -1, EINVAL);

	try {
		if (object) {
			if (object->isReader()) {
				return -1;
			}
			else {
				object->getWriter().cancel();
			}
			QingStor::Internal::isCanceled = true;
		}
		return 0;
	} catch (const std::bad_alloc & e) {
		SetErrorMessage("Out of memory");
		errno = ENOMEM;
	} catch (...) {
		SetLastException(QingStor::current_exception());
		handleException(QingStor::current_exception());
	}

	return -1;
}

int qingstorCloseObject(qingstorContext context, qingstorObject object)
{
	PARAMETER_ASSERT(object, -1, EINVAL);

	try {
		if (object)
		{
			if (object->isReader())
			{
				object->getReader().close();
			}
			else
			{
				object->getWriter().close();
			}
			delete object;
		}
		return 0;
	} catch (const std::bad_alloc & e)
	{
		delete object;
		SetErrorMessage("Out of memory");
		errno = ENOMEM;
	} catch (...) {
		delete object;
		SetLastException(QingStor::current_exception());
		handleException(QingStor::current_exception());
	}

	return -1;
}

int32_t qingstorRead(qingstorContext context, qingstorObject object, void *buffer, int32_t length)
{
	PARAMETER_ASSERT(context && object && buffer && length > 0, -1, EINVAL);
	PARAMETER_ASSERT(object->isReader(), -1, EINVAL);

	try {
		return object->getReader().transferData(static_cast<char *>(buffer), length);
	} catch (const QingStor::QingStorEndOfStream & e)
	{
		return 0;
	} catch (const std::bad_alloc & e)
	{
		SetErrorMessage("Out of memory");
		errno = ENOMEM;
	} catch (...) {
		SetLastException(QingStor::current_exception());
		handleException(QingStor::current_exception());
	}

	return -1;
}

int32_t qingstorWrite(qingstorContext context, qingstorObject object, const void *buffer, int32_t length)
{
	PARAMETER_ASSERT(context && object && buffer && length > 0, -1, EINVAL);
	PARAMETER_ASSERT(!object->isReader(), -1, EINVAL);

	try {
		object->getWriter().transferData(static_cast<const char *>(buffer), length);
		return length;
	} catch (const std::bad_alloc & e)
	{
		SetErrorMessage("Out of memory");
		errno = ENOMEM;
	} catch (...) {
		SetLastException(QingStor::current_exception());
		handleException(QingStor::current_exception());
	}

	return -1;
}

#ifdef __cplusplus
}
#endif




