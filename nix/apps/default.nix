{ ... }:
{
  perSystem = { pkgs, self', ... }:
  {
    apps.install = import ./install { inherit pkgs; vtr = self'.packages.debug; };
  };
}