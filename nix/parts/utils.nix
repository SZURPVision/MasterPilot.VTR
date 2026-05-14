{ ... }: {
  perSystem = { pkgs, self', ... }: {
    packages = pkgs.lib.optionalAttrs pkgs.stdenv.isLinux {
      fhsEnv = pkgs.buildFHSEnv {
        name = "fhsEnv";
        targetPkgs = pkgs: with pkgs; [
          scons
          pkg-config
          self'.packages.ffmpeg-vtr
          self'.packages.godot-cpp
          gcc
          patchelf
          python3
          glibc.dev
          zlib
          alsa-lib
        ];
        runScript = "bash";
      };
    };
  };
}
