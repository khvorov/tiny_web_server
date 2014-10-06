#include <fstream>
#include <iostream>
#include <map>
#include <regex>
#include <sstream>

#include <sys/socket.h>
#include <unistd.h>

#include "configuration.hpp"
#include "request_processor.hpp"

namespace
{
struct http_request
{
public:
    http_request(const ByteBuffer & req)
    {
        std::match_results<ByteBuffer::const_iterator> request_match;

        if (std::regex_search(req.begin(), req.end(), request_match, request_regex))
        {
            if (request_match.size() == 4)
            {
                request = request_match[1].str();
                data = request_match[2].str();
                version = request_match[3].str();
            }
        }
    }

    std::string request;
    std::string data;
    std::string version;

private:
    static std::regex request_regex;
};

std::regex http_request::request_regex("^\\s*([A-Z]*)\\s+(.*?)\\s+HTTP\\/(\\d\\.\\d).*");

const std::string & get_content_type(const std::string & file)
{
    // TODO: that function can be implemented using libmagic library
    static std::regex file_extension_regex("^.*\\.(\\w+)$");

    static const std::string defaultContentType("text/plain");
    static const std::map<std::string, std::string> fileToContentType = {
        {"html", "text/html"},
        {"htm", "text/html"},
        {"shtm", "text/html"},
        {"shtml", "text/html"},
        {"css", "text/css"},
        {"js", "application/x-javascript"},
        {"ico", "image/x-icon"},
        {"gif", "image/gif"},
        {"jpg", "image/jpeg"},
        {"jpeg", "image/jpeg"},
        {"png", "image/png"},
        {"svg", "image/svg+xml"},
        {"txt", "text/plain"},
        {"torrent", "application/x-bittorrent"},
        {"wav", "audio/x-wav"},
        {"mp3", "audio/x-mp3"},
        {"mid", "audio/mid"},
        {"m3u", "audio/x-mpegurl"},
        {"ogg", "application/ogg"},
        {"ram", "audio/x-pn-realaudio"},
        {"xml", "text/xml"},
        {"json", "application/json"},
        {"xslt", "application/xml"},
        {"xsl", "application/xml"},
        {"ra", "audio/x-pn-realaudio"},
        {"doc", "application/msword"},
        {"exe", "application/octet-stream"},
        {"zip", "application/x-zip-compressed"},
        {"xls", "application/excel"},
        {"tgz", "application/x-tar-gz"},
        {"tar", "application/x-tar"},
        {"gz", "application/x-gunzip"},
        {"arj", "application/x-arj-compressed"},
        {"rar", "application/x-rar-compressed"},
        {"rtf", "application/rtf"},
        {"pdf", "application/pdf"},
        {"swf", "application/x-shockwave-flash"},
        {"mpg", "video/mpeg"},
        {"webm", "video/webm"},
        {"mpeg", "video/mpeg"},
        {"mov", "video/quicktime"},
        {"mp4", "video/mp4"},
        {"m4v", "video/x-m4v"},
        {"asf", "video/x-ms-asf"},
        {"avi", "video/x-msvideo"},
        {"bmp", "image/bmp"},
        {"ttf", "application/x-font-ttf"}
    };

    std::smatch request_match;

    if (std::regex_search(file, request_match, file_extension_regex))
    {
        if (request_match.size() == 2)
        {
            auto contentType = fileToContentType.find(request_match[1].str());

            if (contentType != fileToContentType.end())
            {
                return contentType->second;
            }
        }
    }

    return defaultContentType;
}

bool is_relative_path(const std::string & path)
{
    static std::regex rpath_regex("\\.\\.");

    std::smatch request_match;

    return std::regex_search(path, request_match, rpath_regex);
}

std::string generate_error_response(const std::string & reason, const std::string & text)
{
    std::stringstream ss;

    ss << "HTTP/1.1 " << reason << "\r\n"
       << "Connection: close\r\n"
       << "Server: localhost\r\n"
       << "Content-Type: text/html\r\n\r\n"
       << "<html><head><title>" << reason << "</title>"
       << "<style type=\"text/css\"><!--/*--><![CDATA[/*><!--*/body{color:#000;background-color:#FFF;font-family:sans-serif;}/*]]>*/--></style>"
       << "</head><body><h1>" << reason << "</h1><p>"
       << text
       << "</p></body></html>";

    return ss.str();
}

}

void request_processor::operator()(ByteBufferPtr buffer, int fd)
{
    if (!buffer)
    {
        // pointer to the buffer is null
        return;
    }

    // parse header
    http_request request(*buffer);
    std::stringstream ss;
    bool closeConnection = true;

    if (request.request == "GET")
    {
        // checking for relative path in the request to deny access to top-level directories
        if (!is_relative_path(request.data))
        {
            ss << configuration::instance().rootPath;

            if (request.data.empty() || request.data == "/")
            {
                ss << "/index.html";
            }
            else
            {
                ss << request.data;
            }

            std::string fullPath = ss.str();
            std::cout << "trying to open a file " << fullPath << std::endl;

            // search for the file
            std::ifstream inFile(fullPath.c_str(), std::ios::in | std::ios::binary);

            ss.str(std::string());

            if (inFile.is_open())
            {
                // getting file size
                inFile.seekg(0, std::ios::end);
                auto fileSize = inFile.tellg();
                inFile.seekg(0, std::ios::beg);

                // read file
                ss << "HTTP/" << request.version << " 200 OK\r\n"
                   << "Connection: close\r\n"
                   << "Server: localhost\r\n"
                   << "Content-Length:" << fileSize << "\r\n"
                   << "Content-Type: " << get_content_type(fullPath) << "\r\n\r\n"
                   << inFile.rdbuf();
                
                closeConnection = true;
            }
            else
            {
                // 404 Not Found
                std::stringstream message;
                message << "The requested URL (" << request.data << ") was not found on this server.";
                ss << generate_error_response("404 Not found", message.str());
            }
        }
        else
        {
            std::stringstream message;
            message << "Access to " << request.data << " is forbidden";
            ss << generate_error_response("403 Forbidden", message.str());
        }
    }
    else
    {
        ss << generate_error_response("400 Bad Request", "Request is not supported");
    }

    std::string output(ss.str());

    send(fd, output.c_str(), output.size(), 0);

    if (closeConnection)
    {
        close(fd);
    }
}
