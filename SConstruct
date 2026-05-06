#!/usr/bin/env python
import os
import shutil
import subprocess
import sys
from typing import List
# Explicitly import SCons symbols to satisfy VS Code / Pylance
from SCons.Script import SConscript, Glob, Default
from SCons.Environment import Environment as SConsEnvironment

# SCons is used to build the godot-cpp bindings first.
# We type-hint 'env' so IntelliSense knows available methods (Append, ParseConfig, etc.)
env: SConsEnvironment = SConscript("godot-cpp/SConstruct") # type: ignore
env["ENV"].update(os.environ)

# --- Configuration ---

env.Append(CPPPATH=["src"])
env.Append(CCFLAGS=["-std=c++23", "-fPIC","-fvisibility=hidden"])
env.Append(LINKFLAGS=["-fvisibility=hidden"])

# --- Platform: Linux ---
platform: str = env["platform"] # type: ignore
runtime_rpath = "$ORIGIN:/usr/lib:/usr/lib64:/lib:/lib64"
patchelf_path = None

if platform == "linux":
    # Nix compiler wrappers inject store paths through these env vars.
    for key in (
        "NIX_LDFLAGS",
        "NIX_CFLAGS_COMPILE",
        "NIX_CXXSTDLIB_COMPILE",
        "NIX_CC_WRAPPER_FLAGS_SET",
    ):
        env["ENV"].pop(key, None)

    # Use pkg-config to find FFmpeg libraries
    if os.system("pkg-config --exists libavcodec libavformat libavutil libswscale") == 0:
        env.ParseConfig("pkg-config --cflags --libs libavcodec libavformat libavutil libswscale")
    else:
        print("Error: FFmpeg libraries not found via pkg-config.")
        sys.exit(1)

    env.Append(LINKFLAGS=["-Wl,-rpath,$$ORIGIN:/usr/lib:/usr/lib64:/lib:/lib64"])
    patchelf_path = shutil.which("patchelf", path=env["ENV"].get("PATH"))

# --- Sources ---
sources = Glob("src/*.cpp")

# --- Build ---
# Create the shared library
target_name: str = "addons/vtr_texture/bin/vtrtexture{}{}".format(
    env["suffix"], env["SHLIBSUFFIX"] # type: ignore
)

library = env.SharedLibrary(
    target=target_name,
    source=sources,
)

if platform == "linux" and patchelf_path:
    def normalize_runpath(target: List[str], source: List[str], env: SConsEnvironment, **_kwargs: object) -> None:
        subprocess.run(
            [patchelf_path, "--set-rpath", runtime_rpath, str(target[0])],
            check=True,
            env=env["ENV"],
        )
    env.AddPostAction(library, normalize_runpath) # type: ignore

# --- Compilation Database ---
compile_db = env.CompilationDatabase(target="compile_commands.json") # type: ignore

Default(library, compile_db)
