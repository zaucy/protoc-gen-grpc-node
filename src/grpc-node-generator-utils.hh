#pragma once

#include <string>
#include <vector>
#include <sstream>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/compiler/code_generator.h>

namespace GrpcNodeGeneratorUtils {

  inline bool stripSuffix(std::string* filename, const std::string& suffix) {
    if (filename->length() >= suffix.length()) {
      size_t suffix_pos = filename->length() - suffix.length();
      if (filename->compare(suffix_pos, std::string::npos, suffix) == 0) {
        filename->resize(filename->size() - suffix.size());
        return true;
      }
    }

    return false;
  }

  inline bool stripPrefix(std::string* name, const std::string& prefix) {
    if (name->length() >= prefix.length()) {
      if (name->substr(0, prefix.size()) == prefix) {
        *name = name->substr(prefix.size());
        return true;
      }
    }
    return false;
  }

  inline std::string stripProto(std::string filename) {
    if (!stripSuffix(&filename, ".protodevel")) {
      stripSuffix(&filename, ".proto");
    }
    return filename;
  }

  inline std::string stringReplace(std::string str, const std::string& from,
                                    const std::string& to, bool replace_all) {
    size_t pos = 0;

    do {
      pos = str.find(from, pos);
      if (pos == std::string::npos) {
        break;
      }
      str.replace(pos, from.length(), to);
      pos += to.length();
    } while (replace_all);

    return str;
  }

  inline std::string stringReplace(std::string str, const std::string& from,
                                    const std::string& to) {
    return stringReplace(str, from, to, true);
  }

  inline std::vector<std::string> tokenize(const std::string& input,
                                            const std::string& delimiters) {
    std::vector<std::string> tokens;
    size_t pos, last_pos = 0;

    for (;;) {
      bool done = false;
      pos = input.find_first_of(delimiters, last_pos);
      if (pos == std::string::npos) {
        done = true;
        pos = input.length();
      }

      tokens.push_back(input.substr(last_pos, pos - last_pos));
      if (done) return tokens;

      last_pos = pos + 1;
    }
  }

  inline std::string capitalizeFirstLetter(std::string s) {
    if (s.empty()) {
      return s;
    }
    s[0] = ::toupper(s[0]);
    return s;
  }

  inline std::string lowercaseFirstLetter(std::string s) {
    if (s.empty()) {
      return s;
    }
    s[0] = ::tolower(s[0]);
    return s;
  }

  inline std::string lowerUnderscoreToUpperCamel(std::string str) {
    std::vector<std::string> tokens = tokenize(str, "_");
    std::string result = "";
    for (unsigned int i = 0; i < tokens.size(); i++) {
      result += capitalizeFirstLetter(tokens[i]);
    }
    return result;
  }

  inline std::string fileNameInUpperCamel(
      const google::protobuf::FileDescriptor* file, bool include_package_path) {
    std::vector<std::string> tokens = tokenize(stripProto(file->name()), "/");
    std::string result = "";
    if (include_package_path) {
      for (unsigned int i = 0; i < tokens.size() - 1; i++) {
        result += tokens[i] + "/";
      }
    }
    result += lowerUnderscoreToUpperCamel(tokens.back());
    return result;
  }

  inline std::string fileNameInUpperCamel(
      const google::protobuf::FileDescriptor* file) {
    return fileNameInUpperCamel(file, true);
  }

  enum MethodType {
    METHODTYPE_NO_STREAMING,
    METHODTYPE_CLIENT_STREAMING,
    METHODTYPE_SERVER_STREAMING,
    METHODTYPE_BIDI_STREAMING
  };

  inline MethodType getMethodType(
      const google::protobuf::MethodDescriptor* method) {
    if (method->client_streaming()) {
      if (method->server_streaming()) {
        return METHODTYPE_BIDI_STREAMING;
      } else {
        return METHODTYPE_CLIENT_STREAMING;
      }
    } else {
      if (method->server_streaming()) {
        return METHODTYPE_SERVER_STREAMING;
      } else {
        return METHODTYPE_NO_STREAMING;
      }
    }
  }

  void split
    ( const std::string&         str
    , char                       delim
    , std::vector<std::string>*  append_to
    );

