set(ACSVM_NOFLAGS ON)
set(ACSVM_SHARED ON)
add_subdirectory(ACSVM)
target_sources(SRB2SDL2 PRIVATE
	environment.cpp
	environment.hpp
	thread.cpp
	thread.hpp
	call-funcs.cpp
	call-funcs.hpp
	stream.cpp
	stream.hpp
	interface.cpp
	interface.h
)
