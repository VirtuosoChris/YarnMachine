# YarnMachine
Standalone C++ virtual machine for Yarn Spinner.

****
Notes:

There may be commands, markup, etc. built into the "real" Yarn interpreter for Unity or other platforms that are not supported / have to be added yourself.  This is a generic runtime so any commands that affect UI / audio / visuals are meant to be bring-your-own as custom commands.

The accompanying terminal sample shows basic usage of the API.

Some prototype code showing how you might make a more advanced dialogue runner with an IMGUI widget for a real time graphical application can be found at my gist here:
https://gist.github.com/VirtuosoChris/dad8a3e006e6e325964e55507323e41f
It's 1) prototype code, so not guaranteed to be optimal in any way 2) lifted right out of my game so you won't be able to just build and run this
but it's provided as an example / proof of concept for how you might do this.
****

Build Instructions :
 
Meant to be built using cmake;
all dependencies should automatically build and add to the project.

If you open the repo using the open in desktop link, it ought to automatically fetch all submodules.  If it doesn't or if you use the command line, you may get errors for protobuf missing a CMakeLists.txt, and find the directory is empty.  In this event, use

git submodule update --init --recursive

When you first build, the protobuf lib takes a bit to build the first time so be patient.

The cmake should also automatically run protoc on the yarn.proto file to regenerate up to date headers
****
About:

Yarn Spinner is a neat scripting language for branching narrative that is similar to INK and is intuitive because it kind of just resembles a linear screenplay.

https://yarnspinner.dev/

This project is meant to be a standalone (non Unity, engine agnostic) C++ virtual machine and dialogue player that interprets the compiled Yarn script files.

The Github project for the original Yarn Spinner (not affiliated) can be found here:
https://github.com/YarnSpinnerTool/YarnSpinner

and an online sandbox to try the language can be found at:
https://try.yarnspinner.dev/

and the Visual Studio Code plugin (which is my preferred means of authoring scripts) can be found at :
https://marketplace.visualstudio.com/items?itemName=SecretLab.yarn-spinner

This project doesn't interpret .yarn scripts directly; it depends on 3 input files generated by the Yarn Compiler toolchain.  So you'll either need to provide these files with your game or ship the yarn compiler with your game.

1) compiled .yarnc binary files.
The compiled .yarnc files are generated as Google Protobufs
https://developers.google.com/protocol-buffers/docs/reference/cpp-generated

So protobuf is a dependency and protoc is used to generate parsing code from the original yarn_spinner.proto file that defines the Yarn Spinner bytecode format.

2 / 3) the two generated csv files containing the line database, and the metadata.
I am using the following C++ CSV parser as an additional dependency:

https://github.com/vincentlaucsb/csv-parser

Additional Notes, differences from "real" Yarn Spinner:
- All the things mentioned in the tutorial at https://docs.yarnspinner.dev/getting-started/writing-in-yarn were implemented, tested, and should work; beyond that we have serialization including deterministic RNG.
- Since this is C++ / engine agnostic, GUI / user interaction / event loops / threading / etc is bring-your-own.
- This logically includes markup parsing beyond the built-in attributes, eg. as described here:
https://docs.yarnspinner.dev/getting-started/writing-in-yarn/markup
- See the demo in demo.cpp for an example of how to extend the YarnRunnerBase class with a custom dialogue runner for your game
- Built in operators and functions have been tested and implemented and should all work.
- Custom functions and markup callbacks are implemented as std::function callbacks stored in a lookup table
- The sample command binding functionality is supplied via VirtuosoConsole's QuakeStyleConsole.h 
https://github.com/VirtuosoChris/VirtuosoConsole
which can take an arbitrary c++ function and do template magic to generate type correct iostream parsers without writing any code yourself.  But you can do whatever you want with commands, like pass them to a scripting language or whatever else you want.
see instructions or demo.cpp.  This also uses std::function in the implementation.
- The VM state is serializable, and uses nlohmann's c++ JSON library as a dependency to do this:
https://github.com/nlohmann/json
- the std::regex markup parsing should probably be replaced since std::regex is apparently not maintained and horribly slow.
It's absolutely fine for now though.





