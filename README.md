# YarnMachine
(WIP) Standalone C++ virtual machine for Yarn Spinner.

Yarn Spinner is a neat scripting language for branching narrative that is similar to INK and is intuitive because it kind of just resembles a linear screenplay.

https://yarnspinner.dev/

The Github project for the original Yarn Spinner (which I'm unaffiliated with) can be found here:
https://github.com/YarnSpinnerTool/YarnSpinner

This project is meant to be a standalone (non Unity, engine agnostic) virtual machine that interprets the compiled .yarnc script files

The compiled .yarnc files are generated as Google Protobufs
https://developers.google.com/protocol-buffers/docs/reference/cpp-generated

So protobuf is a dependency and protoc is used to generate parsing code from the original yarn_spinner.proto file that defines the Yarn Spinner bytecode format.

The command binding functionality is supplied via VirtuosoConsole's QuakeStyleConsole.h 
https://github.com/VirtuosoChris/VirtuosoConsole

Since this is C++ / engine agnostic, GUI is bring-your-own.

Tutorials and easy dependency fetching / building to come when the actual thing works :)
