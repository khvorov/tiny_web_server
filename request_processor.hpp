#ifndef _request_processor_hpp_
#define _request_processor_hpp_

#include <deque>
#include <memory>

typedef std::deque<char> ByteBuffer;
typedef std::shared_ptr<ByteBuffer> ByteBufferPtr;

class request_processor
{
public:
    void operator()(ByteBufferPtr request, int fd);
};

#endif //_request_processor_hpp_
