#include "grpc-node-generator.hh"

#include "grpc-node-generator-utils.hh"

#include <cctype>
#include <set>
#include <google/protobuf/compiler/code_generator.h>
#include <google/protobuf/compiler/plugin.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/io/printer.h>
#include <google/protobuf/io/zero_copy_stream.h>

using google::protobuf::Descriptor;
using google::protobuf::FieldDescriptor;
using google::protobuf::FileDescriptor;
using google::protobuf::MethodDescriptor;
using google::protobuf::ServiceDescriptor;
using google::protobuf::compiler::CodeGenerator;
using google::protobuf::compiler::GeneratorContext;
using google::protobuf::compiler::ParseGeneratorParameter;
using google::protobuf::compiler::PluginMain;
using google::protobuf::io::Printer;
using google::protobuf::io::ZeroCopyOutputStream;

namespace utils = GrpcNodeGeneratorUtils;

namespace {
  /* Finds all message types used in all services in the file, and returns them
  * as a map of fully qualified message type name to message descriptor */
  std::map<std::string, const Descriptor*> GetAllMessages(
      const FileDescriptor* file) {
    std::map<std::string, const Descriptor*> message_types;
    for (int service_num = 0; service_num < file->service_count();
        service_num++) {
      const ServiceDescriptor* service = file->service(service_num);
      for (int method_num = 0; method_num < service->method_count();
          method_num++) {
        const MethodDescriptor* method = service->method(method_num);
        const Descriptor* input_type = method->input_type();
        const Descriptor* output_type = method->output_type();
        message_types[input_type->full_name()] = input_type;
        message_types[output_type->full_name()] = output_type;
      }
    }
    return message_types;
  }

  std::string GetTsMessageFilename(const std::string& filename) {
    std::string name = filename;
    return utils::stripProto(name) + "_pb";
  }

  std::string GetMethodInterfaceName(const MethodDescriptor* method) {
    std::string methodInterfaceName =
      utils::lowercaseFirstLetter(method->name());

    if(methodInterfaceName == "new") {
      methodInterfaceName = "'new'";
    }

    return methodInterfaceName;
  }
}

bool GrpcNodeGenerator::PrintServiceImplementationInterface
  ( google::protobuf::io::Printer&              printer
  , const GrpcNodeGeneratorOptions&          options
  , const google::protobuf::ServiceDescriptor*  service
  , std::string*                                error
  ) const
{

  std::map<std::string, std::string> vars = {
    {"ServiceName", service->name()}
  };

  printer.Print(vars, "export interface I$ServiceName$Implementation {\n");
  printer.Indent();

  auto methodCount = service->method_count();

  for(auto i=0; methodCount > i; ++i) {
    auto method = service->method(i);
    auto inputType = method->input_type();
    auto outputType = method->output_type();

    vars["methodName"] = GetMethodInterfaceName(method);
    vars["RequestType"] = utils::nodeObjectPath(inputType);
    vars["ResponseType"] = utils::nodeObjectPath(outputType);
    
    if(method->client_streaming() && method->server_streaming()) {
      printer.Print(vars, "$methodName$: "
        "grpc.handleBidiStreamingCall<$RequestType$, $ResponseType$>;\n");
    } else
    if(method->client_streaming()) {
      printer.Print(vars, "$methodName$: "
        "grpc.handleClientStreamingCall<$RequestType$, $ResponseType$>;\n");
    } else
    if(method->server_streaming()) {
      printer.Print(vars, "$methodName$: "
        "grpc.handleServerStreamingCall<$RequestType$, $ResponseType$>;\n");
    } else {
      printer.Print(vars, "$methodName$: "
        "grpc.handleUnaryCall<$RequestType$, $ResponseType$>;\n");
    }
  }

  printer.Outdent();
  printer.Print(vars, "}\n\n");

  return true;
}

