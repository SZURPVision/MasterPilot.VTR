{ ... }: {
  perSystem = { pkgs, self', ... }: 
  let
    vtrSrc = pkgs.lib.cleanSourceWith {
      src = ../../.;
      filter = path: type: 
        let base = baseNameOf path; in
        (type == "directory" && (base == "src" || base == "addons")) ||
        (base == "SConstruct" || 
         pkgs.lib.hasSuffix ".cpp" base || 
         pkgs.lib.hasSuffix ".h" base || 
         pkgs.lib.hasSuffix ".hpp" base);
    };

    mkInstallExample = pkg : pkgs.writeShellScriptBin "install-vtr-example" ''
      DEST="example/addons"
      mkdir -p "$DEST"
      ln -sf ${pkg}/addons/* "$DEST/"
    '';

    mkVtr = { target, devBuild ? false, fhs ? false }: pkgs.stdenv.mkDerivation {
      pname = "vtr-${target}${pkgs.lib.optionalString devBuild "-dev"}";
      version = "0.1.0";
      src = vtrSrc;

      nativeBuildInputs = [ pkgs.scons pkgs.pkg-config ] ++ pkgs.lib.optional fhs pkgs.patchelf;
      buildInputs = [ self'.packages.ffmpeg-vtr self'.packages.godot-cpp ];

      VTR_NIX_BUILD = if fhs then "1" else "0";

      dontPatchELF = fhs;
      dontPatchShebangs = fhs;

      buildPhase =
        let
          prepareScript = ''
            mkdir -p godot-cpp
            cp -r ${self'.packages.godot-cpp}/* godot-cpp/
            chmod -R +w godot-cpp
          '';

          buildScript = ''
            scons platform=linux \
                  target=${target} \
                  ${pkgs.lib.optionalString devBuild "dev_build=yes"}
          '';

        in ''
            ${prepareScript}
            ${buildScript}
          '';

      installPhase = ''
        mkdir -p $out/addons
        cp -r addons/vtr_texture $out/addons/
      '';

      postFixup = pkgs.lib.optionalString fhs ''
        find $out/addons/vtr_texture/bin -name "*.so" -exec ${pkgs.patchelf}/bin/patchelf --set-rpath "" {} \;
      '';
    };

  in {
    packages = {
      vtr-debug = mkVtr { target = "template_debug"; };
      vtr-debug-dev = mkVtr { target = "template_debug";  };
      vtr-release = mkVtr { target = "template_release";  };
      vtr-release-fhs = mkVtr { target = "template_release"; devBuild = false; fhs = true;} ;
    };

    # 将输出放到example/addons/, 方便调试
    apps.install-example = {
      type = "app";
      program = mkInstallExample self'.packages.vtr-debug;
    };
  };
}
