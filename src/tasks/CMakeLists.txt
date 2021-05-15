# CMakeList.txt : CMake project for CMakeProject1, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)


# Add source to this project's executable.
add_library (tasks STATIC
	"include/tasks/detail/win32.hpp"

	"include/tasks/config.h"
	"include/tasks/task.h" 
	"include/tasks/shared_task.h" 
	"include/tasks/static_thread_pool.h" 

	"src/static_thread_pool.cpp"

	"src/auto_reset_event.hpp"
	"src/auto_reset_event.cpp"

	"src/spin_mutex.hpp"
	"src/spin_mutex.cpp"

	"src/spin_wait.hpp"
	"src/spin_wait.cpp"
	"src/win32.cpp" "include/tasks/detail/sync_wait_task.hpp" "include/tasks/detail/lightweight_manual_reset_event.hpp" "src/lightweight_manual_reset_event.cpp")

SET_PROJECT_WARNINGS(tasks)
target_include_directories(tasks PUBLIC include)
target_link_libraries(tasks PRIVATE Synchronization.lib)
format_pre_built(tasks)
