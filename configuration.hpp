#ifndef _configuration_hpp_
#define _configuration_hpp_

class configuration
{
public:
    static configuration & instance()
    {
        // TODO: Generally, this approach is not thread-safe.
        // However GCC by default initializes statics in functions in thread-safe fashion. So clang does.
        // For the sake of saving some time I used that approach here
        static configuration conf;
        
        return conf;
    }

    std::string rootPath;
};

#endif // _configuration_hpp_
