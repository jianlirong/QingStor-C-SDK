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
#ifndef _QINGSTOR_LIBQINGSTOR_EXCEPTION_H_
#define _QINGSTOR_LIBQINGSTOR_EXCEPTION_H_

#include <stdexcept>
#include <string>

namespace QingStor {

class QingStorException: public std::runtime_error {
public:
    QingStorException(const std::string & arg, const char * file, int line,
                  const char * stack);

    ~QingStorException() throw () {
    }

    virtual const char * msg() const {
        return detail.c_str();
    }

protected:
    std::string detail;
};

class QingStorIOException: public QingStorException {
public:
    QingStorIOException(const std::string & arg, const char * file, int line,
                    const char * stack) :
           QingStorException(arg, file, line, stack) {
    }

    ~QingStorIOException() throw () {
    }

public:
    static const char * ReflexName;
};

class QingStorEndOfStream: public QingStorIOException {
public:
    QingStorEndOfStream(const std::string & arg, const char * file, int line,
                         const char * stack) :
        QingStorIOException(arg, file, line, stack) {
    }

    ~QingStorEndOfStream() throw () {
    }
};

class QingStorNetworkException: public QingStorIOException {
public:
    QingStorNetworkException(const std::string & arg, const char * file, int line,
                         const char * stack) :
        QingStorIOException(arg, file, line, stack) {
    }

    ~QingStorNetworkException() throw () {
    }
};

class QingStorNetworkConnectException: public QingStorNetworkException {
public:
    QingStorNetworkConnectException(const std::string & arg, const char * file, int line,
                                const char * stack) :
        QingStorNetworkException(arg, file, line, stack) {
    }

    ~QingStorNetworkConnectException() throw () {
    }
};

class AccessControlException: public QingStorException {
public:
    AccessControlException(const std::string & arg, const char * file, int line,
                           const char * stack) :
        QingStorException(arg, file, line, stack) {
    }

    ~AccessControlException() throw () {
    }

public:
    static const char * ReflexName;
};

class QingStorCanceled: public QingStorException {
public:
    QingStorCanceled(const std::string & arg, const char * file, int line,
                 const char * stack) :
        QingStorException(arg, file, line, stack) {
    }

    ~QingStorCanceled() throw () {
    }
};

class QingStorConfigInvalid: public QingStorException {
public:
    QingStorConfigInvalid(const std::string & arg, const char * file, int line,
                      const char * stack) :
        QingStorException(arg, file, line, stack) {
    }

    ~QingStorConfigInvalid() throw () {
    }
};

class QingStorConfigNotFound: public QingStorException {
public:
    QingStorConfigNotFound(const std::string & arg, const char * file, int line,
                       const char * stack) :
        QingStorException(arg, file, line, stack) {
    }

    ~QingStorConfigNotFound() throw () {
    }
};

class QingStorTimeoutException: public QingStorException {
public:
    QingStorTimeoutException(const std::string & arg, const char * file, int line,
                         const char * stack) :
        QingStorException(arg, file, line, stack) {
    }

    ~QingStorTimeoutException() throw () {
    }
};

class InvalidParameter: public QingStorException {
public:
    InvalidParameter(const std::string & arg, const char * file, int line,
                     const char * stack) :
        QingStorException(arg, file, line, stack) {
    }

    ~InvalidParameter() throw () {
    }

public:
    static const char * ReflexName;
};

class IllegalArgumentException : public InvalidParameter {
public:
    IllegalArgumentException(const std::string& arg, const char* file,
                                   int line, const char* stack)
        : InvalidParameter(arg, file, line, stack) {
    }

    ~IllegalArgumentException() throw() {
    }

public:
    static const char* ReflexName;
};

class OutOfMemoryException : public QingStorException {
public:
	OutOfMemoryException(const std::string& arg, const char* file,
							int line, const char *stack) :
		QingStorException(arg, file, line, stack) {

	}
	~OutOfMemoryException() throw() {

	}
};

}

#endif /* _QINGSTOR_LIBQINGSTOR_EXCEPTION_H_ */
