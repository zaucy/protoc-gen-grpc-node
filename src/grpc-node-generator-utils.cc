#include "grpc-node-generator-utils.hh"

void GrpcNodeGeneratorUtils::split
  ( const std::string&         str
  , char                       delim
  , std::vector<std::string>*  append_to
  )
{
  std::istringstream iss(str);
  std::string piece;
  while(std::getline(iss, piece)) {
    append_to->push_back(piece);
  }
}

std::string GrpcNodeGeneratorUtils::GenerateCommentsWithPrefix
  ( const std::vector<std::string>&  in
  , const std::string&               prefix
  )
{
  std::ostringstream oss;
  for (auto it = in.begin(); it != in.end(); it++) {
    const std::string& elem = *it;
    if (elem.empty()) {
      oss << prefix << "\n";
    } else if (elem[0] == ' ') {
      oss << prefix << elem << "\n";
    } else {
      oss << prefix << " " << elem << "\n";
    }
  }
  return oss.str();
}

std::string GrpcNodeGeneratorUtils::removePathExtname
  ( const std::string& path
  )
{
  auto dotIndex = path.find_last_of('.');

  if(dotIndex != std::string::npos) {
    return path.substr(0, dotIndex);
  }

  return path;
}

std::string GrpcNodeGeneratorUtils::moduleAlias
  ( const std::string filename
  )
{
  std::string basename = stripProto(filename);
  basename = stringReplace(basename, "-", "$");
  basename = stringReplace(basename, "/", "__");
  basename = stringReplace(basename, ".", "_");
  return basename + "_pb";
}

std::string GrpcNodeGeneratorUtils::nodeObjectPath
  ( const google::protobuf::Descriptor* descriptor
  )
{
  std::string module_alias = moduleAlias(descriptor->file()->name());
  std::string name = descriptor->full_name();
  stripPrefix(&name, descriptor->file()->package() + ".");
  return module_alias + "." + name;
}

std::string GrpcNodeGeneratorUtils::getRootPath
  ( const std::string& from_filename
  , const std::string& to_filename
  )
{
  auto slashes = std::count(from_filename.begin(), from_filename.end(), '/');
  if(slashes == 0) {
    return "./";
  }

  std::string result = "";
  for(auto i=0; slashes > i; ++i) {
    result += "../";
  }

  return result;
}

std::string GrpcNodeGeneratorUtils::messageIdentifierName
  ( const std::string& name
  )
{
  return GrpcNodeGeneratorUtils::stringReplace(name, ".", "_");
}

std::string GrpcNodeGeneratorUtils::getRelativePath
  ( const std::string& fromFile
  , const std::string& toFile
  )
{
  return GrpcNodeGeneratorUtils::getRootPath(fromFile, toFile) + toFile;
}
