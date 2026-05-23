{
  src,
  lib,
  stdenv,
  scons,
  pkg-config,
  patchelf,
  godot-cpp,
  ffmpeg,
  # Build control
  withDebug ? true,
  withRelease ? true,
  devBuild ? false,
  fhs ? false,
}:
stdenv.mkDerivation {
  pname = "vtr";
  version = "0.1.0";
  inherit src;

  nativeBuildInputs = [ scons pkg-config ] ++ lib.optional fhs patchelf;
  buildInputs = [ ffmpeg godot-cpp ];

  VTR_NIX_BUILD = "1";
  GODOT_CPP_PATH = godot-cpp;

  dontPatchELF = fhs;
  dontPatchShebangs = fhs;

  buildPhase = ''
    if [ "${if withRelease then "1" else "0"}" = "1" ]; then
      scons platform=linux target=template_release dev_build=no
    fi
    if [ "${if withDebug then "1" else "0"}" = "1" ]; then
      scons platform=linux target=template_debug dev_build=${if devBuild then "yes" else "no"}
    fi
  '';

  installPhase = ''
    mkdir -p $out/addons/vtr/bin
    
    # 1. Copy generated binaries
    if [ -d "addons/vtr/bin" ]; then
      cp -r addons/vtr/bin/* $out/addons/vtr/bin/
    fi

    # 2. Copy metadata files
    find addons/vtr -maxdepth 1 -type f -name "*.uid" -exec cp {} $out/addons/vtr/ \;

    # 3. Dynamically generate .gdextension
    cat > $out/addons/vtr/vtr.gdextension <<EOF
[configuration]
entry_symbol = "vtr_library_init"
compatibility_minimum = "4.6"
reloadable = true

[libraries]
${lib.optionalString withDebug "linux.debug.x86_64 = \"res://addons/vtr/bin/libvtr.linux.template_debug.x86_64.so\""}
${lib.optionalString withRelease "linux.release.x86_64 = \"res://addons/vtr/bin/libvtr.linux.template_release.x86_64.so\""}
EOF
  '';

  postFixup = lib.optionalString fhs ''
    find $out/addons/vtr/bin -name "*.so" -exec ${patchelf}/bin/patchelf --set-rpath "" {} \;
  '';
}
