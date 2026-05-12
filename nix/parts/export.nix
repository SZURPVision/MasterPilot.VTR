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

    mkVtr = { target, devBuild ? false }: pkgs.stdenv.mkDerivation {
      pname = "vtr-${target}${pkgs.lib.optionalString devBuild "-dev"}";
      version = "0.1.0";
      src = vtrSrc;

      nativeBuildInputs = [ pkgs.scons pkgs.pkg-config ];
      buildInputs = [ self'.packages.ffmpeg-vtr self'.packages.godot-cpp ];

      VTR_NIX_BUILD = "1";

      buildPhase = ''
        echo "Hydrating godot-cpp build cache into sandbox..."
        mkdir -p godot-cpp
        cp -r ${self'.packages.godot-cpp}/* godot-cpp/
        chmod -R +w godot-cpp
        
        echo "Executing SCons (Incrementally in sandbox)..."
        scons platform=linux \
              target=${target} \
              ${pkgs.lib.optionalString devBuild "dev_build=yes"}
      '';

      installPhase = ''
        mkdir -p $out/addons/vtr_texture/bin
        find addons/vtr_texture/bin -name "*.so" -exec cp -v {} $out/addons/vtr_texture/bin/ \;
      '';
    };

    installScript = pkgs.writeShellScriptBin "vtr-install" ''
      target=''${1:-debug}
      OUT_PATH=$(nix build .#vtr-''${target} --no-link --print-out-paths)
      [ -z "$OUT_PATH" ] && exit 1
      mkdir -p addons/vtr_texture/bin
      ln -sf $OUT_PATH/addons/vtr_texture/bin/* addons/vtr_texture/bin/
    '';

  in {
    packages = {
      vtr-debug = mkVtr { target = "template_debug"; devBuild = false; };
      vtr-debug-dev = mkVtr { target = "template_debug"; devBuild = true; };
      vtr-release = mkVtr { target = "template_release"; devBuild = false; };
      build-nix = self'.packages.vtr-debug;
      build-nix-dev = self'.packages.vtr-debug-dev;
      build-fhs = self'.packages.vtr-fhs;
    };
    apps.install = { type = "app"; program = "${installScript}/bin/vtr-install"; };
  };
}
