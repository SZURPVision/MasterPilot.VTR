#!/usr/bin/env python
import os
import shutil
import subprocess
import sys
from typing import List
from SCons.Script import SConscript, Glob, Default
from SCons.Environment import Environment as SConsEnvironment

# 路径感知：默认在当前目录找 godot-cpp，也可通过环境变量覆盖
godot_cpp_path = os.environ.get("GODOT_CPP_PATH", "godot-cpp")

# --- 核心：一致性编译 ---
if os.environ.get("VTR_NIX_BUILD") == "1":
    # Nix 预编译模式：跳过 SConscript，直接链接已编译好的库
    # 这样可以避免在 Nix 构建时重新运行 godot-cpp 的 binding generator
    env = SConsEnvironment()
    env["ENV"].update(os.environ)

    # 模拟 godot-cpp SConscript 提供的基本变量
    target = ARGUMENTS.get("target", "template_debug")
    is_debug = "debug" in target
    env["suffix"] = ".debug.x86_64" if is_debug else ".release.x86_64"
    env["SHLIBSUFFIX"] = ".so"

    # 头文件路径
    env.Append(CPPPATH=[
        os.path.join(godot_cpp_path, "include"),
        os.path.join(godot_cpp_path, "gdextension"),
        os.path.join(godot_cpp_path, "gen", "include"),
    ])

    # 库路径
    env.Append(LIBPATH=[os.path.join(godot_cpp_path, "bin")])

    # 链接静态库
    lib_suffix = "template_debug" if is_debug else "template_release"
    env.Append(LIBS=[f"godot-cpp.linux.{lib_suffix}.x86_64"])

    # 必要的编译宏
    env.Append(CPPDEFINES=["TYPED_METHOD_BIND"])
else:
    if not os.path.exists(os.path.join(godot_cpp_path, "SConstruct")):
        print(f"Error: godot-cpp not found at {godot_cpp_path}. Please check NIX_CONTEXT or submodules.")
        sys.exit(1)

    # 直接调用官方 SConscript
    env: SConsEnvironment = SConscript(os.path.join(godot_cpp_path, "SConstruct")) # type: ignore
    env["ENV"].update(os.environ)

env.Append(CPPPATH=["src"])
env.Append(CCFLAGS=["-std=c++23", "-fPIC"])

# --- FFmpeg Linking ---
env.ParseConfig("pkg-config --cflags --libs libavcodec libavformat libavutil libswscale libavdevice")

# --- 源码与目标 ---
sources = Glob("src/*.cpp")
target_name = "addons/vtr_texture/bin/vtrtexture{}{}".format(
    env["suffix"], env["SHLIBSUFFIX"] # type: ignore
)

library = env.SharedLibrary(target=target_name, source=sources)

# --- 编译数据库 ---
# 在 Nix 构建沙盒中跳过，因为路径是临时的
if hasattr(env, "CompilationDatabase") and os.environ.get("VTR_NIX_BUILD") != "1":
    compile_db = env.CompilationDatabase(target="compile_commands.json") # type: ignore
    Default(library, compile_db)
else:
    Default(library)