bool GrpcNodeGenerator::PrintServiceDefinition
  ( google::protobuf::io::Printer&              printer
  , const GrpcNodeGeneratorOptions&          options
  , const google::protobuf::ServiceDescriptor*  service
  , std::string*                                error
  ) const
{
  std::map<std::string, std::string> vars = {
    {"ServiceName", service->name()}
  };

  printer.Print(vars,
    "export const $ServiceName$Service: "
    "grpc.ServiceDefinition<I$ServiceName$Implementation> = {\n");
  printer.Indent();

  auto methodCount = service->method_count();

  for(auto i=0; methodCount > i; ++i) {
    auto method = service->method(i);
    auto inputType = method->input_type();
    auto outputType = method->output_type();

    vars["methodName"] = GetMethodInterfaceName(method);
    vars["RequestType"] = utils::nodeObjectPath(inputType);
    vars["ResponseType"] = utils::nodeObjectPath(outputType);
    
    printer.Print(vars,
      "$methodName$: <grpc.MethodDefinition<$RequestType$, $ResponseType$>>");

    if(!PrintServiceMethodDefinition(printer, options, method, error)) {
      return false;
    }

    printer.Print(",\n");
  }

  printer.Outdent();
  printer.Print(vars, "}\n\n");

  return true;
}

bool GrpcNodeGenerator::PrintMessageTransformer
  ( google::protobuf::io::Printer&       printer
  , const GrpcNodeGeneratorOptions&      options
  , const google::protobuf::Descriptor*  descriptor
  , std::string*                         error
  ) const
{
  std::map<std::string, std::string> vars;
  std::string fullName = descriptor->full_name();
  vars["identifierName"] = utils::messageIdentifierName(fullName);
  vars["name"] = fullName;
  vars["NodeName"] = utils::nodeObjectPath(descriptor);

  // Print the serializer
  printer.Print(vars, "function serialize_$identifierName$(arg: $NodeName$): any {\n");
  printer.Indent();
  // printer.Print(vars, "if (!(arg instanceof $NodeName$)) {\n");
  // printer.Indent();
  // printer.Print(vars, "throw new Error('Expected argument of type $name$');\n");
  // printer.Outdent();
  // printer.Print("}\n");
  // printer.Print("console.trace(arg);\n");
  printer.Print("return Buffer.from(arg.serializeBinary());\n");
  printer.Outdent();
  printer.Print("}\n\n");

  // Print the deserializer
  printer.Print(vars,
    "function deserialize_$identifierName$(buffer_arg: any): any {\n");
  printer.Indent();
  printer.Print(vars,
    "return $NodeName$.deserializeBinary(new Uint8Array(buffer_arg));\n");
  printer.Outdent();
  printer.Print("}\n\n");
  return true;
}

bool GrpcNodeGenerator::PrintServiceMethodDefinition
  ( google::protobuf::io::Printer&              printer
  , const GrpcNodeGeneratorOptions&             options
  , const google::protobuf::MethodDescriptor*   method
  , std::string*                                error
  ) const
{
  const Descriptor* inputType = method->input_type();
  const Descriptor* outputType = method->output_type();
  std::map<std::string, std::string> vars;
  vars["ServiceFullName"] = method->service()->full_name();
  vars["MethodName"] = method->name();
  vars["inputType"] = utils::nodeObjectPath(inputType);
  vars["inputTypeId"] = utils::messageIdentifierName(inputType->full_name());
  vars["outputType"] = utils::nodeObjectPath(outputType);
  vars["outputTypeId"] = utils::messageIdentifierName(outputType->full_name());
  vars["isClientStream"] = method->client_streaming() ? "true" : "false";
  vars["isServerStream"] = method->server_streaming() ? "true" : "false";
  printer.Print("{\n");
  printer.Indent();
  printer.Print(vars, "path: '/$ServiceFullName$/$MethodName$',\n");
  printer.Print(vars, "requestStream: $isClientStream$,\n");
  printer.Print(vars, "responseStream: $isServerStream$,\n");
  printer.Print(vars, "requestType: $inputType$,\n");
  printer.Print(vars, "responseType: $outputType$,\n");
  printer.Print(vars, "requestSerialize: serialize_$inputTypeId$,\n");
  printer.Print(vars, "requestDeserialize: deserialize_$inputTypeId$,\n");
  printer.Print(vars, "responseSerialize: serialize_$outputTypeId$,\n");
  printer.Print(vars, "responseDeserialize: deserialize_$outputTypeId$,\n");
  printer.Outdent();
  printer.Print("}");

  return true;
}

