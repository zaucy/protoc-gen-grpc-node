#include <google/protobuf/compiler/plugin.h>

#include "grpc-node-generator.hh"

int main(int argc, char* argv[]) {
  GrpcNodeGenerator generator;
  PluginMain(argc, argv, &generator);
  return 0;
}
