# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.6

# Delete rule output on recipe failure.
.DELETE_ON_ERROR:


#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:


# Remove some rules from gmake that .SUFFIXES does not remove.
SUFFIXES =

.SUFFIXES: .hpux_make_needs_suffix_list


# Suppress display of executed commands.
$(VERBOSE).SILENT:


# A target that is always out of date.
cmake_force:

.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

# The shell in which to execute make rules.
SHELL = /bin/sh

# The CMake executable.
CMAKE_COMMAND = /usr/local/clion-2016.3.2/bin/cmake/bin/cmake

# The command to remove a file.
RM = /usr/local/clion-2016.3.2/bin/cmake/bin/cmake -E remove -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /home/rob/370/assignment

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/rob/370/assignment/cmake-build-debug

# Include any dependencies generated for this target.
include CMakeFiles/assignment.dir/depend.make

# Include the progress variables for this target.
include CMakeFiles/assignment.dir/progress.make

# Include the compile flags for this target's objects.
include CMakeFiles/assignment.dir/flags.make

CMakeFiles/assignment.dir/main.c.o: CMakeFiles/assignment.dir/flags.make
CMakeFiles/assignment.dir/main.c.o: ../main.c
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/rob/370/assignment/cmake-build-debug/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building C object CMakeFiles/assignment.dir/main.c.o"
	/usr/bin/cc  $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -o CMakeFiles/assignment.dir/main.c.o   -c /home/rob/370/assignment/main.c

CMakeFiles/assignment.dir/main.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/assignment.dir/main.c.i"
	/usr/bin/cc  $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /home/rob/370/assignment/main.c > CMakeFiles/assignment.dir/main.c.i

CMakeFiles/assignment.dir/main.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/assignment.dir/main.c.s"
	/usr/bin/cc  $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /home/rob/370/assignment/main.c -o CMakeFiles/assignment.dir/main.c.s

CMakeFiles/assignment.dir/main.c.o.requires:

.PHONY : CMakeFiles/assignment.dir/main.c.o.requires

CMakeFiles/assignment.dir/main.c.o.provides: CMakeFiles/assignment.dir/main.c.o.requires
	$(MAKE) -f CMakeFiles/assignment.dir/build.make CMakeFiles/assignment.dir/main.c.o.provides.build
.PHONY : CMakeFiles/assignment.dir/main.c.o.provides

CMakeFiles/assignment.dir/main.c.o.provides.build: CMakeFiles/assignment.dir/main.c.o


# Object files for target assignment
assignment_OBJECTS = \
"CMakeFiles/assignment.dir/main.c.o"

# External object files for target assignment
assignment_EXTERNAL_OBJECTS =

assignment: CMakeFiles/assignment.dir/main.c.o
assignment: CMakeFiles/assignment.dir/build.make
assignment: CMakeFiles/assignment.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/home/rob/370/assignment/cmake-build-debug/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Linking C executable assignment"
	$(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/assignment.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
CMakeFiles/assignment.dir/build: assignment

.PHONY : CMakeFiles/assignment.dir/build

CMakeFiles/assignment.dir/requires: CMakeFiles/assignment.dir/main.c.o.requires

.PHONY : CMakeFiles/assignment.dir/requires

CMakeFiles/assignment.dir/clean:
	$(CMAKE_COMMAND) -P CMakeFiles/assignment.dir/cmake_clean.cmake
.PHONY : CMakeFiles/assignment.dir/clean

CMakeFiles/assignment.dir/depend:
	cd /home/rob/370/assignment/cmake-build-debug && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/rob/370/assignment /home/rob/370/assignment /home/rob/370/assignment/cmake-build-debug /home/rob/370/assignment/cmake-build-debug /home/rob/370/assignment/cmake-build-debug/CMakeFiles/assignment.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : CMakeFiles/assignment.dir/depend