bool GrpcNodeGenerator::PrintServicePromiseClientInterface
  ( google::protobuf::io::Printer&              printer
  , const GrpcNodeGeneratorOptions&          options
  , const google::protobuf::ServiceDescriptor*  service
  , std::string*                                error
  ) const
{
  std::map<std::string, std::string> vars = {
    {"ServiceName", service->name()}
  };

  printer.Print(vars, "export interface I$ServiceName$PromiseClient {\n");
  printer.Indent();

  auto methodCount = service->method_count();

  for(auto i=0; methodCount > i; ++i) {
    auto method = service->method(i);
    auto inputType = method->input_type();
    auto outputType = method->output_type();

    vars["methodName"] = GetMethodInterfaceName(method);
    vars["RequestType"] = utils::nodeObjectPath(inputType);
    vars["ResponseType"] = utils::nodeObjectPath(outputType);

    if(method->client_streaming() && method->server_streaming()) {

    } else
    if(method->client_streaming()) {

    } else
    if(method->server_streaming()) {

    } else {
      printer.Print(vars, "$methodName$\n");
      printer.Indent();
      printer.Print(vars, "( request: $RequestType$\n");
      printer.Print(vars, "): Promise<$ResponseType$>;\n");
      printer.Outdent();

      printer.Print(vars, "$methodName$\n");
      printer.Indent();
      printer.Print(vars, "( request: $RequestType$\n");
      printer.Print(vars, ", metadata: grpc.Metadata | null\n");
      printer.Print(vars, "): Promise<$ResponseType$>;\n");
      printer.Outdent();

      printer.Print(vars, "$methodName$\n");
      printer.Indent();
      printer.Print(vars, "( request: $RequestType$\n");
      printer.Print(vars, ", metadata: grpc.Metadata | null\n");
      printer.Print(vars, ", options: grpc.CallOptions | null\n");
      printer.Print(vars, "): Promise<$ResponseType$>;\n\n");
      printer.Outdent();
    }
  }

  printer.Outdent();
  printer.Print(vars, "}\n\n");

  return true;
}

