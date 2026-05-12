{
  stdenv,
  scons,
  src,
  python3,
}:
stdenv.mkDerivation {
  pname = "godot-cpp";
  version = "4.6";
  inherit src;

  nativeBuildInputs = [scons python3];

  buildPhase = ''
    scons platform=linux target=template_debug
    scons platform=linux target=template_release
  '';

  installPhase = ''
    mkdir -p $out
    cp -r . $out/
  '';
}
