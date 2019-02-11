add_library(hdlc hdlc.c)
set(SOURCES
    ax25.c
    bell202.c
    util.c
    main.c
)
add_executable(liquidwolf ${SOURCES})
target_link_libraries(liquidwolf m ${LIQUIDDSP} ${SNDFILE} hdlc)