  enum CommentType {
    COMMENTTYPE_LEADING,
    COMMENTTYPE_TRAILING,
    COMMENTTYPE_LEADING_DETACHED
  };

  // Get all the raw comments and append each line without newline to out.
  template <typename DescriptorType>
  inline void getComment
    ( const DescriptorType*      desc
    , CommentType                type
    , std::vector<std::string>*  out
    )
  {
    google::protobuf::SourceLocation location;
    if (!desc->GetSourceLocation(&location)) {
      return;
    }
    if (type == COMMENTTYPE_LEADING || type == COMMENTTYPE_TRAILING) {
      const std::string& comments = type == COMMENTTYPE_LEADING
                                        ? location.leading_comments
                                        : location.trailing_comments;
      split(comments, '\n', out);
    } else if (type == COMMENTTYPE_LEADING_DETACHED) {
      for (unsigned int i = 0; i < location.leading_detached_comments.size();
          i++) {
        split(location.leading_detached_comments[i], '\n', out);
        out->push_back("");
      }
    } else {
      std::cerr << "Unknown comment type " << type << std::endl;
      abort();
    }
  }

  // Each raw comment line without newline is appended to out.
  // For file level leading and detached leading comments, we return comments
  // above syntax line. Return nothing for trailing comments.
  template <>
  inline void getComment
    ( const google::protobuf::FileDescriptor*  desc
    , CommentType                              type
    , std::vector<std::string>*                out
    )
  {
    if(type == COMMENTTYPE_TRAILING) {
      return;
    }

    google::protobuf::SourceLocation location;
    std::vector<int> path;
    path.push_back(google::protobuf::FileDescriptorProto::kSyntaxFieldNumber);
    if(!desc->GetSourceLocation(path, &location)) {
      return;
    }

    if(type == COMMENTTYPE_LEADING) {
      split(location.leading_comments, '\n', out);
    } else if (type == COMMENTTYPE_LEADING_DETACHED) {
      auto detachedCommentsSize = location.leading_detached_comments.size();
      for(auto i=0; i < detachedCommentsSize; ++i) {
        split(location.leading_detached_comments[i], '\n', out);
        out->push_back("");
      }
    } else {
      std::cerr << "Unknown comment type " << type << std::endl;
      abort();
    }
  }

  // Add prefix and newline to each comment line and concatenate them together.
  // Make sure there is a space after the prefix unless the line is empty.
  std::string GenerateCommentsWithPrefix
    ( const std::vector<std::string>&  in
    , const std::string&               prefix
    );

  template <typename DescriptorType>
  inline std::string GetPrefixedComments
    ( const DescriptorType*  desc
    , const bool             leading
    , const std::string&     prefix
    )
  {
    std::vector<std::string> out;
    if(leading) {
      GrpcNodeGeneratorUtils::getComment(
        desc, GrpcNodeGeneratorUtils::COMMENTTYPE_LEADING_DETACHED, &out);
      std::vector<std::string> leading;
      GrpcNodeGeneratorUtils::getComment(
        desc, GrpcNodeGeneratorUtils::COMMENTTYPE_LEADING, &leading);
      out.insert(out.end(), leading.begin(), leading.end());
    } else {
      GrpcNodeGeneratorUtils::getComment(
        desc, GrpcNodeGeneratorUtils::COMMENTTYPE_TRAILING, &out);
    }

    return GenerateCommentsWithPrefix(out, prefix);
  }

  std::string removePathExtname
    ( const std::string& path
    );

  // Returns the alias we assign to the module of the given .proto filename
  // when importing.
  std::string moduleAlias
    ( const std::string filename
    );

  std::string nodeObjectPath
    ( const google::protobuf::Descriptor* descriptor
    );

  // Given a filename like foo/bar/baz.proto, returns the root directory
  // path ../../
  std::string getRootPath
    ( const std::string& fromFilename
    , const std::string& toFilename
    );

  // Return the relative path to load `toFile` from the directory containing
  // `fromFile`, assuming that both paths are relative to the same directory
  std::string getRelativePath
    ( const std::string& fromFile
    , const std::string& toFile
    );

  std::string messageIdentifierName
    ( const std::string& name
    );

} // namespace GrpcNodeGeneratorUtils