bool GrpcNodeGenerator::PrintServiceClientClass
  ( google::protobuf::io::Printer&              printer
  , const GrpcNodeGeneratorOptions&          options
  , const google::protobuf::ServiceDescriptor*  service
  , std::string*                                error
  ) const
{
  std::map<std::string, std::string> vars = {
    {"ServiceName", service->name()},
    {"ServiceFullName", service->full_name()}
  };

  printer.Print(vars,
    "export interface I$ServiceName$Client extends grpc.Client {\n");
  printer.Indent();

  auto methodCount = service->method_count();

  for(auto i=0; methodCount > i; ++i) {
    auto method = service->method(i);
    auto inputType = method->input_type();
    auto outputType = method->output_type();

    vars["methodName"] = GetMethodInterfaceName(method);
    vars["RequestType"] = utils::nodeObjectPath(inputType);
    vars["ResponseType"] = utils::nodeObjectPath(outputType);
    
    if(method->client_streaming() && method->server_streaming()) {
      printer.Print(vars, "$methodName$\n");
      printer.Indent();
      printer.Print(vars,
        "(): grpc.ClientDuplexStream<$RequestType$, $ResponseType$>;\n");
      printer.Outdent();

      printer.Print(vars, "$methodName$\n");
      printer.Indent();
      printer.Print(vars,
        "(metadata: grpc.Metadata | null): "
        "grpc.ClientDuplexStream<$RequestType$, $ResponseType$>;\n");
      printer.Outdent();
    } else
    if(method->client_streaming()) {
      printer.Print(vars, "$methodName$\n");
      printer.Indent();
      printer.Print(vars, "(): grpc.ClientWritableStream<$RequestType$>;\n");
      printer.Outdent();

      printer.Print(vars, "$methodName$\n");
      printer.Indent();
      printer.Print(vars, "(metadata: grpc.Metadata | null): grpc.ClientWritableStream<$RequestType$>;\n");
      printer.Outdent();
    } else
    if(method->server_streaming()) {
      printer.Print(vars, "$methodName$\n");
      printer.Indent();
      printer.Print(vars, "( request: $RequestType$\n");
      printer.Print(vars, "): grpc.ClientReadableStream<$ResponseType$>;\n");
      printer.Outdent();

      printer.Print(vars, "$methodName$\n");
      printer.Indent();
      printer.Print(vars, "( request: $RequestType$\n");
      printer.Print(vars, ", metadata: grpc.Metadata | null\n");
      printer.Print(vars, "): grpc.ClientReadableStream<$ResponseType$>;\n");
      printer.Outdent();
    } else {
      printer.Print(vars, "$methodName$\n");
      printer.Indent();
      printer.Print(vars, "( request: $RequestType$\n");
      printer.Print(vars, ", callback: grpc.requestCallback<$ResponseType$>\n");
      printer.Print(vars, "): void;\n");
      printer.Outdent();

      printer.Print(vars, "$methodName$\n");
      printer.Indent();
      printer.Print(vars, "( request: $RequestType$\n");
      printer.Print(vars, ", metadata: grpc.Metadata | null\n");
      printer.Print(vars, ", callback: grpc.requestCallback<$ResponseType$>\n");
      printer.Print(vars, "): void;\n");
      printer.Outdent();

      printer.Print(vars, "$methodName$\n");
      printer.Indent();
      printer.Print(vars, "( request: $RequestType$\n");
      printer.Print(vars, ", metadata: grpc.Metadata | null\n");
      printer.Print(vars, ", options: grpc.CallOptions | null\n");
      printer.Print(vars, ", callback: grpc.requestCallback<$ResponseType$>\n");
      printer.Print(vars, "): void;\n\n");
      printer.Outdent();
    }
  }

  printer.Outdent();
  printer.Print(vars, "}\n\n");

  printer.Print(vars, "export interface $ServiceName$ClientConstructor {\n");
  printer.Indent();
  printer.Print(vars, "new ("
    "address: string, "
    "credentials: grpc.ChannelCredentials, "
    "options?: object"
    "): I$ServiceName$Client;\n");
  printer.Outdent();
  printer.Print("}\n\n");

  printer.Print(vars,
    "export const $ServiceName$Client = <$ServiceName$ClientConstructor>\n");
  printer.Indent();
  printer.Print(vars,
    "grpc.makeGenericClientConstructor("
      "$ServiceName$Service, '$ServiceFullName$', {});\n\n");
  printer.Outdent();

  return true;
}

