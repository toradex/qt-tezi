#pragma once
#ifndef __VALIJSON_UTILS_RAPIDJSON_UTILS_HPP
#define __VALIJSON_UTILS_RAPIDJSON_UTILS_HPP

#include <iostream>

#include <rapidjson/document.h>

#include <valijson/utils/file_utils.hpp>

namespace valijson {
namespace utils {

template<typename Encoding, typename Allocator>
inline bool loadDocumentFromString(const char *string, rapidjson::GenericDocument<Encoding, Allocator> &document)
{
    // Parse schema
    document.template Parse<0>(string);
    if (document.HasParseError()) {
        std::cerr << "RapidJson failed to parse the document:" << std::endl;
        std::cerr << "Parse error: " << document.GetParseError() << std::endl;
        std::cerr << "Near: " <<  std::string(string, (std::max)(size_t(0), document.GetErrorOffset() - 20), 40) << std::endl;
        return false;
    }

    return true;
}

template<typename Encoding, typename Allocator>
inline bool loadDocument(const std::string &path, rapidjson::GenericDocument<Encoding, Allocator> &document)
{
    // Load schema JSON from file
    std::string file;
    if (!loadFile(path, file)) {
        std::cerr << "Failed to load json from file '" << path << "'." << std::endl;
        return false;
    }

    return loadDocumentFromString(file.c_str(), document);
}

}  // namespace utils
}  // namespace valijson

#endif
