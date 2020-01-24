project(drv-stm32-bootloader)

include(${CPATH}/src/github.com/lobaro/c-build/build.cmake)
cbuild_module()

cbuild_dependency(_ github.com/lobaro/c-utils)
cbuild_dependency(_ github.com/lobaro/util-ringbuf)

