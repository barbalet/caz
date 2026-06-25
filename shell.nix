# shell.nix — development environment for Caz on NixOS.
#
# Caz is a small, dependency-free C99 project built with GNU make and a C
# compiler (it only uses the standard library). Enter the environment and
# build the command-line simulator:
#
#     nix-shell
#     make
#     ./build/caz
#
# Or in one shot, without dropping into an interactive shell:
#
#     nix-shell --run 'make && ./build/caz --list'
#
{ pkgs ? import <nixpkgs> { } }:

pkgs.mkShell {
  name = "caz-dev";

  # Build tools only. The Makefile invokes `cc` (CC ?= cc) and GNU make;
  # nixpkgs' cc-wrapper exposes the compiler as both `gcc` and `cc`.
  packages = with pkgs; [
    gcc
    gnumake
  ];

  shellHook = ''
    echo "caz dev shell — build with 'make', then run './build/caz'"
  '';
}
