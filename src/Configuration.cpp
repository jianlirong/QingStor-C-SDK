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

#include "Configuration.h"
#include "Logger.h"
#include "Exception.h"
#include "ExceptionInternal.h"

#include <stdio.h>
#include <yaml.h>

#include <map>

namespace QingStor {
namespace Internal {

static const char *CONFIG_KEY_ACCESS_KEY_ID = "access_key_id";

static const char *CONFIG_KEY_SECRET_ACCESS_KEY = "secret_access_key";

static const char *CONFIG_KEY_HOST = "host";

static const char *CONFIG_KEY_LOCATION = "location";

static const char *CONFIG_KEY_PORT = "port";

static const char *CONFIG_KEY_PROTOCOL = "protocol";

static const char *CONFIG_KEY_CONNECTION_RETRIES = "connection_retries";

static const char *CONFIG_KEY_NUM_CONNECTIONS = "num_connections";

static const char *CONFIG_KEY_CHUNK_SIZE = "chunk_size";

static const char *CONFIG_KEY_LOG_LEVEL = "log_level";

Configuration::Configuration(std::string location, std::string access_key_id, std::string secret_access_key)
{
	mAccessKeyId = access_key_id;
	mSecretAccessKey = secret_access_key;
	mLocation = location;

	mHost = "qingstor.com";
	mPort = 443;
	mProtocol = "https";
	mConnectionRetries = 3;
	mNConnections = 3;
	mChunkSize = (32 * 1024 * 1024);
	mLogLevel = "debug";
}

Configuration::Configuration(std::string config_file)
{
	FILE *fh = NULL;
	yaml_parser_t parser;
	yaml_token_t token;

	std::map<std::string,std::string> kvs;
	std::string token_key;
	std::string token_value;
	bool reach_key = false;
	bool reach_value = false;

	/* Initialize parser */
	if (!yaml_parser_initialize(&parser))
	{
		THROW(OutOfMemoryException, "couldn't initialize yaml parser");
	}

	/* Open configuration file */
	if ((fh = fopen(config_file.c_str(), "rb")) == NULL)
	{
		yaml_parser_delete(&parser);
		THROW(QingStorIOException, "couldn't open configure file %s", config_file.c_str());
	}

	/* Set input file */
	yaml_parser_set_input_file(&parser, fh);

	do {
		yaml_parser_scan(&parser, &token);
		switch (token.type)
		{
		case YAML_STREAM_START_TOKEN:
		case YAML_STREAM_END_TOKEN:
		case YAML_BLOCK_SEQUENCE_START_TOKEN:
		case YAML_BLOCK_ENTRY_TOKEN:
		case YAML_BLOCK_END_TOKEN:
		case YAML_BLOCK_MAPPING_START_TOKEN:
			break;
		case YAML_KEY_TOKEN:
			{
				reach_key = true;
			}
			break;
		case YAML_VALUE_TOKEN:
			{
				reach_value = true;
			}
			break;
		case YAML_SCALAR_TOKEN:
			{
				bool config_invalid = false;
				if (reach_key)
				{
					token_key = std::string((const char*)(token.data.scalar.value));
					reach_key = false;
				}
				else if (reach_value)
				{
					token_value = std::string((const char *)(token.data.scalar.value));
					reach_value = false;
					if (token_key.empty() || token_value.empty())
					{
						config_invalid = true;
					}
				}
				else
				{
					config_invalid = true;
				}

				if (config_invalid)
				{
					yaml_token_delete(&token);
					yaml_parser_delete(&parser);
					fclose(fh);
					THROW(QingStorConfigInvalid, "YAML configuration is invalid");
				}
				else
				{
					kvs.insert(std::pair<std::string, std::string>(token_key, token_value));
					token_key.clear();
					token_value.clear();
				}
			}
			break;
		default:
			{
				yaml_token_delete(&token);
				yaml_parser_delete(&parser);
				fclose(fh);
				THROW(QingStorConfigInvalid, "YAML configuration is invalid with token type %d\n", token.type);
			}
			break;
		}
		if (token.type != YAML_STREAM_END_TOKEN)
		{
			yaml_token_delete(&token);
		}
	} while (token.type != YAML_STREAM_END_TOKEN);

	yaml_token_delete(&token);
	yaml_parser_delete(&parser);
	fclose(fh);

	/* Go through the interested configurations */
	if (kvs[std::string(CONFIG_KEY_ACCESS_KEY_ID)].empty())
	{
		THROW(QingStorConfigInvalid, "missing access_key_id");
	}
	else
	{
		mAccessKeyId = kvs[std::string(CONFIG_KEY_ACCESS_KEY_ID)];
	}

	if (kvs[std::string(CONFIG_KEY_SECRET_ACCESS_KEY)].empty())
	{
		THROW(QingStorConfigInvalid, "missing secret_access_key");
	}
	else
	{
		mSecretAccessKey = kvs[std::string(CONFIG_KEY_SECRET_ACCESS_KEY)];
	}

	if (kvs[std::string(CONFIG_KEY_HOST)].empty())
	{
		mHost = "qingstor.com";
	}
	else
	{
		mHost = kvs[std::string(CONFIG_KEY_HOST)];
	}

	if (kvs[std::string(CONFIG_KEY_LOCATION)].empty())
	{
		mLocation = "pek3a";
	}
	else
	{
		mLocation = kvs[std::string(CONFIG_KEY_LOCATION)];
	}

	if (kvs[std::string(CONFIG_KEY_PORT)].empty())
	{
		mPort = 443;
	}
	else
	{
		std::string port_str = kvs[std::string(CONFIG_KEY_PORT)];
		int port = atoi(port_str.c_str());
		if (port <= 0 || port > 65535)
		{
			LOG(WARNING, "Configuration Port %s is invalid, using default 443", port_str.c_str());
			port = 443;
		}
		mPort = port;
	}

	if (kvs[std::string(CONFIG_KEY_PROTOCOL)].empty())
	{
		mProtocol = "https";
	}
	else
	{
		mProtocol = kvs[std::string(CONFIG_KEY_PROTOCOL)];
	}

	if (kvs[std::string(CONFIG_KEY_CONNECTION_RETRIES)].empty())
	{
		mConnectionRetries = 3;
	}
	else
	{
		std::string retries_str = kvs[std::string(CONFIG_KEY_CONNECTION_RETRIES)];
		int retries = atoi(retries_str.c_str());
		if (retries <= 0 || retries > 16)
		{
			LOG(WARNING, "Configuration connection retries %s is invalid, using default 3", retries_str.c_str());
			retries = 3;
		}
		mConnectionRetries = retries;
	}

	if (kvs[std::string(CONFIG_KEY_NUM_CONNECTIONS)].empty())
	{
		mNConnections = 3;
	}
	else
	{
		std::string num_connections_str = kvs[std::string(CONFIG_KEY_NUM_CONNECTIONS)];
		int num = atoi(num_connections_str.c_str());
		if (num <= 0 || num > 8)
		{
			LOG(WARNING, "Configuration connection retries %s is invalid, using default 3", num_connections_str.c_str());
			num = 3;
		}
		mNConnections = num;
	}

	if (kvs[std::string(CONFIG_KEY_CHUNK_SIZE)].empty())
	{
		mChunkSize = (32 * 1024 * 1024);
	}
	else
	{
		std::string chunk_size_str = kvs[std::string(CONFIG_KEY_CHUNK_SIZE)];
		int num = atoi(chunk_size_str.c_str());
		if (num <= 0)
		{
			LOG(WARNING, "Configuration connection retries %s is invalid, using default 32MB", chunk_size_str.c_str());
			num = 32 * 1024 * 1024;
		}
		mChunkSize = num;
	}


	if (kvs[std::string(CONFIG_KEY_LOG_LEVEL)].empty())
	{
		mLogLevel = "debug";
	}
	else
	{
		mLogLevel = kvs[std::string(CONFIG_KEY_LOG_LEVEL)];
	}
}

}
}


