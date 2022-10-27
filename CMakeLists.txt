cmake_minimum_required(VERSION 3.22)

# set the project name
project(Project_3_Matrix)

aux_source_directory(./ DIR_SRCS)


# add the executable
add_executable(test ${DIR_SRCS})