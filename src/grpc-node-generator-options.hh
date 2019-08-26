#pragma once

#include <string>
#include <map>

class GrpcNodeGeneratorOptions {
private:
  std::string error_;

public:

  GrpcNodeGeneratorOptions
    ( const std::string& parameter
    );

  bool hasError
    ( std::string* error
    ) const;

  const std::map<std::string, std::string> vars
    () const;
};
