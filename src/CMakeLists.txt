set(SOURCES
    ax25.c
    hdlc.c
    bell202.c
    util.c
    wav_src.c
    stdin_src.c
    aprs_rx.c
    main.c
)
add_executable(liquidwolf ${SOURCES})
target_link_libraries(liquidwolf m ${LIQUIDDSP} ${SNDFILE})
