#include <stdio.h>
#include <stdlib.h>

#include <qingstor/qingstor.h>

const char *access_key_id = "access_key_id";
const char *secret_access_key = "secret_access_key";


/*
 * test list objects
 */
void testListObjects()
{
	printf("testListObjects......\n");
	int i;
	qingstorContext qsContext = qingstorInitContext(access_key_id, secret_access_key);
	qingstorListObjectResult* result = qingstorListObjects(qsContext, "hdw-hashdata-cn", "tpch");
	
	printf("name=%s, prefix=%s, limit=%d, nObjects=%d\n", result->name, result->prefix,
                result->limit, result->nObjects);
	
	for (i = 0; i < result->nObjects; i++)
        {
                printf("key=%s, size=%u\n", result->objects[i].key,
                                result->objects[i].size);
        }

	qingstorDestroyContext(qsContext);

	return;
}

/*
 * test list buckets
 */
void testListBuckets_without_location()
{
	printf("testListBuckets_without_location.......\n");
	int i;
	qingstorContext qsContext = qingstorInitContext(access_key_id, secret_access_key);
	
	int count = 0;
    qingstorBucketInfo *buckets = qingstorListBuckets(qsContext, NULL, &count);
    if (count > 0)
    {
            for (i = 0; i < count; i++)
            {
                    printf("name=%s, location=%s, url=%s, created=%s\n",
                            buckets[i].name,
                            buckets[i].location,
                            buckets[i].url,
                            buckets[i].created);
            }
    }
    qingstorDestroyContext(qsContext);
	return;	
}

void testListBuckets_with_location()
{
        printf("testListBuckets_with_location.......\n");
        int i;
        qingstorContext qsContext = qingstorInitContext(access_key_id, secret_access_key);

        int count = 0;
        qingstorBucketInfo *buckets = qingstorListBuckets(qsContext, "sh1a", &count);
        if (count > 0)
        {
                for (i = 0; i < count; i++)
                {
                        printf("name=%s, location=%s, url=%s, created=%s\n",
                                buckets[i].name,
                                buckets[i].location,
                                buckets[i].url,
                                buckets[i].created);
                }
        }
        qingstorDestroyContext(qsContext);
        return;
}

void testHeadObject_positive()
{
	printf("testHeadObject_positive.............\n");
	qingstorContext qsContext = qingstorInitContext(access_key_id, secret_access_key);

	qingstorHeadObjectResult *result = qingstorHeadObject(qsContext, "hellobucket", "badformatted");
	if (result > 0)
	{
		printf("content_type=%s, content_length=%ld, last_modified=%s, etag=%s\n",
			result->content_type,
			result->content_length,
			result->last_modified,
			result->etag);
	}
	else
	{
		printf("error_message=%s\n", qingstorGetLastError());
	}
	qingstorDestroyContext(qsContext);
	return;
}

void testHeadObject_negative()
{
        printf("testHeadObject_negative.............\n");
        qingstorContext qsContext = qingstorInitContext(access_key_id, secret_access_key);

        qingstorHeadObjectResult *result = qingstorHeadObject(qsContext, "hellobucket", "badobject");
        if (result > 0)
        {
                printf("content_type=%s, content_length=%ld, last_modified=%s, etag=%s\n",
                        result->content_type, 
                        result->content_length,
                        result->last_modified,
                        result->etag);
        }
        else
        {
        		printf("error_message=%s\n", qingstorGetLastError());
        }
        qingstorDestroyContext(qsContext);
        return;
}

void testGetObject()
{
	printf("testGetObject...................\n");
	qingstorContext qsContext = qingstorInitContext(access_key_id, secret_access_key);

	qingstorObject object = qingstorGetObject(qsContext, "hdw-hashdata-cn", "tpch10g/partsupp/gpqsext.0.0");
	if (object)
	{
		char buffer[1024];
		int rnum = 0;
		while ((rnum = qingstorRead(qsContext, object, buffer, 1023)) > 0)
		{
			buffer[rnum] = '\0';
			printf("%s", buffer);
		}
		printf("\n");
		qingstorCloseObject(qsContext, object);
	}
	else
	{
		printf("error_message=%s\n", qingstorGetLastError());
	}
	qingstorDestroyContext(qsContext);
	return;
}

void testPutObject_positive()
{
	printf("testPutObject.....................\n");
	qingstorContext qsContext = qingstorInitContext(access_key_id, secret_access_key);

	qingstorObject object = qingstorPutObject(qsContext, "hellobucket", "zeros");
	if (object)
	{
		char buffer[10240] = {0};
		const int iter = 1024;
		int i;
		for (i = 0; i < iter; i++)
		{
			if (qingstorWrite(qsContext, object, buffer, 10240) != 10240)
			{
				printf("qingstor write failed with error message: %s\n", qingstorGetLastError());
				return;
			}
		}
		qingstorCloseObject(qsContext, object);
	}
	else
	{
		printf("qingstor put object failed with error message: %s\n", qingstorGetLastError());
	}
	qingstorDestroyContext(qsContext);
	return;
}

void testPutObject_negative()
{
	testPutObject_positive();
}

void testDeleteObject_positive()
{
	printf("testDeleteObject.....................\n");
	qingstorContext qsContext = qingstorInitContext(access_key_id, secret_access_key);

	if (qingstorDeleteObject(qsContext, "hellobucket", "zeros") == 0)
	{
		printf("successfully delete object hellobucket/zeros\n");
	}
	else
	{
		printf("qingstor delete object failed with error message: %s\n", qingstorGetLastError());
	}
	qingstorDestroyContext(qsContext);
	return;
}

void testCreateBucket_positive()
{
	printf("testCreateBucket......................\n");
	qingstorContext qsContext = qingstorInitContext(access_key_id, secret_access_key);

	if (qingstorCreateBucket(qsContext, "pek3a", "lirongbucket") == 0)
	{
		printf("successfully create bucket lirongbucket.pek3a\n");
	}
	else
	{
		printf("qingstor create bucket failed with error message: %s\n", qingstorGetLastError());
	}
	qingstorDestroyContext(qsContext);
	return;
}

void testDeleteBucket_positive()
{
	printf("testDeleteBucket......................\n");
	qingstorContext qsContext = qingstorInitContext(access_key_id, secret_access_key);

	if (qingstorDeleteBucket(qsContext, "pek3a", "lirongbucket") == 0)
	{
		printf("successfully delete bucket lirongbucket.pek3a\n");
	}
	else
	{
		printf("qingstor delete bucket failed with error message: %s\n", qingstorGetLastError());
	}
	qingstorDestroyContext(qsContext);
	return;
}

int main(int argc, char *argv[])
{
	/**
	testListObjects();
	testListBuckets_without_location();
	testListBuckets_with_location();
	testHeadObject_positive();
	testHeadObject_negative();
	testGetObject();
	testPutObject_positive();
	testPutObject_negative();
	testDeleteObject_positive();
	**/
	testCreateBucket_positive();
	testDeleteBucket_positive();

	return 0;
}
