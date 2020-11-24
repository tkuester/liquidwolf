add_executable(liquidwolf ${liquidwolf_SOURCES})

target_compile_definitions(
	liquidwolf
	PUBLIC
	_LIQUIDWOLF_VERSION=\"${GIT_DESCRIBE}\"
)

target_sources(
	liquidwolf
	PUBLIC
	aprs_rx.c
	ax25.c
	bell202.c
	hdlc.c
	stdin_src.c
	util.c
	wav_src.c
	main.c
)

target_link_libraries(liquidwolf m ${LIQUIDDSP} ${SNDFILE})
