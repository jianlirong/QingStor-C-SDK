#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include "qingstor/qingstor.h"

int64_t getcurrenttime()
{
   struct timeval tv;
   gettimeofday(&tv, NULL);
   return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

static double CalculateThroughput(int64_t elapsed, int64_t size) 
{
    return size / 1024.0 * 1000.0 / 1024.0 / elapsed;
}

const char *access_key_id = "CNQHLNCMKNSMQXMMTGVL";
const char *secret_access_key = "RV9HRXHLpcBQe5cSqwZN7i2OBYpmvEO1wXpRugx7";
const char *location = "pek3a";
const int qingstor_buffer_size = 48 << 20;
const int write_buffer_size = 8 << 20;
int64_t file_length = 256 << 20;
int64_t file_nums = 16;

void testPutObject_positive(char *filename)
{
	printf("testPutObject %s.............\n", filename);
	qingstorContext qsContext = qingstorInitContext(location, access_key_id, secret_access_key, qingstor_buffer_size);

	qingstorObject object = qingstorPutObject(qsContext, "alluxio", filename);
	if (object)
	{
		char *buffer = (char *)malloc(write_buffer_size * sizeof(char));
		const int iter = (int)(file_length / write_buffer_size);
		int i;
		for (i = 0; i < iter; i++)
		{
			if (qingstorWrite(qsContext, object, buffer, write_buffer_size) != write_buffer_size)
			{
				printf("qingstor write failed with error message: %s\n", qingstorGetLastError());
				return;
			}
		}
		qingstorCloseObject(qsContext, object);
		free(buffer);
	}
	else
	{
		printf("qingstor put object failed with error message: %s\n", qingstorGetLastError());
	}
	qingstorDestroyContext(qsContext);
	return;
}

void testPutObject()
{
	int i;
	for(i = 0; i < file_nums; i ++)
	{
		char filename[50];
		sprintf(filename, "tmp/test_write_%d", i);
		testPutObject_positive(filename);
	}
}

int main(int argc, char *argv[])
{
	int64_t start, stop;
	int64_t elapsed;
	start = getcurrenttime();
	testPutObject();
	stop = getcurrenttime();
	elapsed = stop - start;
	printf("clapsed time %ld ms, throughput is %lf mbytes/s\n", elapsed, CalculateThroughput(elapsed, file_length * file_nums));
	return 0;
}
