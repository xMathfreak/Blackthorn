add_library(BlackthornShared INTERFACE)

target_include_directories(BlackthornShared
	INTERFACE
		${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(BlackthornShared INTERFACE cxx_std_20)
