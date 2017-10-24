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

#ifndef __QINGSTOR_LIBQINGSTOR_QINGSTOR_H_
#define __QINGSTOR_LIBQINGSTOR_QINGSTOR_H_

#include <stdint.h>

#ifndef EINTERNAL
#define EINTERNAL 255
#endif

/** All APIs set errno to meaningful values */
#ifdef __cplusplus
extern "C" {
#endif

struct QingStorContextInternalWrapper;
typedef struct QingStorContextInternalWrapper *qingstorContext;

struct QingStorObjectInternalWrapper;
typedef struct QingStorObjectInternalWrapper *qingstorObject;

/**
 * qingstorBucketInfo - Information about a QingStor bucket.
 */
typedef struct {
	char *name;
	char *location;
	char *url;
	char *created;
} qingstorBucketInfo;

/*
 * qingstorObjectInfo - Information about a QingStor object.
 */
typedef struct
{
	char *key;
	int64_t size;
} qingstorObjectInfo;

/*
 * qingstorListObjectResult - Result of ListObject
 */
typedef struct
{
	char *name;
	char *prefix;
	unsigned int limit;
	qingstorObjectInfo *objects;
	int nObjects;
} qingstorListObjectResult;

/*
 * qingstorHeadObjectResult - Result of HeadObject
 */
typedef struct
{
	char *content_type;
	int64_t content_length;
	char *last_modified;
	char *etag;
} qingstorHeadObjectResult;

/**
 * Return error information of last failed operation.
 *
 * @return			A non NULL constant string pointer of last error information.
 * 					Caller can only read this message and keep it unchanged. No need
 * 					to free it. If last operation finished successfully, the returned
 * 					message is undefined.
 */
const char * qingstorGetLastError();

/**
 * qingstorInitContext - Initialize the QingStor Context.
 *
 * @param location				The access location for accessing QingStor.
 * @param access_key_id			The access key id for accessing QingStor.
 * @param secret_access_key		The secret access key for accessing QingStor.
 * @param buffer_size			The write buffer size of QingStor.
 * @return 						Returns a handle to the QingStor Context, or NULL on error.
 */
qingstorContext qingstorInitContext(const char *location, const char *access_key_id, const char *secret_access_key, int64_t buffer_size);

/**
 * qingstorInitContextFromFile - Initialize the QingStor Context through a configuration file.
 *
 * @param config_file			The configuration file for initializing the QingStor Context.
 * @return						Returns a handle to the QingStor Context, or NULL on error.
 */
qingstorContext qingstorInitContextFromFile(const char *config_file);

/**
 * qingstorDestroyContext - Destroy the QignStor Context.
 *
 * @param context				The context to be destroy.
 */
void qingstorDestroyContext(qingstorContext context);

/**
 * qingstorListBuckets - Get list of buckets given a location
 *
 * @param location				The location of buckets, currently only supporting pek3a, sh1a.
 * @param count					Set to the number of buckets.
 * @return						Returns a dynamically-allocated array of qingstorBucketInfo.
 */
qingstorBucketInfo* qingstorListBuckets(qingstorContext context, const char *location, int *count);

/**
 * qingstorCreateBucket - Create a bucket given a location
 *
 * @param location				The location of buckets, currently only supporting pek3a, sh1a.
 * @param bucket					The name of the bucket.
 * @return 						Return 0 on success, -1 on error.
 */
int qingstorCreateBucket(qingstorContext context, const char *location, const char *bucket);

/**
 * qingstorDeleteBucket - Delete a bucket given a location and bucket name
 *
 * @param location				The location of buckets, currently only supporting pek3a, sh1a.
 * @param bucket					The name of the bucket.
 * @return						Return 0 on success, -1 on error.
 */
int qingstorDeleteBucket(qingstorContext context, const char *location, const char *bucket);

/**
 * qingstorListObjects - Get list of objects given a bucket
 *
 * @param bucket					The name of the targeted bucket.
 * @param path					The targeted path.
 * @return						Returns a dynamically-allocated qingstorListObjectResult.
 */
qingstorListObjectResult* qingstorListObjects(qingstorContext context, const char *bucket,
									const char *path);

/**
 * qingstorGetObject - open a object for read
 *
 * @param bucket					The name of the targeted bucket.
 * @param key					The key of the targeted object.
 * @return						An object handler if exists; otherwise NULL.
 */
qingstorObject qingstorGetObject(qingstorContext context, const char *bucket,
									const char *key, int64_t range_start, int64_t range_end);


/**
 * qingstorPutObject - create a new object for write
 *
 * @param bucket					The name of the targeted bucket.
 * @param key					The key of the targeted object.
 * @param cache                 if cache is true, will be cached first;
 * 								otherwise, will be written immediately
 * @return						An object handler if a new object created successfully;
 * 								otherwise NULL.
 */
qingstorObject qingstorPutObject(qingstorContext context, const char *bucket,
									const char *key, bool cache = true);

/**
 * @param bucket					The name of the targeted bucket.
 * @param key					The key of the targeted object.
 * @return						Returns a dynamically-allocated qingstorHeadObjectResult.
 */
qingstorHeadObjectResult* qingstorHeadObject(qingstorContext context, const char *bucket,
									const char *key);

/**
 * qingstorCancelObject - cancel a object created for read or write
 *
 * @param object				The targeted object to be cancel.
 * @return						Return true on success, false on error.
 * 								On error, errno will be set appropriately.
 */
int qingstorCancelObject(qingstorContext context, qingstorObject object);

/**
 * qingstorCloseObject - close a object created for read or write
 *
 * @param object					The targeted object to be close.
 * @return						Return 0 on success, -1 on error.
 * 								On error, errno will be set appropriately.
 * 								If the object is valid, the memory assocated with it
 * 								will be freed at the end of this call, even if there
 * 								was an I/O error.
 */
int qingstorCloseObject(qingstorContext context, qingstorObject object);

/**
 * qingstorDeleteObject - delete a object
 *
 * @param bucket					The name of the targeted bucket.
 * @param key					The key of the targeted object.
 * @return						Return 0 on success, -1 on error.
 */
int qingstorDeleteObject(qingstorContext context, const char *bucket,
									const char *key);

/**
 * qingstorRead - Read data from a open object
 *
 * @param object					The targeted object gain by calling qingstorGetObject.
 * @param buffer					The buffer to copy read bytes into.
 * @param length					The size of the buffer.
 * @return						On success, a positive number indicating how many bytes were read.
 * 								On end-of-file, 0.
 * 								On error, -1. Errno will be set to the error code.
 */
int32_t qingstorRead(qingstorContext context, qingstorObject object, void *buffer, int32_t length);

/**
 * qingstorWrite - Write data to a open object
 *
 * @param object					The targeted object gain by calling qingstorPutObject.
 * @param buffer					The buffer of data to write out.
 * @param length					The size of the buffer.
 * @return						Returns the number of bytes written, -1 on error.
 */
int32_t qingstorWrite(qingstorContext context, qingstorObject object, const void *buffer, int32_t length);

#ifdef __cplusplus
}
#endif

#endif /* __QINGSTOR_LIBQINGSTOR_QINGSTOR_H_ */
