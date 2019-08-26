load("@npm_bazel_typescript//:defs.bzl", "ts_library")
load("@com_google_protobuf//:protobuf.bzl", "proto_gen")

def _GrpcPbTsSrcs(srcs):
    ret = [s[:-len(".proto")] + "_grpc_pb.ts" for s in srcs]
    return ret

def ts_grpc_proto_library(
        name,
        srcs = [],
        deps = [],
        ts_libs = [],
        include = None,
        protoc = "@com_google_protobuf//:protoc",
        **kargs):
"""
"""

    includes = []
    if include != None:
        includes = [include]

    outs = _GrpcPbTsSrcs(srcs)
    grpc_node_plugin = "@com_github_zaucy_protoc_gen_grpc_node//:protoc-gen-grpc-node"

    proto_gen(
        name = name + "_genproto",
        srcs = srcs,
        deps = [s + "_genproto" for s in deps],
        includes = includes,
        protoc = protoc,
        plugin = grpc_node_plugin,
        plugin_language = "grpc-node",
        outs = outs,
        visibility = ["//visibility:public"],
    )

    ts_library(
        name = name,
        srcs = [":" + srcs_target_name],
        deps = ts_libs,
        **kwargs
    )
