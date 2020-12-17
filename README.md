welcome to neuropil
===================

## Build instructions
### Manual build

#### required installations

**mandatory:**
- libsodium - crypto library

**optional:**
- criterion - test suite framework
- sphinx - python documentation building
- scons - python build tool
- clang - our preferred C compiler
- ncurses - for the displays in the example programs

#### building the library and example programs

clone the repository from https://neuropil.org/repo/neuropil/ with mercurial.

cd into the folder and build the code with scons.

build in debug mode:

	scons debug=1

build in release mode:

	scons release=1

build the documentation (sphinx installation required):

	scons doc=1

build the tests (criterion installation required):

    scons test=1

clean the project:

	scons -c

there is also a Makefile available, but some path infos are hard coded, sorry.
the Makefile is mainly used to run the llvm scan-build tooling for a static code analysis

no autoconf or cmake available until now, to be done

### building with Nix

Neuropil is available for the [nix ecosystem](https://nixos.org/) which provides reproducible builds of the library and clean development environments.

This project comes as a nix flake but comes with wrappers for nix `< 3.x`.

It provides the following packages:
- libneuropil
- integration.luajit
- integration.python
- shell

Although this is probably **not** what you want to do (see next section), building the individual packages works as follows:

| Command                                                                    | Description                                                                                                                                                           |
| -------------------------------------------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `nix build ./`                                                             | Build library and header files                                                                                                                                        |
| `nix build ./#packages.x86_64-{darwin,linux}.neuropil_{python,luajit}` | builds python module or lua module respectively                                                                                                                         |
| `nix develop` or `nix-shell -A devShell.x86_64-{darwin,linux}`             | opens a shell with libneuropil on `LDPATH` and lua/python bindings available. Use this to build C sources manually (e.g. `$CC examples/neuropil_sender.c -lneuropil`)       |
| `nix shell ./#packages.x86_64-{darwin,linux}.libneuropil`                  | opens a shell with neuropil's dependencies available, allows the common workflow using `scons debug=1` and `scons release=1`.                                               |

To open the shell/build the library without cloning the repository, using nix flakes replace `./` by `git+<url to repo>` or `gitlab:pi-lar/neuropil` for the latest branch.

#### importing from nix

In most cases though you will want to integrate `neuropil` into your project using nix. To make neuropil part of your application you can simply import the flake and add its overlay to your package set like this for example:

```nix
{nixpkgs}:
let
	// add rev=... to point to a specific version
	neuropil = import (fetchGit { url = "https://gitlab.com/pi-lar/neuropil.git"; });
	pkgs = import nixpkgs { overlays = [neuropil.overlay]};
	python = pkgs.python3.withPackages (ps: with ps; [ neuropil_python <other python>])
in with pkgs; mkDerivarion {
	...
	buildInputs = [libneuropil neuropil_luajit python <other deps>]
	...
}
```
If your project is flake based just add this repo as input and import the overlay.

#### Python Module on Nix

Currently the `cffi` library on python 3.8 fails to parse a critical header file in the stddef. We therefore provide the library for python3.7 only until this is resolved.


## directory structure

input directories:
 - src - c source code
 - examples - c source code for the example programs
 - test - c test source code
 - include - c header files
 - lib - used as a third party library directory
 - doc - sphinx documentation source files

output directories:
 - build - library, object files and documentation
 - bin - example executables and test suite

4) running the programs

you can run the executables just as any executable, please have a look at the parameters
of each program:

example 1: run the controller on port 1111

	./bin/neuropil_controller -b 1111

example 2: run a node on port 2222 and send a join message to another node:

	./bin/neuropil_node -b 2222 -j b3b680a867849efe5886a5db751392e9d3079779e3f3c240ed849c11f4ba7d4a:udp6:test.local:3141

example 3: run a node on port 2222 and send a wildcard join message to another node:

	./bin/neuropil_node -b 2222 -j *:udp6:test.local:3141

to run the test suites please us the parameter "-j1" to limit parallel execution.
usually we use the following command:

	./bin/neuropil_test_suite --tap -j1