bool GrpcNodeGenerator::GenerateImports
  ( google::protobuf::io::Printer&           printer
  , const GrpcNodeGeneratorOptions&          options
  , const google::protobuf::FileDescriptor*  file
  , std::string*                             error
  ) const
{
  std::map<std::string, std::string> vars;
  printer.Print(vars, "import * as grpc from 'grpc';\n");

  auto fileName = file->name();

  if(file->message_type_count() > 0) {
    std::string filePath = utils::getRelativePath(
      fileName, GetTsMessageFilename(fileName));

    printer.Print("import * as $ModuleAlias$ from '$filePath$';\n", 
      "ModuleAlias", utils::moduleAlias(file->name()),
      "filePath", filePath);
  }

  for (auto i=0; file->dependency_count() > i; ++i) {
    auto dependency = file->dependency(i);
    auto dependencyName = dependency->name();

    std::string filePath = utils::getRelativePath(
      file->name(), GetTsMessageFilename(dependencyName));
    printer.Print("import * as $ModuleAlias$ from '$filePath$';\n",
      "ModuleAlias", utils::moduleAlias(dependencyName),
      "filePath", filePath);
  }

  printer.Print("\n");

  // auto serviceCount = file->service_count();

  // std::map<std::string, std::set<const Descriptor*>> descriptors;

  // for(auto i=0; serviceCount > i; ++i) {
  //   auto service = file->service(i);
  //   auto methodCount = service->method_count();

  //   for(auto i=0; methodCount > i; ++i) {
  //     auto method = service->method(i);
  //     auto inputType = method->input_type();
  //     auto outputType = method->output_type();

  //     auto inputPath = utils::removePathExtname(inputType->file()->name());
  //     auto outputPath = utils::removePathExtname(outputType->file()->name());

  //     if(descriptors.find(inputPath) == descriptors.end()) {
  //       descriptors.insert({inputPath, {}});
  //     }

  //     if(descriptors.find(outputPath) == descriptors.end()) {
  //       descriptors.insert({outputPath, {}});
  //     }

  //     descriptors.find(inputPath)->second.insert(inputType);
  //     descriptors.find(outputPath)->second.insert(outputType);
  //   }
  // }

  // for(auto pair : descriptors) {
  //   auto path = pair.first;
  //   auto lastSlashIndex = path.find_last_of('/');
    
  //   if(lastSlashIndex != std::string::npos) {
  //     path = path.substr(lastSlashIndex + 1);
  //   }

  //   vars["ImportPath"] = "./" + path + "_pb";

  //   printer.Print(vars, "import {\n");
  //   printer.Indent();

  //   for(auto descriptor : pair.second) {
  //     vars["ImportName"] = descriptor->name();
  //     printer.Print(vars, "$ImportName$,\n");
  //   }

  //   printer.Outdent();
  //   printer.Print(vars, "} from '$ImportPath$';\n\n");
  // }

  return true;
}

bool GrpcNodeGenerator::Generate
  ( const google::protobuf::FileDescriptor*        file
  , const std::string&                             parameter
  , google::protobuf::compiler::GeneratorContext*  context
  , std::string*                                   error
  ) const
{
  GrpcNodeGeneratorOptions options(parameter);

  if(options.hasError(error)) {
    return false;
  }

  std::unique_ptr<ZeroCopyOutputStream> indexDtsOutput(
    context->Open(utils::removePathExtname(file->name()) + "_grpc_pb.ts")
  );
  Printer printer(indexDtsOutput.get(), '$');

  auto serviceCount = file->service_count();

  if(serviceCount == 0) {
    printer.Print("// GENERATED CODE -- NO SERVICES IN PROTO\n\n");
  } else {
    printer.Print("// GENERATED CODE\n\n");
  }

  if(!GenerateImports(printer, options, file, error)) {
    return false;
  }

  std::map<std::string, const Descriptor*> messages = GetAllMessages(file);
  for(const auto& it : messages) {
    if(!PrintMessageTransformer(printer, options, it.second, error)) {
      return false;
    }
  }

  for(auto i=0; serviceCount > i; ++i) {
    auto service = file->service(i);

    if(!PrintServiceImplementationInterface(printer, options, service, error)) {
      return false;
    }

    if(!PrintServiceDefinition(printer, options, service, error)) {
      return false;
    }

    if(!PrintServiceClientClass(printer, options, service, error)) {
      return false;
    }

    if(!PrintServicePromiseClientInterface(printer, options, service, error)) {
      return false;
    }
  }

  return true;
}
